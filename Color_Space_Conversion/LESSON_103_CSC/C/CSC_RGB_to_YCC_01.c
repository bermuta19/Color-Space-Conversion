// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// RGB to YCC conversion

#include <stdio.h>
#include <stdint.h>
#include "CSC_global.h"
#include <arm_neon.h>

// private data

// private prototypes
// =======
static void CSC_RGB_to_YCC_brute_force_float( int row, int col);

// =======
static void CSC_RGB_to_YCC_brute_force_int( int row, int col);

// =======
static void CSC_RGB_to_YCC_vectors( int row, int col, uint16x8_t y_base, uint16x8_t c_base, int chroma_mode);
// =======

static uint8_t chrominance_downsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4);
    static void CSC_RGB_to_YCC_vectors( int row, int col, uint16x8_t y_base,
                                     uint16x8_t c_base, int chroma_mode)
{
  //----------------------------------------------------------------
  // Load + widen both rows (needed for Y regardless of chroma mode)
  //----------------------------------------------------------------
  uint8x8_t r_row0 = vld1_u8(&R[row][col]);
  uint8x8_t g_row0 = vld1_u8(&G[row][col]);
  uint8x8_t b_row0 = vld1_u8(&B[row][col]);
  uint16x8_t r0_16 = vmovl_u8(r_row0);
  uint16x8_t g0_16 = vmovl_u8(g_row0);
  uint16x8_t b0_16 = vmovl_u8(b_row0);

  uint16x8_t y_row0 = vmlaq_n_u16(y_base, r0_16, (uint16_t)C11);
  y_row0 = vmlaq_n_u16(y_row0, g0_16, (uint16_t)C12);
  y_row0 = vmlaq_n_u16(y_row0, b0_16, (uint16_t)C13);
  vst1_u8(&Y[row][col], vshrn_n_u16(y_row0, K));

  uint8x8_t r_row1 = vld1_u8(&R[row + 1][col]);
  uint8x8_t g_row1 = vld1_u8(&G[row + 1][col]);
  uint8x8_t b_row1 = vld1_u8(&B[row + 1][col]);
  uint16x8_t r1_16 = vmovl_u8(r_row1);
  uint16x8_t g1_16 = vmovl_u8(g_row1);
  uint16x8_t b1_16 = vmovl_u8(b_row1);

  uint16x8_t y_row1 = vmlaq_n_u16(y_base, r1_16, (uint16_t)C11);
  y_row1 = vmlaq_n_u16(y_row1, g1_16, (uint16_t)C12);
  y_row1 = vmlaq_n_u16(y_row1, b1_16, (uint16_t)C13);
  vst1_u8(&Y[row + 1][col], vshrn_n_u16(y_row1, K));

  if (chroma_mode == 2)
  {
    //----------------------------------------------------------------
    // MODE 2: box average. Sum R/G/B over the 2x2 block FIRST, then
    // run the color-matrix multiply once instead of twice-then-average.
    // Roughly halves the chroma MAC work vs computing Cb/Cr per row.
    //----------------------------------------------------------------
    uint16x8_t r_colsum = vaddq_u16(r0_16, r1_16);   // max 510, safe in u16
    uint16x8_t g_colsum = vaddq_u16(g0_16, g1_16);
    uint16x8_t b_colsum = vaddq_u16(b0_16, b1_16);

    uint32x4_t r_block = vpaddlq_u16(r_colsum);      // sum of 4 pixels, max 1020
    uint32x4_t g_block = vpaddlq_u16(g_colsum);
    uint32x4_t b_block = vpaddlq_u16(b_colsum);

    // 4 copies of the DC offset + rounding bias for a single (K+2)-bit shift
    const uint32_t cbase4 = ((uint32_t)128 << K) * 4 + (1u << (K + 1));
    uint32x4_t cbase4_vec = vdupq_n_u32(cbase4);

    uint32x4_t cb32 = vmlaq_n_u32(cbase4_vec, b_block, (uint32_t)C23);
    cb32 = vmlsq_n_u32(cb32, r_block, (uint32_t)C21);
    cb32 = vmlsq_n_u32(cb32, g_block, (uint32_t)C22);
    uint16x4_t cb16 = vshrn_n_u32(cb32, K + 2);       // rounded average, 0-255

    uint32x4_t cr32 = vmlaq_n_u32(cbase4_vec, r_block, (uint32_t)C31);
    cr32 = vmlsq_n_u32(cr32, g_block, (uint32_t)C32);
    cr32 = vmlsq_n_u32(cr32, b_block, (uint32_t)C33);
    uint16x4_t cr16 = vshrn_n_u32(cr32, K + 2);

    uint8x8_t cb_cr_final = vmovn_u16(vcombine_u16(cb16, cr16));
    vst1_lane_u32((uint32_t*)&Cb[row>>1][col>>1], vreinterpret_u32_u8(cb_cr_final), 0);
    vst1_lane_u32((uint32_t*)&Cr[row>>1][col>>1], vreinterpret_u32_u8(cb_cr_final), 1);
  }
  else
  {
    //----------------------------------------------------------------
    // MODE 1: drop. Keep only the top-left pixel of each 2x2 block;
    // row1's chroma is never computed at all.
    //----------------------------------------------------------------
    uint16x8_t cb_row0 = vmlaq_n_u16(c_base, b0_16, (uint16_t)C23);
    cb_row0 = vmlsq_n_u16(cb_row0, r0_16, (uint16_t)C21);
    cb_row0 = vmlsq_n_u16(cb_row0, g0_16, (uint16_t)C22);
    uint8x8_t cb_8bit_row0 = vshrn_n_u16(cb_row0, K);

    uint16x8_t cr_row0 = vmlaq_n_u16(c_base, r0_16, (uint16_t)C31);
    cr_row0 = vmlsq_n_u16(cr_row0, g0_16, (uint16_t)C32);
    cr_row0 = vmlsq_n_u16(cr_row0, b0_16, (uint16_t)C33);
    uint8x8_t cr_8bit_row0 = vshrn_n_u16(cr_row0, K);

    uint8x8x2_t cb_deint = vuzp_u8(cb_8bit_row0, cb_8bit_row0);
    vst1_lane_u32((uint32_t*)&Cb[row>>1][col>>1], vreinterpret_u32_u8(cb_deint.val[0]), 0);

    uint8x8x2_t cr_deint = vuzp_u8(cr_8bit_row0, cr_8bit_row0);
    vst1_lane_u32((uint32_t*)&Cr[row>>1][col>>1], vreinterpret_u32_u8(cr_deint.val[0]), 0);
  }
}

