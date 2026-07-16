/* USER CODE BEGIN SUNLIGHT_R12_APPLICATION */
/* SUNLIGHT R12 主控制模块。 */
#define SystemClock_Config  SunlightR12_InternalClockConfig
#define Error_Handler       SunlightR12_ErrorHandler
#define assert_failed       SunlightR12_AssertFailed

#include "sunlight_r12.h"
#include "main.h"
#include "adc.h"
#include "gpio.h"
#include "i2c.h"
#include "tim.h"

#include "oled.h"

#include <stdio.h>
#include <string.h>

#define ARRAY_COUNT(array)                    (sizeof(array) / sizeof((array)[0]))

#define PWM_FULL_SCALE                        1000U
#define CHARGE_PATH_STOP_COMPARE              0U
#define CHARGE_PATH_FULL_COMPARE              PWM_FULL_SCALE
#define LED_MOS_DIRECT_COMPARE                0U
#define LED_MOS_OFF_COMPARE                   PWM_FULL_SCALE
#define ADC_PWM_PHASE_SAMPLES                 20U
#define ADC_PWM_WAIT_TIMEOUT_MS               5U
#define SOLAR_PROBE_ADC_SAMPLES               8U
#define SOLAR_PROBE_DAY_CURRENT_MA             50L
#define SOLAR_PROBE_DAY_INTERVAL_CONTROLS     100U
#define SOLAR_PROBE_NIGHT_INTERVAL_CONTROLS   600U
#define SOLAR_PROBE_SETTLE_COUNTER             500U
#define SOLAR_START_PROBE_RECOVERY_MS          200U
#define SOLAR_PROBE_GUARD_INTERVAL_MS          10U
#define SOLAR_PROBE_GUARD_ADC_SAMPLES          4U
#define SOLAR_PROBE_RESULT_ERROR               0U
#define SOLAR_PROBE_RESULT_VALID               1U
#define SOLAR_PROBE_RESULT_OVERVOLTAGE         2U
#define LED_CURRENT_FILTER_SAMPLES            4U
#define BATTERY_VOLTAGE_FILTER_SAMPLES        40U
#define CHARGE_CURRENT_FILTER_SAMPLES         40U
#define CHARGE_BURST_WINDOW_STEPS             40U
#define CHARGE_BURST_MIN_ON_STEPS             4U
#define CHARGE_SOLAR_STABLE_ON_STEPS          4U
#define BATTERY_REST_SAMPLE_DELAY_TICKS        20U
#define ADC_REFERENCE_MV                      3300L
#define ADC_FULL_SCALE                        4095L

/* VBAT 分压采样。 */
#define BATTERY_VOLTAGE_SCALE_NUMERATOR       147L
#define BATTERY_VOLTAGE_SCALE_DENOMINATOR     47L
#define BATTERY_CAL_SCALE_DENOMINATOR         10000L
#define BATTERY_LED_ON_CAL_NUMERATOR          10874L
#define BATTERY_LED_ON_OFFSET_MV              (-289L)
#define BATTERY_LED_OFF_CAL_NUMERATOR         10073L
#define BATTERY_LED_OFF_OFFSET_MV             (-34L)
#define BATTERY_LOAD_BLEND_DENOMINATOR        1000L
/* 根据实测值修正 VBAT 映射。 */
#define BATTERY_FINAL_CORRECTION_MV            50L
/* 根据静置电压实测值修正 VBAT 映射。 */
#define BATTERY_REST_CAL_LOW_INPUT_MV         6400L
#define BATTERY_REST_CAL_MID_INPUT_MV         7610L
#define BATTERY_REST_CAL_MID_OUTPUT_MV        7670L
#define BATTERY_REST_CAL_HIGH_INPUT_MV        8020L
#define BATTERY_REST_CAL_HIGH_OUTPUT_MV       8120L
#define LED_CURRENT_SENSE_GAIN                10L
#define LED_CURRENT_SHUNT_MILLIOHMS           1000L
#define LED_CURRENT_RAW_HIGH_OFFSET_MA         10L
/* 根据实测值调节 PA2 与 LED 电流的映射。 */
#define LED_CURRENT_CAL_KNEE_MA               100L
#define LED_CURRENT_CAL_INDICATED_SPAN_MA     200L
#define LED_CURRENT_CAL_PHYSICAL_SPAN_MA      188L
#define CHARGE_CURRENT_SENSE_GAIN             10L
#define CHARGE_CURRENT_SHUNT_MILLIOHMS        500L

#define OLED_I2C_ADDRESS_PRIMARY              0x78U
#define OLED_I2C_ADDRESS_SECONDARY            0x7AU
#define OLED_RETRY_INTERVAL_TICKS             100U
#define OLED_I2C_RECOVERY_PULSES              9U
#define OLED_I2C_RECOVERY_DELAY_MS            1U
#define OLED_CHARGE_SWITCH_GUARD_TICKS        20U

#define ENABLE_BATTERY_UNDERVOLTAGE_PROTECTION 1
#define ENABLE_SOLAR_CHARGING                  1
#define ENABLE_LED_OUTPUT                      1

#define BATTERY_CHARGE_RESTART_MV             7800L
#define BATTERY_CV_ENTRY_MV                   7800L
#define BATTERY_CV_RECOVERY_MV                7750L
#define BATTERY_CV_SOFT_HIGH_MV               8200L
#define BATTERY_CV_MIN_DUTY_MV                8300L
#define BATTERY_CHARGE_STOP_MV                8450L
#define BATTERY_CV_END_MIN_MV                 8200L
#define BATTERY_CHARGE_END_CURRENT_MA         100L
#define BATTERY_CHARGE_END_SAMPLES            100U
#define BATTERY_CHARGE_RESTART_SAMPLES        20U
#define BATTERY_STOP_CONFIRM_SETTLE_TICKS     200U
#define BATTERY_STOP_CONFIRM_SAMPLES          60U
#define CHARGE_BULK_COMPARE                   900U
#define CHARGE_CV_ENTRY_COMPARE               500L
#define CHARGE_CV_SOFT_HIGH_COMPARE           200L
#define CHARGE_CV_RECOVERY_STEP               20L
#define CHARGE_CV_CONFIRM_REDUCE_STEP          50L
#define CHARGE_CV_CONFIRM_RESUME_MAX          250L
#define CHARGE_CV_MIN_COMPARE                 100L
#define BATTERY_CUTOFF_MV                     6000L
#define BATTERY_RECOVERY_MV                   6400L
#define BATTERY_RECOVERY_SAMPLES              20U

#define LED_CURRENT_MIN_MA                    0U
#define LED_CURRENT_HARDWARE_MAX_MA           300U
#define LED_CURRENT_DEFAULT_MA                300U
#define LED_CURRENT_STEP_MA                   25U
#define LED_CURRENT_DEADBAND_MA               1L
#define TIMED_ON_STEPS_50MS                   200U
#define POWER_PATH_BREAK_TICKS                20U

#define BUTTON_DEBOUNCE_TICKS                 3U

typedef enum
{
  SYSTEM_MODE_AUTO = 0,
  SYSTEM_MODE_FORCE_ON,
  SYSTEM_MODE_FORCE_OFF
} SystemMode;

typedef enum
{
  LED_STOP_NONE = 0,
  LED_STOP_ADC_ERROR,
  LED_STOP_LOW_BATTERY,
  LED_STOP_SOLAR_UNKNOWN,
  LED_STOP_DAYLIGHT,
  LED_STOP_CHARGE_REST,
  LED_STOP_FORCE_OFF,
  LED_STOP_TIMER_EXPIRED
} LedStopReason;

typedef enum
{
  CHARGE_STATE_IDLE = 0,
  CHARGE_STATE_BULK,
  CHARGE_STATE_CV,
  CHARGE_STATE_DONE
} ChargeState;

typedef enum
{
  POWER_PATH_SAFE = 0,
  POWER_PATH_CHARGE,
  POWER_PATH_LED
} PowerPathState;

typedef struct
{
  uint16_t pin;
  uint8_t stable_pressed;
  uint8_t transition_ticks;
} ButtonState;

static volatile uint32_t g_tick_10ms;
static uint32_t g_control_last_tick;
static uint8_t g_control_started;
static uint8_t g_heartbeat_pwm_phase;
static uint8_t g_heartbeat_level = 10U;
static uint8_t g_heartbeat_rising;
static uint8_t g_heartbeat_step_ms;

static int32_t g_vbt_uv_mv;
static int32_t g_vbt_instant_mv;
static int32_t g_vbt_safety_mv;
static int32_t g_vbt_terminal_avg_mv;
static int32_t g_vbt_rest_mv;
static int32_t g_vbt_mv;
static int32_t g_solar_probe_current_ma;
static int32_t g_iled_raw_instant_ma;
static int32_t g_iled_raw_ma;
static int32_t g_iled_ma;
static int32_t g_icharge_raw_ma;
static int32_t g_icharge_ma;
static int32_t g_iled_filter_sum;
static int32_t g_iled_filter_buffer[LED_CURRENT_FILTER_SAMPLES];
static uint8_t g_iled_filter_index;
static uint8_t g_iled_filter_count;
static int32_t g_vbt_filter_sum;
static int32_t g_vbt_filter_buffer[BATTERY_VOLTAGE_FILTER_SAMPLES];
static uint8_t g_vbt_filter_index;
static uint8_t g_vbt_filter_count;
static int32_t g_icharge_filter_sum;
static int32_t g_icharge_filter_buffer[CHARGE_CURRENT_FILTER_SAMPLES];
static uint8_t g_icharge_filter_index;
static uint8_t g_icharge_filter_count;

