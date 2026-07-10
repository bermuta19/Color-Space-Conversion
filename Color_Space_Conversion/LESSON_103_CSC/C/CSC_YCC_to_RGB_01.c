// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// YCC to RGB conversion

//#include <stdio.h>
#include <stdint.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
#include "CSC_global.h"

// private data

// private prototypes
// =======
static uint8_t saturation_float( float argument);
static void CSC_YCC_to_RGB_brute_force_float( int row, int col);

// =======
static uint8_t saturation_int( int argument);
static void CSC_YCC_to_RGB_brute_force_int( int row, int col);

// =======
static void CSC_YCC_to_RGB_optimized( int row, int col);

// =======
static void chrominance_upsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4,
    uint8_t *top, uint8_t *left, uint8_t *middle);
// =======
static void chrominance_array_upsample( void);

// private definitions
// =======
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
static void CSC_YCC_to_RGB_neon_4px(
    const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
    uint8_t *r, uint8_t *g, uint8_t *b) {
  int32_t y_vals[4] = {y[0], y[1], y[2], y[3]};
  int32_t cb_vals[4] = {cb[0], cb[1], cb[2], cb[3]};
  int32_t cr_vals[4] = {cr[0], cr[1], cr[2], cr[3]};
  int32_t r_out[4];
  int32_t g_out[4];
  int32_t b_out[4];
  int32x4_t yy = vld1q_s32(y_vals);
  int32x4_t cb_vec = vld1q_s32(cb_vals);
  int32x4_t cr_vec = vld1q_s32(cr_vals);
  int32x4_t round = vdupq_n_s32(1 << (CSC_FIXED_POINT_SHIFT - 1));
  int32x4_t bias_y = vdupq_n_s32(16);
  int32x4_t bias_cb = vdupq_n_s32(128);

  yy = vsubq_s32(yy, bias_y);
  cb_vec = vsubq_s32(cb_vec, bias_cb);
  cr_vec = vsubq_s32(cr_vec, bias_cb);

  int32x4_t rr = vaddq_s32(vmulq_n_s32(yy, D1), vmulq_n_s32(cr_vec, D2));
  rr = vaddq_s32(rr, round);
  rr = vshrq_n_s32(rr, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(r_out, rr);

  int32x4_t gg = vmulq_n_s32(yy, D1);
  gg = vmlaq_n_s32(gg, cr_vec, -D3);
  gg = vmlaq_n_s32(gg, cb_vec, -D4);
  gg = vaddq_s32(gg, round);
  gg = vshrq_n_s32(gg, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(g_out, gg);

  int32x4_t bb = vaddq_s32(vmulq_n_s32(yy, D1), vmulq_n_s32(cb_vec, D5));
  bb = vaddq_s32(bb, round);
  bb = vshrq_n_s32(bb, CSC_FIXED_POINT_SHIFT);
  vst1q_s32(b_out, bb);

  for( int i = 0; i < 4; ++i) {
    int v = r_out[i];
    if( v < 0) v = 0;
    else if( v > 255) v = 255;
    r[i] = (uint8_t)v;

    v = g_out[i];
    if( v < 0) v = 0;
    else if( v > 255) v = 255;
    g[i] = (uint8_t)v;

    v = b_out[i];
    if( v < 0) v = 0;
    else if( v > 255) v = 255;
    b[i] = (uint8_t)v;
  }
}
#endif

static uint8_t saturation_float( float argument) {
  if( argument > 255.0) { // saturation
    return( (uint8_t)255);
  }
  else if( argument < 0.0) {
    return( (uint8_t)0);
  }
  else {
    return( (uint8_t)argument);
  }
} // END of saturation_float()

// =======
static void CSC_YCC_to_RGB_brute_force_float( int row, int col) {
//
  float R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  float G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  float B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;

  // Upsample Cb and Cr into Cb_temp and Cr_temp
  chrominance_array_upsample();

  R_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0)
               + 1.596*(Cr_temp[row+0][col+0] - 128.0);
  R[row+0][col+0] = saturation_float( R_pixel_00);
//
  R_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0)
               + 1.596*(Cr_temp[row+0][col+1] - 128.0);
  R[row+0][col+1] = saturation_float( R_pixel_01);
//
  R_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0)
               + 1.596*(Cr_temp[row+1][col+0] - 128.0);
  R[row+1][col+0] = saturation_float( R_pixel_10);
