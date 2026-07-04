`timescale 1ns/1ps
// DDS 正弦信号发生器，使用 1/4 周期 ROM 通过象限对称生成完整周期
module dds (
    input        clk,
    input        reset,
    input        sampling_pulse,
    input [21:0] k,
    output [15:0] sample,
    output        new_sample_ready
);

localparam PHASE_W = 22;        // 相位累加器位宽
localparam ADDR_W = 10;         // ROM 地址位宽
localparam PHASE_HIGH_W = ADDR_W + 2; // 相位高12位

wire [PHASE_W-1:0] phase_acc, phase_next;
wire [PHASE_HIGH_W-1:0] phase_high = phase_acc[PHASE_W-1:PHASE_W-PHASE_HIGH_W]; // 取高12位
wire [ADDR_W-1:0] rom_addr;
wire sign;
wire [15:0] rom_data;          // ROM 原始数据
wire [15:0] signed_data;       // 符号处理后的数据

// 1. 相位累加器加法器（22位加法）
full_adder_n #(.WIDTH(PHASE_W)) adder (
    .a(phase_acc), .b(k), .ci(0), .s(phase_next), .co()
);

// 2. 相位累加器寄存器
dffre #(.n(PHASE_W)) ph_reg (
    .clk(clk), .r(reset), .en(sampling_pulse), .d(phase_next), .q(phase_acc)
);

// 3. 地址处理器
addr_process #(.ADDR_WIDTH(ADDR_W)) addr_proc (
    .phase_high(phase_high), .rom_addr(rom_addr), .sign(sign)
);

// 4. 正弦查找表 ROM
sine_rom rom (
    .clk(clk), .addr(rom_addr), .dout(rom_data)
);

// 5. 符号处理：负半周取补码
assign signed_data = sign ? (~rom_data + 1'b1) : rom_data;

// 6. 输出样品寄存器
dffre #(.n(16)) samp_reg (
    .clk(clk), .r(reset), .en(sampling_pulse), .d(signed_data), .q(sample)
);

// 7. 新样品就绪信号
dffre #(.n(1)) ready_reg (
    .clk(clk), .r(reset), .en(1'b1), .d(sampling_pulse), .q(new_sample_ready)
);
endmodule