static uint16_t g_pwm_charge;
static uint16_t g_charge_actual_compare;
static uint16_t g_pwm_led = LED_MOS_OFF_COMPARE;
static uint16_t g_target_iled_ma = LED_CURRENT_DEFAULT_MA;
static uint16_t g_timed_on_steps;

static SystemMode g_system_mode = SYSTEM_MODE_AUTO;
static LedStopReason g_led_stop_reason = LED_STOP_NONE;
static ChargeState g_charge_state = CHARGE_STATE_IDLE;
static PowerPathState g_power_path_state = POWER_PATH_SAFE;
static PowerPathState g_power_path_pending = POWER_PATH_SAFE;

static uint8_t g_sensor_valid;
static uint8_t g_vbt_rest_valid;
static uint8_t g_vbt_rest_window_sampled;
static uint8_t g_battery_ready;
static uint8_t g_battery_recovery_samples;
static uint8_t g_solar_dark = 1U;
static uint8_t g_solar_charge_available;
static uint8_t g_solar_probe_updated;
static uint8_t g_solar_state_valid;
static uint8_t g_solar_probe_request = 1U;
static uint16_t g_solar_probe_countdown;
static uint8_t g_charge_end_current_samples;
static uint8_t g_charge_restart_samples;
static uint8_t g_charge_stop_confirm_samples;
static uint8_t g_charge_stop_confirm_pending;
static int32_t g_charge_stop_confirm_sum_mv;
static uint32_t g_charge_stop_confirm_start_tick;
static uint32_t g_charge_stop_confirm_last_sample_tick;
static uint32_t g_charge_end_last_sample_tick;
static uint16_t g_charge_resume_pwm;
static uint8_t g_charge_burst_phase;
static uint8_t g_charge_burst_on_steps;
static uint8_t g_charge_burst_window_complete;
static uint8_t g_charge_output_on;
static uint8_t g_charge_on_run_steps;
static uint8_t g_charge_off_run_steps;
static uint32_t g_charge_off_start_tick;
static uint8_t g_led_enabled;
static uint8_t g_led_output_active;
static uint8_t g_led_direct_drive = 1U;
static uint32_t g_power_path_safe_until_tick;

static uint8_t g_oled_ready;
static uint8_t g_oled_recovery_pending;
static uint16_t g_oled_address = OLED_I2C_ADDRESS_PRIMARY;
static uint32_t g_last_charge_switch_tick;
static uint32_t g_oled_retry_not_before_tick;

static ButtonState g_buttons[] =
{
  {GPIO_PIN_5, 0U, 0U},
  {GPIO_PIN_6, 0U, 0U},
  {GPIO_PIN_7, 0U, 0U},
  {GPIO_PIN_8, 0U, 0U}
};

void SystemClock_Config(void);

static uint8_t ReadAdcAverage(uint32_t channel, uint16_t *raw);
static uint8_t ReadAdcBurstAverage(uint32_t channel, uint16_t *raw,
                                   uint32_t sample_count);
static uint8_t WaitForPwmWrap(void);
static void SetChargeHardwareCompare(uint16_t compare);
static uint8_t ProbeSolarCurrent(uint16_t *raw_icharge);
static uint8_t UpdateSensorData(void);
static int32_t CalibrateBatteryVoltage(int32_t raw_mv,
                                       uint16_t led_pwm_compare);
static int32_t CorrectRestedBatteryVoltage(int32_t calibrated_mv);
static int32_t FilterBatteryVoltage(int32_t sample_mv);
static int32_t FilterLedRawCurrent(int32_t sample_ma);
static int32_t FilterChargeCurrent(int32_t sample_ma);
static int32_t DivideRounded(int32_t numerator, int32_t denominator);
static uint16_t ChargeCvDutyLimit(int32_t battery_mv);
static int32_t CalibrateLedCurrent(int32_t raw_ma);
static void ProcessButtons10ms(void);
static void HandleButtonPress(uint8_t button_index);
static void StepLedCurrent(int8_t direction);
static uint8_t SetLedCurrentTarget(uint16_t target_ma);
static uint16_t LedControlRawTargetMa(uint16_t target_ma);
static void Control50ms(void);
static void UpdateSolarState(void);
static void UpdateBatteryProtection(void);
static void BeginChargeStopConfirmation(void);
static void UpdateChargeControl(void);
static void UpdateLedControl(void);
static void DrivePowerPathSafe(void);
static void ApplyPwmOutputs(void);
static uint8_t RecoverOledI2cBus(void);
static uint8_t TryInitializeOled(void);
static HAL_StatusTypeDef ShowStartupHello(void);
static void UpdateDisplay500ms(void);
static void InitializePowerOutputs(void);
static const char *SystemModeText(void);
static const char *ChargeStateText(void);

void SunlightR12_PreClockInit(void)
{
  /* 设置充电和 LED 功率通路的安全初值。 */
  InitializePowerOutputs();
}

void SunlightR12_HeartbeatTick1ms(void)
{
  if (!__HAL_RCC_GPIOC_IS_CLK_ENABLED())
  {
    return;
  }

  g_heartbeat_pwm_phase++;
  if (g_heartbeat_pwm_phase >= 10U)
  {
    g_heartbeat_pwm_phase = 0U;
  }

  g_heartbeat_step_ms++;
  if (g_heartbeat_step_ms >= 100U)
  {
    g_heartbeat_step_ms = 0U;
    if (g_heartbeat_rising)
    {
      if (g_heartbeat_level < 10U)
      {
        g_heartbeat_level++;
      }
      else
      {
        g_heartbeat_rising = 0U;
      }
    }
    else if (g_heartbeat_level > 0U)
    {
      g_heartbeat_level--;
    }
    else
    {
      g_heartbeat_rising = 1U;
    }
  }

  /* PC13 为低电平点亮，通过软件 PWM 形成呼吸效果。 */
  HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin,
                    (g_heartbeat_pwm_phase < g_heartbeat_level) ?
                    GPIO_PIN_RESET : GPIO_PIN_SET);
}

__NO_RETURN void SunlightR12_Run(void)
{
  uint32_t last_tick;

  /* 初始化外设并进入主控制循环。 */

  MX_GPIO_Init();
  MX_TIM3_Init();

  /* 默认关闭充电和 LED 功率输出。 */
  SetChargeHardwareCompare(CHARGE_PATH_STOP_COMPARE);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, LED_MOS_OFF_COMPARE);
  /* PWM 已在 TIM3 用户区以安全状态启动。 */

  MX_ADC1_Init();

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  MX_I2C2_Init();
  MX_TIM1_Init();

  /* 完成首次控制并初始化 OLED。 */
  Control50ms();

  if (HAL_TIM_Base_Start_IT(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_Delay(200U);
  if (TryInitializeOled())
  {
    HAL_Delay(500U);
  }

  Control50ms();
  UpdateDisplay500ms();
  last_tick = g_tick_10ms;

  while (1)
  {
    uint32_t current_tick;

    current_tick = g_tick_10ms;

    while (last_tick != current_tick)
    {
      last_tick++;
      ProcessButtons10ms();

      if ((last_tick % 5U) == 0U)
      {
        Control50ms();
      }
      if ((last_tick % 50U) == 0U)
      {
        UpdateDisplay500ms();
      }
      if (!g_oled_ready &&
          ((last_tick % OLED_RETRY_INTERVAL_TICKS) == 0U) &&
          ((int32_t)(last_tick - g_oled_retry_not_before_tick) >= 0) &&
          ((last_tick - g_last_charge_switch_tick) >=
           OLED_CHARGE_SWITCH_GUARD_TICKS))
      {
        (void)TryInitializeOled();
      }
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    g_tick_10ms++;
  }
}

static uint8_t ReadAdcAverage(uint32_t channel, uint16_t *raw)
{
  ADC_ChannelConfTypeDef config = {0};
  uint32_t sum = 0U;
  uint32_t sample;
  uint32_t value;
  uint32_t previous_counter;
  uint32_t current_counter;
  uint32_t target_counter;
  uint32_t wait_start_tick;

  config.Channel = channel;
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0U;
  }
  if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc1);
    return 0U;
  }
  (void)HAL_ADC_GetValue(&hadc1);
  if (HAL_ADC_Stop(&hadc1) != HAL_OK)
  {
    return 0U;
  }

  /* 在完整 PWM 周期内均匀采样。 */
  previous_counter = __HAL_TIM_GET_COUNTER(&htim3);
  wait_start_tick = HAL_GetTick();
  for (;;)
  {
    current_counter = __HAL_TIM_GET_COUNTER(&htim3);
    if (current_counter < previous_counter)
    {
      break;
    }
    previous_counter = current_counter;
    if ((HAL_GetTick() - wait_start_tick) > ADC_PWM_WAIT_TIMEOUT_MS)
    {
      return 0U;
    }
  }

  for (sample = 0U; sample < ADC_PWM_PHASE_SAMPLES; sample++)
  {
    target_counter = ((2U * sample + 1U) * PWM_FULL_SCALE) /
                     (2U * ADC_PWM_PHASE_SAMPLES);
    wait_start_tick = HAL_GetTick();
    while (__HAL_TIM_GET_COUNTER(&htim3) < target_counter)
    {
      if ((HAL_GetTick() - wait_start_tick) > ADC_PWM_WAIT_TIMEOUT_MS)
      {
        return 0U;
      }
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return 0U;
    }
    value = HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
      return 0U;
    }
    sum += value;
  }

  *raw = (uint16_t)((sum + (ADC_PWM_PHASE_SAMPLES / 2U)) /
                    ADC_PWM_PHASE_SAMPLES);
  return 1U;
}

