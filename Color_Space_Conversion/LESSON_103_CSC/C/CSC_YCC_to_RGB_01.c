// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// YCC to RGB conversion

#include <stdio.h>
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
#if CSC_ENABLE_YCC_TO_RGB_OPTIMIZED && CSC_ENABLE_YCC_TO_RGB_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))

// Processes one full contiguous sub-row of `count` pixels directly out of
// the image arrays. Real contiguous loads (no gather through a temp array),
// so this amortizes NEON setup/teardown cost over a whole row instead of a
// single 2x2 block.
static void CSC_YCC_to_RGB_neon_subrow(
    const uint8_t *y_row, const uint8_t *cb_row, const uint8_t *cr_row,
    uint8_t *r_row, uint8_t *g_row, uint8_t *b_row, int count) {

  int32x4_t round  = vdupq_n_s32(1 << (CSC_FIXED_POINT_SHIFT - 1));
  int32x4_t bias_y  = vdupq_n_s32(16);
  int32x4_t bias_ch = vdupq_n_s32(128);

  int i = 0;
  for( ; i + 8 <= count; i += 8) {
    uint8x8_t y_u8  = vld1_u8( y_row + i);
    uint8x8_t cb_u8 = vld1_u8( cb_row + i);
    uint8x8_t cr_u8 = vld1_u8( cr_row + i);

    // Widen all 8 lanes to 16-bit once...
    int16x8_t y_s16  = vreinterpretq_s16_u16(vmovl_u8(y_u8));
    int16x8_t cb_s16 = vreinterpretq_s16_u16(vmovl_u8(cb_u8));
    int16x8_t cr_s16 = vreinterpretq_s16_u16(vmovl_u8(cr_u8));

    // ...then process low 4 and high 4 separately, using BOTH halves.
    for( int half = 0; half < 2; ++half) {
      int32x4_t yy  = half == 0 ? vmovl_s16(vget_low_s16(y_s16))  : vmovl_s16(vget_high_s16(y_s16));
      int32x4_t cbv = half == 0 ? vmovl_s16(vget_low_s16(cb_s16)) : vmovl_s16(vget_high_s16(cb_s16));
      int32x4_t crv = half == 0 ? vmovl_s16(vget_low_s16(cr_s16)) : vmovl_s16(vget_high_s16(cr_s16));

      yy  = vsubq_s32(yy, bias_y);
      cbv = vsubq_s32(cbv, bias_ch);
      crv = vsubq_s32(crv, bias_ch);

      int32x4_t rr = vaddq_s32(vmulq_n_s32(yy, D1), vmulq_n_s32(crv, D2));
      rr = vshrq_n_s32(vaddq_s32(rr, round), CSC_FIXED_POINT_SHIFT);

      int32x4_t gg = vmulq_n_s32(yy, D1);
      gg = vmlaq_n_s32(gg, crv, -D3);
      gg = vmlaq_n_s32(gg, cbv, -D4);
      gg = vshrq_n_s32(vaddq_s32(gg, round), CSC_FIXED_POINT_SHIFT);

      int32x4_t bb = vaddq_s32(vmulq_n_s32(yy, D1), vmulq_n_s32(cbv, D5));
      bb = vshrq_n_s32(vaddq_s32(bb, round), CSC_FIXED_POINT_SHIFT);

      int32_t r_out[4], g_out[4], b_out[4];
      vst1q_s32(r_out, rr);
      vst1q_s32(g_out, gg);
      vst1q_s32(b_out, bb);

      int base = i + half * 4;
      for( int k = 0; k < 4; ++k) {
        int v;
        v = r_out[k]; if( v < 0) v = 0; else if( v > 255) v = 255; r_row[base+k] = (uint8_t)v;
        v = g_out[k]; if( v < 0) v = 0; else if( v > 255) v = 255; g_row[base+k] = (uint8_t)v;
        v = b_out[k]; if( v < 0) v = 0; else if( v > 255) v = 255; b_row[base+k] = (uint8_t)v;
      }
    }
  }

  // Scalar cleanup for any remainder (count not divisible by 8).
  for( ; i < count; ++i) {
    int y  = (int)y_row[i]  - 16;
    int cb = (int)cb_row[i] - 128;
    int cr = (int)cr_row[i] - 128;

    int r = D1*y + D2*cr + CSC_ROUNDING;
    r >>= CSC_FIXED_POINT_SHIFT;
    int g = D1*y - D3*cr - D4*cb + CSC_ROUNDING;
    g >>= CSC_FIXED_POINT_SHIFT;
    int b = D1*y + D5*cb + CSC_ROUNDING;
    b >>= CSC_FIXED_POINT_SHIFT;

    if( r < 0) r = 0; else if( r > 255) r = 255;
    if( g < 0) g = 0; else if( g > 255) g = 255;
    if( b < 0) b = 0; else if( b > 255) b = 255;

    r_row[i] = (uint8_t)r;
    g_row[i] = (uint8_t)g;
    b_row[i] = (uint8_t)b;
  }
}