static void CSC_RGB_to_YCC_brute_force_float( int row, int col) {
//
  uint8_t Cb_pixel_00, Cb_pixel_01;
  uint8_t Cb_pixel_10, Cb_pixel_11;
  uint8_t Cr_pixel_00, Cr_pixel_01;
  uint8_t Cr_pixel_10, Cr_pixel_11;

  Y[row+0][col+0] = (uint8_t)(16.0 + 0.257*R[row+0][col+0]
                                   + 0.504*G[row+0][col+0]
                                   + 0.098*B[row+0][col+0]);
  Y[row+0][col+1] = (uint8_t)(16.0 + 0.257*R[row+0][col+1]
                                   + 0.504*G[row+0][col+1]
                                   + 0.098*B[row+0][col+1]);
  Y[row+1][col+0] = (uint8_t)(16.0 + 0.257*R[row+1][col+0]
                                   + 0.504*G[row+1][col+0]
                                   + 0.098*B[row+1][col+0]);
  Y[row+1][col+1] = (uint8_t)(16.0 + 0.257*R[row+1][col+1]
                                   + 0.504*G[row+1][col+1]
                                   + 0.098*B[row+1][col+1]);

  Cb_pixel_00 = (uint8_t)(128.0 - 0.148*R[row+0][col+0]
                                - 0.291*G[row+0][col+0]
                                + 0.439*B[row+0][col+0]);
  Cb_pixel_01 = (uint8_t)(128.0 - 0.148*R[row+0][col+1]
                                - 0.291*G[row+0][col+1]
                                + 0.439*B[row+0][col+1]);
  Cb_pixel_10 = (uint8_t)(128.0 - 0.148*R[row+1][col+0]
                                - 0.291*G[row+1][col+0]
                                + 0.439*B[row+1][col+0]);
  Cb_pixel_11 = (uint8_t)(128.0 - 0.148*R[row+1][col+1]
                                - 0.291*G[row+1][col+1]
                                + 0.439*B[row+1][col+1]);

  Cr_pixel_00 = (uint8_t)(128.0 + 0.439*R[row+0][col+0]
                                - 0.368*G[row+0][col+0]
                                - 0.071*B[row+0][col+0]);
  Cr_pixel_01 = (uint8_t)(128.0 + 0.439*R[row+0][col+1]
                                - 0.368*G[row+0][col+1]
                                - 0.071*B[row+0][col+1]);
  Cr_pixel_10 = (uint8_t)(128.0 + 0.439*R[row+1][col+0]
                                - 0.368*G[row+1][col+0]
                                - 0.071*B[row+1][col+0]);
  Cr_pixel_11 = (uint8_t)(128.0 + 0.439*R[row+1][col+1]
                                - 0.368*G[row+1][col+1]
                                - 0.071*B[row+1][col+1]);

  Cb[row>>1][col>>1] = chrominance_downsample( Cb_pixel_00,
                                               Cb_pixel_01,
                                               Cb_pixel_10,
                                               Cb_pixel_11);

  Cr[row>>1][col>>1] = chrominance_downsample( Cr_pixel_00,
                                               Cr_pixel_01,
                                               Cr_pixel_10,
                                               Cr_pixel_11);
} // END of CSC_RGB_to_YCC_brute_force_float()

