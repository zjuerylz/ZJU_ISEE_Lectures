`timescale 1ns / 1ps

module counter_n
#(
    parameter n = 5,            // 分频比（模值）
    parameter counter_bits = 3  // 计数器二进制位数，需满足 2^counter_bits >= n
)
(
    input  wire                 clk,   // 时钟
    input  wire                 r,     // 异步复位，高有效
    input  wire                 en,    // 计数使能，高有效
    output reg                  co,    // 进位输出，当计数值达到 n-1 时输出高电平一个周期
    output reg [counter_bits-1:0] q   // 当前计数值
);

    always @(posedge clk or posedge r) begin
        if (r) begin
            q <= {counter_bits{1'b0}};
            co <= 1'b0;
        end
        else if (en) begin
            if (q == n - 1) begin
                q <= {counter_bits{1'b0}};
                co <= 1'b1;
            end
            else begin
                q <= q + 1'b1;
                co <= 1'b0;
            end
        end
        else begin
            co <= 1'b0;   // 未使能时 co 保持 0
        end
    end

endmodule