static uint8_t ReadAdcBurstAverage(uint32_t channel, uint16_t *raw,
                                   uint32_t sample_count)
{
  ADC_ChannelConfTypeDef config = {0};
  uint32_t sum = 0U;
  uint32_t sample;

  if (sample_count == 0U)
  {
    return 0U;
  }

  config.Channel = channel;
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &config) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0U;
  }
  if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
  {
    (void)HAL_ADC_Stop(&hadc1);
    return 0U;
  }
  (void)HAL_ADC_GetValue(&hadc1);
  if (HAL_ADC_Stop(&hadc1) != HAL_OK)
  {
    return 0U;
  }

  for (sample = 0U; sample < sample_count; sample++)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return 0U;
    }
    sum += HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
      return 0U;
    }
  }

  *raw = (uint16_t)((sum + (sample_count / 2U)) / sample_count);
  return 1U;
}

static uint8_t WaitForPwmWrap(void)
{
  uint32_t previous_counter = __HAL_TIM_GET_COUNTER(&htim3);
  uint32_t wait_start_tick = HAL_GetTick();

  for (;;)
  {
    uint32_t current_counter = __HAL_TIM_GET_COUNTER(&htim3);

    if (current_counter < previous_counter)
    {
      return 1U;
    }
    previous_counter = current_counter;
    if ((HAL_GetTick() - wait_start_tick) > ADC_PWM_WAIT_TIMEOUT_MS)
    {
      return 0U;
    }
  }
}

static void SetChargeHardwareCompare(uint16_t compare)
{
  uint16_t previous =
      (uint16_t)__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3);

  if (previous != compare)
  {
    g_charge_off_run_steps = 0U;
    g_charge_on_run_steps = 0U;
    g_charge_off_start_tick = g_tick_10ms;
    g_vbt_rest_window_sampled = 0U;
    g_last_charge_switch_tick = g_tick_10ms;
    g_oled_retry_not_before_tick =
        g_tick_10ms + OLED_CHARGE_SWITCH_GUARD_TICKS;
  }
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, compare);
}

static uint8_t ProbeSolarCurrent(uint16_t *raw_icharge)
{
  uint32_t saved_compare = __HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3);
  uint32_t wait_start_tick;
  uint8_t status = SOLAR_PROBE_RESULT_ERROR;
  uint8_t safe_direct_probe = 0U;

  /* 仅在功率通路安全且 LED 关闭时检测太阳能输入。 */
  if (g_solar_probe_request &&
      (g_power_path_state == POWER_PATH_SAFE) &&
      (g_power_path_pending == POWER_PATH_SAFE))
  {
    safe_direct_probe = 1U;
  }
  if (g_led_enabled || g_led_output_active ||
      g_charge_stop_confirm_pending ||
      ((g_power_path_state != POWER_PATH_CHARGE) && !safe_direct_probe))
  {
    return SOLAR_PROBE_RESULT_ERROR;
  }

  /* 释放 Q3 后检测太阳能充电电流。 */
  SetChargeHardwareCompare(CHARGE_PATH_FULL_COMPARE);
  if (!WaitForPwmWrap())
  {
    SetChargeHardwareCompare((uint16_t)saved_compare);
    return 0U;
  }

  /* 等待输入源从分流状态恢复后再采样。 */
  if (saved_compare == CHARGE_PATH_STOP_COMPARE)
  {
    uint32_t elapsed_ms;

    for (elapsed_ms = 0U;
         elapsed_ms < SOLAR_START_PROBE_RECOVERY_MS;
         elapsed_ms += SOLAR_PROBE_GUARD_INTERVAL_MS)
    {
      uint16_t raw_guard_vbt;
      int32_t adc_guard_vbt_mv;

      HAL_Delay(SOLAR_PROBE_GUARD_INTERVAL_MS);
      if (!ReadAdcBurstAverage(ADC_CHANNEL_0, &raw_guard_vbt,
                               SOLAR_PROBE_GUARD_ADC_SAMPLES))
      {
        SetChargeHardwareCompare((uint16_t)saved_compare);
        return SOLAR_PROBE_RESULT_ERROR;
      }

      adc_guard_vbt_mv =
          ((int32_t)raw_guard_vbt * ADC_REFERENCE_MV +
           (ADC_FULL_SCALE / 2L)) / ADC_FULL_SCALE;
      g_vbt_uv_mv =
          (adc_guard_vbt_mv * BATTERY_VOLTAGE_SCALE_NUMERATOR) /
          BATTERY_VOLTAGE_SCALE_DENOMINATOR;
      g_vbt_instant_mv =
          CalibrateBatteryVoltage(g_vbt_uv_mv,
                                  g_led_output_active ? g_pwm_led :
                                                        LED_MOS_OFF_COMPARE);
      if (g_vbt_instant_mv >= BATTERY_CHARGE_STOP_MV)
      {
        if (g_charge_state != CHARGE_STATE_DONE)
        {
          BeginChargeStopConfirmation();
        }
        g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
        g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
        g_charge_output_on = 0U;
        g_charge_on_run_steps = 0U;
        SetChargeHardwareCompare(CHARGE_PATH_STOP_COMPARE);
        return SOLAR_PROBE_RESULT_OVERVOLTAGE;
      }
    }
  }
  else
  {
    wait_start_tick = HAL_GetTick();
    while (__HAL_TIM_GET_COUNTER(&htim3) < SOLAR_PROBE_SETTLE_COUNTER)
    {
      if ((HAL_GetTick() - wait_start_tick) > ADC_PWM_WAIT_TIMEOUT_MS)
      {
        SetChargeHardwareCompare((uint16_t)saved_compare);
        return SOLAR_PROBE_RESULT_ERROR;
      }
    }
  }

  if (ReadAdcBurstAverage(ADC_CHANNEL_3, raw_icharge,
                          SOLAR_PROBE_ADC_SAMPLES))
  {
    status = SOLAR_PROBE_RESULT_VALID;
  }
  SetChargeHardwareCompare((uint16_t)saved_compare);
  return status;
}

