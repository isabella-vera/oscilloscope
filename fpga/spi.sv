// soft SPI
// shifts in 16 bits at a time from MCU (bc pingpong buffer reads 16 bits at a time)
// CPOL = 0, CPHA = 0: MOSI is sampled on the rising edge of SCK
// MSB first
// SPI pins are oversampled by clk, so clk must be much faster than SCK (currently 25MHz vs less than 1MHz which should be good)

module spi (
  // from fpga
  input logic clk,
  input logic reset,
  
  // from mcu/spi
  input logic sck,
  input logic cs,
  input logic mosi,

  // connect these directly to the ping pong buffer in top 
  output logic wr_en,    // 1 tick pulse for each 16-bit word
  output logic msg_done, // 1 tick pulse for after all words are loaded in
  output logic [13:0] wr_addr, 
  output logic [15:0] wr_data
  
  // MISO is wired in top from ping pong buffer READY 
);

  // ---------------------------------------------------
  // first synchronize data from mcu/spi (sck, cs, mosi)
  // get everything in the fpga clock domain
  // ---------------------------------------------------
  logic [2:0] sck_s;          // synchronized sck, 3 bits to see rising edge and avoid metastable states
  logic [1:0] cs_s, mosi_s; 

  always_ff @(posedge clk) begin
    sck_s  <= {sck_s[1:0],  sck};
    cs_s   <= {cs_s[0],     cs};
    mosi_s <= {mosi_s[0],   mosi};
  end

  // register rising sck edges
  // check when cs is active
  // grab last mosi bit
  logic sck_rise, cs_active, mosi_bit;
  assign sck_rise  =  sck_s[1] & ~sck_s[2];   // aligned with mosi_s[1]
  assign cs_active = ~cs_s[1];
  assign mosi_bit  =  mosi_s[1];

  // ----------------------------------
  // BIT COUNTER
  // want to shift in 16 bits at a time
  // create bit counter, count 0 to 15
  // ----------------------------------
  logic [3:0] bitcount;
  counter #(.width(4), .max(16)) bitcounter (.clk(clk), .reset(reset | ~cs_active), .enable(sck_rise), .out(bitcount));

  // ----------------------------------------------------------------------
  // ADDRESS COUNTER
  // goes through addresses
  // wired in top to ping pong buffer
  // tells buffer where to store the new 16-bits that were just shifted in
  // max is 364 bc we have 728 bytes which 728/2=364 16-bit words
  // ----------------------------------------------------------------------
  counter #(.width(14), .max(364)) addr_counter (.clk(clk), .reset(reset), .enable(wr_en), .out(wr_addr));

  assign msg_done = wr_en && (wr_addr == 363);

  //--------------------------------------------------
  // Implement SPI / SHIFT REGISTER
  // generate 16 bit output
  //--------------------------------------------------
  logic [15:0] shift;

  always_ff @(posedge clk) begin
    wr_en <= 0;                                 // don't want to store weird intermediate values
    if (reset) begin
      shift <= '0;                              // if reset, make it all zero
    end else if (sck_rise && cs_active) begin
      shift <= {shift[14:0], mosi_bit};         // get rid of [15], shift it all over, add current mosi_bit at [0]
      if (bitcount == 4'd15) begin
        wr_data <= {shift[14:0], mosi_bit};     // set output (goes to pingpong buffer in top)
        wr_en   <= 1;                           // enable SPRAM writing to store the data
      end
    end
  end


endmodule
 