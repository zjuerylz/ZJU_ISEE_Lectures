module mux_2to1(out,in0,in1,addr);
parameter n=1;
output[n-1:0] out;
input[n-1:0] in0,in1;
input addr;

// Select in1 when addr=1, otherwise select in0
assign out = addr ? in1 : in0;

endmodule