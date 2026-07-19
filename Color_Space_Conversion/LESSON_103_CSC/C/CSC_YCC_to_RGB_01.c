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
static uint8_t saturate_to_u8( int value);
static uint8_t saturation_float( float argument);
static void CSC_YCC_to_RGB_brute_force_float( int row, int col);

// =======
static uint8_t saturation_int( int argument);
static void CSC_YCC_to_RGB_brute_force_int( int row, int col);

// =======
//static void CSC_YCC_to_RGB_optimized( int row, int col);

// =======
static void chrominance_upsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4,
    uint8_t *top, uint8_t *left, uint8_t *middle);
// =======
static void chrominance_array_upsample( void);

//TODO verify

// D1..D5 and K are assumed to be the same compile-time constants used by
// the scalar version. K MUST be a compile-time immediate (1..32) because
// vshrq_n_s32 requires an immediate shift amount.
// D1..D5, K: same fixed-point constants as the scalar version.
// K must be a compile-time immediate (vshrq_n_s32 requires it).

static inline void CSC_YCC_to_RGB_neon_8( const uint8_t *Yp,
                                           const uint8_t *Cbp,
                                           const uint8_t *Crp,
                                           uint8_t *Rp,
                                           uint8_t *Gp,
                                           uint8_t *Bp)
{
    // ---- load 8 pixels, widen u8 -> s16 -> s32 ----
    int16x8_t y16  = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(Yp)));
    int16x8_t cb16 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(Cbp)));
    int16x8_t cr16 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(Crp)));

    y16  = vsubq_s16(y16,  vdupq_n_s16(16));
    cb16 = vsubq_s16(cb16, vdupq_n_s16(128));
    cr16 = vsubq_s16(cr16, vdupq_n_s16(128));

    int32x4_t y_lo  = vmovl_s16(vget_low_s16(y16));
    int32x4_t y_hi  = vmovl_s16(vget_high_s16(y16));
    int32x4_t cb_lo = vmovl_s16(vget_low_s16(cb16));
    int32x4_t cb_hi = vmovl_s16(vget_high_s16(cb16));
    int32x4_t cr_lo = vmovl_s16(vget_low_s16(cr16));
    int32x4_t cr_hi = vmovl_s16(vget_high_s16(cr16));

    const int32x4_t d1    = vdupq_n_s32(D1);
    const int32x4_t d2    = vdupq_n_s32(D2);
    const int32x4_t d3    = vdupq_n_s32(D3);
    const int32x4_t d4    = vdupq_n_s32(D4);
    const int32x4_t d5    = vdupq_n_s32(D5);
    const int32x4_t round = vdupq_n_s32(1 << (K - 1));

    // ---- R = D1*Y + D2*Cr, +round, >>K ----
    int32x4_t r_lo = vmlaq_s32(vmulq_s32(d1, y_lo), d2, cr_lo);
    int32x4_t r_hi = vmlaq_s32(vmulq_s32(d1, y_hi), d2, cr_hi);
    r_lo = vshrq_n_s32(vaddq_s32(r_lo, round), K);
    r_hi = vshrq_n_s32(vaddq_s32(r_hi, round), K);

    // ---- G = D1*Y - D3*Cr - D4*Cb, +round, >>K ----
    int32x4_t g_lo = vmlsq_s32(vmlsq_s32(vmulq_s32(d1, y_lo), d3, cr_lo), d4, cb_lo);
    int32x4_t g_hi = vmlsq_s32(vmlsq_s32(vmulq_s32(d1, y_hi), d3, cr_hi), d4, cb_hi);
    g_lo = vshrq_n_s32(vaddq_s32(g_lo, round), K);
    g_hi = vshrq_n_s32(vaddq_s32(g_hi, round), K);

    // ---- B = D1*Y + D5*Cb, +round, >>K ----
    int32x4_t b_lo = vmlaq_s32(vmulq_s32(d1, y_lo), d5, cb_lo);
    int32x4_t b_hi = vmlaq_s32(vmulq_s32(d1, y_hi), d5, cb_hi);
    b_lo = vshrq_n_s32(vaddq_s32(b_lo, round), K);
    b_hi = vshrq_n_s32(vaddq_s32(b_hi, round), K);

    // ---- narrow s32 -> s16 (SATURATING) -> u8 (SATURATING) ----
    // vqmovn_s32:  saturates s32 -> s16 into the signed range
    // vqmovun_s16: saturates s16 -> u8, clamping negatives to 0
    //              and values > 255 to 255.
    // This is the vector equivalent of calling saturate_to_u8()
    // on every lane, instead of the raw truncating cast.
    int16x8_t r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
    int16x8_t g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
    int16x8_t b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

    uint8x8_t r8 = vqmovun_s16(r16);
    uint8x8_t g8 = vqmovun_s16(g16);
    uint8x8_t b8 = vqmovun_s16(b16);

    vst1_u8(Rp, r8);
    vst1_u8(Gp, g8);
    vst1_u8(Bp, b8);
}

