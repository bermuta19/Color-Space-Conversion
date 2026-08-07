// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// YCC to RGB conversion

#include <stdio.h>
#include <stdint.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
#include "CSC_global.h"

// private prototypes
static uint8_t saturate_to_u8( int value);
static uint8_t saturation_float( float argument);
static void CSC_YCC_to_RGB_brute_force_float( int row, int col);
static void CSC_YCC_to_RGB_brute_force_int( int row, int col);
static void chrominance_upsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4,
    uint8_t *top, uint8_t *left, uint8_t *middle);
static void chrominance_array_upsample( void);
static void chroma_upsample_neon_8( const uint8_t *Cp0, const uint8_t *Cp1,
                                    uint8_t *Cp_out0, uint8_t *Cp_out1);

// ============================================================
// Fused core: takes already-loaded 8-lane Y/Cb/Cr vectors, returns R/G/B
// vectors. No loads, no stores -- pure register-to-register.
// D1*Y is computed once and reused for R, G, and B (was 3x redundant
// in the original scalar-style port). vrshrq_n_s32 folds "+round; >>K"
// into a single rounding-shift instruction. vmlaq_n_s32/vmlsq_n_s32
// fold the *D2..*D5 multiply directly, no dup'd constant register needed.
// ============================================================
static inline void color_matrix_neon_8( uint8x8_t y8, uint8x8_t cb8, uint8x8_t cr8,
                                         uint8x8_t *r8, uint8x8_t *g8, uint8x8_t *b8)
{
    int16x8_t y16  = vreinterpretq_s16_u16(vmovl_u8(y8));
    int16x8_t cb16 = vreinterpretq_s16_u16(vmovl_u8(cb8));
    int16x8_t cr16 = vreinterpretq_s16_u16(vmovl_u8(cr8));

    y16  = vsubq_s16(y16,  vdupq_n_s16(16));
    cb16 = vsubq_s16(cb16, vdupq_n_s16(128));
    cr16 = vsubq_s16(cr16, vdupq_n_s16(128));

    int32x4_t y_lo  = vmovl_s16(vget_low_s16(y16));
    int32x4_t y_hi  = vmovl_s16(vget_high_s16(y16));
    int32x4_t cb_lo = vmovl_s16(vget_low_s16(cb16));
    int32x4_t cb_hi = vmovl_s16(vget_high_s16(cb16));
    int32x4_t cr_lo = vmovl_s16(vget_low_s16(cr16));
    int32x4_t cr_hi = vmovl_s16(vget_high_s16(cr16));

    int32x4_t dy_lo = vmulq_n_s32(y_lo, D1);
    int32x4_t dy_hi = vmulq_n_s32(y_hi, D1);

    int32x4_t r_lo = vrshrq_n_s32(vmlaq_n_s32(dy_lo, cr_lo, D2), K);
    int32x4_t r_hi = vrshrq_n_s32(vmlaq_n_s32(dy_hi, cr_hi, D2), K);

    int32x4_t g_lo = vrshrq_n_s32(vmlsq_n_s32(vmlsq_n_s32(dy_lo, cr_lo, D3), cb_lo, D4), K);
    int32x4_t g_hi = vrshrq_n_s32(vmlsq_n_s32(vmlsq_n_s32(dy_hi, cr_hi, D3), cb_hi, D4), K);

    int32x4_t b_lo = vrshrq_n_s32(vmlaq_n_s32(dy_lo, cb_lo, D5), K);
    int32x4_t b_hi = vrshrq_n_s32(vmlaq_n_s32(dy_hi, cb_hi, D5), K);

    int16x8_t r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
    int16x8_t g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
    int16x8_t b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

    *r8 = vqmovun_s16(r16);
    *g8 = vqmovun_s16(g16);
    *b8 = vqmovun_s16(b16);
}

// Pointer-based wrapper around color_matrix_neon_8 -- loads 8 pixels,
// runs the color matrix, stores 8 pixels. Used by both the fused
// pipeline below and the older bulk-row CSC_YCC_to_RGB_neon().
static inline void CSC_YCC_to_RGB_neon_8( const uint8_t *Yp,
                                           const uint8_t *Cbp,
                                           const uint8_t *Crp,
                                           uint8_t *Rp,
                                           uint8_t *Gp,
                                           uint8_t *Bp)
{
    uint8x8_t r, g, b;
    color_matrix_neon_8( vld1_u8(Yp), vld1_u8(Cbp), vld1_u8(Crp), &r, &g, &b);
    vst1_u8(Rp, r);
    vst1_u8(Gp, g);
    vst1_u8(Bp, b);
}

