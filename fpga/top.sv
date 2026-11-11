module top(
  input  logic clk,
  input  logic reset,
  output logic out
);

  // 48 MHz / 2 = 24 MHz pixel clock (nominal is 25.175 MHz)
  logic clk;
  HSOSC #(.CLKHF_DIV("0b01")) hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk));

  
endmodule
