// 模式比较器：m=0 输出最大值，m=1 输出最小值
module ModeComparator (
    input  [7:0] a, b,   // 两个8位输入
    input        m,       // 模式选择
    output [7:0] y        // 输出结果
);
    wire agb;   // a > b 标志

    // 比较器：比较 a 和 b
    comp #(.n(8)) comp_inst (
        .a(a), .b(b), .agb(agb), .aeb(), .alb()
    );

    wire [7:0] max, min;   // 最大值和最小值中间变量

    // 选择器1：选出最大值，地址为 ~agb
    mux_2to1 #(.n(8)) mux_max (
        .out(max), .in0(a), .in1(b), .addr(~agb)
    );

    // 选择器2：选出最小值，地址为 agb
    mux_2to1 #(.n(8)) mux_min (
        .out(min), .in0(a), .in1(b), .addr(agb)
    );

    // 选择器3：根据 m 决定输出最大值还是最小值
    mux_2to1 #(.n(8)) mux_out (
        .out(y), .in0(max), .in1(min), .addr(m)
    );

endmodule