// ============================================================
// Chroma upsample: box-average box filter (unchanged math), 8 chroma
// columns at a time. Low register pressure by design -- only the 4
// loaded vectors plus top/left/middle intermediates are live at once,
// nothing close to pressuring the 32 D-register file on armv7.
// ============================================================
static inline void chroma_upsample_neon_8( const uint8_t *src_row0,
                                            const uint8_t *src_row1,
                                            uint8_t *dst_row0,
                                            uint8_t *dst_row1)
{
    uint8x8_t c0 = vld1_u8(src_row0 + 0);
    uint8x8_t c1 = vld1_u8(src_row0 + 1);
    uint8x8_t n0 = vld1_u8(src_row1 + 0);
    uint8x8_t n1 = vld1_u8(src_row1 + 1);

    uint8x8_t top  = vrhadd_u8(c0, c1);
    uint8x8_t left = vrhadd_u8(c0, n0);

    uint16x8_t sum = vaddl_u8(c0, c1);
    sum = vaddq_u16(sum, vaddl_u8(n0, n1));
    sum = vaddq_u16(sum, vdupq_n_u16(2));
    uint8x8_t middle = vmovn_u16(vshrq_n_u16(sum, 2));

    uint8x8x2_t out_top = { { c0,   top    } };
    uint8x8x2_t out_mid = { { left, middle } };
    vst2_u8(dst_row0, out_top);
    vst2_u8(dst_row1, out_mid);
}

