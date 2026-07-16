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

//TODO verify

// D1..D5 and K are assumed to be the same compile-time constants used by
// the scalar version. K MUST be a compile-time immediate (1..32) because
// vshrq_n_s32 requires an immediate shift amount.

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

    // ---- narrow s32 -> s16 -> u8, TRUNCATING (no saturation) ----
    // This matches the original `(uint8_t)int_value` cast exactly:
    // it just keeps the low 8 bits, wrapping on overflow instead of
    // clamping to 0..255.
    int16x8_t r16 = vcombine_s16(vmovn_s32(r_lo), vmovn_s32(r_hi));
    int16x8_t g16 = vcombine_s16(vmovn_s32(g_lo), vmovn_s32(g_hi));
    int16x8_t b16 = vcombine_s16(vmovn_s32(b_lo), vmovn_s32(b_hi));

    uint8x8_t r8 = vmovn_u16(vreinterpretq_u16_s16(r16));
    uint8x8_t g8 = vmovn_u16(vreinterpretq_u16_s16(g16));
    uint8x8_t b8 = vmovn_u16(vreinterpretq_u16_s16(b16));

    vst1_u8(Rp, r8);
    vst1_u8(Gp, g8);
    vst1_u8(Bp, b8);
}

// ---- Driver: replaces the 2x2 grouped scalar loop with a per-row,
// 8-pixels-at-a-time NEON loop over the whole image (or a region).
// width must be handled for the case where it's not a multiple of 8;
// the remainder falls back to the scalar-equivalent computation.
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

        // scalar remainder (identical arithmetic/truncation behaviour
        // to the original code, just per-pixel instead of per-2x2-block)
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

            Rp[col] = (uint8_t)r;
            Gp[col] = (uint8_t)g;
            Bp[col] = (uint8_t)b;
        }
    }
}

// private definitions
// =======
#if CSC_ENABLE_YCC_TO_RGB_OPTIMIZED && CSC_ENABLE_YCC_TO_RGB_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))