// Processes an entire row-pair (row, row+1) in two subrow calls instead of
// (IMAGE_COL_SIZE / 2) separate 2x2-block calls.
static void CSC_YCC_to_RGB_optimized_row_neon( int row) {
  CSC_YCC_to_RGB_neon_subrow( Y[row+0], Cb_temp[row+0], Cr_temp[row+0],
                               R[row+0], G[row+0], B[row+0], IMAGE_COL_SIZE);
  CSC_YCC_to_RGB_neon_subrow( Y[row+1], Cb_temp[row+1], Cr_temp[row+1],
                               R[row+1], G[row+1], B[row+1], IMAGE_COL_SIZE);
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

  // NOTE: chrominance_array_upsample() is now called once per frame by the
  // CSC_YCC_to_RGB() driver, not per-block here.

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

  // NOTE: chrominance_array_upsample() is now called once per frame by the
  // CSC_YCC_to_RGB() driver, not per-block here.

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
// Two-operand multiply-accumulate-shift-saturate. Used for the R and B
// channels, which only ever need Y plus a single chroma term. Splitting
// this out from the 3-operand version below avoids computing (and, in the
// assembly path, actually executing) a wasted "+ 0*0" MAC instruction.
static inline int csc_macc2_shift_sat(
    int a, int b, int coeff_a, int coeff_b) {
#if CSC_ENABLE_YCC_TO_RGB_ASM && (defined(__arm__) || defined(__thumb__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__))
  int out;
  __asm__ volatile (
      "mla %[out], %[a], %[coeff_a], %[round]\n\t"
      "mla %[out], %[b], %[coeff_b], %[out]\n\t"
      "asr %[out], %[out], #8\n\t"
      "usat %[out], #8, %[out]\n\t"
      : [out] "=&r" (out)
      : [a] "r" (a), [b] "r" (b),
        [coeff_a] "r" (coeff_a), [coeff_b] "r" (coeff_b),
        [round] "r" (CSC_ROUNDING)
      : "cc");
  return out;
#else
  int tmp = coeff_a * a + coeff_b * b + CSC_ROUNDING;
  tmp >>= CSC_FIXED_POINT_SHIFT;
  if( tmp < 0) {
    return 0;
  }
  if( tmp > 255) {
    return 255;
  }
  return tmp;
#endif
}

// Three-operand multiply-accumulate-shift-saturate. Used only for the G
// channel, which genuinely needs Y, Cr, and Cb all at once.
static inline int csc_macc3_shift_sat(
    int a, int b, int c,
    int coeff_a, int coeff_b, int coeff_c) {
#if CSC_ENABLE_YCC_TO_RGB_ASM && (defined(__arm__) || defined(__thumb__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__))
  int out;
  __asm__ volatile (
      "mla %[out], %[a], %[coeff_a], %[round]\n\t"
      "mla %[out], %[b], %[coeff_b], %[out]\n\t"
      "mla %[out], %[c], %[coeff_c], %[out]\n\t"
      "asr %[out], %[out], #8\n\t"
      "usat %[out], #8, %[out]\n\t"
      : [out] "=&r" (out)
      : [a] "r" (a), [b] "r" (b), [c] "r" (c),
        [coeff_a] "r" (coeff_a), [coeff_b] "r" (coeff_b), [coeff_c] "r" (coeff_c),
        [round] "r" (CSC_ROUNDING)
      : "cc");
  return out;
#else
  int tmp = coeff_a * a + coeff_b * b + coeff_c * c + CSC_ROUNDING;
  tmp >>= CSC_FIXED_POINT_SHIFT;
  if( tmp < 0) {
    return 0;
  }
  if( tmp > 255) {
    return 255;
  }
  return tmp;
#endif
}

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
// Per-2x2-block path. Used directly when NEON is disabled (or unavailable);
// also used as the ultimate fallback if the optimized path itself is
// disabled entirely.
static void CSC_YCC_to_RGB_optimized( int row, int col) {
  int y00 = (int)Y[row+0][col+0] - 16;
  int y01 = (int)Y[row+0][col+1] - 16;
  int y10 = (int)Y[row+1][col+0] - 16;
  int y11 = (int)Y[row+1][col+1] - 16;

  int cb00 = (int)Cb_temp[row+0][col+0] - 128;
  int cb01 = (int)Cb_temp[row+0][col+1] - 128;
  int cb10 = (int)Cb_temp[row+1][col+0] - 128;
  int cb11 = (int)Cb_temp[row+1][col+1] - 128;

  int cr00 = (int)Cr_temp[row+0][col+0] - 128;
  int cr01 = (int)Cr_temp[row+0][col+1] - 128;
  int cr10 = (int)Cr_temp[row+1][col+0] - 128;
  int cr11 = (int)Cr_temp[row+1][col+1] - 128;

#if CSC_ENABLE_YCC_TO_RGB_OPTIMIZED
  int r00 = csc_macc2_shift_sat( y00, cr00, D1, D2);
  int r01 = csc_macc2_shift_sat( y01, cr01, D1, D2);
  int r10 = csc_macc2_shift_sat( y10, cr10, D1, D2);
  int r11 = csc_macc2_shift_sat( y11, cr11, D1, D2);

  int g00 = csc_macc3_shift_sat( y00, cr00, cb00, D1, -D3, -D4);
  int g01 = csc_macc3_shift_sat( y01, cr01, cb01, D1, -D3, -D4);
  int g10 = csc_macc3_shift_sat( y10, cr10, cb10, D1, -D3, -D4);
  int g11 = csc_macc3_shift_sat( y11, cr11, cb11, D1, -D3, -D4);

  int b00 = csc_macc2_shift_sat( y00, cb00, D1, D5);
  int b01 = csc_macc2_shift_sat( y01, cb01, D1, D5);
  int b10 = csc_macc2_shift_sat( y10, cb10, D1, D5);
  int b11 = csc_macc2_shift_sat( y11, cb11, D1, D5);

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
#else
  CSC_YCC_to_RGB_brute_force_int( row, col);
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
  for( col=0; col<((IMAGE_COL_SIZE>>1)-1); col+=1) {
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
  // Cb/Cr only need to be upsampled once per frame -- all three routines
  // (float, brute-force int, optimized) read from Cb_temp/Cr_temp, so this
  // is hoisted out of every per-block routine and done exactly once here.
  if( YCC_to_RGB_ROUTINE == 1 || YCC_to_RGB_ROUTINE == 2 || YCC_to_RGB_ROUTINE == 3) {
    chrominance_array_upsample();
  }

#if CSC_ENABLE_YCC_TO_RGB_OPTIMIZED && CSC_ENABLE_YCC_TO_RGB_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))
  if( YCC_to_RGB_ROUTINE == 3) {
    // NEON path processes a full row-pair per iteration rather than
    // dispatching per 2x2 block.
    for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
      CSC_YCC_to_RGB_optimized_row_neon( row);
    }
    return;
  }
#endif

  for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
    for( col=0; col<IMAGE_COL_SIZE; col+=2) { 
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
    }
  }

} // END of CSC_YCC_to_RGB()