// ============================================================
// Fused pipeline: chroma-upsample into small per-row-pair strips, then
// immediately consume via CSC_YCC_to_RGB_neon_8 while hot in L1.
// This is the function selected by YCC_to_RGB_ROUTINE == 4.
// ============================================================
static void CSC_YCC_to_RGB_neon_fused( void)
{
    const int ch_rows = IMAGE_ROW_SIZE >> 1;
    const int ch_cols = IMAGE_COL_SIZE >> 1;

    uint8_t cb_strip0[IMAGE_COL_SIZE];
    uint8_t cb_strip1[IMAGE_COL_SIZE];
    uint8_t cr_strip0[IMAGE_COL_SIZE];
    uint8_t cr_strip1[IMAGE_COL_SIZE];

    for (int crow = 0; crow < ch_rows; crow++) {
        int row  = crow << 1;
        int next = (crow + 1 < ch_rows) ? (crow + 1) : crow;

        const uint8_t *cb_row0 = &Cb[crow][0];
        const uint8_t *cb_row1 = &Cb[next][0];
        const uint8_t *cr_row0 = &Cr[crow][0];
        const uint8_t *cr_row1 = &Cr[next][0];

        int col = 0;

        // ---- Stage 1: vectorized chroma upsample into strips ----
        for (; col + 8 <= ch_cols - 1; col += 8) {
            int oc = col << 1;
            chroma_upsample_neon_8( cb_row0 + col, cb_row1 + col,
                                    cb_strip0 + oc, cb_strip1 + oc);
            chroma_upsample_neon_8( cr_row0 + col, cr_row1 + col,
                                    cr_strip0 + oc, cr_strip1 + oc);
        }

        // scalar remainder: interior columns left over (< 8 of them)
        for (; col < ch_cols - 1; col++) {
            int cb00 = cb_row0[col], cb01 = cb_row0[col + 1];
            int cb10 = cb_row1[col], cb11 = cb_row1[col + 1];
            int cr00 = cr_row0[col], cr01 = cr_row0[col + 1];
            int cr10 = cr_row1[col], cr11 = cr_row1[col + 1];

            int cb_top    = (cb00 + cb01 + 1) >> 1;
            int cb_left   = (cb00 + cb10 + 1) >> 1;
            int cb_middle = (cb00 + cb01 + cb10 + cb11 + 2) >> 2;
            int cr_top    = (cr00 + cr01 + 1) >> 1;
            int cr_left   = (cr00 + cr10 + 1) >> 1;
            int cr_middle = (cr00 + cr01 + cr10 + cr11 + 2) >> 2;

            int oc = col << 1;
            cb_strip0[oc + 0] = (uint8_t)cb00;    cb_strip0[oc + 1] = (uint8_t)cb_top;
            cb_strip1[oc + 0] = (uint8_t)cb_left; cb_strip1[oc + 1] = (uint8_t)cb_middle;
            cr_strip0[oc + 0] = (uint8_t)cr00;    cr_strip0[oc + 1] = (uint8_t)cr_top;
            cr_strip1[oc + 0] = (uint8_t)cr_left; cr_strip1[oc + 1] = (uint8_t)cr_middle;
        }

        // last chroma column: replicate horizontally
        {
            int lc = ch_cols - 1;
            int oc = lc << 1;
            int cb00 = cb_row0[lc], cb10 = cb_row1[lc];
            int cr00 = cr_row0[lc], cr10 = cr_row1[lc];
            int cb_left = (cb00 + cb10 + 1) >> 1;
            int cr_left = (cr00 + cr10 + 1) >> 1;

            cb_strip0[oc + 0] = (uint8_t)cb00;    cb_strip0[oc + 1] = (uint8_t)cb00;
            cb_strip1[oc + 0] = (uint8_t)cb_left; cb_strip1[oc + 1] = (uint8_t)cb_left;
            cr_strip0[oc + 0] = (uint8_t)cr00;    cr_strip0[oc + 1] = (uint8_t)cr00;
            cr_strip1[oc + 0] = (uint8_t)cr_left; cr_strip1[oc + 1] = (uint8_t)cr_left;
        }

        // ---- Stage 2: consume the strip immediately while hot in L1 ----
        const uint8_t *Yp0 = &Y[row][0];
        const uint8_t *Yp1 = &Y[row + 1][0];
        uint8_t *Rp0 = &R[row][0], *Gp0 = &G[row][0], *Bp0 = &B[row][0];
        uint8_t *Rp1 = &R[row + 1][0], *Gp1 = &G[row + 1][0], *Bp1 = &B[row + 1][0];

        int c = 0;
        for (; c + 8 <= IMAGE_COL_SIZE; c += 8) {
            CSC_YCC_to_RGB_neon_8( Yp0 + c, cb_strip0 + c, cr_strip0 + c, Rp0 + c, Gp0 + c, Bp0 + c);
            CSC_YCC_to_RGB_neon_8( Yp1 + c, cb_strip1 + c, cr_strip1 + c, Rp1 + c, Gp1 + c, Bp1 + c);
        }
        for (; c < IMAGE_COL_SIZE; c++) {
            int y0 = (int)Yp0[c] - 16,  y1 = (int)Yp1[c] - 16;
            int cb0 = (int)cb_strip0[c] - 128, cb1 = (int)cb_strip1[c] - 128;
            int cr0 = (int)cr_strip0[c] - 128, cr1 = (int)cr_strip1[c] - 128;

            int r0 = D1*y0 + D2*cr0;              r0 += (1 << (K-1)); r0 >>= K;
            int g0 = D1*y0 - D3*cr0 - D4*cb0;      g0 += (1 << (K-1)); g0 >>= K;
            int b0 = D1*y0 + D5*cb0;               b0 += (1 << (K-1)); b0 >>= K;
            Rp0[c] = saturate_to_u8(r0); Gp0[c] = saturate_to_u8(g0); Bp0[c] = saturate_to_u8(b0);

            int r1 = D1*y1 + D2*cr1;              r1 += (1 << (K-1)); r1 >>= K;
            int g1 = D1*y1 - D3*cr1 - D4*cb1;      g1 += (1 << (K-1)); g1 >>= K;
            int b1 = D1*y1 + D5*cb1;               b1 += (1 << (K-1)); b1 >>= K;
            Rp1[c] = saturate_to_u8(r1); Gp1[c] = saturate_to_u8(g1); Bp1[c] = saturate_to_u8(b1);
        }
    }
}

// Bulk-row NEON routine kept for compatibility with any other caller
// that still uses it directly (reads from fully-materialized
// Cb_temp/Cr_temp rather than the fused per-row-pair strips).
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
        for (; col < width; col++) {
            int y  = (int)Yp[col]  - 16;
            int cb = (int)Cbp[col] - 128;
            int cr = (int)Crp[col] - 128;

            int r = D1 * y + D2 * cr;
            r += (1 << (K - 1)); r >>= K;
            int g = D1 * y - D3 * cr - D4 * cb;
            g += (1 << (K - 1)); g >>= K;
            int b = D1 * y + D5 * cb;
            b += (1 << (K - 1)); b >>= K;

            Rp[col] = saturate_to_u8(r);
            Gp[col] = saturate_to_u8(g);
            Bp[col] = saturate_to_u8(b);
        }
    }
}

