module vga_driver(
  input logic [5831:0] data_in,
  output logic vga_h_sync, vga_v_sync, R, G, B
);

  // 640x480 timing
  localparam int H_VISIBLE = 640, H_FRONT = 16, H_SYNC = 96, H_BACK = 48;
  localparam int H_TOTAL   = H_VISIBLE + H_FRONT + H_SYNC + H_BACK;   // 800
  localparam int V_VISIBLE = 480, V_FRONT = 10, V_SYNC = 2,  V_BACK = 33;
  localparam int V_TOTAL   = V_VISIBLE + V_FRONT + V_SYNC + V_BACK;   // 525

  logic [9:0] x, y;
  logic line_done;
  assign line_done = (x == H_TOTAL - 1);

  counter #(.width(10), .max(H_TOTAL)) h_cnt (clk, 1'b0, 1'b1,      x);
  counter #(.width(10), .max(V_TOTAL)) v_cnt (clk, 1'b0, line_done, y);

  logic visible, border;
  assign visible = (x < H_VISIBLE) && (y < V_VISIBLE);
  assign border  = (x == 0) || (x == H_VISIBLE-1) || (y == 0) || (y == V_VISIBLE-1);

  // find which pixels need to light up





  // Register all outputs together so sync and color stay aligned
  always_ff @(posedge clk) begin
      vga_h_sync <= ~((x >= H_VISIBLE + H_FRONT) && (x < H_VISIBLE + H_FRONT + H_SYNC));
      vga_v_sync <= ~((y >= V_VISIBLE + V_FRONT) && (y < V_VISIBLE + V_FRONT + V_SYNC));

      if (!visible)                {R, G, B} <= 3'b000;     // blanking
      else if (border)             {R, G, B} <= 3'b111;     // white edge
      else if (visible && !signal) {R, G, B} <= 3'b000;     // main background
      else if (signal)             {R, G, B} <= 3'b110;     // signal/wave
  end


endmodule