// Processes one full contiguous sub-row of `count` pixels directly out of
// the image arrays. Real contiguous loads (no gather through a temp array),
// so this amortizes NEON setup/teardown cost over a whole row instead of a
// single 2x2 block.
static void CSC_YCC_to_RGB_neon_subrow(
    const uint8_t *y_row,
    const uint8_t *cb_row,
    const uint8_t *cr_row,
    uint8_t *r_row,
    uint8_t *g_row,
    uint8_t *b_row,
    int count)
{
    const int32x4_t round   = vdupq_n_s32(1 << (CSC_FIXED_POINT_SHIFT - 1));
    const int32x4_t biasY   = vdupq_n_s32(16);
    const int32x4_t biasC   = vdupq_n_s32(128);

    int i;

    for (i = 0; i + 8 <= count; i += 8)
    {
        uint8x8_t y8  = vld1_u8(y_row  + i);
        uint8x8_t cb8 = vld1_u8(cb_row + i);
        uint8x8_t cr8 = vld1_u8(cr_row + i);

        int16x8_t y16  = vreinterpretq_s16_u16(vmovl_u8(y8));
        int16x8_t cb16 = vreinterpretq_s16_u16(vmovl_u8(cb8));
        int16x8_t cr16 = vreinterpretq_s16_u16(vmovl_u8(cr8));

        uint16x4_t r16_half[2];
        uint16x4_t g16_half[2];
        uint16x4_t b16_half[2];

        for (int half = 0; half < 2; half++)
        {
            int32x4_t y =
                vmovl_s16(half ? vget_high_s16(y16) : vget_low_s16(y16));
            int32x4_t cb =
                vmovl_s16(half ? vget_high_s16(cb16) : vget_low_s16(cb16));
            int32x4_t cr =
                vmovl_s16(half ? vget_high_s16(cr16) : vget_low_s16(cr16));

            y  = vsubq_s32(y, biasY);
            cb = vsubq_s32(cb, biasC);
            cr = vsubq_s32(cr, biasC);

            //-----------------------------------------
            // Compute Y contribution ONCE
            //-----------------------------------------

            int32x4_t yscaled = vmulq_n_s32(y, D1);

            //-----------------------------------------
            // R
            //-----------------------------------------

            int32x4_t r =
                vaddq_s32(yscaled, vmulq_n_s32(cr, D2));

            r = vshrq_n_s32(vaddq_s32(r, round),
                            CSC_FIXED_POINT_SHIFT);

            //-----------------------------------------
            // G
            //-----------------------------------------

            int32x4_t g = yscaled;

            g = vmlaq_n_s32(g, cr, -D3);
            g = vmlaq_n_s32(g, cb, -D4);

            g = vshrq_n_s32(vaddq_s32(g, round),
                            CSC_FIXED_POINT_SHIFT);

            //-----------------------------------------
            // B
            //-----------------------------------------

            int32x4_t b =
                vaddq_s32(yscaled, vmulq_n_s32(cb, D5));

            b = vshrq_n_s32(vaddq_s32(b, round),
                            CSC_FIXED_POINT_SHIFT);

            //-----------------------------------------
            // Saturating narrowing
            //-----------------------------------------

            r16_half[half] = vqmovun_s32(r);
            g16_half[half] = vqmovun_s32(g);
            b16_half[half] = vqmovun_s32(b);
        }

        //---------------------------------------------
        // Pack 8 pixels
        //---------------------------------------------

        uint8x8_t r8out =
            vqmovn_u16(vcombine_u16(r16_half[0], r16_half[1]));

        uint8x8_t g8out =
            vqmovn_u16(vcombine_u16(g16_half[0], g16_half[1]));

        uint8x8_t b8out =
            vqmovn_u16(vcombine_u16(b16_half[0], b16_half[1]));

        vst1_u8(r_row + i, r8out);
        vst1_u8(g_row + i, g8out);
        vst1_u8(b_row + i, b8out);
    }

    //-----------------------------------------
    // Scalar cleanup
    //-----------------------------------------

    for (; i < count; i++)
    {
        int y  = y_row[i]  - 16;
        int cb = cb_row[i] - 128;
        int cr = cr_row[i] - 128;

        int r = (D1*y + D2*cr + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
        int g = (D1*y - D3*cr - D4*cb + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;
        int b = (D1*y + D5*cb + CSC_ROUNDING) >> CSC_FIXED_POINT_SHIFT;

        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;

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

  if( r00 < 0) r00 = 0; else if( r00 > 255) r00 = 255;
  if( r01 < 0) r01 = 0; else if( r01 > 255) r01 = 255;
  if( r10 < 0) r10 = 0; else if( r10 > 255) r10 = 255;
  if( r11 < 0) r11 = 0; else if( r11 > 255) r11 = 255;

  if( g00 < 0) g00 = 0; else if( g00 > 255) g00 = 255;
  if( g01 < 0) g01 = 0; else if( g01 > 255) g01 = 255;
  if( g10 < 0) g10 = 0; else if( g10 > 255) g10 = 255;
  if( g11 < 0) g11 = 0; else if( g11 > 255) g11 = 255;

  if( b00 < 0) b00 = 0; else if( b00 > 255) b00 = 255;
  if( b01 < 0) b01 = 0; else if( b01 > 255) b01 = 255;
  if( b10 < 0) b10 = 0; else if( b10 > 255) b10 = 255;
  if( b11 < 0) b11 = 0; else if( b11 > 255) b11 = 255;

  R[row+0][col+0] = (uint8_t)r00;
  R[row+0][col+1] = (uint8_t)r01;
  R[row+1][col+0] = (uint8_t)r10;
  R[row+1][col+1] = (uint8_t)r11;

  G[row+0][col+0] = (uint8_t)g00;
  G[row+0][col+1] = (uint8_t)g01;
  G[row+1][col+0] = (uint8_t)g10;
  G[row+1][col+1] = (uint8_t)g11;

  B[row+0][col+0] = (uint8_t)b00;
  B[row+0][col+1] = (uint8_t)b01;
  B[row+1][col+0] = (uint8_t)b10;
  B[row+1][col+1] = (uint8_t)b11;
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
  printf("NEON path is enabled for YCC to RGB conversion.\n");  
    // NEON path processes a full row-pair per iteration rather than
    // dispatching per 2x2 block.
    for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
      CSC_YCC_to_RGB_optimized_row_neon( row);
    }
    return;
  }
#endif
  if( YCC_to_RGB_ROUTINE == 4) {
    CSC_YCC_to_RGB_neon(IMAGE_ROW_SIZE, IMAGE_COL_SIZE);
    return;
  }
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