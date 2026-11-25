// // modified from https://www.fpga4fun.com/VGA.html

module vga_sine(output logic vga_h_sync, vga_v_sync, R, G, B);

    // 48 MHz / 2 = 24 MHz pixel clock (nominal is 25.175 MHz)
    logic clk;
    HSOSC #(.CLKHF_DIV("0b01")) hf_osc (.CLKHFPU(1'b1), .CLKHFEN(1'b1), .CLKHF(clk));

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

    // Generating 2 cycles of a sine wave across the screen (period 320 px),
    // centered on row 240, amplitude 120 (rows 120..360).
    //
    // Pipelined so each clock does only one small step:
    //   1. phase counter steps along the wave one column per clock
    //   2. table lookup + add/sub from center   -> s1 (sample for x+1)
    //   3. min/max of neighboring samples       -> lo, hi (for x)
    //   4. compare y against lo..hi             -> R,G,B
    localparam int CENTER = 240;

    // round(120 * sin(2*pi*i/320)) for i = 0..80: the first quarter of the wave
    function automatic logic [6:0] sine_quarter(input logic [6:0] i);
        case (i)
            7'd0 : sine_quarter = 7'd0;
            7'd1 : sine_quarter = 7'd2;
            7'd2 : sine_quarter = 7'd5;
            7'd3 : sine_quarter = 7'd7;
            7'd4 : sine_quarter = 7'd9;
            7'd5 : sine_quarter = 7'd12;
            7'd6 : sine_quarter = 7'd14;
            7'd7 : sine_quarter = 7'd16;
            7'd8 : sine_quarter = 7'd19;
            7'd9 : sine_quarter = 7'd21;
            7'd10: sine_quarter = 7'd23;
            7'd11: sine_quarter = 7'd26;
            7'd12: sine_quarter = 7'd28;
            7'd13: sine_quarter = 7'd30;
            7'd14: sine_quarter = 7'd33;
            7'd15: sine_quarter = 7'd35;
            7'd16: sine_quarter = 7'd37;
            7'd17: sine_quarter = 7'd39;
            7'd18: sine_quarter = 7'd42;
            7'd19: sine_quarter = 7'd44;
            7'd20: sine_quarter = 7'd46;
            7'd21: sine_quarter = 7'd48;
            7'd22: sine_quarter = 7'd50;
            7'd23: sine_quarter = 7'd52;
            7'd24: sine_quarter = 7'd54;
            7'd25: sine_quarter = 7'd57;
            7'd26: sine_quarter = 7'd59;
            7'd27: sine_quarter = 7'd61;
            7'd28: sine_quarter = 7'd63;
            7'd29: sine_quarter = 7'd65;
            7'd30: sine_quarter = 7'd67;
            7'd31: sine_quarter = 7'd69;
            7'd32: sine_quarter = 7'd71;
            7'd33: sine_quarter = 7'd72;
            7'd34: sine_quarter = 7'd74;
            7'd35: sine_quarter = 7'd76;
            7'd36: sine_quarter = 7'd78;
            7'd37: sine_quarter = 7'd80;
            7'd38: sine_quarter = 7'd81;
            7'd39: sine_quarter = 7'd83;
            7'd40: sine_quarter = 7'd85;
            7'd41: sine_quarter = 7'd87;
            7'd42: sine_quarter = 7'd88;
            7'd43: sine_quarter = 7'd90;
            7'd44: sine_quarter = 7'd91;
            7'd45: sine_quarter = 7'd93;
            7'd46: sine_quarter = 7'd94;
            7'd47: sine_quarter = 7'd96;
            7'd48: sine_quarter = 7'd97;
            7'd49: sine_quarter = 7'd98;
            7'd50: sine_quarter = 7'd100;
            7'd51: sine_quarter = 7'd101;
            7'd52: sine_quarter = 7'd102;
            7'd53: sine_quarter = 7'd104;
            7'd54: sine_quarter = 7'd105;
            7'd55: sine_quarter = 7'd106;
            7'd56: sine_quarter = 7'd107;
            7'd57: sine_quarter = 7'd108;
            7'd58: sine_quarter = 7'd109;
            7'd59: sine_quarter = 7'd110;
            7'd60: sine_quarter = 7'd111;
            7'd61: sine_quarter = 7'd112;
            7'd62: sine_quarter = 7'd113;
            7'd63: sine_quarter = 7'd113;
            7'd64: sine_quarter = 7'd114;
            7'd65: sine_quarter = 7'd115;
            7'd66: sine_quarter = 7'd115;
            7'd67: sine_quarter = 7'd116;
            7'd68: sine_quarter = 7'd117;
            7'd69: sine_quarter = 7'd117;
            7'd70: sine_quarter = 7'd118;
            7'd71: sine_quarter = 7'd118;
            7'd72: sine_quarter = 7'd119;
            7'd73: sine_quarter = 7'd119;
            7'd74: sine_quarter = 7'd119;
            7'd75: sine_quarter = 7'd119;
            7'd76: sine_quarter = 7'd120;
            7'd77: sine_quarter = 7'd120;
            7'd78: sine_quarter = 7'd120;
            7'd79: sine_quarter = 7'd120;
            7'd80: sine_quarter = 7'd120;
            default: sine_quarter = 7'd0;
        endcase
    endfunction

    // Phase along the wave. Each half-cycle (160 columns) walks
    // idx 0,1,...,80,79,...,1 and then flips to the other side of center.
    logic [6:0] idx;
    logic       rising;   // idx counting up
    logic       neg;      // second half of the cycle: wave is below center

    always_ff @(posedge clk) begin
        if (x == H_TOTAL - 3) begin           // line restart: line up phase with column 0
            idx <= 7'd0;  rising <= 1'b1;  neg <= 1'b0;
        end else if (rising) begin
            idx <= idx + 7'd1;
            if (idx == 7'd79) rising <= 1'b0; // reaches the peak (80) next
        end else if (idx == 7'd1) begin
            idx <= 7'd0;  rising <= 1'b1;  neg <= ~neg;
        end else begin
            idx <= idx - 7'd1;
        end
    end

    // Samples: s1 = row for column x+1, s0 = row for column x
    logic [6:0] mag;
    logic [8:0] s1, s0;
    assign mag = sine_quarter(idx);

    always_ff @(posedge clk) begin
        s1 <= neg ? 9'(CENTER + mag) : 9'(CENTER - mag);   // screen y grows downward
        s0 <= s1;
    end

    // Checks next  colum 
    logic [8:0] lo, hi;
    always_ff @(posedge clk) begin
        if (x == H_TOTAL - 1) begin           // next column is 0: no previous sample
            lo <= s1;  hi <= s1;
        end else if (s1 < s0) begin
            lo <= s1;  hi <= s0;
        end else begin
            lo <= s0;  hi <= s1;
        end
    end

    logic signal;
    assign signal = (y >= lo) && (y <= hi);

    // Register all outputs together so sync and color stay aligned
    always_ff @(posedge clk) begin
        vga_h_sync <= ~((x >= H_VISIBLE + H_FRONT) && (x < H_VISIBLE + H_FRONT + H_SYNC));
        vga_v_sync <= ~((y >= V_VISIBLE + V_FRONT) && (y < V_VISIBLE + V_FRONT + V_SYNC));

        if (!visible)    {R, G, B} <= 3'b000;     // blanking
        else if (border) {R, G, B} <= 3'b111;     // white edge
        else if (signal) {R, G, B} <= 3'b110;     // signal/wave (yellow)
        else             {R, G, B} <= 3'b000;     // background
    end
endmodule
