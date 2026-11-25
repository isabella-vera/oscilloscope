module oscope_top(
  input logic reset,

  input logic sck,
  input logic cs,
  input logic mosi,
  
  output logic miso,

  output logic vga_h_sync, vga_v_sync, R, G, B
);

  // 48 MHz / 2 = 24 MHz pixel clock 
  // for VGA we want close to 25.175 MHz
  // prob will switch to PLL instead of HSOSC to get closer
  logic clk;
  HSOSC #(.CLKHF_DIV("0b01")) hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk));

  // reset is currently active high so flip here if the physical button doesn't work that way
  // synchronize reset
  logic [1:0] reset_s;
  always_ff @(posedge clk) reset_s <= {reset_s[0], reset};  // reset_s[1] is the synchronized reset

  logic wr_enable;
  logic msg_done;
  logic [13:0] wr_addr;
  logic [15:0] wr_data;

  logic frame_start;
  logic [13:0] read_address;
  logic [15:0] read_data;
  logic ready;

  ping_pong_buffer pingpong(.clk(clk), .rst(reset_s[1]), .wr_en(wr_enable), .msg_done(msg_done), .wr_addr(wr_addr), .wr_data(wr_data),
                          .frame_start(frame_start), .rd_addr(read_address), .rd_data(read_data), .ready(ready));

  spi spi(.clk(clk), .reset(reset_s[1]), .sck(sck), .cs(cs), .mosi(mosi),
          .wr_en(wr_enable), .msg_done(msg_done), .wr_addr(wr_addr), .wr_data(wr_data));

  // still need a vga driver for the last five outputs

  assign miso = ready;

endmodule
