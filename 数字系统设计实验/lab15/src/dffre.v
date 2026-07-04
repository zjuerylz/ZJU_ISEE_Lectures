`timescale 1ns / 1ps

module dffre
#(parameter n = 1)           // 数据位宽，默认为1
(
    input  wire       clk,   // 时钟信号，上升沿有效
    input  wire       r,     // 异步复位信号，高电平有效
    input  wire       en,    // 使能信号，高电平时允许写入
    input  wire [n-1:0] d,   // 数据输入
    output reg  [n-1:0] q    // 数据输出
);

// 时序逻辑：上升沿时钟或上升沿异步复位
always @(posedge clk or posedge r) begin
    if (r)                      // 异步复位：输出清零
        q <= {n{1'b0}};
    else if (en)                // 使能有效：锁存输入数据
        q <= d;
    // 若en=0，则q保持原值
end

endmodule