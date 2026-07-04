module full_adder_n #(parameter WIDTH = 22)(
    input  [WIDTH-1:0] a,
    input  [WIDTH-1:0] b,
    input              ci,
    output [WIDTH-1:0] s,
    output             co
);
    assign {co, s} = a + b + ci;
endmodule