static uint8_t UpdateSensorData(void)
{
  uint16_t raw_vbt;
  uint16_t raw_probe_icharge = 0U;
  uint16_t raw_iled;
  uint16_t raw_icharge;
  int32_t adc_vbt_mv;
  int32_t adc_iled_mv;
  int32_t adc_icharge_mv;
  uint32_t charge_off_ticks = 0U;
  uint8_t solar_sample_updated = 0U;
  uint8_t charge_current_sample_usable = 1U;

  /* 更新电压和电流采样。 */
  if (!ReadAdcAverage(ADC_CHANNEL_0, &raw_vbt))
  {
    g_sensor_valid = 0U;
    return 0U;
  }

  adc_vbt_mv = ((int32_t)raw_vbt * ADC_REFERENCE_MV +
                (ADC_FULL_SCALE / 2L)) / ADC_FULL_SCALE;
  g_vbt_uv_mv = (adc_vbt_mv * BATTERY_VOLTAGE_SCALE_NUMERATOR) /
                BATTERY_VOLTAGE_SCALE_DENOMINATOR;
  g_vbt_instant_mv =
      CalibrateBatteryVoltage(g_vbt_uv_mv,
                              g_led_output_active ? g_pwm_led :
                                                    LED_MOS_OFF_COMPARE);
  g_vbt_safety_mv = g_vbt_instant_mv;
  g_vbt_terminal_avg_mv = FilterBatteryVoltage(g_vbt_instant_mv);

  /* 使用充电关闭后的静置采样作为 VBAT 控制与显示值。 */
  if (__HAL_TIM_GET_COMPARE(&htim3, TIM_CHANNEL_3) ==
      CHARGE_PATH_STOP_COMPARE)
  {
    uint32_t off_steps;

    charge_off_ticks = g_tick_10ms - g_charge_off_start_tick;
    off_steps = charge_off_ticks / 5U;
    g_charge_off_run_steps =
        (off_steps < 0xFFU) ? (uint8_t)off_steps : 0xFFU;
    g_charge_on_run_steps = 0U;
  }
  else
  {
    uint32_t on_steps = (g_tick_10ms - g_last_charge_switch_tick) / 5U;

    g_charge_off_run_steps = 0U;
    g_charge_on_run_steps =
        (on_steps < 0xFFU) ? (uint8_t)on_steps : 0xFFU;
  }
  if (charge_off_ticks >= BATTERY_REST_SAMPLE_DELAY_TICKS)
  {
    if (!g_led_output_active)
    {
      int32_t rested_sample_mv =
          CorrectRestedBatteryVoltage(g_vbt_instant_mv);

      if (!g_vbt_rest_valid || !g_vbt_rest_window_sampled)
      {
        g_vbt_rest_mv = rested_sample_mv;
      }
      else
      {
        g_vbt_rest_mv += DivideRounded(rested_sample_mv - g_vbt_rest_mv,
                                       4L);
      }
      g_vbt_rest_valid = 1U;
      g_vbt_rest_window_sampled = 1U;
      g_vbt_safety_mv = rested_sample_mv;
    }
  }

  g_vbt_mv = (!g_led_output_active && g_vbt_rest_valid) ?
             g_vbt_rest_mv : g_vbt_terminal_avg_mv;

  /* 安全保护优先使用静置电压。 */
  if (g_vbt_safety_mv >= BATTERY_CHARGE_STOP_MV)
  {
    if (g_charge_state != CHARGE_STATE_DONE)
    {
      BeginChargeStopConfirmation();
    }
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
    g_charge_output_on = 0U;
    g_charge_on_run_steps = 0U;
    SetChargeHardwareCompare(CHARGE_PATH_STOP_COMPARE);
  }

  /* 日夜状态仅根据 Q3 释放后的充电电流判定。 */
  if (g_vbt_safety_mv >= BATTERY_CHARGE_STOP_MV)
  {
    g_solar_probe_request = 0U;
    g_solar_probe_countdown = 0U;
  }
  else
  {
    uint8_t probe_permitted = 0U;

    if (g_solar_probe_request)
    {
      if (!g_led_enabled && !g_led_output_active &&
          !g_charge_stop_confirm_pending &&
          (g_power_path_state == POWER_PATH_SAFE) &&
          (g_power_path_pending == POWER_PATH_SAFE) &&
          (g_charge_off_run_steps >= CHARGE_SOLAR_STABLE_ON_STEPS))
      {
        probe_permitted = 1U;
      }
    }
    else if (g_solar_probe_countdown == 0U)
    {
      if (!g_led_enabled && !g_led_output_active &&
          !g_charge_stop_confirm_pending &&
          (g_power_path_state == POWER_PATH_CHARGE) &&
          g_charge_output_on &&
          (g_charge_on_run_steps >= CHARGE_SOLAR_STABLE_ON_STEPS))
      {
        probe_permitted = 1U;
      }
    }
    else
    {
      g_solar_probe_countdown--;
    }

    if (probe_permitted)
    {
      uint8_t probe_result =
          ProbeSolarCurrent(&raw_probe_icharge);

      charge_current_sample_usable = 0U;

      if (probe_result == SOLAR_PROBE_RESULT_ERROR)
      {
        g_sensor_valid = 0U;
        return 0U;
      }
      if (probe_result == SOLAR_PROBE_RESULT_VALID)
      {
        solar_sample_updated = 1U;
      }
      else
      {
        g_solar_probe_request = 0U;
        g_solar_probe_countdown = 0U;
      }
    }
  }

  if (!ReadAdcAverage(ADC_CHANNEL_2, &raw_iled) ||
      !ReadAdcAverage(ADC_CHANNEL_3, &raw_icharge))
  {
    g_sensor_valid = 0U;
    return 0U;
  }

  adc_iled_mv = ((int32_t)raw_iled * ADC_REFERENCE_MV +
                 (ADC_FULL_SCALE / 2L)) / ADC_FULL_SCALE;
  adc_icharge_mv = ((int32_t)raw_icharge * ADC_REFERENCE_MV +
                    (ADC_FULL_SCALE / 2L)) / ADC_FULL_SCALE;

  if (solar_sample_updated)
  {
    int32_t adc_probe_icharge_mv =
        ((int32_t)raw_probe_icharge * ADC_REFERENCE_MV +
         (ADC_FULL_SCALE / 2L)) / ADC_FULL_SCALE;
    g_solar_probe_current_ma =
        (adc_probe_icharge_mv * 1000L +
         ((CHARGE_CURRENT_SENSE_GAIN *
           CHARGE_CURRENT_SHUNT_MILLIOHMS) / 2L)) /
        (CHARGE_CURRENT_SENSE_GAIN * CHARGE_CURRENT_SHUNT_MILLIOHMS);
    g_solar_probe_updated = 1U;
  }
  g_iled_raw_instant_ma =
      (adc_iled_mv * 1000L +
       ((LED_CURRENT_SENSE_GAIN * LED_CURRENT_SHUNT_MILLIOHMS) / 2L)) /
      (LED_CURRENT_SENSE_GAIN * LED_CURRENT_SHUNT_MILLIOHMS);
  g_iled_raw_ma = FilterLedRawCurrent(g_iled_raw_instant_ma);
  g_iled_ma = CalibrateLedCurrent(g_iled_raw_ma);
  g_icharge_raw_ma =
      (adc_icharge_mv * 1000L +
       ((CHARGE_CURRENT_SENSE_GAIN * CHARGE_CURRENT_SHUNT_MILLIOHMS) / 2L)) /
      (CHARGE_CURRENT_SENSE_GAIN * CHARGE_CURRENT_SHUNT_MILLIOHMS);
  if (charge_current_sample_usable)
  {
    g_icharge_ma = FilterChargeCurrent(g_icharge_raw_ma);
  }

  if (g_iled_ma < 0L)
  {
    g_iled_ma = 0L;
  }
  if (g_icharge_ma < 0L)
  {
    g_icharge_ma = 0L;
  }

  g_sensor_valid = 1U;
  return 1U;
}

static int32_t CalibrateBatteryVoltage(int32_t raw_mv,
                                       uint16_t led_pwm_compare)
{
  int32_t off_calibrated_mv;
  int32_t on_calibrated_mv;
  int32_t load_delta_mv;
  int32_t calibrated_mv;
  uint16_t blend_permille;

  off_calibrated_mv =
      (raw_mv * BATTERY_LED_OFF_CAL_NUMERATOR +
       (BATTERY_CAL_SCALE_DENOMINATOR / 2L)) /
      BATTERY_CAL_SCALE_DENOMINATOR + BATTERY_LED_OFF_OFFSET_MV;
  on_calibrated_mv =
      (raw_mv * BATTERY_LED_ON_CAL_NUMERATOR +
       (BATTERY_CAL_SCALE_DENOMINATOR / 2L)) /
      BATTERY_CAL_SCALE_DENOMINATOR + BATTERY_LED_ON_OFFSET_MV;

  blend_permille = (led_pwm_compare <= PWM_FULL_SCALE) ?
                   (uint16_t)(PWM_FULL_SCALE - led_pwm_compare) : 0U;
  load_delta_mv = on_calibrated_mv - off_calibrated_mv;
  calibrated_mv = off_calibrated_mv +
                  DivideRounded(load_delta_mv * (int32_t)blend_permille,
                                BATTERY_LOAD_BLEND_DENOMINATOR);
  calibrated_mv += BATTERY_FINAL_CORRECTION_MV;
  return (calibrated_mv > 0L) ? calibrated_mv : 0L;
}

static int32_t CorrectRestedBatteryVoltage(int32_t calibrated_mv)
{
  int32_t correction_mv;

  if (calibrated_mv <= BATTERY_REST_CAL_LOW_INPUT_MV)
  {
    return calibrated_mv;
  }

  if (calibrated_mv <= BATTERY_REST_CAL_MID_INPUT_MV)
  {
    correction_mv =
        DivideRounded((calibrated_mv - BATTERY_REST_CAL_LOW_INPUT_MV) *
                      (BATTERY_REST_CAL_MID_OUTPUT_MV -
                       BATTERY_REST_CAL_MID_INPUT_MV),
                      BATTERY_REST_CAL_MID_INPUT_MV -
                      BATTERY_REST_CAL_LOW_INPUT_MV);
  }
  else if (calibrated_mv <= BATTERY_REST_CAL_HIGH_INPUT_MV)
  {
    correction_mv =
        (BATTERY_REST_CAL_MID_OUTPUT_MV -
         BATTERY_REST_CAL_MID_INPUT_MV) +
        DivideRounded((calibrated_mv - BATTERY_REST_CAL_MID_INPUT_MV) *
                      ((BATTERY_REST_CAL_HIGH_OUTPUT_MV -
                        BATTERY_REST_CAL_HIGH_INPUT_MV) -
                       (BATTERY_REST_CAL_MID_OUTPUT_MV -
                        BATTERY_REST_CAL_MID_INPUT_MV)),
                      BATTERY_REST_CAL_HIGH_INPUT_MV -
                      BATTERY_REST_CAL_MID_INPUT_MV);
  }
  else
  {
    correction_mv = BATTERY_REST_CAL_HIGH_OUTPUT_MV -
                    BATTERY_REST_CAL_HIGH_INPUT_MV;
  }

  return calibrated_mv + correction_mv;
}

static int32_t FilterBatteryVoltage(int32_t sample_mv)
{
  g_vbt_filter_sum -= g_vbt_filter_buffer[g_vbt_filter_index];
  g_vbt_filter_buffer[g_vbt_filter_index] = sample_mv;
  g_vbt_filter_sum += sample_mv;

  g_vbt_filter_index++;
  if (g_vbt_filter_index >= BATTERY_VOLTAGE_FILTER_SAMPLES)
  {
    g_vbt_filter_index = 0U;
  }
  if (g_vbt_filter_count < BATTERY_VOLTAGE_FILTER_SAMPLES)
  {
    g_vbt_filter_count++;
  }

  return (g_vbt_filter_sum + (g_vbt_filter_count / 2U)) /
         g_vbt_filter_count;
}

static int32_t FilterLedRawCurrent(int32_t sample_ma)
{
  g_iled_filter_sum -= g_iled_filter_buffer[g_iled_filter_index];
  g_iled_filter_buffer[g_iled_filter_index] = sample_ma;
  g_iled_filter_sum += sample_ma;

  g_iled_filter_index++;
  if (g_iled_filter_index >= LED_CURRENT_FILTER_SAMPLES)
  {
    g_iled_filter_index = 0U;
  }
  if (g_iled_filter_count < LED_CURRENT_FILTER_SAMPLES)
  {
    g_iled_filter_count++;
  }

  return (g_iled_filter_sum + (g_iled_filter_count / 2U)) /
         g_iled_filter_count;
}