// =======
static void CSC_RGB_to_YCC_brute_force_int( int row, int col) {
//
  int R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  int G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  int B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;

  int  Y_pixel_00,  Y_pixel_01,  Y_pixel_10,  Y_pixel_11;
  int Cb_pixel_00, Cb_pixel_01, Cb_pixel_10, Cb_pixel_11;
  int Cr_pixel_00, Cr_pixel_01, Cr_pixel_10, Cr_pixel_11;

  R_pixel_00 = (int)R[row+0][col+0];
  R_pixel_01 = (int)R[row+0][col+1];
  R_pixel_10 = (int)R[row+1][col+0];
  R_pixel_11 = (int)R[row+1][col+1];

  G_pixel_00 = (int)G[row+0][col+0];
  G_pixel_01 = (int)G[row+0][col+1];
  G_pixel_10 = (int)G[row+1][col+0];
  G_pixel_11 = (int)G[row+1][col+1];

  B_pixel_00 = (int)B[row+0][col+0];
  B_pixel_01 = (int)B[row+0][col+1];
  B_pixel_10 = (int)B[row+1][col+0];
  B_pixel_11 = (int)B[row+1][col+1];

  Y_pixel_00 = (16 << (K)) + C11 * R_pixel_00
                           + C12 * G_pixel_00
                           + C13 * B_pixel_00;
  Y_pixel_00 += (1 << (K-1)); // rounding
  Y_pixel_00 = Y_pixel_00 >> K;

  Y_pixel_01 = (16 << (K)) + C11 * R_pixel_01
                           + C12 * G_pixel_01
                           + C13 * B_pixel_01;
  Y_pixel_01 += (1 << (K-1)); // rounding
  Y_pixel_01 = Y_pixel_01 >> K;

  Y_pixel_10 = (16 << (K)) + C11 * R_pixel_10
                           + C12 * G_pixel_10
                           + C13 * B_pixel_10;
  Y_pixel_10 += (1 << (K-1)); // rounding
  Y_pixel_10 = Y_pixel_10 >> K;

  Y_pixel_11 = (16 << (K)) + C11 * R_pixel_11
                           + C12 * G_pixel_11
                           + C13 * B_pixel_11;
  Y_pixel_11 += (1 << (K-1)); // rounding
  Y_pixel_11 = Y_pixel_11 >> K;

  Y[row+0][col+0] = (uint8_t)Y_pixel_00;
  Y[row+0][col+1] = (uint8_t)Y_pixel_01;
  Y[row+1][col+0] = (uint8_t)Y_pixel_10;
  Y[row+1][col+1] = (uint8_t)Y_pixel_11;

  Cb_pixel_00 = (128 << (K)) - C21 * R_pixel_00
                             - C22 * G_pixel_00
                             + C23 * B_pixel_00;
  Cb_pixel_00 += (1 << (K-1)); // rounding
  Cb_pixel_00 = Cb_pixel_00 >> K;

  Cb_pixel_01 = (128 << (K)) - C21 * R_pixel_01
                             - C22 * G_pixel_01
                             + C23 * B_pixel_01;
  Cb_pixel_01 += (1 << (K-1)); // rounding
  Cb_pixel_01 = Cb_pixel_01 >> K;

  Cb_pixel_10 = (128 << (K)) - C21 * R_pixel_10
                             - C22 * G_pixel_10
                             + C23 * B_pixel_10;
  Cb_pixel_10 += (1 << (K-1)); // rounding
  Cb_pixel_10 = Cb_pixel_10 >> K;

  Cb_pixel_11 = (128 << (K)) - C21 * R_pixel_11
                             - C22 * G_pixel_11
                             + C23 * B_pixel_11;
  Cb_pixel_11 += (1 << (K-1)); // rounding
  Cb_pixel_11 = Cb_pixel_11 >> K;

  Cr_pixel_00 = (128 << (K)) + C31 * R_pixel_00
                             - C32 * G_pixel_00
                             - C33 * B_pixel_00;
  Cr_pixel_00 += (1 << (K-1)); // rounding
  Cr_pixel_00 = Cr_pixel_00 >> K;

  Cr_pixel_01 = (128 << (K)) + C31 * R_pixel_01
                             - C32 * G_pixel_01
                             - C33 * B_pixel_01;
  Cr_pixel_01 += (1 << (K-1)); // rounding
  Cr_pixel_01 = Cr_pixel_01 >> K;

  Cr_pixel_10 = (128 << (K)) + C31 * R_pixel_10
                             - C32 * G_pixel_10
                             - C33 * B_pixel_10;
  Cr_pixel_10 += (1 << (K-1)); // rounding
  Cr_pixel_10 = Cr_pixel_10 >> K;

  Cr_pixel_11 = (128 << (K)) + C31 * R_pixel_11
                             - C32 * G_pixel_11
                             - C33 * B_pixel_11;
  Cr_pixel_11 += (1 << (K-1)); // rounding
  Cr_pixel_11 = Cr_pixel_11 >> K;

  Cb[row>>1][col>>1] = chrominance_downsample( (uint8_t)Cb_pixel_00,
                                               (uint8_t)Cb_pixel_01,
                                               (uint8_t)Cb_pixel_10,
                                               (uint8_t)Cb_pixel_11);

  Cr[row>>1][col>>1] = chrominance_downsample( (uint8_t)Cr_pixel_00,
                                               (uint8_t)Cr_pixel_01,
                                               (uint8_t)Cr_pixel_10,
                                               (uint8_t)Cr_pixel_11);
} // END of CSC_RGB_to_YCC_brute_force_int()

