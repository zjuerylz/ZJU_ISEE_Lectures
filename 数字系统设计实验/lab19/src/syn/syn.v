`timescale 1ns / 1ps

//同步化电路及脉冲宽度变换电路
module syn (
    input  wire clk,
    input  wire in,
    output wire out
);
    reg q1;
    reg q2;

    always @(posedge clk) begin
        q1 <= in;      //捕获/同步输入信号
        q2 <= q1;      //延迟一个时钟周期
    end
    assign out = q1 & (~q2);

endmodule