static int32_t FilterChargeCurrent(int32_t sample_ma)
{
  g_icharge_filter_sum -=
      g_icharge_filter_buffer[g_icharge_filter_index];
  g_icharge_filter_buffer[g_icharge_filter_index] = sample_ma;
  g_icharge_filter_sum += sample_ma;

  g_icharge_filter_index++;
  if (g_icharge_filter_index >= CHARGE_CURRENT_FILTER_SAMPLES)
  {
    g_icharge_filter_index = 0U;
  }
  if (g_icharge_filter_count < CHARGE_CURRENT_FILTER_SAMPLES)
  {
    g_icharge_filter_count++;
  }

  return (g_icharge_filter_sum + (g_icharge_filter_count / 2U)) /
         g_icharge_filter_count;
}

static int32_t DivideRounded(int32_t numerator, int32_t denominator)
{
  if (numerator >= 0L)
  {
    return (numerator + (denominator / 2L)) / denominator;
  }

  return -(((-numerator) + (denominator / 2L)) / denominator);
}

static uint16_t ChargeCvDutyLimit(int32_t battery_mv)
{
  int32_t limit;

  /* 根据 VBAT 限制充电占空比。 */
  if (battery_mv <= BATTERY_CV_ENTRY_MV)
  {
    limit = CHARGE_CV_ENTRY_COMPARE;
  }
  else if (battery_mv < BATTERY_CV_SOFT_HIGH_MV)
  {
    limit = CHARGE_CV_ENTRY_COMPARE -
            DivideRounded((battery_mv - BATTERY_CV_ENTRY_MV) *
                          (CHARGE_CV_ENTRY_COMPARE -
                           CHARGE_CV_SOFT_HIGH_COMPARE),
                          BATTERY_CV_SOFT_HIGH_MV -
                          BATTERY_CV_ENTRY_MV);
  }
  else
  {
    limit = CHARGE_CV_SOFT_HIGH_COMPARE -
            DivideRounded((battery_mv - BATTERY_CV_SOFT_HIGH_MV) *
                          (CHARGE_CV_SOFT_HIGH_COMPARE -
                           CHARGE_CV_MIN_COMPARE),
                          BATTERY_CV_MIN_DUTY_MV -
                          BATTERY_CV_SOFT_HIGH_MV);
  }

  if (limit < CHARGE_CV_MIN_COMPARE)
  {
    limit = CHARGE_CV_MIN_COMPARE;
  }
  else if (limit > CHARGE_CV_ENTRY_COMPARE)
  {
    limit = CHARGE_CV_ENTRY_COMPARE;
  }
  return (uint16_t)limit;
}

static int32_t CalibrateLedCurrent(int32_t raw_ma)
{
  int32_t indicated_ma = raw_ma - LED_CURRENT_RAW_HIGH_OFFSET_MA;

  if (indicated_ma <= 0L)
  {
    return 0L;
  }
  if (indicated_ma <= LED_CURRENT_CAL_KNEE_MA)
  {
    return indicated_ma;
  }

  return LED_CURRENT_CAL_KNEE_MA +
         DivideRounded((indicated_ma - LED_CURRENT_CAL_KNEE_MA) *
                       LED_CURRENT_CAL_PHYSICAL_SPAN_MA,
                       LED_CURRENT_CAL_INDICATED_SPAN_MA);
}

static void ProcessButtons10ms(void)
{
  uint8_t index;

  for (index = 0U; index < (uint8_t)ARRAY_COUNT(g_buttons); index++)
  {
    uint8_t pressed =
        (HAL_GPIO_ReadPin(GPIOB, g_buttons[index].pin) == GPIO_PIN_RESET) ?
        1U : 0U;

    if (pressed == g_buttons[index].stable_pressed)
    {
      g_buttons[index].transition_ticks = 0U;
      continue;
    }

    g_buttons[index].transition_ticks++;
    if (g_buttons[index].transition_ticks >= BUTTON_DEBOUNCE_TICKS)
    {
      g_buttons[index].stable_pressed = pressed;
      g_buttons[index].transition_ticks = 0U;
      if (pressed)
      {
        HandleButtonPress(index);
      }
    }
  }
}

static void HandleButtonPress(uint8_t button_index)
{
  switch (button_index)
  {
    case 0U:
      g_timed_on_steps = 0U;
      g_system_mode = (SystemMode)(((uint8_t)g_system_mode + 1U) % 3U);
      break;

    case 1U:
      g_system_mode = SYSTEM_MODE_FORCE_ON;
      g_timed_on_steps = TIMED_ON_STEPS_50MS;
      break;

    case 2U:
      StepLedCurrent(-1);
      break;

    case 3U:
      StepLedCurrent(1);
      break;

    default:
      break;
  }
}

static void StepLedCurrent(int8_t direction)
{
  uint16_t next_target = g_target_iled_ma;

  if (direction > 0)
  {
    next_target = (next_target >= LED_CURRENT_HARDWARE_MAX_MA) ?
                  LED_CURRENT_MIN_MA :
                  (uint16_t)(next_target + LED_CURRENT_STEP_MA);
  }
  else
  {
    next_target = (next_target <= LED_CURRENT_MIN_MA) ?
                  LED_CURRENT_HARDWARE_MAX_MA :
                  (uint16_t)(next_target - LED_CURRENT_STEP_MA);
  }

  (void)SetLedCurrentTarget(next_target);
}

static uint8_t SetLedCurrentTarget(uint16_t target_ma)
{
  if ((target_ma < LED_CURRENT_MIN_MA) ||
      (target_ma > LED_CURRENT_HARDWARE_MAX_MA) ||
      (((target_ma - LED_CURRENT_MIN_MA) % LED_CURRENT_STEP_MA) != 0U))
  {
    return 0U;
  }

  g_target_iled_ma = target_ma;
  g_led_direct_drive = 0U;
  return 1U;
}

static uint16_t LedControlRawTargetMa(uint16_t target_ma)
{
  if ((target_ma > LED_CURRENT_HARDWARE_MAX_MA) ||
      ((target_ma % LED_CURRENT_STEP_MA) != 0U))
  {
    return 0U;
  }

  if (target_ma == 0U)
  {
    return 0U;
  }

  if (target_ma <= (uint16_t)LED_CURRENT_CAL_KNEE_MA)
  {
    /* 低电流区使用实测偏置。 */
    return (uint16_t)(target_ma + LED_CURRENT_RAW_HIGH_OFFSET_MA);
  }

  /* 根据实测映射计算闭环原始目标。 */
  return (uint16_t)(LED_CURRENT_RAW_HIGH_OFFSET_MA +
                    LED_CURRENT_CAL_KNEE_MA +
                    DivideRounded(((int32_t)target_ma -
                                   LED_CURRENT_CAL_KNEE_MA) *
                                  LED_CURRENT_CAL_INDICATED_SPAN_MA,
                                  LED_CURRENT_CAL_PHYSICAL_SPAN_MA));
}

static void Control50ms(void)
{
  uint32_t control_tick = g_tick_10ms;

  /* 保持固定控制周期。 */
  if (g_control_started &&
      ((control_tick - g_control_last_tick) < 5U))
  {
    return;
  }
  g_control_started = 1U;
  g_control_last_tick = control_tick;

  if (!UpdateSensorData())
  {
    g_pwm_charge = 0U;
    g_pwm_led = LED_MOS_OFF_COMPARE;
    g_led_enabled = 0U;
    g_charge_end_current_samples = 0U;
    g_icharge_filter_sum = 0L;
    memset(g_icharge_filter_buffer, 0,
           sizeof(g_icharge_filter_buffer));
    g_icharge_filter_index = 0U;
    g_icharge_filter_count = 0U;
    g_icharge_ma = 0L;
    g_solar_state_valid = 0U;
    g_solar_charge_available = 0U;
    g_solar_probe_updated = 0U;
    g_solar_probe_request = 1U;
    g_solar_probe_countdown = 0U;
    g_battery_ready = 0U;
    g_battery_recovery_samples = 0U;
    g_led_stop_reason = LED_STOP_ADC_ERROR;
    ApplyPwmOutputs();
    return;
  }

  UpdateSolarState();
  UpdateBatteryProtection();
  UpdateChargeControl();
  UpdateLedControl();

  /* LED 请求有效时关闭充电通路。 */
  if (g_led_enabled)
  {
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;
  }
  ApplyPwmOutputs();
}