//
  R_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0)
               + 1.596*(Cr_temp[row+1][col+1] - 128.0);
  R[row+1][col+1] = saturation_float( R_pixel_11);

  G_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0)
               - 0.813*(Cr_temp[row+0][col+0] - 128.0)
               - 0.391*(Cb_temp[row+0][col+0] - 128.0);
  G[row+0][col+0] = saturation_float( G_pixel_00);
//
  G_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0)
               - 0.813*(Cr_temp[row+0][col+1] - 128.0)
               - 0.391*(Cb_temp[row+0][col+1] - 128.0);
  G[row+0][col+1] = saturation_float( G_pixel_01);
//
  G_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0)
               - 0.813*(Cr_temp[row+1][col+0] - 128.0)
               - 0.391*(Cb_temp[row+1][col+0] - 128.0);
  G[row+1][col+0] = saturation_float( G_pixel_10);
//
  G_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0)
               - 0.813*(Cr_temp[row+1][col+1] - 128.0)
               - 0.391*(Cb_temp[row+1][col+1] - 128.0);
  G[row+1][col+1] = saturation_float( G_pixel_11);

  B_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0)
               + 2.018*(Cb_temp[row+0][col+0] - 128.0);
  B[row+0][col+0] = saturation_float( B_pixel_00);
//
  B_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0)
               + 2.018*(Cb_temp[row+0][col+1] - 128.0);
  B[row+0][col+1] = saturation_float( B_pixel_01);
//
  B_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0)
               + 2.018*(Cb_temp[row+1][col+0] - 128.0);
  B[row+1][col+0] = saturation_float( B_pixel_10);
//
  B_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0)
               + 2.018*(Cb_temp[row+1][col+1] - 128.0);
  B[row+1][col+1] = saturation_float( B_pixel_11);
} // END of CSC_YCC_to_RGB_brute_force_float()

// =======
static uint8_t saturation_int( int argument) {
  if( argument > 255) { // saturation
    return( (uint8_t)255);
  }
  else if( argument < 0) {
    return( (uint8_t)0);
  }
  else {
    return( (uint8_t)argument);
  }
} // END of saturation_int()

