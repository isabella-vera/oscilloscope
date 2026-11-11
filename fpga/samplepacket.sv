// Used Claude to generate a sample packet (similar to what the FPGA will receive from the MCU) 
// to try displaying a signal on VGA
// 
// Prompt:
// I want you to generate one SPI transaction/packet of data similar to what I expect my 
// FPGA to receive from an MCU over SPI. Each SPI transaction/packet will be 729 bytes of data. The first 
// 9 bytes are measurements and trigger data. First 4 bytes are the signal frequency, the next 2 bytes are 
// the max voltage, the next 2 are min voltage, the next 1 byte is for the trigger level, and the last 720 
// bytes are the signal. The 720 byte signal can be split into 640 9 bit sections. Each 9 bit section represents 
// which of the 480 rows holds the signal. min and max voltage can be anywhere from -5.00V to 5.00V. can you 
// generate me a System Verilog declaration of a packet like this? have the frequency value be 320kHZ, the max 
// voltage value be 3.20 V, the min voltage value be 0.13 V, and the trigger level value be 1.6V. For the signal, 
// just generate two periods of a sine wave centered at the middle of the screen and with peaks halfway to the top 
// and troughs halfway to the bottom.
//
// Test SPI packet (flat vector): MCU -> FPGA, 729 bytes, sent byte 0 first, each byte MSB first.
//
// Byte(s)  Field        Encoding                                  Value in this packet
// 0-3      frequency    unsigned 32-bit, Hz, big-endian           320000 Hz  (0x0004E200)
// 4-5      max voltage  signed 16-bit, millivolts, big-endian     3200 mV    (0x0C80)
// 6-7      min voltage  signed 16-bit, millivolts, big-endian     130 mV     (0x0082)
// 8        trigger      signed 8-bit, units of 0.1 V              16 = 1.6 V (0x10)
// 9-728    signal       640 x 9-bit row numbers (0 = top row, 479 = bottom),
//                       packed MSB-first as one continuous bitstream:
//                       sample 0 = first 9 bits of byte 9, sample 1 = next 9 bits, ...
//
// Signal: two periods of a sine wave, row = round(240 - 120*sin(2*pi*2*i/640)),
// so it is centered at row 240, peaks at row 120 and troughs at row 360.

// Flat layout: byte 0 is in the top 8 bits, i.e. TEST_PACKET[5831:5824].
//   frequency   = TEST_PACKET[5831:5800]
//   max voltage = TEST_PACKET[5799:5784]
//   min voltage = TEST_PACKET[5783:5768]
//   trigger     = TEST_PACKET[5767:5760]
//   signal      = TEST_PACKET[5759:0], sample i = TEST_PACKET[5759 - 9*i -: 9]
// Each line below is 9 bytes (72 bits); the first line is the 9-byte header.

localparam logic [5831:0] TEST_PACKET = {
    72'h0004E2_000C80_008210, 
    72'h783B9D_6E973B_91C4E0,
    72'h6EB6DB_2D66A3_499ECD,
    72'h65B258_CC4613_017CBC,
    72'h5D2DD6_AB358A_BD5AAB,
    72'h54AA14_CA4512_813E9D,
    72'h4DA653_0964AA_4D2490,
    72'h47A391_88B452_210E86,
    72'h42A110_68240A_00FE7F,
    72'h3F1F4F_A7C3D9_ECF47A,
    72'h3C9E4F_2793C1_E0F078,
    72'h3C1E0F_0783C1_E4F279,
    72'h3C9E8F_47B3D9_F0FA7D,
    72'h3F1FCF_E8040A_090684,
    72'h42A190_E88452_2D188E,
    72'h47A412_4934AA_593099,
    72'h4DA753_EA0512_914CA8,
    72'h54AAD5_AAF58A_CD6AB7,
    72'h5D2F17_CC0613_118CC9,
    72'h65B359_ED26A3_59B2DB,
    72'h6EB81C_4E473B_A5D6EE,
    72'h783C9E_AF77CB_F1FD00,
    72'h81C160_F0A864_3A2313,
    72'h8AC5E3_51C8F4_824524,
    72'h934A65_72D97C_C66735,
    72'h9BCE27_53C9F5_028343,
    72'hA2D1E9_14AA5D_369D50,
    72'hA8D4AA_955AB5_62B35A,
    72'hADD72B_B5EAFD_82C361,
    72'hB158EC_764B2D_96CD66,
    72'hB3D9EC_F67B45_A2D168,
    72'hB45A2D_168B45_9ECF67,
    72'hB3D9AC_D65B2D_92C763,
    72'hB1586C_360AFD_7ABB5C,
    72'hADD6AB_358AB5_56A952,
    72'hA8D429_D4DA5D_2A9147,
    72'hA2D0E8_3409F4_F27538,
    72'h9BCD66_73197C_B65729,
    72'h934924_5208F4_723517,
    72'h8AC4E2_30E864_2A0F05,
    72'h81C01F_CFC7CB_DDEAF2,
    72'h783B9D_6E973B_91C4E0,
    72'h6EB6DB_2D66A3_499ECD,
    72'h65B258_CC4613_017CBC,
    72'h5D2DD6_AB358A_BD5AAB,
    72'h54AA14_CA4512_813E9D,
    72'h4DA653_0964AA_4D2490,
    72'h47A391_88B452_210E86,
    72'h42A110_68240A_00FE7F,
    72'h3F1F4F_A7C3D9_ECF47A,
    72'h3C9E4F_2793C1_E0F078,
    72'h3C1E0F_0783C1_E4F279,
    72'h3C9E8F_47B3D9_F0FA7D,
    72'h3F1FCF_E8040A_090684,
    72'h42A190_E88452_2D188E,
    72'h47A412_4934AA_593099,
    72'h4DA753_EA0512_914CA8,
    72'h54AAD5_AAF58A_CD6AB7,
    72'h5D2F17_CC0613_118CC9,
    72'h65B359_ED26A3_59B2DB,
    72'h6EB81C_4E473B_A5D6EE,
    72'h783C9E_AF77CB_F1FD00,
    72'h81C160_F0A864_3A2313,
    72'h8AC5E3_51C8F4_824524,
    72'h934A65_72D97C_C66735,
    72'h9BCE27_53C9F5_028343,
    72'hA2D1E9_14AA5D_369D50,
    72'hA8D4AA_955AB5_62B35A,
    72'hADD72B_B5EAFD_82C361,
    72'hB158EC_764B2D_96CD66,
    72'hB3D9EC_F67B45_A2D168,
    72'hB45A2D_168B45_9ECF67,
    72'hB3D9AC_D65B2D_92C763,
    72'hB1586C_360AFD_7ABB5C,
    72'hADD6AB_358AB5_56A952,
    72'hA8D429_D4DA5D_2A9147,
    72'hA2D0E8_3409F4_F27538,
    72'h9BCD66_73197C_B65729,
    72'h934924_5208F4_723517,
    72'h8AC4E2_30E864_2A0F05,
    72'h81C01F_CFC7CB_DDEAF2
};