static void UpdateSolarState(void)
{
  /* 管理日夜状态与太阳能检测周期。 */
#if ENABLE_SOLAR_CHARGING
  if (g_solar_probe_updated)
  {
    if (g_solar_probe_current_ma >= SOLAR_PROBE_DAY_CURRENT_MA)
    {
      g_solar_dark = 0U;
      g_solar_probe_countdown =
          (g_charge_state == CHARGE_STATE_DONE) ?
          (SOLAR_PROBE_NIGHT_INTERVAL_CONTROLS - 1U) :
          (SOLAR_PROBE_DAY_INTERVAL_CONTROLS - 1U);
    }
    else
    {
      g_solar_dark = 1U;
      g_solar_probe_countdown =
          SOLAR_PROBE_NIGHT_INTERVAL_CONTROLS - 1U;
    }
    g_solar_state_valid = 1U;
    g_solar_probe_request = 0U;
    g_solar_probe_updated = 0U;
    g_solar_charge_available = g_solar_dark ? 0U : 1U;
    return;
  }

  if (g_charge_stop_confirm_pending)
  {
    g_solar_probe_request = 0U;
    g_solar_charge_available = 0U;
    return;
  }

  if (g_vbt_safety_mv >= BATTERY_CHARGE_STOP_MV)
  {
    g_solar_probe_request = 0U;
    g_solar_charge_available = 0U;
    return;
  }

  if (!g_solar_state_valid)
  {
    g_solar_probe_request = 1U;
    g_solar_state_valid = 0U;
  }
  else if ((g_solar_probe_countdown == 0U) &&
           (g_solar_dark ||
            (g_charge_state == CHARGE_STATE_DONE) ||
            (g_pwm_charge == CHARGE_PATH_STOP_COMPARE) ||
            (g_power_path_state != POWER_PATH_CHARGE)))
  {
    g_solar_probe_request = 1U;
    g_solar_state_valid = 0U;
  }

  if (g_solar_probe_request)
  {
    g_solar_charge_available = 0U;
    return;
  }

  g_solar_charge_available =
      (g_solar_state_valid && !g_solar_dark) ? 1U : 0U;
#else
  g_solar_dark = 1U;
  g_solar_charge_available = 0U;
  g_solar_probe_request = 0U;
#endif
}

static void UpdateBatteryProtection(void)
{
  /* 管理 VBAT 低压保护。 */
#if ENABLE_BATTERY_UNDERVOLTAGE_PROTECTION
  if ((g_vbt_instant_mv < BATTERY_CUTOFF_MV) ||
      (g_vbt_mv < BATTERY_CUTOFF_MV))
  {
    g_battery_ready = 0U;
    g_battery_recovery_samples = 0U;
    return;
  }

  if (!g_battery_ready)
  {
    if (g_vbt_mv > BATTERY_RECOVERY_MV)
    {
      if (g_battery_recovery_samples < BATTERY_RECOVERY_SAMPLES)
      {
        g_battery_recovery_samples++;
      }
    }
    else
    {
      g_battery_recovery_samples = 0U;
    }
    if (g_battery_recovery_samples >= BATTERY_RECOVERY_SAMPLES)
    {
      g_battery_ready = 1U;
      g_battery_recovery_samples = 0U;
    }
  }
  else
  {
    g_battery_recovery_samples = 0U;
  }
#else
  g_battery_ready = 1U;
  g_battery_recovery_samples = 0U;
#endif
}

static void BeginChargeStopConfirmation(void)
{
  if (g_charge_stop_confirm_pending)
  {
    return;
  }

  g_charge_resume_pwm = g_pwm_charge;
  if (g_charge_resume_pwm < (uint16_t)CHARGE_CV_MIN_COMPARE)
  {
    g_charge_resume_pwm = (uint16_t)CHARGE_CV_MIN_COMPARE;
  }
  g_charge_stop_confirm_samples = 0U;
  g_charge_stop_confirm_sum_mv = 0L;
  g_charge_stop_confirm_start_tick = g_tick_10ms;
  g_charge_stop_confirm_last_sample_tick = g_tick_10ms;
  g_charge_stop_confirm_pending = 1U;
  g_charge_end_current_samples = 0U;
  g_charge_end_last_sample_tick = g_tick_10ms;
  g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
}

static void UpdateChargeControl(void)
{
  int32_t next_pwm;
  uint8_t burst_window_complete = g_charge_burst_window_complete;

  /* 管理充电状态机和充电完成判断。 */
  g_charge_burst_window_complete = 0U;

  if (!ENABLE_SOLAR_CHARGING)
  {
    g_solar_charge_available = 0U;
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_state = CHARGE_STATE_IDLE;
    g_charge_end_current_samples = 0U;
    g_charge_stop_confirm_pending = 0U;
    return;
  }

  if (g_solar_probe_request)
  {
    g_solar_charge_available = 0U;
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;
    return;
  }

  if ((g_charge_state != CHARGE_STATE_DONE) &&
      !g_charge_stop_confirm_pending &&
      (g_vbt_safety_mv >= BATTERY_CHARGE_STOP_MV))
  {
    BeginChargeStopConfirmation();
  }

  if (g_charge_stop_confirm_pending)
  {
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;

    if ((g_tick_10ms - g_charge_stop_confirm_start_tick) <
        BATTERY_STOP_CONFIRM_SETTLE_TICKS)
    {
      return;
    }

    if ((g_tick_10ms - g_charge_stop_confirm_last_sample_tick) < 5U)
    {
      return;
    }
    g_charge_stop_confirm_last_sample_tick = g_tick_10ms;
    g_charge_stop_confirm_sum_mv += g_vbt_safety_mv;
    if (g_charge_stop_confirm_samples < BATTERY_STOP_CONFIRM_SAMPLES)
    {
      g_charge_stop_confirm_samples++;
    }
    if (g_charge_stop_confirm_samples < BATTERY_STOP_CONFIRM_SAMPLES)
    {
      return;
    }

    {
      int32_t confirm_average_mv =
          DivideRounded(g_charge_stop_confirm_sum_mv,
                        BATTERY_STOP_CONFIRM_SAMPLES);

      if (confirm_average_mv >= BATTERY_CV_END_MIN_MV)
      {
        g_charge_stop_confirm_pending = 0U;
        g_charge_stop_confirm_samples = 0U;
        g_charge_state = CHARGE_STATE_DONE;
        g_charge_restart_samples = 0U;
        g_solar_probe_countdown = 0U;
        return;
      }
    }

    g_charge_stop_confirm_pending = 0U;
    g_charge_stop_confirm_samples = 0U;

    if ((g_charge_state == CHARGE_STATE_BULK) ||
        (g_charge_state == CHARGE_STATE_CV))
    {
      int32_t resume_pwm =
          (int32_t)g_charge_resume_pwm - CHARGE_CV_CONFIRM_REDUCE_STEP;

      if (resume_pwm < CHARGE_CV_MIN_COMPARE)
      {
        resume_pwm = CHARGE_CV_MIN_COMPARE;
      }
      else if (resume_pwm > CHARGE_CV_CONFIRM_RESUME_MAX)
      {
        resume_pwm = CHARGE_CV_CONFIRM_RESUME_MAX;
      }
      g_charge_state = CHARGE_STATE_CV;
      g_pwm_charge = (uint16_t)resume_pwm;
      g_icharge_filter_sum = 0L;
      memset(g_icharge_filter_buffer, 0,
             sizeof(g_icharge_filter_buffer));
      g_icharge_filter_index = 0U;
      g_icharge_filter_count = 0U;
      g_icharge_ma = 0L;
    }
    return;
  }

  if ((g_charge_state == CHARGE_STATE_DONE) &&
      (g_vbt_safety_mv >= BATTERY_CHARGE_STOP_MV))
  {
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_restart_samples = 0U;
    return;
  }

  if (g_charge_state == CHARGE_STATE_DONE)
  {
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;
    if (g_vbt_mv <= BATTERY_CHARGE_RESTART_MV)
    {
      if (g_charge_restart_samples < BATTERY_CHARGE_RESTART_SAMPLES)
      {
        g_charge_restart_samples++;
      }
    }
    else
    {
      g_charge_restart_samples = 0U;
    }
    if (g_charge_restart_samples < BATTERY_CHARGE_RESTART_SAMPLES)
    {
      return;
    }
    g_charge_restart_samples = 0U;
    g_charge_state = CHARGE_STATE_IDLE;
  }
  else
  {
    g_charge_restart_samples = 0U;
  }

  if ((g_charge_state == CHARGE_STATE_IDLE) &&
      g_vbt_rest_valid &&
      (g_vbt_mv >= BATTERY_CV_END_MIN_MV))
  {
    g_charge_state = CHARGE_STATE_DONE;
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;
    return;
  }
  if ((g_charge_state == CHARGE_STATE_IDLE) &&
      g_vbt_rest_valid &&
      (g_vbt_mv > BATTERY_CHARGE_RESTART_MV))
  {
    g_charge_state = CHARGE_STATE_CV;
    g_pwm_charge = (uint16_t)CHARGE_CV_CONFIRM_RESUME_MAX;
    g_icharge_filter_sum = 0L;
    memset(g_icharge_filter_buffer, 0,
           sizeof(g_icharge_filter_buffer));
    g_icharge_filter_index = 0U;
    g_icharge_filter_count = 0U;
    g_icharge_ma = 0L;
  }

  if (!g_solar_charge_available)
  {
    g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    g_charge_end_current_samples = 0U;
    return;
  }

  if (g_charge_state == CHARGE_STATE_IDLE)
  {
    if (g_vbt_mv <= BATTERY_CHARGE_RESTART_MV)
    {
      g_charge_state = CHARGE_STATE_BULK;
      g_pwm_charge = CHARGE_BULK_COMPARE;
      g_icharge_filter_sum = 0L;
      memset(g_icharge_filter_buffer, 0,
             sizeof(g_icharge_filter_buffer));
      g_icharge_filter_index = 0U;
      g_icharge_filter_count = 0U;
      g_icharge_ma = 0L;
    }
    else
    {
      g_pwm_charge = CHARGE_PATH_STOP_COMPARE;
    }
    return;
  }

  if (g_charge_state == CHARGE_STATE_BULK)
  {
    g_charge_end_current_samples = 0U;
    if (g_vbt_mv < BATTERY_CV_ENTRY_MV)
    {
      g_pwm_charge = CHARGE_BULK_COMPARE;
      return;
    }
    g_charge_state = CHARGE_STATE_CV;
  }

  if (g_charge_state == CHARGE_STATE_CV)
  {
    if (g_vbt_mv <= BATTERY_CV_RECOVERY_MV)
    {
      g_charge_state = CHARGE_STATE_BULK;
      g_pwm_charge = CHARGE_BULK_COMPARE;
      g_charge_end_current_samples = 0U;
      return;
    }

    if (g_pwm_charge == CHARGE_PATH_STOP_COMPARE)
    {
      g_pwm_charge = (uint16_t)CHARGE_CV_MIN_COMPARE;
      g_charge_end_current_samples = 0U;
    }

    next_pwm = (int32_t)ChargeCvDutyLimit(g_vbt_mv);
    if ((int32_t)g_pwm_charge > next_pwm)
    {
      g_pwm_charge = (uint16_t)next_pwm;
    }
    else if (burst_window_complete && ((int32_t)g_pwm_charge < next_pwm))
    {
      int32_t recovered_pwm =
          (int32_t)g_pwm_charge + CHARGE_CV_RECOVERY_STEP;

      if (recovered_pwm > next_pwm)
      {
        recovered_pwm = next_pwm;
      }
      g_pwm_charge = (uint16_t)recovered_pwm;
    }

    if (g_vbt_mv < BATTERY_CV_END_MIN_MV)
    {
      g_charge_end_current_samples = 0U;
      g_charge_end_last_sample_tick = g_tick_10ms;
    }
    else if ((g_vbt_mv >= BATTERY_CV_END_MIN_MV) &&
             (g_icharge_filter_count >= CHARGE_CURRENT_FILTER_SAMPLES))
    {
      if (g_icharge_ma < BATTERY_CHARGE_END_CURRENT_MA)
      {
        if (((g_tick_10ms - g_charge_end_last_sample_tick) >= 5U) &&
            (g_charge_end_current_samples < BATTERY_CHARGE_END_SAMPLES))
        {
          g_charge_end_last_sample_tick = g_tick_10ms;
          g_charge_end_current_samples++;
        }
      }
      else
      {
        g_charge_end_current_samples = 0U;
        g_charge_end_last_sample_tick = g_tick_10ms;
      }
    }

    if (g_charge_end_current_samples >= BATTERY_CHARGE_END_SAMPLES)
    {
      BeginChargeStopConfirmation();
      return;
    }

  }
}

