`default_nettype none
module sandbox0
  (output logic unlock,
   input wire [7:0] key,
   input wire 	    clk, rstn);

   always_ff @(posedge clk) begin
      if (!rstn) unlock <= 1'b0;
      else
	if (key inside {8'b1?0??1?0}) unlock <= 1'b1;
   end

`ifdef FORMAL
   default clocking fpv_clk @(posedge clk); endclocking
   default disable iff (!rstn);
   restrict_val: assume property (key < 8'h83);
   unlock_test:  assert property (key[7] && !key[5] && key[2] && !key[0] |-> ##1 unlock);
   witness:      cover  property (key[7] && !key[5] && key[2] && !key[0] ##1 unlock);
`endif
endmodule // sandbox0

module sandbox1
  (output logic delayed_rst,
   input wire clk, rstn);

   var logic [1:0] sreg;
   always_ff @(posedge clk) begin
      if (!rstn) sreg <= 2'h0;
      else       sreg <= {sreg[0], 1'b1};
   end
   assign delayed_rst = sreg[1];
`ifdef FORMAL
   default clocking fpv_clk @(posedge clk); endclocking
   default disable iff (!rstn);
   delayed_reset: assert property (!rstn |-> ##2 delayed_rst);
   witness:       cover  property (!rstn ##2 delayed_rst);
`endif
endmodule // sandbox1