// =======
static uint8_t chrominance_downsample(
    uint8_t C_pixel_00, uint8_t C_pixel_01,
    uint8_t C_pixel_10, uint8_t C_pixel_11) {

  int temp;

  switch (CHROMINANCE_DOWNSAMPLING_MODE) {
    case 0:
      return( 0);
    case 1:
      return( C_pixel_00);
    case 2:
      temp = (int)C_pixel_00 + (int)C_pixel_01 + 
             (int)C_pixel_10 + (int)C_pixel_11;
      temp += (1 << 1); // rounding
      temp = temp >> 2;
      return( (uint8_t)temp);
    default:
      return( 0);
  }
} // END of chrominance_downsample()

// =======
void CSC_RGB_to_YCC( void) {
  int row, col; // indices for row and column
  uint16x8_t y_base = vdupq_n_u16((16 << K) + 128); //4,224 based on K=8
  uint16x8_t c_base = vdupq_n_u16((128 << K) + 128); //32,896 based on K=8

  // expected, by hand calc: Cb = 55, Cr = 35 for all four
  //
  for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
      //printf( "\n[row,col] = [%02i,%02i]\n\n", row, col);
      switch (RGB_to_YCC_ROUTINE) {
        case 0:
          break;
        case 1:
          for( col=0; col<IMAGE_COL_SIZE; col+=2) { 
          CSC_RGB_to_YCC_brute_force_float( row, col);
          }
          break;
        case 2:
          for( col=0; col<IMAGE_COL_SIZE; col+=2) { 
          CSC_RGB_to_YCC_brute_force_int( row, col);
          }
          break;
        case 3:
      for( col=0; col<IMAGE_COL_SIZE; col+=8) {
        CSC_RGB_to_YCC_vectors( row, col, y_base, c_base, CHROMINANCE_DOWNSAMPLING_MODE);
      }
      break;
          break;
        default:
          break;
      }
      // after a full CSC_RGB_to_YCC() run on the real striped test image

//      printf( "Luma_00  = %02hhx\n", Y[row+0][col+0]);
//      printf( "Luma_01  = %02hhx\n", Y[row+0][col+1]);
//      printf( "Luma_10  = %02hhx\n", Y[row+1][col+0]);
//      printf( "Luma_11  = %02hhx\n\n", Y[row+1][col+1]);
  }

} // END of CSC_RGB_to_YCC()