void CSC_YCC_to_RGB_neon( int height, int width)
{
    for (int row = 0; row < height; row++) {

        const uint8_t *Yp  = &Y[row][0];
        const uint8_t *Cbp = &Cb_temp[row][0];
        const uint8_t *Crp = &Cr_temp[row][0];
        uint8_t *Rp = &R[row][0];
        uint8_t *Gp = &G[row][0];
        uint8_t *Bp = &B[row][0];

        int col = 0;
        for (; col + 8 <= width; col += 8) {
            CSC_YCC_to_RGB_neon_8( Yp + col, Cbp + col, Crp + col,
                                   Rp + col, Gp + col, Bp + col);
        }

        // Scalar remainder path — now uses saturate_to_u8() so it
        // matches the NEON path's clamping behaviour exactly.
        for (; col < width; col++) {
            int y  = (int)Yp[col]  - 16;
            int cb = (int)Cbp[col] - 128;
            int cr = (int)Crp[col] - 128;

            int r = D1 * y + D2 * cr;
            r += (1 << (K - 1));
            r >>= K;

            int g = D1 * y - D3 * cr - D4 * cb;
            g += (1 << (K - 1));
            g >>= K;

            int b = D1 * y + D5 * cb;
            b += (1 << (K - 1));
            b >>= K;

            Rp[col] = saturate_to_u8(r);
            Gp[col] = saturate_to_u8(g);
            Bp[col] = saturate_to_u8(b);
        }
    }
}

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



