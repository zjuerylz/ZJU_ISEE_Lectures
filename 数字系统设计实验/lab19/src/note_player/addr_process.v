// addr_process.v
// 将相位高 (ADDR_WIDTH+2) 位转换为 ROM 地址和符号
module addr_process #(
    parameter ADDR_WIDTH = 10          // ROM 地址宽度，默认10位（1024点）
)(
    input  wire [ADDR_WIDTH+1:0] phase_high, // 相位高 (ADDR_WIDTH+2) 位
    output wire [ADDR_WIDTH-1:0] rom_addr,   // 输出 ROM 地址
    output wire                  sign        // 输出符号：1-负半周，0-正半周
);
    wire [1:0] quadrant = phase_high[ADDR_WIDTH+1 : ADDR_WIDTH]; // 高2位
    wire [ADDR_WIDTH-1:0] base_addr = phase_high[ADDR_WIDTH-1:0];

    // 第2、4象限地址镜像
    assign rom_addr = (quadrant == 2'b01 || quadrant == 2'b11) ?
                      ({ADDR_WIDTH{1'b1}} - base_addr) : base_addr;
    // 第3、4象限符号取反
    assign sign = (quadrant == 2'b10 || quadrant == 2'b11);
endmodule