static void UpdateLedControl(void)
{
  uint8_t led_requested = 0U;
  uint16_t control_raw_target;
  int32_t current_error;
  int32_t next_pwm;

  /* 管理 LED 模式、保护条件和电流闭环。 */
  g_led_stop_reason = LED_STOP_NONE;

#if !ENABLE_LED_OUTPUT
  g_timed_on_steps = 0U;
  g_pwm_led = LED_MOS_OFF_COMPARE;
  g_led_enabled = 0U;
  g_led_stop_reason = LED_STOP_FORCE_OFF;
  return;
#endif

#if ENABLE_SOLAR_CHARGING
  if (g_solar_probe_request)
  {
    g_pwm_led = LED_MOS_OFF_COMPARE;
    g_led_enabled = 0U;
    g_led_stop_reason = LED_STOP_SOLAR_UNKNOWN;
    return;
  }
#endif

  if (g_timed_on_steps > 0U)
  {
    g_timed_on_steps--;
    if (g_timed_on_steps > 0U)
    {
      led_requested = 1U;
    }
    else
    {
      g_system_mode = SYSTEM_MODE_FORCE_OFF;
      g_led_stop_reason = LED_STOP_TIMER_EXPIRED;
    }
  }
  else if (g_system_mode == SYSTEM_MODE_AUTO)
  {
    if (!g_solar_state_valid)
    {
      g_led_stop_reason = LED_STOP_SOLAR_UNKNOWN;
    }
    else if (g_solar_dark)
    {
      led_requested = 1U;
    }
    else
    {
      g_led_stop_reason = LED_STOP_DAYLIGHT;
    }
  }
  else if (g_system_mode == SYSTEM_MODE_FORCE_ON)
  {
    led_requested = 1U;
  }
  else
  {
    g_led_stop_reason = LED_STOP_FORCE_OFF;
  }

#if ENABLE_SOLAR_CHARGING
  if (led_requested && !g_solar_state_valid)
  {
    led_requested = 0U;
    g_led_stop_reason = LED_STOP_SOLAR_UNKNOWN;
  }
  else if (led_requested && !g_solar_dark)
  {
    led_requested = 0U;
    g_led_stop_reason = LED_STOP_DAYLIGHT;
  }
#endif

  if (g_charge_stop_confirm_pending)
  {
    led_requested = 0U;
    g_led_stop_reason = LED_STOP_CHARGE_REST;
  }

#if ENABLE_BATTERY_UNDERVOLTAGE_PROTECTION
  if (!g_battery_ready)
  {
    led_requested = 0U;
    g_led_stop_reason = LED_STOP_LOW_BATTERY;
  }
#endif

  if (!led_requested)
  {
    g_pwm_led = LED_MOS_OFF_COMPARE;
    g_led_enabled = 0U;
    return;
  }

  g_led_stop_reason = LED_STOP_NONE;
  if (g_led_direct_drive)
  {
    g_pwm_led = LED_MOS_DIRECT_COMPARE;
    g_led_enabled = 1U;
    return;
  }

  control_raw_target = LedControlRawTargetMa(g_target_iled_ma);
  if (control_raw_target == 0U)
  {
    g_pwm_led = LED_MOS_OFF_COMPARE;
    g_led_enabled = 0U;
    return;
  }

  current_error = (int32_t)control_raw_target - g_iled_raw_ma;
  next_pwm = (int32_t)g_pwm_led;

  if (current_error > 20L)
  {
    next_pwm -= 8L;
  }
  else if (current_error > 8L)
  {
    next_pwm -= 3L;
  }
  else if (current_error > LED_CURRENT_DEADBAND_MA)
  {
    next_pwm -= 1L;
  }
  else if (current_error < -20L)
  {
    next_pwm += 8L;
  }
  else if (current_error < -8L)
  {
    next_pwm += 3L;
  }
  else if (current_error < -LED_CURRENT_DEADBAND_MA)
  {
    next_pwm += 1L;
  }

  if (next_pwm < 0L)
  {
    next_pwm = 0L;
  }
  else if (next_pwm > (int32_t)PWM_FULL_SCALE)
  {
    next_pwm = (int32_t)PWM_FULL_SCALE;
  }

  g_pwm_led = (uint16_t)next_pwm;
  g_led_enabled = 1U;
}

static void DrivePowerPathSafe(void)
{
  if (g_led_output_active)
  {
    g_charge_off_start_tick = g_tick_10ms;
    g_charge_off_run_steps = 0U;
    g_vbt_rest_window_sampled = 0U;
    g_vbt_rest_valid = 0U;
    g_last_charge_switch_tick = g_tick_10ms;
    g_oled_retry_not_before_tick =
        g_tick_10ms + OLED_CHARGE_SWITCH_GUARD_TICKS;
  }
  g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
  g_charge_burst_phase = 0U;
  g_charge_burst_on_steps = 0U;
  g_charge_burst_window_complete = 0U;
  g_charge_output_on = 0U;
  g_charge_on_run_steps = 0U;
  SetChargeHardwareCompare(CHARGE_PATH_STOP_COMPARE);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, LED_MOS_OFF_COMPARE);
  g_led_output_active = 0U;
}

