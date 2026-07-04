module comp(a,b,agb,aeb,alb);
parameter n=1;
input[n-1:0] a,b;
output agb;
output aeb;
output alb;

// use relational operators for comparison
assign agb = (a > b);
assign aeb = (a == b);
assign alb = (a < b);

endmodule