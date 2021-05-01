// Property:
tready_max_wait: assert property(@(posedge ACLK) disable iff(!ARESETn)
                                 TVALID & !TREADY |-> ##[1:16] TREADY);
