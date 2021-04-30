/* dh[notes]: This module is perfectly a synthesizable RTL,
 *            for a model to run FPV, it must be synthesizable.*/
`default_nettype none
`ifndef _CHI5_PKG_
 `define _CHI5_PKG_
package chi5_link;
   // TX Link state labels
   typedef enum logic [2:0] {TxStop, TxActp, TxAct, TxRunp, TxRun,
			     TxDeact, TxDeactp, TxStopp} TxLnk_t;
   // RX Link state labels
   typedef enum logic [2:0] {RxStop, RxActp, RxAct, RxRunp, RxRun,
			     RxDeact, RxDeactp, RxStopp} RxLnk_t;
   // Generating a big field consisting of both Tx/Rx link state labels
   typedef struct packed {TxLnk_t chi_tx_t; RxLnk_t chi_rx_t;} chi_link_fsm_t;
   // For the test
   typedef enum   logic [2:0] {s1, s2, s3, s4, stop} path1_t;
   // For the interaction of both fsms
   let link_interaction (state_a, state_b) = {state_a, state_b};
endpackage // chi5_link
`endif //  `ifndef _CHI5_PKG_

module amba5_chi_link_fsm
  import chi5_link::*;
   (output chi_link_fsm_t chi_link_states,
    input wire rxlinkactivereq, txlinkactivereq,
    input wire txlinkactiveack, rxlinkactiveack,
    input wire ACLK, ARESETn);

   // Present and next state logic
   chi_link_fsm_t fsm_lnk_ps, fsm_lnk_ns;
   always_ff @(posedge ACLK) begin
      if (!ARESETn) fsm_lnk_ps <= '{chi_tx_t:TxStop, chi_rx_t:RxStop};
      else          fsm_lnk_ps <= fsm_lnk_ns;
   end

   always_comb begin
      // Cover only expected state transitions
      fsm_lnk_ns = fsm_lnk_ps;
      case (fsm_lnk_ps)
	// figure 13-6 Expected Tx and Rx transitions [incomplete]
	link_interaction(TxStop, RxStop): begin
	   // Local and remote initiate
	   if (txlinkactivereq) fsm_lnk_ns.chi_tx_t = TxAct;
	   if (rxlinkactivereq) fsm_lnk_ns.chi_rx_t = RxAct;
	end
	link_interaction(TxAct, RxAct): begin
	   if (txlinkactiveack) fsm_lnk_ns.chi_tx_t = TxRun;
	   if (rxlinkactiveack) fsm_lnk_ns.chi_rx_t = RxRun;
	end
	link_interaction(TxStop, RxAct): begin
	   if (txlinkactivereq && rxlinkactivereq) begin
	      fsm_lnk_ns.chi_tx_t = TxAct;
	      fsm_lnk_ns.chi_rx_t = RxRun;
	   end
	   else fsm_lnk_ns.chi_tx_t = TxAct;
	end
	link_interaction(TxAct, RxRun): begin
	   if (txlinkactiveack) begin
	      fsm_lnk_ns.chi_tx_t = TxRun;
	      if (!rxlinkactivereq)
		fsm_lnk_ns.chi_rx_t = RxDeact;
	   end
	   // Async input race
	   else if (!rxlinkactivereq) fsm_lnk_ns.chi_rx_t = RxDeactp;
	end
	link_interaction(TxAct, RxStop): begin
	   if (rxlinkactivereq) begin
	      fsm_lnk_ns.chi_rx_t = RxAct;
	      if (txlinkactiveack)
		fsm_lnk_ns.chi_tx_t = TxRun;
	   end
	   // Async input race
	   else if (txlinkactiveack) fsm_lnk_ns.chi_tx_t = TxRunp;
	end
	link_interaction(TxRun, RxAct): begin
	   if (rxlinkactiveack) begin
	      fsm_lnk_ns.chi_rx_t = RxRun;
	      if (!rxlinkactivereq)
		fsm_lnk_ns.chi_tx_t = TxDeact;
	   end
	end
	link_interaction(TxRun, RxRun): begin
	   if (!rxlinkactivereq) fsm_lnk_ns.chi_rx_t = RxDeact;
	   if (!txlinkactivereq) fsm_lnk_ns.chi_tx_t = TxDeact;
	end
	link_interaction(TxDeact, RxDeact): begin
	   if (!rxlinkactiveack) fsm_lnk_ns.chi_rx_t = RxStop;
	   if (!txlinkactiveack) fsm_lnk_ns.chi_tx_t = TxStop;
	end
	default: fsm_lnk_ns = fsm_lnk_ps;
      endcase // unique case (fsm_lnk_ps)
   end // always_comb
   assign chi_link_states = fsm_lnk_ps;

`ifdef FORMAL
   default clocking fpv_clk @(posedge ACLK); endclocking
   default disable iff (!ARESETn);

   logic initial_current_state;
   logic initial_next_state;
   logic banned_output;
   logic completed_path;

   assign initial_current_state = fsm_lnk_ps.chi_tx_t == TxStop && fsm_lnk_ps.chi_rx_t == RxStop;
   assign initial_next_state    = fsm_lnk_ps.chi_tx_t == TxAct || fsm_lnk_ps.chi_rx_t == RxAct;
   ap_initial_path: assert property (initial_current_state && (txlinkactivereq || rxlinkactivereq)
				     |-> ##1 initial_next_state);
   wp_initial_path: cover property (initial_current_state && (txlinkactivereq || rxlinkactivereq)
				    ##1 initial_next_state);

   // for this bug1, this path is not implemented and the controller does not execute the paths to transition to that state
   assign banned_output = fsm_lnk_ps.chi_tx_t == TxStop && fsm_lnk_ps.chi_rx_t == RxRunp;
   ap_banned_output: assert property (initial_current_state |-> ##[+] banned_output);
   wp_banned_output: cover property (initial_current_state  ##[+] banned_output);

   // Bug2 current inputs are not leading to this scenario
   assign completed_path = fsm_lnk_ps.chi_tx_t == TxDeact && fsm_lnk_ps.chi_rx_t == RxDeact;
   ap_completed_path: assert property (initial_current_state |-> ##[+] completed_path);
   wp_completed_path: cover property (initial_current_state ##[+] completed_path);
`endif
endmodule // amba5_chi_link_fsm
/* This module is not exactly how a link interaction is done.
 * It is just for demonstration purposes. A real link needs
 * flits, credits, etc. */
module test
  import chi5_link::*;
   (output chi_link_fsm_t chi_link_states,
    input wire clk, rstn);

   path1_t ps, ns;
   logic       rxlinkactivereq;
   logic       txlinkactivereq;
   logic       txlinkactiveack;
   logic       rxlinkactiveack;

   always_ff @(posedge clk) begin
      if (!rstn) ps <= s1;
      else       ps <= ns;
   end

   always_comb begin
      ns = ps;
      rxlinkactivereq = 1'b0;
      txlinkactivereq = 1'b0;
      txlinkactiveack = 1'b0;
      rxlinkactiveack = 1'b0;
      case (ps)
	s1: begin ns = s2; end //TxStop/RxStop
	s2: begin {txlinkactivereq, rxlinkactivereq} = 2'b11; ns = s3; end //TxStop/RxAct
	s3: begin txlinkactiveack = 1'b1; ns = s4; end // TxAct/RxRun
	s4: begin {rxlinkactivereq, txlinkactivereq} = 2'b00; ns = stop; end //TxRun/RxRun
	stop:;
      endcase // unique case (ps)
   end
   amba5_chi_link_fsm amba5_chk
     (.*, .ACLK(clk), .ARESETn(rstn));
endmodule // test