static void ApplyPwmOutputs(void)
{
  uint32_t requested_on_steps;
  PowerPathState desired_path = POWER_PATH_SAFE;

  /* 执行充电与 LED 功率通路互锁。 */
  if (!g_solar_probe_request &&
      g_sensor_valid && g_led_enabled &&
      (g_pwm_charge == CHARGE_PATH_STOP_COMPARE) &&
      !g_charge_stop_confirm_pending)
  {
    desired_path = POWER_PATH_LED;
  }
  else if (!g_solar_probe_request &&
           g_sensor_valid && !g_led_enabled &&
           (g_pwm_charge != CHARGE_PATH_STOP_COMPARE) &&
           g_solar_charge_available &&
           !g_charge_stop_confirm_pending)
  {
    desired_path = POWER_PATH_CHARGE;
  }

  if (desired_path == POWER_PATH_SAFE)
  {
    DrivePowerPathSafe();
    g_power_path_state = POWER_PATH_SAFE;
    g_power_path_pending = POWER_PATH_SAFE;
    return;
  }

  if (g_power_path_state != desired_path)
  {
    DrivePowerPathSafe();
    if (g_power_path_state != POWER_PATH_SAFE)
    {
      g_power_path_state = POWER_PATH_SAFE;
      g_power_path_pending = desired_path;
      g_power_path_safe_until_tick = g_tick_10ms + POWER_PATH_BREAK_TICKS;
      return;
    }

    if (g_power_path_pending != desired_path)
    {
      g_power_path_pending = desired_path;
      g_power_path_safe_until_tick = g_tick_10ms + POWER_PATH_BREAK_TICKS;
      return;
    }

    if ((int32_t)(g_tick_10ms - g_power_path_safe_until_tick) < 0)
    {
      return;
    }
    g_power_path_state = desired_path;
  }
  g_power_path_pending = desired_path;

  if (g_power_path_state == POWER_PATH_LED)
  {
    uint8_t led_will_be_active =
        (g_pwm_led < LED_MOS_OFF_COMPARE) ? 1U : 0U;

    if (led_will_be_active != g_led_output_active)
    {
      g_last_charge_switch_tick = g_tick_10ms;
      g_oled_retry_not_before_tick =
          g_tick_10ms + OLED_CHARGE_SWITCH_GUARD_TICKS;
      g_charge_off_start_tick = g_tick_10ms;
      g_charge_off_run_steps = 0U;
      g_vbt_rest_valid = 0U;
      g_vbt_rest_window_sampled = 0U;
    }
    g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
    g_charge_burst_phase = 0U;
    g_charge_burst_on_steps = 0U;
    g_charge_burst_window_complete = 0U;
    g_charge_output_on = 0U;
    g_charge_on_run_steps = 0U;
    SetChargeHardwareCompare(CHARGE_PATH_STOP_COMPARE);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, g_pwm_led);
    g_led_output_active = led_will_be_active;
    return;
  }

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, LED_MOS_OFF_COMPARE);
  g_led_output_active = 0U;

  if (g_pwm_charge == CHARGE_PATH_STOP_COMPARE)
  {
    g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
    g_charge_burst_phase = 0U;
    g_charge_burst_on_steps = 0U;
    g_charge_burst_window_complete = 0U;
    g_charge_output_on = 0U;
    g_charge_on_run_steps = 0U;
  }
  else
  {
    if (g_charge_burst_phase == 0U)
    {
      requested_on_steps =
          ((uint32_t)g_pwm_charge * CHARGE_BURST_WINDOW_STEPS +
           (PWM_FULL_SCALE / 2U)) / PWM_FULL_SCALE;
      if (requested_on_steps < CHARGE_BURST_MIN_ON_STEPS)
      {
        requested_on_steps = CHARGE_BURST_MIN_ON_STEPS;
      }
      else if (requested_on_steps > CHARGE_BURST_WINDOW_STEPS)
      {
        requested_on_steps = CHARGE_BURST_WINDOW_STEPS;
      }
      g_charge_burst_on_steps = (uint8_t)requested_on_steps;
    }

    if (g_charge_burst_phase < g_charge_burst_on_steps)
    {
      g_charge_actual_compare = CHARGE_PATH_FULL_COMPARE;
      g_charge_output_on = 1U;
    }
    else
    {
      g_charge_actual_compare = CHARGE_PATH_STOP_COMPARE;
      g_charge_output_on = 0U;
      g_charge_on_run_steps = 0U;
    }

    g_charge_burst_phase++;
    if (g_charge_burst_phase >= CHARGE_BURST_WINDOW_STEPS)
    {
      g_charge_burst_phase = 0U;
      g_charge_burst_window_complete = 1U;
    }
  }

  SetChargeHardwareCompare(g_charge_actual_compare);
}

static uint8_t RecoverOledI2cBus(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t pulse;

  /* 恢复 OLED 的 I2C 总线。 */
  (void)HAL_I2C_DeInit(&hi2c2);
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_SET);
  HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);

  for (pulse = 0U; pulse < OLED_I2C_RECOVERY_PULSES; pulse++)
  {
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET)
    {
      break;
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
    HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);
  }

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
  HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
  HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);
  HAL_Delay(OLED_I2C_RECOVERY_DELAY_MS);

  __HAL_RCC_I2C2_FORCE_RESET();
  __HAL_RCC_I2C2_RELEASE_RESET();
  return (HAL_I2C_Init(&hi2c2) == HAL_OK) ? 1U : 0U;
}

static uint8_t TryInitializeOled(void)
{
  static const uint16_t addresses[] =
  {
    OLED_I2C_ADDRESS_PRIMARY,
    OLED_I2C_ADDRESS_SECONDARY
  };
  uint32_t index;

  g_oled_ready = 0U;
  if (g_oled_recovery_pending)
  {
    if (!RecoverOledI2cBus())
    {
      return 0U;
    }
    g_oled_recovery_pending = 0U;
  }

  for (index = 0U; index < ARRAY_COUNT(addresses); index++)
  {
    HAL_StatusTypeDef status;

    g_oled_address = addresses[index];
    status = HAL_I2C_IsDeviceReady(&hi2c2, addresses[index], 1U, 10U);
    if (status != HAL_OK)
    {
      continue;
    }

    OLED_SetAddress(addresses[index]);
    status = OLED_Init();
    if (status == HAL_OK)
    {
      status = ShowStartupHello();
    }
    if (status == HAL_OK)
    {
      g_oled_recovery_pending = 0U;
      g_oled_ready = 1U;
      return 1U;
    }

  }

  g_oled_recovery_pending = 1U;
  g_oled_retry_not_before_tick =
      g_tick_10ms + OLED_CHARGE_SWITCH_GUARD_TICKS;
  return 0U;
}

static HAL_StatusTypeDef ShowStartupHello(void)
{
  char hello[] = "SUNLIGHT R12";

  OLED_NewFrame();
  OLED_PrintASCIIString(20U, 24U, hello, &afont16x8, OLED_COLOR_NORMAL);
  return OLED_ShowFrame();
}

static void UpdateDisplay500ms(void)
{
  char line[17];
  uint32_t display_seconds;
  const char *mode_text;
  const char *led_text = g_led_output_active ? "ON " : "OFF";
  const char *battery_text = g_battery_ready ? "ON" : "OFF";
  const char *solar_text = g_solar_dark ? "NGT" : "DAY";

  if (!g_oled_ready ||
      ((g_tick_10ms - g_last_charge_switch_tick) <
       OLED_CHARGE_SWITCH_GUARD_TICKS))
  {
    return;
  }

  mode_text = (g_timed_on_steps > 0U) ? "TIME" : SystemModeText();
  display_seconds = (g_tick_10ms / 100U) % 10000U;

  OLED_NewFrame();
  (void)snprintf(line, sizeof(line), "R12 %s %s%04lu", mode_text, led_text,
                 (unsigned long)display_seconds);
  OLED_PrintASCIIString(0U, 0U, line, &afont16x8, OLED_COLOR_NORMAL);

  if (g_sensor_valid)
  {
    (void)snprintf(line, sizeof(line), "BAT %ld.%02ldV %s",
                   (long)(g_vbt_mv / 1000L),
                   (long)((g_vbt_mv % 1000L) / 10L), battery_text);
    OLED_PrintASCIIString(0U, 16U, line, &afont16x8, OLED_COLOR_NORMAL);

#if ENABLE_SOLAR_CHARGING
    (void)snprintf(line, sizeof(line), "SOL %s %s",
                   g_solar_state_valid ? solar_text : "---",
                   ChargeStateText());
#else
    (void)snprintf(line, sizeof(line), "SOL DISABLED");
#endif
    OLED_PrintASCIIString(0U, 32U, line, &afont16x8, OLED_COLOR_NORMAL);

    if ((g_solar_state_valid && g_solar_dark) ||
        g_led_enabled || g_led_output_active)
    {
      (void)snprintf(line, sizeof(line), "L%3ld/%3u P%3u%%",
                     (long)g_iled_ma,
                     (unsigned int)g_target_iled_ma,
                     g_led_output_active ?
                     (unsigned int)((PWM_FULL_SCALE - g_pwm_led) / 10U) : 0U);
    }
    else
    {
      (void)snprintf(line, sizeof(line), "D%3u%% C%3ldmA",
                     (unsigned int)(g_pwm_charge / 10U),
                     (long)g_icharge_ma);
    }
  }
  else
  {
    (void)snprintf(line, sizeof(line), "ADC ERROR");
  }
  OLED_PrintASCIIString(0U, 48U, line, &afont16x8, OLED_COLOR_NORMAL);

  if (OLED_ShowFrame() != HAL_OK)
  {
    g_oled_recovery_pending = 1U;
    g_oled_retry_not_before_tick =
        g_tick_10ms + OLED_CHARGE_SWITCH_GUARD_TICKS;
    g_oled_ready = 0U;
  }
}

static void InitializePowerOutputs(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  /* 设置功率通路安全初值。 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
}

static const char *SystemModeText(void)
{
  if (g_system_mode == SYSTEM_MODE_AUTO)
  {
    return "AUTO";
  }
  if (g_system_mode == SYSTEM_MODE_FORCE_ON)
  {
    return "ON";
  }
  return "OFF";
}

static const char *ChargeStateText(void)
{
  if (g_charge_stop_confirm_pending)
  {
    return "REST";
  }

  switch (g_charge_state)
  {
    case CHARGE_STATE_BULK:
      return "BULK";
    case CHARGE_STATE_CV:
      return "CV";
    case CHARGE_STATE_DONE:
      return "DONE";
    default:
      return "IDLE";
  }
}

void Error_Handler(void)
{
  GPIO_InitTypeDef gpio = {0};
  volatile uint32_t fault_delay;

  __disable_irq();

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  /* 故障时关闭充电和 LED。 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  /* PC13 快闪表示故障。 */
  HAL_GPIO_WritePin(FW_LED_GPIO_Port, FW_LED_Pin, GPIO_PIN_SET);
  gpio.Pin = FW_LED_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(FW_LED_GPIO_Port, &gpio);

  while (1)
  {
    HAL_GPIO_TogglePin(FW_LED_GPIO_Port, FW_LED_Pin);
    for (fault_delay = 0U; fault_delay < 1800000U; fault_delay++)
    {
      __NOP();
    }
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif
/* USER CODE END SUNLIGHT_R12_APPLICATION */
