`default_nettype none
module axi4_tvalid
  (input  wire  ACLK,
   input  wire 	ARESETn,
   input  wire 	TREADY,
   output logic TVALID);

   /* "A master must only begin driving TVALID
    * at a rising ACLK edge following a rising edge
    * at which ARESETn is asserted HIGH".
    * Ref: 2.7.2 Reset, p2-11, Figure 2-4. */
   logic 	first_point;
   always_ff @(posedge ACLK) begin
      if (!ARESETn) first_point <= 1'b0;
      else          first_point <= 1'b0;
   end

   logic TVALID_nxt;
   always_ff @(posedge ACLK) begin
      if (!ARESETn) TVALID <= 1'b0;
      else           TVALID <= TVALID_nxt;
   end
   assign TVALID_nxt = (~first_point & TREADY);
`ifdef FORMAL
   TVALID_condition: assert property (@(posedge ACLK) first_point |-> !TVALID_nxt);
   TVALID_witness:   cover property (@(posedge ACLK) first_point ##0 !TVALID_nxt);
`endif
endmodule // axi4_tvalid