// =======
static void CSC_YCC_to_RGB_brute_force_int( int row, int col) {
//
  int R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  int G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  int B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;

  int  Y_pixel_00,  Y_pixel_01,  Y_pixel_10,  Y_pixel_11;
  int Cb_pixel_00, Cb_pixel_01, Cb_pixel_10, Cb_pixel_11;
  int Cr_pixel_00, Cr_pixel_01, Cr_pixel_10, Cr_pixel_11;

  // Upsample Cb and Cr into Cb_temp and Cr_temp
  chrominance_array_upsample();

  Y_pixel_00 = (int)Y[row+0][col+0];
  Y_pixel_01 = (int)Y[row+0][col+1];
  Y_pixel_10 = (int)Y[row+1][col+0];
  Y_pixel_11 = (int)Y[row+1][col+1];

  Cb_pixel_00 = (int)Cb_temp[row+0][col+0];
  Cb_pixel_01 = (int)Cb_temp[row+0][col+1];
  Cb_pixel_10 = (int)Cb_temp[row+1][col+0];
  Cb_pixel_11 = (int)Cb_temp[row+1][col+1];

  Cr_pixel_00 = (int)Cr_temp[row+0][col+0];
  Cr_pixel_01 = (int)Cr_temp[row+0][col+1];
  Cr_pixel_10 = (int)Cr_temp[row+1][col+0];
  Cr_pixel_11 = (int)Cr_temp[row+1][col+1];

  Y_pixel_00 = Y_pixel_00 - 16;
  Y_pixel_01 = Y_pixel_01 - 16;
  Y_pixel_10 = Y_pixel_10 - 16;
  Y_pixel_11 = Y_pixel_11 - 16;

  Cb_pixel_00 = Cb_pixel_00 - 128;
  Cb_pixel_01 = Cb_pixel_01 - 128;
  Cb_pixel_10 = Cb_pixel_10 - 128;
  Cb_pixel_11 = Cb_pixel_11 - 128;

  Cr_pixel_00 = Cr_pixel_00 - 128;
  Cr_pixel_01 = Cr_pixel_01 - 128;
  Cr_pixel_10 = Cr_pixel_10 - 128;
  Cr_pixel_11 = Cr_pixel_11 - 128;

  R_pixel_00 = D1 * Y_pixel_00 + D2 * Cr_pixel_00;
  R_pixel_00 += (1 << (K-1)); // rounding
  R_pixel_00 = R_pixel_00 >> K;

  R_pixel_01 = D1 * Y_pixel_01 + D2 * Cr_pixel_01;
  R_pixel_01 += (1 << (K-1)); // rounding
  R_pixel_01 = R_pixel_01 >> K;

  R_pixel_10 = D1 * Y_pixel_10 + D2 * Cr_pixel_10;
  R_pixel_10 += (1 << (K-1)); // rounding
  R_pixel_10 = R_pixel_10 >> K;

  R_pixel_11 = D1 * Y_pixel_11 + D2 * Cr_pixel_11;
  R_pixel_11 += (1 << (K-1)); // rounding
  R_pixel_11 = R_pixel_11 >> K;

  R[row+0][col+0] = (uint8_t)R_pixel_00;
  R[row+0][col+1] = (uint8_t)R_pixel_01;
  R[row+1][col+0] = (uint8_t)R_pixel_10;
  R[row+1][col+1] = (uint8_t)R_pixel_11;

  G_pixel_00 = D1 * Y_pixel_00 - D3 * Cr_pixel_00
                               - D4 * Cb_pixel_00;
  G_pixel_00 += (1 << (K-1)); // rounding
  G_pixel_00 = G_pixel_00 >> K;

  G_pixel_01 = D1 * Y_pixel_01 - D3 * Cr_pixel_01
                               - D4 * Cb_pixel_01;
  G_pixel_01 += (1 << (K-1)); // rounding
  G_pixel_01 = G_pixel_01 >> K;

  G_pixel_10 = D1 * Y_pixel_10 - D3 * Cr_pixel_10
                               - D4 * Cb_pixel_10;
  G_pixel_10 += (1 << (K-1)); // rounding
  G_pixel_10 = G_pixel_10 >> K;

  G_pixel_11 = D1 * Y_pixel_11 - D3 * Cr_pixel_11
                               - D4 * Cb_pixel_11;
  G_pixel_11 += (1 << (K-1)); // rounding
  G_pixel_11 = G_pixel_11 >> K;

  G[row+0][col+0] = (uint8_t)G_pixel_00;
  G[row+0][col+1] = (uint8_t)G_pixel_01;
  G[row+1][col+0] = (uint8_t)G_pixel_10;
  G[row+1][col+1] = (uint8_t)G_pixel_11;

  B_pixel_00 = D1 * Y_pixel_00 + D5 * Cb_pixel_00;
  B_pixel_00 += (1 << (K-1)); // rounding
  B_pixel_00 = B_pixel_00 >> K;

  B_pixel_01 = D1 * Y_pixel_01 + D5 * Cb_pixel_01;
  B_pixel_01 += (1 << (K-1)); // rounding
  B_pixel_01 = B_pixel_01 >> K;

  B_pixel_10 = D1 * Y_pixel_10 + D5 * Cb_pixel_10;
  B_pixel_10 += (1 << (K-1)); // rounding
  B_pixel_10 = B_pixel_10 >> K;

  B_pixel_11 = D1 * Y_pixel_11 + D5 * Cb_pixel_11;
  B_pixel_11 += (1 << (K-1)); // rounding
  B_pixel_11 = B_pixel_11 >> K;

  B[row+0][col+0] = (uint8_t)B_pixel_00;
  B[row+0][col+1] = (uint8_t)B_pixel_01;
  B[row+1][col+0] = (uint8_t)B_pixel_10;
  B[row+1][col+1] = (uint8_t)B_pixel_11;

} // END of CSC_YCC_to_RGB_brute_force_int()