// =======
static uint8_t saturate_to_u8( int value) {
  if( value < 0)   return 0;
  if( value > 255) return 255;
  return (uint8_t)value;
}

static uint8_t saturation_float( float argument) {
  if( argument > 255.0)      return( (uint8_t)255);
  else if( argument < 0.0)   return( (uint8_t)0);
  else                       return( (uint8_t)argument);
} // END of saturation_float()

// =======
static void CSC_YCC_to_RGB_brute_force_float( int row, int col) {
  float R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  float G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  float B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;

  R_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0) + 1.596*(Cr_temp[row+0][col+0] - 128.0);
  R[row+0][col+0] = saturation_float( R_pixel_00);
  R_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0) + 1.596*(Cr_temp[row+0][col+1] - 128.0);
  R[row+0][col+1] = saturation_float( R_pixel_01);
  R_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0) + 1.596*(Cr_temp[row+1][col+0] - 128.0);
  R[row+1][col+0] = saturation_float( R_pixel_10);
  R_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0) + 1.596*(Cr_temp[row+1][col+1] - 128.0);
  R[row+1][col+1] = saturation_float( R_pixel_11);

  G_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0) - 0.813*(Cr_temp[row+0][col+0] - 128.0) - 0.391*(Cb_temp[row+0][col+0] - 128.0);
  G[row+0][col+0] = saturation_float( G_pixel_00);
  G_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0) - 0.813*(Cr_temp[row+0][col+1] - 128.0) - 0.391*(Cb_temp[row+0][col+1] - 128.0);
  G[row+0][col+1] = saturation_float( G_pixel_01);
  G_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0) - 0.813*(Cr_temp[row+1][col+0] - 128.0) - 0.391*(Cb_temp[row+1][col+0] - 128.0);
  G[row+1][col+0] = saturation_float( G_pixel_10);
  G_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0) - 0.813*(Cr_temp[row+1][col+1] - 128.0) - 0.391*(Cb_temp[row+1][col+1] - 128.0);
  G[row+1][col+1] = saturation_float( G_pixel_11);

  B_pixel_00 =   1.164*(Y[row+0][col+0] - 16.0) + 2.018*(Cb_temp[row+0][col+0] - 128.0);
  B[row+0][col+0] = saturation_float( B_pixel_00);
  B_pixel_01 =   1.164*(Y[row+0][col+1] - 16.0) + 2.018*(Cb_temp[row+0][col+1] - 128.0);
  B[row+0][col+1] = saturation_float( B_pixel_01);
  B_pixel_10 =   1.164*(Y[row+1][col+0] - 16.0) + 2.018*(Cb_temp[row+1][col+0] - 128.0);
  B[row+1][col+0] = saturation_float( B_pixel_10);
  B_pixel_11 =   1.164*(Y[row+1][col+1] - 16.0) + 2.018*(Cb_temp[row+1][col+1] - 128.0);
  B[row+1][col+1] = saturation_float( B_pixel_11);
} // END of CSC_YCC_to_RGB_brute_force_float()

