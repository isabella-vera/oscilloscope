// Isabella Vera
// ivera@g.hmc.edu
// Created 9/7/2026
// This module was created for lab 1 to blink an LED at 2.4 Hz
// Last updated 9/11/2026
// This module was modified for lab 2 to include cleaner verilog and be more compatible for various counter uses
// no test bench here because it has been previously verified

module counter #( parameter int width = 4,
                  parameter int max = 10 )
                  
                ( input logic clk,
                  input logic reset,
                  input logic enable,

                  output logic [width-1:0] out = '0 );
				  
				  

                  always_ff @(posedge clk) begin
                      if (reset) begin
                          out <= '0;
                      end
                      else if (enable) begin
                          if (out == max - 1) begin
                              out <= '0;
                          end
                          else begin
                              out <= out + 1'b1;
                          end
                      end
                  end

endmodule