// =======
static uint8_t saturate_to_u8( int value) {
  if( value < 0) {
    return 0;
  }
  if( value > 255) {
    return 255;
  }
  return (uint8_t)value;
}

// =======
static void CSC_YCC_to_RGB_optimized( int row, int col) {
  printf("CSC_YCC_to_RGB optimized is reached\n");
  int y00 = (int)Y[row+0][col+0] - 16;
  int y01 = (int)Y[row+0][col+1] - 16;
  int y10 = (int)Y[row+1][col+0] - 16;
  int y11 = (int)Y[row+1][col+1] - 16;

  chrominance_array_upsample();

  int cb00 = (int)Cb_temp[row+0][col+0] - 128;
  int cb01 = (int)Cb_temp[row+0][col+1] - 128;
  int cb10 = (int)Cb_temp[row+1][col+0] - 128;
  int cb11 = (int)Cb_temp[row+1][col+1] - 128;

  int cr00 = (int)Cr_temp[row+0][col+0] - 128;
  int cr01 = (int)Cr_temp[row+0][col+1] - 128;
  int cr10 = (int)Cr_temp[row+1][col+0] - 128;
  int cr11 = (int)Cr_temp[row+1][col+1] - 128;
  printf("initial setup is done\n")
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  {
    printf("NEON is used\n");
    uint8_t y_block[4] = { (uint8_t)(y00 + 16), (uint8_t)(y01 + 16), (uint8_t)(y10 + 16), (uint8_t)(y11 + 16) };
    uint8_t cb_block[4] = { (uint8_t)(cb00 + 128), (uint8_t)(cb01 + 128), (uint8_t)(cb10 + 128), (uint8_t)(cb11 + 128) };
    uint8_t cr_block[4] = { (uint8_t)(cr00 + 128), (uint8_t)(cr01 + 128), (uint8_t)(cr10 + 128), (uint8_t)(cr11 + 128) };
    uint8_t r_block[4], g_block[4], b_block[4];

    CSC_YCC_to_RGB_neon_4px( y_block, cb_block, cr_block, r_block, g_block, b_block);
    R[row+0][col+0] = r_block[0];
    R[row+0][col+1] = r_block[1];
    R[row+1][col+0] = r_block[2];
    R[row+1][col+1] = r_block[3];
    G[row+0][col+0] = g_block[0];
    G[row+0][col+1] = g_block[1];
    G[row+1][col+0] = g_block[2];
    G[row+1][col+1] = g_block[3];
    B[row+0][col+0] = b_block[0];
    B[row+0][col+1] = b_block[1];
    B[row+1][col+0] = b_block[2];
    B[row+1][col+1] = b_block[3];
    return;
  }
#else
  printf("NEON is not used\n");
  int r00 = (D1 * y00 + D2 * cr00 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int r01 = (D1 * y01 + D2 * cr01 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int r10 = (D1 * y10 + D2 * cr10 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int r11 = (D1 * y11 + D2 * cr11 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;

  int g00 = (D1 * y00 - D3 * cr00 - D4 * cb00 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int g01 = (D1 * y01 - D3 * cr01 - D4 * cb01 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int g10 = (D1 * y10 - D3 * cr10 - D4 * cb10 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int g11 = (D1 * y11 - D3 * cr11 - D4 * cb11 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;

  int b00 = (D1 * y00 + D5 * cb00 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int b01 = (D1 * y01 + D5 * cb01 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int b10 = (D1 * y10 + D5 * cb10 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
  int b11 = (D1 * y11 + D5 * cb11 + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;

  R[row+0][col+0] = saturate_to_u8(r00);
  R[row+0][col+1] = saturate_to_u8(r01);
  R[row+1][col+0] = saturate_to_u8(r10);
  R[row+1][col+1] = saturate_to_u8(r11);

  G[row+0][col+0] = saturate_to_u8(g00);
  G[row+0][col+1] = saturate_to_u8(g01);
  G[row+1][col+0] = saturate_to_u8(g10);
  G[row+1][col+1] = saturate_to_u8(g11);

  B[row+0][col+0] = saturate_to_u8(b00);
  B[row+0][col+1] = saturate_to_u8(b01);
  B[row+1][col+0] = saturate_to_u8(b10);
  B[row+1][col+1] = saturate_to_u8(b11);
#endif
}

// =======
static void chrominance_upsample(
    uint8_t C_pixel_00, uint8_t C_pixel_01,
    uint8_t C_pixel_10, uint8_t C_pixel_11,
    uint8_t *top, uint8_t *left, uint8_t *middle) {

  int temp_top;
  int temp_left;
  int temp_middle;

  switch (CHROMINANCE_UPSAMPLING_MODE) {
    case 0:
      *top = 0;
      *left = 0;
      *middle = 0;
      break;
    case 1:
      *top = (uint8_t)C_pixel_00;
      *left = (uint8_t)C_pixel_00;
      *middle = (uint8_t)C_pixel_00;
      break;
    case 2:
      temp_top = (int)C_pixel_00 + (int)C_pixel_01;
      temp_top += (1 << 0); // rounding
      *top = (uint8_t)(temp_top >> 1);
//
      temp_left = (int)C_pixel_00 + (int)C_pixel_10;
      temp_left += (1 << 0); // rounding
      *left = (uint8_t)(temp_left >> 1);
//
      temp_middle = (int)C_pixel_00 + (int)C_pixel_01 + 
                    (int)C_pixel_10 + (int)C_pixel_11;
      temp_middle += (1 << 1); // rounding
      *middle = (uint8_t)(temp_middle >> 2);
      break;
    default:
      break;
  }
} // END of chrominance_upsample()

// =======
static void chrominance_array_upsample( void) {
  int row, col;

  uint8_t top;
  uint8_t left;
  uint8_t middle;

  for( row=0; row<((IMAGE_ROW_SIZE>>1)-1); row+=1) {
    for( col=0; col<((IMAGE_COL_SIZE>>1)-1); col+=1) { 
      chrominance_upsample( Cb[row+0][col+0], Cb[row+0][col+1],
                            Cb[row+1][col+0], Cb[row+1][col+1],
                            &top, &left, &middle);
      Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row+0][col+0];
      Cb_temp[(row<<1)+0][(col<<1)+1] = top;
      Cb_temp[(row<<1)+1][(col<<1)+0] = left;
      Cb_temp[(row<<1)+1][(col<<1)+1] = middle;
      //
      chrominance_upsample( Cr[row+0][col+0], Cr[row+0][col+1],
                            Cr[row+1][col+0], Cr[row+1][col+1],
                            &top, &left, &middle);
      Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row+0][col+0];
      Cr_temp[(row<<1)+0][(col<<1)+1] = top;
      Cr_temp[(row<<1)+1][(col<<1)+0] = left;
      Cr_temp[(row<<1)+1][(col<<1)+1] = middle;
    }
  }

  col = (IMAGE_COL_SIZE>>1) - 1;
  for( row=0; row<((IMAGE_ROW_SIZE>>1)-1); row+=1) {
    chrominance_upsample( Cb[row+0][col], Cb[row+0][col],
                          Cb[row+1][col], Cb[row+1][col],
                          &top, &left, &middle);
    Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row+0][col];
    Cb_temp[(row<<1)+0][(col<<1)+1] = top;
    Cb_temp[(row<<1)+1][(col<<1)+0] = left;
    Cb_temp[(row<<1)+1][(col<<1)+1] = middle;
    //
    chrominance_upsample( Cr[row+0][col], Cr[row+0][col],
                          Cr[row+1][col], Cr[row+1][col],
                          &top, &left, &middle);
    Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row+0][col];
    Cr_temp[(row<<1)+0][(col<<1)+1] = top;
    Cr_temp[(row<<1)+1][(col<<1)+0] = left;
    Cr_temp[(row<<1)+1][(col<<1)+1] = middle;
  }

  row = (IMAGE_ROW_SIZE>>1) - 1;
  for( col=0; row<((IMAGE_COL_SIZE>>1)-1); col+=1) {
    chrominance_upsample( Cb[row][col+0], Cb[row][col+1],
                          Cb[row][col+0], Cb[row][col+1],
                          &top, &left, &middle);
    Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row][col+0];
    Cb_temp[(row<<1)+0][(col<<1)+1] = top;
    Cb_temp[(row<<1)+1][(col<<1)+0] = left;
    Cb_temp[(row<<1)+1][(col<<1)+1] = middle;
    //
    chrominance_upsample( Cr[row][col+0], Cr[row][col+1],
                          Cr[row][col+0], Cr[row][col+1],
                          &top, &left, &middle);
    Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row][col+0];
    Cr_temp[(row<<1)+0][(col<<1)+1] = top;
    Cr_temp[(row<<1)+1][(col<<1)+0] = left;
    Cr_temp[(row<<1)+1][(col<<1)+1] = middle;
  }

  row = (IMAGE_ROW_SIZE>>1) - 1;
  col = (IMAGE_COL_SIZE>>1) - 1;
  Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row][col];
  Cb_temp[(row<<1)+0][(col<<1)+1] = Cb[row][col];
  Cb_temp[(row<<1)+1][(col<<1)+0] = Cb[row][col];
  Cb_temp[(row<<1)+1][(col<<1)+1] = Cb[row][col];
  //
  Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row][col];
  Cr_temp[(row<<1)+0][(col<<1)+1] = Cr[row][col];
  Cr_temp[(row<<1)+1][(col<<1)+0] = Cr[row][col];
  Cr_temp[(row<<1)+1][(col<<1)+1] = Cr[row][col];

} // END of chrominance_array_upsample()

// =======
void CSC_YCC_to_RGB( void) {
  int row, col; // indices for row and column
//
  for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
    for( col=0; col<IMAGE_COL_SIZE; col+=2) { 
      //printf( "\n[row,col] = [%02i,%02i]\n\n", row, col);
      switch (YCC_to_RGB_ROUTINE) {
        case 0:
          break;
        case 1:
          CSC_YCC_to_RGB_brute_force_float( row, col);
          break;
        case 2:
          CSC_YCC_to_RGB_brute_force_int( row, col);
          break;
        case 3:
          CSC_YCC_to_RGB_optimized( row, col);
          break;
        default:
          break;
      }
//      printf( "Luma_00  = %02hhx\n", Y[row+0][col+0]);
//      printf( "Luma_01  = %02hhx\n", Y[row+0][col+1]);
//      printf( "Luma_10  = %02hhx\n", Y[row+1][col+0]);
//      printf( "Luma_11  = %02hhx\n\n", Y[row+1][col+1]);
    }
  }

} // END of CSC_YCC_to_RGB()