// =======
static void CSC_YCC_to_RGB_brute_force_int( int row, int col) {
  int R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  int G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  int B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;
  int  Y_pixel_00,  Y_pixel_01,  Y_pixel_10,  Y_pixel_11;
  int Cb_pixel_00, Cb_pixel_01, Cb_pixel_10, Cb_pixel_11;
  int Cr_pixel_00, Cr_pixel_01, Cr_pixel_10, Cr_pixel_11;

  Y_pixel_00 = (int)Y[row+0][col+0]; Y_pixel_01 = (int)Y[row+0][col+1];
  Y_pixel_10 = (int)Y[row+1][col+0]; Y_pixel_11 = (int)Y[row+1][col+1];

  Cb_pixel_00 = (int)Cb_temp[row+0][col+0]; Cb_pixel_01 = (int)Cb_temp[row+0][col+1];
  Cb_pixel_10 = (int)Cb_temp[row+1][col+0]; Cb_pixel_11 = (int)Cb_temp[row+1][col+1];

  Cr_pixel_00 = (int)Cr_temp[row+0][col+0]; Cr_pixel_01 = (int)Cr_temp[row+0][col+1];
  Cr_pixel_10 = (int)Cr_temp[row+1][col+0]; Cr_pixel_11 = (int)Cr_temp[row+1][col+1];

  Y_pixel_00 -= 16; Y_pixel_01 -= 16; Y_pixel_10 -= 16; Y_pixel_11 -= 16;
  Cb_pixel_00 -= 128; Cb_pixel_01 -= 128; Cb_pixel_10 -= 128; Cb_pixel_11 -= 128;
  Cr_pixel_00 -= 128; Cr_pixel_01 -= 128; Cr_pixel_10 -= 128; Cr_pixel_11 -= 128;

  R_pixel_00 = D1*Y_pixel_00 + D2*Cr_pixel_00; R_pixel_00 += (1<<(K-1)); R_pixel_00 >>= K;
  R_pixel_01 = D1*Y_pixel_01 + D2*Cr_pixel_01; R_pixel_01 += (1<<(K-1)); R_pixel_01 >>= K;
  R_pixel_10 = D1*Y_pixel_10 + D2*Cr_pixel_10; R_pixel_10 += (1<<(K-1)); R_pixel_10 >>= K;
  R_pixel_11 = D1*Y_pixel_11 + D2*Cr_pixel_11; R_pixel_11 += (1<<(K-1)); R_pixel_11 >>= K;
  R[row+0][col+0] = (uint8_t)R_pixel_00; R[row+0][col+1] = (uint8_t)R_pixel_01;
  R[row+1][col+0] = (uint8_t)R_pixel_10; R[row+1][col+1] = (uint8_t)R_pixel_11;

  G_pixel_00 = D1*Y_pixel_00 - D3*Cr_pixel_00 - D4*Cb_pixel_00; G_pixel_00 += (1<<(K-1)); G_pixel_00 >>= K;
  G_pixel_01 = D1*Y_pixel_01 - D3*Cr_pixel_01 - D4*Cb_pixel_01; G_pixel_01 += (1<<(K-1)); G_pixel_01 >>= K;
  G_pixel_10 = D1*Y_pixel_10 - D3*Cr_pixel_10 - D4*Cb_pixel_10; G_pixel_10 += (1<<(K-1)); G_pixel_10 >>= K;
  G_pixel_11 = D1*Y_pixel_11 - D3*Cr_pixel_11 - D4*Cb_pixel_11; G_pixel_11 += (1<<(K-1)); G_pixel_11 >>= K;
  G[row+0][col+0] = (uint8_t)G_pixel_00; G[row+0][col+1] = (uint8_t)G_pixel_01;
  G[row+1][col+0] = (uint8_t)G_pixel_10; G[row+1][col+1] = (uint8_t)G_pixel_11;

  B_pixel_00 = D1*Y_pixel_00 + D5*Cb_pixel_00; B_pixel_00 += (1<<(K-1)); B_pixel_00 >>= K;
  B_pixel_01 = D1*Y_pixel_01 + D5*Cb_pixel_01; B_pixel_01 += (1<<(K-1)); B_pixel_01 >>= K;
  B_pixel_10 = D1*Y_pixel_10 + D5*Cb_pixel_10; B_pixel_10 += (1<<(K-1)); B_pixel_10 >>= K;
  B_pixel_11 = D1*Y_pixel_11 + D5*Cb_pixel_11; B_pixel_11 += (1<<(K-1)); B_pixel_11 >>= K;
  B[row+0][col+0] = (uint8_t)B_pixel_00; B[row+0][col+1] = (uint8_t)B_pixel_01;
  B[row+1][col+0] = (uint8_t)B_pixel_10; B[row+1][col+1] = (uint8_t)B_pixel_11;
} // END of CSC_YCC_to_RGB_brute_force_int()

// =======
static void chrominance_upsample(
    uint8_t C_pixel_00, uint8_t C_pixel_01,
    uint8_t C_pixel_10, uint8_t C_pixel_11,
    uint8_t *top, uint8_t *left, uint8_t *middle) {

  int temp_top, temp_left, temp_middle;

  switch (CHROMINANCE_UPSAMPLING_MODE) {
    case 0:
      *top = 0; *left = 0; *middle = 0;
      break;
    case 1:
      *top = (uint8_t)C_pixel_00; *left = (uint8_t)C_pixel_00; *middle = (uint8_t)C_pixel_00;
      break;
    case 2:
      temp_top = (int)C_pixel_00 + (int)C_pixel_01; temp_top += 1;
      *top = (uint8_t)(temp_top >> 1);
      temp_left = (int)C_pixel_00 + (int)C_pixel_10; temp_left += 1;
      *left = (uint8_t)(temp_left >> 1);
      temp_middle = (int)C_pixel_00 + (int)C_pixel_01 + (int)C_pixel_10 + (int)C_pixel_11;
      temp_middle += 2;
      *middle = (uint8_t)(temp_middle >> 2);
      break;
    default:
      break;
  }
} // END of chrominance_upsample()

