`timescale 1ns / 1ps

// 接收音符和持续时间，控制DDS生成对应频率的正弦波音频样本
module note_player (
    input  wire        clk,
    input  wire        reset,
    input  wire        play_enable,
    input  wire [5:0]  note_to_load,
    input  wire [5:0]  duration_to_load,
    input  wire        load_new_note,
    input  wire        sampling_pulse,
    input  wire        beat,

    output wire        note_done,
    output wire [15:0] sample,
    output wire        sample_ready
);

    wire        timer_clear;   
    wire        timer_done; 
    wire        load;       
    wire [5:0]  note_played;
    wire [19:0] k_rom_out;
    wire [21:0] k_dds;
    wire        note_reg_rst;

    // 1. 音符寄存器
    assign note_reg_rst = reset | (~play_enable);
    
    dffre #(.n(6)) u_note_reg (
        .clk(clk),
        .r  (note_reg_rst),
        .en (load),
        .d  (note_to_load),
        .q  (note_played)
    );

   
    // 2. FreqROM (频率查找表)
    frequency_rom u_freq_rom (
        .clk  (clk),
        .addr (note_played),
        .dout (k_rom_out)
    );

    assign k_dds = {2'b00, k_rom_out};

    
    // 3. DDS 子模块实例化
    dds u_dds (
        .clk              (clk),
        .reset            (reset),
        .sampling_pulse   (sampling_pulse),
        .k                (k_dds),
        .sample           (sample),
        .new_sample_ready (sample_ready)
    );

    // 4. 音符节拍定时器 (6位计数器)
    reg [5:0] beat_cnt;
    always @(posedge clk) begin
        if (reset || timer_clear) begin
            beat_cnt <= 6'd0;
        end 
        else if (beat && play_enable) begin
            beat_cnt <= beat_cnt + 1'b1;
        end
    end

    // 当节拍计数达到设定的音长时，拉高 timer_done
    assign timer_done = (beat_cnt >= duration_to_load) && (duration_to_load > 0);

    // 5. note_player 控制器 (FSM 状态机)
    localparam RESET_ST = 2'b00;
    localparam WAIT_ST  = 2'b01;
    localparam DONE_ST  = 2'b10;
    localparam LOAD_ST  = 2'b11;

    reg [1:0] state, next_state;

    // 状态寄存器 (时序逻辑)
    always @(posedge clk) begin
        if (reset) begin
            state <= RESET_ST;
        end else begin
            state <= next_state;
        end
    end

    // 下一状态逻辑 (组合逻辑)
    always @(*) begin
        next_state = state; // 默认保持当前状态
        case (state)
            RESET_ST: begin
                next_state = WAIT_ST;
            end
            
            WAIT_ST: begin
                if (play_enable) begin
                    if (timer_done) begin
                        next_state = DONE_ST;
                    end
                    else if (load_new_note) begin
                        next_state = LOAD_ST;
                    end
                end
            end
            
            DONE_ST: begin
                next_state = WAIT_ST; // 输出高电平一个周期后自动回到 WAIT
            end
            
            LOAD_ST: begin
                next_state = WAIT_ST; // 输出高电平一个周期后自动回到 WAIT
            end
            
            default: next_state = RESET_ST;
        endcase
    end

    // 输出逻辑 (组合逻辑)
    reg timer_clear_reg;
    reg load_reg;
    reg note_done_reg;

    always @(*) begin
        // 默认输出为 0
        timer_clear_reg = 1'b0;
        load_reg        = 1'b0;
        note_done_reg   = 1'b0;

        case (state)
            RESET_ST: begin
                timer_clear_reg = 1'b1;
                load_reg        = 1'b0;
                note_done_reg   = 1'b0;
            end
            
            WAIT_ST: begin
                timer_clear_reg = 1'b0;
                load_reg        = 1'b0;
                note_done_reg   = 1'b0;
            end
            
            DONE_ST: begin
                timer_clear_reg = 1'b1;
                load_reg        = 1'b0;
                note_done_reg   = 1'b1;
            end
            
            LOAD_ST: begin
                timer_clear_reg = 1'b1;
                load_reg        = 1'b1;
                note_done_reg   = 1'b0;
            end
        endcase
    end

    assign timer_clear = timer_clear_reg;
    assign load        = load_reg;
    assign note_done   = note_done_reg;

endmodule