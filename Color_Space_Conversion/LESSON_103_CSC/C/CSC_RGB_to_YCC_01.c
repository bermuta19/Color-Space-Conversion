// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// RGB to YCC conversion

#include <stdio.h>
#include <stdint.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
#include "CSC_global.h"

// private data

// private prototypes
// =======
static void CSC_RGB_to_YCC_brute_force_float( int row, int col);

// =======
static void CSC_RGB_to_YCC_brute_force_int( int row, int col);

// =======
static void CSC_RGB_to_YCC_optimized(
    int row, int col,
    int *out_y0, int *out_y1, int *out_y2, int *out_y3,
    int *out_cb0, int *out_cb1, int *out_cb2, int *out_cb3,
    int *out_cr0, int *out_cr1, int *out_cr2, int *out_cr3);

// =======
static uint8_t chrominance_downsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4);

// private definitions
// =======
#if CSC_ENABLE_RGB_TO_YCC_OPTIMIZED && CSC_ENABLE_RGB_TO_YCC_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))
static void CSC_RGB_to_YCC_neon_4px(
    const uint8_t *r, const uint8_t *g, const uint8_t *b,
    uint8_t *y, uint8_t *cb, uint8_t *cr) {
  int32_t r_vals[4] = {r[0], r[1], r[2], r[3]};
  int32_t g_vals[4] = {g[0], g[1], g[2], g[3]};
  int32_t b_vals[4] = {b[0], b[1], b[2], b[3]};
  int32_t y_out[4];
  int32_t cb_out[4];
  int32_t cr_out[4];
  int32x4_t rr = vld1q_s32(r_vals);
  int32x4_t gg = vld1q_s32(g_vals);
  int32x4_t bb = vld1q_s32(b_vals);
  int32x4_t round = vdupq_n_s32(1 << (CSC_FIXED_POINT_SHIFT - 1));

  int32x4_t y_vec = vdupq_n_s32(16 << CSC_FIXED_POINT_SHIFT);
  y_vec = vmlaq_n_s32(y_vec, rr, C11);
  y_vec = vmlaq_n_s32(y_vec, gg, C12);
  y_vec = vmlaq_n_s32(y_vec, bb, C13);
  y_vec = vaddq_s32(y_vec, round);
  y_vec = vshrq_n_s32(y_vec, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(y_out, y_vec);

  int32x4_t cb_vec = vdupq_n_s32(128 << CSC_FIXED_POINT_SHIFT);
  cb_vec = vsubq_s32(cb_vec, vmulq_n_s32(rr, C21));
  cb_vec = vsubq_s32(cb_vec, vmulq_n_s32(gg, C22));
  cb_vec = vaddq_s32(cb_vec, vmulq_n_s32(bb, C23));
  cb_vec = vaddq_s32(cb_vec, round);
  cb_vec = vshrq_n_s32(cb_vec, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(cb_out, cb_vec);

  int32x4_t cr_vec = vdupq_n_s32(128 << CSC_FIXED_POINT_SHIFT);
  cr_vec = vaddq_s32(cr_vec, vmulq_n_s32(rr, C31));
  cr_vec = vsubq_s32(cr_vec, vmulq_n_s32(gg, C32));
  cr_vec = vsubq_s32(cr_vec, vmulq_n_s32(bb, C33));
  cr_vec = vaddq_s32(cr_vec, round);
  cr_vec = vshrq_n_s32(cr_vec, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(cr_out, cr_vec);

  for( int i = 0; i < 4; ++i) {
    y[i] = (uint8_t)y_out[i];
    cb[i] = (uint8_t)cb_out[i];
    cr[i] = (uint8_t)cr_out[i];
  }
}
#endif

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
static void CSC_RGB_to_YCC_optimized(
    int row, int col,
    int *out_y0, int *out_y1, int *out_y2, int *out_y3,
    int *out_cb0, int *out_cb1, int *out_cb2, int *out_cb3,
    int *out_cr0, int *out_cr1, int *out_cr2, int *out_cr3) {
  const int r0 = (int)R[row+0][col+0];
  const int r1 = (int)R[row+0][col+1];
  const int r2 = (int)R[row+1][col+0];
  const int r3 = (int)R[row+1][col+1];
  const int g0 = (int)G[row+0][col+0];
  const int g1 = (int)G[row+0][col+1];
  const int g2 = (int)G[row+1][col+0];
  const int g3 = (int)G[row+1][col+1];
  const int b0 = (int)B[row+0][col+0];
  const int b1 = (int)B[row+0][col+1];
  const int b2 = (int)B[row+1][col+0];
  const int b3 = (int)B[row+1][col+1];

  const int bias = 16 << CSC_FIXED_POINT_SHIFT;
  const int bias_ch = 128 << CSC_FIXED_POINT_SHIFT;
  const int round = 1 << (CSC_FIXED_POINT_SHIFT - 1);

#if CSC_ENABLE_RGB_TO_YCC_OPTIMIZED
#if CSC_ENABLE_RGB_TO_YCC_NEON && CSC_USE_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))
  {
    fprintf(stderr, "[CSC_RGB_to_YCC] using NEON optimized path\n");
    uint8_t y_block[4];
    uint8_t cb_block[4];
    uint8_t cr_block[4];
    const uint8_t r_block[4] = { (uint8_t)r0, (uint8_t)r1, (uint8_t)r2, (uint8_t)r3 };
    const uint8_t g_block[4] = { (uint8_t)g0, (uint8_t)g1, (uint8_t)g2, (uint8_t)g3 };
    const uint8_t b_block[4] = { (uint8_t)b0, (uint8_t)b1, (uint8_t)b2, (uint8_t)b3 };

    CSC_RGB_to_YCC_neon_4px( r_block, g_block, b_block, y_block, cb_block, cr_block);
    *out_y0 = y_block[0];
    *out_y1 = y_block[1];
    *out_y2 = y_block[2];
    *out_y3 = y_block[3];
    *out_cb0 = cb_block[0];
    *out_cb1 = cb_block[1];
    *out_cb2 = cb_block[2];
    *out_cb3 = cb_block[3];
    *out_cr0 = cr_block[0];
    *out_cr1 = cr_block[1];
    *out_cr2 = cr_block[2];
    *out_cr3 = cr_block[3];
    return;
  }
#else
  int y0 = bias + C11 * r0 + C12 * g0 + C13 * b0 + round;
  int y1 = bias + C11 * r1 + C12 * g1 + C13 * b1 + round;
  int y2 = bias + C11 * r2 + C12 * g2 + C13 * b2 + round;
  int y3 = bias + C11 * r3 + C12 * g3 + C13 * b3 + round;
  y0 >>= CSC_FIXED_POINT_SHIFT;
  y1 >>= CSC_FIXED_POINT_SHIFT;
  y2 >>= CSC_FIXED_POINT_SHIFT;
  y3 >>= CSC_FIXED_POINT_SHIFT;

  int cb0 = bias_ch - C21 * r0 - C22 * g0 + C23 * b0 + round;
  int cb1 = bias_ch - C21 * r1 - C22 * g1 + C23 * b1 + round;
  int cb2 = bias_ch - C21 * r2 - C22 * g2 + C23 * b2 + round;
  int cb3 = bias_ch - C21 * r3 - C22 * g3 + C23 * b3 + round;
  cb0 >>= CSC_FIXED_POINT_SHIFT;
  cb1 >>= CSC_FIXED_POINT_SHIFT;
  cb2 >>= CSC_FIXED_POINT_SHIFT;
  cb3 >>= CSC_FIXED_POINT_SHIFT;

  int cr0 = bias_ch + C31 * r0 - C32 * g0 - C33 * b0 + round;
  int cr1 = bias_ch + C31 * r1 - C32 * g1 - C33 * b1 + round;
  int cr2 = bias_ch + C31 * r2 - C32 * g2 - C33 * b2 + round;
  int cr3 = bias_ch + C31 * r3 - C32 * g3 - C33 * b3 + round;
  cr0 >>= CSC_FIXED_POINT_SHIFT;
  cr1 >>= CSC_FIXED_POINT_SHIFT;
  cr2 >>= CSC_FIXED_POINT_SHIFT;
  cr3 >>= CSC_FIXED_POINT_SHIFT;

  *out_y0 = y0;
  *out_y1 = y1;
  *out_y2 = y2;
  *out_y3 = y3;
  *out_cb0 = cb0;
  *out_cb1 = cb1;
  *out_cb2 = cb2;
  *out_cb3 = cb3;
  *out_cr0 = cr0;
  *out_cr1 = cr1;
  *out_cr2 = cr2;
  *out_cr3 = cr3;
#endif
#else
  fprintf(stderr, "[CSC_RGB_to_YCC] optimized path disabled; using scalar fallback\n");
  int y0 = bias + C11 * r0 + C12 * g0 + C13 * b0 + round;
  int y1 = bias + C11 * r1 + C12 * g1 + C13 * b1 + round;
  int y2 = bias + C11 * r2 + C12 * g2 + C13 * b2 + round;
  int y3 = bias + C11 * r3 + C12 * g3 + C13 * b3 + round;
  y0 >>= CSC_FIXED_POINT_SHIFT;
  y1 >>= CSC_FIXED_POINT_SHIFT;
  y2 >>= CSC_FIXED_POINT_SHIFT;
  y3 >>= CSC_FIXED_POINT_SHIFT;

  int cb0 = bias_ch - C21 * r0 - C22 * g0 + C23 * b0 + round;
  int cb1 = bias_ch - C21 * r1 - C22 * g1 + C23 * b1 + round;
  int cb2 = bias_ch - C21 * r2 - C22 * g2 + C23 * b2 + round;
  int cb3 = bias_ch - C21 * r3 - C22 * g3 + C23 * b3 + round;
  cb0 >>= CSC_FIXED_POINT_SHIFT;
  cb1 >>= CSC_FIXED_POINT_SHIFT;
  cb2 >>= CSC_FIXED_POINT_SHIFT;
  cb3 >>= CSC_FIXED_POINT_SHIFT;

  int cr0 = bias_ch + C31 * r0 - C32 * g0 - C33 * b0 + round;
  int cr1 = bias_ch + C31 * r1 - C32 * g1 - C33 * b1 + round;
  int cr2 = bias_ch + C31 * r2 - C32 * g2 - C33 * b2 + round;
  int cr3 = bias_ch + C31 * r3 - C32 * g3 - C33 * b3 + round;
  cr0 >>= CSC_FIXED_POINT_SHIFT;
  cr1 >>= CSC_FIXED_POINT_SHIFT;
  cr2 >>= CSC_FIXED_POINT_SHIFT;
  cr3 >>= CSC_FIXED_POINT_SHIFT;

  *out_y0 = y0;
  *out_y1 = y1;
  *out_y2 = y2;
  *out_y3 = y3;
  *out_cb0 = cb0;
  *out_cb1 = cb1;
  *out_cb2 = cb2;
  *out_cb3 = cb3;
  *out_cr0 = cr0;
  *out_cr1 = cr1;
  *out_cr2 = cr2;
  *out_cr3 = cr3;
#endif
}

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
//
  for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
    for( col=0; col<IMAGE_COL_SIZE; col+=2) { 
      //printf( "\n[row,col] = [%02i,%02i]\n\n", row, col);
      switch (RGB_to_YCC_ROUTINE) {
        case 0:
          break;
        case 1:
          CSC_RGB_to_YCC_brute_force_float( row, col);
          break;
        case 2:
          CSC_RGB_to_YCC_brute_force_int( row, col);
          break;
        case 3: {
          int y0, y1, y2, y3;
          int cb0, cb1, cb2, cb3;
          int cr0, cr1, cr2, cr3;

          CSC_RGB_to_YCC_optimized( row, col,
                                    &y0, &y1, &y2, &y3,
                                    &cb0, &cb1, &cb2, &cb3,
                                    &cr0, &cr1, &cr2, &cr3);

          Y[row+0][col+0] = (uint8_t)y0;
          Y[row+0][col+1] = (uint8_t)y1;
          Y[row+1][col+0] = (uint8_t)y2;
          Y[row+1][col+1] = (uint8_t)y3;

          Cb[row>>1][col>>1] = chrominance_downsample((uint8_t)cb0, (uint8_t)cb1,
                                                       (uint8_t)cb2, (uint8_t)cb3);
          Cr[row>>1][col>>1] = chrominance_downsample((uint8_t)cr0, (uint8_t)cr1,
                                                       (uint8_t)cr2, (uint8_t)cr3);
          break;
        }
        default:
          break;
      }
//      printf( "Luma_00  = %02hhx\n", Y[row+0][col+0]);
//      printf( "Luma_01  = %02hhx\n", Y[row+0][col+1]);
//      printf( "Luma_10  = %02hhx\n", Y[row+1][col+0]);
//      printf( "Luma_11  = %02hhx\n\n", Y[row+1][col+1]);
    }
  }

} // END of CSC_RGB_to_YCC()