// =======
static void chrominance_array_upsample( void) {
  int row, col;
  uint8_t top, left, middle;

  for( row=0; row<((IMAGE_ROW_SIZE>>1)-1); row+=1) {
    for( col=0; col<((IMAGE_COL_SIZE>>1)-1); col+=1) {
      chrominance_upsample( Cb[row+0][col+0], Cb[row+0][col+1], Cb[row+1][col+0], Cb[row+1][col+1], &top, &left, &middle);
      Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row+0][col+0];
      Cb_temp[(row<<1)+0][(col<<1)+1] = top;
      Cb_temp[(row<<1)+1][(col<<1)+0] = left;
      Cb_temp[(row<<1)+1][(col<<1)+1] = middle;

      chrominance_upsample( Cr[row+0][col+0], Cr[row+0][col+1], Cr[row+1][col+0], Cr[row+1][col+1], &top, &left, &middle);
      Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row+0][col+0];
      Cr_temp[(row<<1)+0][(col<<1)+1] = top;
      Cr_temp[(row<<1)+1][(col<<1)+0] = left;
      Cr_temp[(row<<1)+1][(col<<1)+1] = middle;
    }
  }

  col = (IMAGE_COL_SIZE>>1) - 1;
  for( row=0; row<((IMAGE_ROW_SIZE>>1)-1); row+=1) {
    chrominance_upsample( Cb[row+0][col], Cb[row+0][col], Cb[row+1][col], Cb[row+1][col], &top, &left, &middle);
    Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row+0][col];
    Cb_temp[(row<<1)+0][(col<<1)+1] = top;
    Cb_temp[(row<<1)+1][(col<<1)+0] = left;
    Cb_temp[(row<<1)+1][(col<<1)+1] = middle;

    chrominance_upsample( Cr[row+0][col], Cr[row+0][col], Cr[row+1][col], Cr[row+1][col], &top, &left, &middle);
    Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row+0][col];
    Cr_temp[(row<<1)+0][(col<<1)+1] = top;
    Cr_temp[(row<<1)+1][(col<<1)+0] = left;
    Cr_temp[(row<<1)+1][(col<<1)+1] = middle;
  }

  row = (IMAGE_ROW_SIZE>>1) - 1;
  for( col=0; col<((IMAGE_COL_SIZE>>1)-1); col+=1) {
    chrominance_upsample( Cb[row][col+0], Cb[row][col+1], Cb[row][col+0], Cb[row][col+1], &top, &left, &middle);
    Cb_temp[(row<<1)+0][(col<<1)+0] = Cb[row][col+0];
    Cb_temp[(row<<1)+0][(col<<1)+1] = top;
    Cb_temp[(row<<1)+1][(col<<1)+0] = left;
    Cb_temp[(row<<1)+1][(col<<1)+1] = middle;

    chrominance_upsample( Cr[row][col+0], Cr[row][col+1], Cr[row][col+0], Cr[row][col+1], &top, &left, &middle);
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
  Cr_temp[(row<<1)+0][(col<<1)+0] = Cr[row][col];
  Cr_temp[(row<<1)+0][(col<<1)+1] = Cr[row][col];
  Cr_temp[(row<<1)+1][(col<<1)+0] = Cr[row][col];
  Cr_temp[(row<<1)+1][(col<<1)+1] = Cr[row][col];
} // END of chrominance_array_upsample()

// =======
void CSC_YCC_to_RGB( void) {
  if( YCC_to_RGB_ROUTINE == 4) {
    CSC_YCC_to_RGB_neon_fused();
    return;
  }
  int row, col;
  chrominance_array_upsample();
  for( row=0; row<IMAGE_ROW_SIZE; row+=2) {
    for( col=0; col<IMAGE_COL_SIZE; col+=2) {
      switch (YCC_to_RGB_ROUTINE) {
        case 0: break;
        case 1: CSC_YCC_to_RGB_brute_force_float( row, col); break;
        case 2: CSC_YCC_to_RGB_brute_force_int( row, col); break;
        default: break;
      }
    }
  }
} // END of CSC_YCC_to_RGB()