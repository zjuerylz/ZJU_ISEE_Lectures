// Top module: |aIn - bIn| for 4-bit unsigned numbers
module abs_dif ( aIn , bIn , out );
    input  [3:0] aIn, bIn;
    output [3:0] out;

    wire agb;   // a > b flag
    comp #(.n(4)) comp_inst (
        .a(aIn), .b(bIn), .agb(agb), .aeb(), .alb()
    );

    wire [3:0] max, min;
    // mux1: select max
    mux_2to1 #(.n(4)) mux1 (.out(max), .in0(aIn), .in1(bIn), .addr(~agb));
    // mux2: select min
    mux_2to1 #(.n(4)) mux2 (.out(min), .in0(aIn), .in1(bIn), .addr(agb));

    wire [2:0] c;
    // Compute max - min = max + (~min) + 1
    full_adder adder0 (.a(max[0]), .b(~min[0]), .s(out[0]), .ci(1'b1), .co(c[0]));
    full_adder adder1 (.a(max[1]), .b(~min[1]), .s(out[1]), .ci(c[0]), .co(c[1]));
    full_adder adder2 (.a(max[2]), .b(~min[2]), .s(out[2]), .ci(c[1]), .co(c[2]));
    full_adder adder3 (.a(max[3]), .b(~min[3]), .s(out[3]), .ci(c[2]), .co());

endmodule