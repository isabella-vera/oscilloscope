module ping_pong_buffer(
  // overall inputs
  input logic clk, rst,

  // spi/write inputs
  input logic wr_en,
  input logic msg_done,
  input logic [13:0] wr_addr,
  input logic [15:0] wr_data,

  // read inputs and output
  input logic frame_start,
  input logic [13:0] rd_addr,
  output logic [15:0] rd_data
  output logic ready; // connect to MISO line in top module
);

  // block == 0: read block A, write block B
  // block == 1: write block A, read block B
  logic block; 

  logic [15:0] data_a, data_b; // data outputed from blocks A and B

  // flip flop to check its a good time & switch SPRAM blocks
  always_ff @(posedge clk) begin
    if (rst) begin
      block <= 0;
      ready <= 0;
    end
    else begin
      if (msg_done) begin
        ready <= 1;
      end
      if (ready && frame_start) begin
        block <= ~block;
        ready <= 0; 
      end
    end
  end


  // instantiation from SPRAM usage guide
  SP256K blockA ( .DI(wr_data),
                  .AD(block ? wr_addr : rd_addr),
                  .MASKWE(4'b1111), // to write all 16 bits
                  .WE(block && wr_en),
                  .CS(1'b1),
                  .CK(clk),
                  .STDBY(1'b0),
                  .SLEEP(1'b0),
                  .PWROFF_N(1'b1),
                  .DO(data_a)
                );

  SP256K blockB ( .DI(wr_data),
                  .AD(block ? rd_addr : wr_addr),
                  .MASKWE(4'b1111), // to write all 16 bits
                  .WE(~block && wr_en),
                  .CS(1'b1),
                  .CK(clk),
                  .STDBY(1'b0),
                  .SLEEP(1'b0),
                  .PWROFF_N(1'b1),
                  .DO(data_b)
                );
  // register bank to match the 1-cycle read latency
  logic backup_block;
  always_ff @(posedge clk) backup_block <= block;
  assign rd_data = backup_blockk ? data_b : data_a;

endmodule