// Processes 8 consecutive interior chroma columns for one row-pair
// (row, row+1) of ONE plane (call once for Cb, once for Cr).
// Writes 16 output bytes into each of two consecutive output rows.
static inline void chroma_upsample_neon_8( const uint8_t *src_row0,
                                            const uint8_t *src_row1,
                                            uint8_t *dst_row0,
                                            uint8_t *dst_row1)
{
    uint8x8_t c0 = vld1_u8(src_row0 + 0);   // C[r][c..c+7]
    uint8x8_t c1 = vld1_u8(src_row0 + 1);   // C[r][c+1..c+8]  (neighbor)
    uint8x8_t n0 = vld1_u8(src_row1 + 0);   // C[r+1][c..c+7]
    uint8x8_t n1 = vld1_u8(src_row1 + 1);   // C[r+1][c+1..c+8]

    // top  = (C00+C01+1)>>1 ; left = (C00+C10+1)>>1
    // vrhadd_u8 IS this rounding-halving-add, exactly, per lane.
    uint8x8_t top  = vrhadd_u8(c0, c1);
    uint8x8_t left = vrhadd_u8(c0, n0);

    // middle = (C00+C01+C10+C11+2)>>2 -- true 4-term rounding average.
    // NOT vrhadd(top,left)-of-vrhadd -- double rounding gives wrong
    // answers on some inputs. Widen instead, matching scalar exactly.
    uint16x8_t sum = vaddl_u8(c0, c1);
    sum = vaddq_u16(sum, vaddl_u8(n0, n1));
    sum = vaddq_u16(sum, vdupq_n_u16(2));      // rounding
    uint8x8_t middle = vmovn_u16(vshrq_n_u16(sum, 2));
    // max possible sum = 4*255+2 = 1022, >>2 = 255 -- fits u8 exactly,
    // no saturation needed here.

    // Interleaved stores match the scalar output layout directly:
    //   dst_row0: C00, top, C00, top, ...
    //   dst_row1: left, middle, left, middle, ...
    uint8x8x2_t out_top = { { c0,   top    } };
    uint8x8x2_t out_mid = { { left, middle } };
    vst2_u8(dst_row0, out_top);
    vst2_u8(dst_row1, out_mid);
}
static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Correct upsample for CHROMINANCE_DOWNSAMPLING_MODE == 2 (4-pixel average).
// Mirrors chrominance_downsample()'s plain unweighted-average style:
// every output pixel is a simple equal-weight average of its 4 nearest
// block-center samples. No output pixel is a verbatim copy, since an
// averaged sample doesn't correspond to any single real pixel location
// (unlike mode 1, which is co-sited with the top-left pixel).
static void chroma_plane_upsample_avg4(
        const uint8_t src[IMAGE_ROW_SIZE>>1][IMAGE_COL_SIZE>>1],
        uint8_t dst[IMAGE_ROW_SIZE][IMAGE_COL_SIZE])
{
    const int ch_rows = IMAGE_ROW_SIZE >> 1;
    const int ch_cols = IMAGE_COL_SIZE >> 1;

    for (int i = 0; i < ch_rows; i++) {
        int i_up   = clampi(i - 1, 0, ch_rows - 1); // edge: replicate
        int i_down = clampi(i + 1, 0, ch_rows - 1);

        for (int j = 0; j < ch_cols; j++) {
            int j_left  = clampi(j - 1, 0, ch_cols - 1);
            int j_right = clampi(j + 1, 0, ch_cols - 1);

            int c00 = src[i][j];
            int c_up    = src[i_up][j];
            int c_down  = src[i_down][j];
            int c_left  = src[i][j_left];
            int c_right = src[i][j_right];
            int c_ul = src[i_up][j_left];
            int c_ur = src[i_up][j_right];
            int c_dl = src[i_down][j_left];
            int c_dr = src[i_down][j_right];

            int out_r0 = i << 1;
            int out_c0 = j << 1;

            int tl = c00 + c_up   + c_left  + c_ul; tl += (1 << 1); tl >>= 2;
            int tr = c00 + c_up   + c_right + c_ur; tr += (1 << 1); tr >>= 2;
            int bl = c00 + c_down + c_left  + c_dl; bl += (1 << 1); bl >>= 2;
            int br = c00 + c_down + c_right + c_dr; br += (1 << 1); br >>= 2;

            dst[out_r0+0][out_c0+0] = (uint8_t)tl;
            dst[out_r0+0][out_c0+1] = (uint8_t)tr;
            dst[out_r0+1][out_c0+0] = (uint8_t)bl;
            dst[out_r0+1][out_c0+1] = (uint8_t)br;
        }
    }
}
// ---- Driver replacing chrominance_array_upsample() ----
static void chrominance_array_upsample_neon( void)
{
#if CHROMINANCE_DOWNSAMPLING_MODE == 1
    chroma_plane_upsample_neon( Cb, Cb_temp);
    chroma_plane_upsample_neon( Cr, Cr_temp);
#elif CHROMINANCE_DOWNSAMPLING_MODE == 2
    chroma_plane_upsample_avg4( Cb, Cb_temp);
    chroma_plane_upsample_avg4( Cr, Cr_temp);
#else
    #error "No matching NEON upsample implementation for this CHROMINANCE_DOWNSAMPLING_MODE"
#endif
}


// =======
void CSC_YCC_to_RGB( void) {

  // Cb/Cr only need to be upsampled once per frame -- all three routines
  // (float, brute-force int, optimized) read from Cb_temp/Cr_temp, so this
  // is hoisted out of every per-block routine and done exactly once here.

  
  if( YCC_to_RGB_ROUTINE == 4) {
    //printf("Using NEON-optimized YCC->RGB conversion\n");
    chrominance_array_upsample_neon();
    CSC_YCC_to_RGB_neon(IMAGE_ROW_SIZE, IMAGE_COL_SIZE);
    return;
  }
  int row, col; // indices for row and column
  chrominance_array_upsample();
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
        default:
          break;
      }
    }
  }

} // END of CSC_YCC_to_RGB()