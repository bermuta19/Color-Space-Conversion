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

static void chroma_upsample_neon_8( const uint8_t *Cp0, const uint8_t *Cp1,
                                    uint8_t *Cp_out0, uint8_t *Cp_out1);

// Fused core: takes already-loaded 8-lane Y/Cb/Cr vectors, returns R/G/B
// vectors. No loads, no stores -- pure register-to-register, meant to be
// inlined directly into the fused pipeline below.
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

    // D1*Y computed once, reused for R, G, and B -- was 3x redundant before.
    int32x4_t dy_lo = vmulq_n_s32(y_lo, D1);
    int32x4_t dy_hi = vmulq_n_s32(y_hi, D1);

    // R = dy + D2*Cr  -- vmlaq_n_s32 folds the *D2 directly, no dup'd
    // constant register needed. vrshrq_n_s32 is a single rounding
    // shift-right instruction, replacing "+round; >>K".
    int32x4_t r_lo = vrshrq_n_s32(vmlaq_n_s32(dy_lo, cr_lo, D2), K);
    int32x4_t r_hi = vrshrq_n_s32(vmlaq_n_s32(dy_hi, cr_hi, D2), K);

    // G = dy - D3*Cr - D4*Cb
    int32x4_t g_lo = vrshrq_n_s32(vmlsq_n_s32(vmlsq_n_s32(dy_lo, cr_lo, D3), cb_lo, D4), K);
    int32x4_t g_hi = vrshrq_n_s32(vmlsq_n_s32(vmlsq_n_s32(dy_hi, cr_hi, D3), cb_hi, D4), K);

    // B = dy + D5*Cb
    int32x4_t b_lo = vrshrq_n_s32(vmlaq_n_s32(dy_lo, cb_lo, D5), K);
    int32x4_t b_hi = vrshrq_n_s32(vmlaq_n_s32(dy_hi, cb_hi, D5), K);

    int16x8_t r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
    int16x8_t g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
    int16x8_t b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

    *r8 = vqmovun_s16(r16);
    *g8 = vqmovun_s16(g16);
    *b8 = vqmovun_s16(b16);
}

// Thin pointer-based wrapper, kept so any other caller (e.g. the older
// CSC_YCC_to_RGB_neon() bulk-row routine, if you still build it) keeps
// working unchanged. Not used by the fused pipeline below.
static inline void CSC_YCC_to_RGB_neon_8( const uint8_t *Yp,
                                           const uint8_t *Cbp,
                                           const uint8_t *Crp,
                                           uint8_t *Rp,
                                           uint8_t *Gp,
                                           uint8_t *Bp)
{
    uint8_t r8[8], g8[8], b8[8];
    uint8x8_t r, g, b;
    color_matrix_neon_8( vld1_u8(Yp), vld1_u8(Cbp), vld1_u8(Crp), &r, &g, &b);
    vst1_u8(Rp, r); vst1_u8(Gp, g); vst1_u8(Bp, b);
    (void)r8; (void)g8; (void)b8;  // (r8/g8/b8 unused -- vst1 writes directly)
}

// Same math as chroma_upsample_neon_8, but takes the "row0" vectors
// (c0, c1) already loaded rather than reloading them, and hands back
// the "row1" vectors it just loaded so the *next* row-pair iteration
// can reuse them as its own row0 instead of reloading that row.
static inline void chroma_upsample_neon_8_cached(
    uint8x8_t c0, uint8x8_t c1,
    const uint8_t *src_row1,
    uint8_t *dst_row0, uint8_t *dst_row1,
    uint8x8_t *c0_next, uint8x8_t *c1_next)
{
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

    *c0_next = n0;
    *c1_next = n1;
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



static void CSC_YCC_to_RGB_brute_force_int( int row, int col) {
//
  int R_pixel_00, R_pixel_01, R_pixel_10, R_pixel_11;
  int G_pixel_00, G_pixel_01, G_pixel_10, G_pixel_11;
  int B_pixel_00, B_pixel_01, B_pixel_10, B_pixel_11;

  int  Y_pixel_00,  Y_pixel_01,  Y_pixel_10,  Y_pixel_11;
  int Cb_pixel_00, Cb_pixel_01, Cb_pixel_10, Cb_pixel_11;
  int Cr_pixel_00, Cr_pixel_01, Cr_pixel_10, Cr_pixel_11;

  // Upsample Cb and Cr into Cb_temp and Cr_temp
  //chrominance_array_upsample();

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

#define CHROMA_BLOCKS ( ((IMAGE_COL_SIZE >> 1) / 8) + 2 )  // small safety margin
// Scalar fallback for the tail: whatever interior chroma columns don't
// fill a full 8-wide block, plus the replicated last column. Computes
// one pixel's R/G/B directly -- no strip buffer involved.
static inline void set_rgb_pixel( int row, int col, int Yv, int Cbv, int Crv)
{
    int y = Yv - 16, cb = Cbv - 128, cr = Crv - 128;

    int r = D1 * y + D2 * cr;                r += (1 << (K - 1)); r >>= K;
    int g = D1 * y - D3 * cr - D4 * cb;       g += (1 << (K - 1)); g >>= K;
    int b = D1 * y + D5 * cb;                 b += (1 << (K - 1)); b >>= K;

    R[row][col] = saturate_to_u8(r);
    G[row][col] = saturate_to_u8(g);
    B[row][col] = saturate_to_u8(b);
}

static void CSC_YCC_to_RGB_neon_fused( void)
{
    const int ch_rows = IMAGE_ROW_SIZE >> 1;
    const int ch_cols = IMAGE_COL_SIZE >> 1;

    for (int crow = 0; crow < ch_rows; crow++) {
        int row  = crow << 1;
        int next = (crow + 1 < ch_rows) ? (crow + 1) : crow;

        int col = 0;

        // ---- vectorized interior: 8 chroma cols -> 16 luma cols per block ----
        for (; col + 8 <= ch_cols - 1; col += 8) {
            int lcol = col << 1;

            uint8x8_t cb00 = vld1_u8(&Cb[crow][col]);
            uint8x8_t cb01 = vld1_u8(&Cb[crow][col + 1]);
            uint8x8_t cb10 = vld1_u8(&Cb[next][col]);
            uint8x8_t cb11 = vld1_u8(&Cb[next][col + 1]);

            uint8x8_t cr00 = vld1_u8(&Cr[crow][col]);
            uint8x8_t cr01 = vld1_u8(&Cr[crow][col + 1]);
            uint8x8_t cr10 = vld1_u8(&Cr[next][col]);
            uint8x8_t cr11 = vld1_u8(&Cr[next][col + 1]);

            // 4 chroma variants per plane, kept in registers -- never stored.
            uint8x8_t cb_top    = vrhadd_u8(cb00, cb01);
            uint8x8_t cb_left   = vrhadd_u8(cb00, cb10);
            uint16x8_t cb_sum   = vaddq_u16(vaddl_u8(cb00, cb01), vaddl_u8(cb10, cb11));
            uint8x8_t cb_middle = vmovn_u16(vshrq_n_u16(vaddq_u16(cb_sum, vdupq_n_u16(2)), 2));

            uint8x8_t cr_top    = vrhadd_u8(cr00, cr01);
            uint8x8_t cr_left   = vrhadd_u8(cr00, cr10);
            uint16x8_t cr_sum   = vaddq_u16(vaddl_u8(cr00, cr01), vaddl_u8(cr10, cr11));
            uint8x8_t cr_middle = vmovn_u16(vshrq_n_u16(vaddq_u16(cr_sum, vdupq_n_u16(2)), 2));

            // Y loaded already de-interleaved into even/odd luma columns --
            // aligns 1:1 with the chroma variants above with no extra work.
            uint8x8x2_t y_row0 = vld2_u8(&Y[row][lcol]);
            uint8x8x2_t y_row1 = vld2_u8(&Y[row + 1][lcol]);

            // ---- row0: even cols use cb00/cr00, odd cols use cb_top/cr_top ----
            uint8x8_t r0e, g0e, b0e, r0o, g0o, b0o;
            color_matrix_neon_8( y_row0.val[0], cb00,   cr00,   &r0e, &g0e, &b0e);
            color_matrix_neon_8( y_row0.val[1], cb_top, cr_top, &r0o, &g0o, &b0o);

            uint8x8x2_t r0 = {{ r0e, r0o }}; vst2_u8(&R[row][lcol], r0);
            uint8x8x2_t g0 = {{ g0e, g0o }}; vst2_u8(&G[row][lcol], g0);
            uint8x8x2_t b0 = {{ b0e, b0o }}; vst2_u8(&B[row][lcol], b0);

            // ---- row1: even cols use cb_left/cr_left, odd cols use cb_middle/cr_middle ----
            uint8x8_t r1e, g1e, b1e, r1o, g1o, b1o;
            color_matrix_neon_8( y_row1.val[0], cb_left,   cr_left,   &r1e, &g1e, &b1e);
            color_matrix_neon_8( y_row1.val[1], cb_middle, cr_middle, &r1o, &g1o, &b1o);

            uint8x8x2_t r1 = {{ r1e, r1o }}; vst2_u8(&R[row + 1][lcol], r1);
            uint8x8x2_t g1 = {{ g1e, g1o }}; vst2_u8(&G[row + 1][lcol], g1);
            uint8x8x2_t b1 = {{ b1e, b1o }}; vst2_u8(&B[row + 1][lcol], b1);
        }

        // ---- scalar tail: leftover interior columns + replicated last column ----
        for (; col < ch_cols; col++) {
            int lcol = col << 1;
            int has_right = (col + 1 < ch_cols);

            int cb00 = Cb[crow][col];
            int cb01 = has_right ? Cb[crow][col + 1] : cb00;
            int cb10 = Cb[next][col];
            int cb11 = has_right ? Cb[next][col + 1] : cb10;

            int cr00 = Cr[crow][col];
            int cr01 = has_right ? Cr[crow][col + 1] : cr00;
            int cr10 = Cr[next][col];
            int cr11 = has_right ? Cr[next][col + 1] : cr10;

            int cb_top    = (cb00 + cb01 + 1) >> 1;
            int cb_left   = (cb00 + cb10 + 1) >> 1;
            int cb_middle = (cb00 + cb01 + cb10 + cb11 + 2) >> 2;

            int cr_top    = (cr00 + cr01 + 1) >> 1;
            int cr_left   = (cr00 + cr10 + 1) >> 1;
            int cr_middle = (cr00 + cr01 + cr10 + cr11 + 2) >> 2;

            set_rgb_pixel( row,     lcol,     Y[row][lcol],       cb00,      cr00);
            set_rgb_pixel( row,     lcol + 1, Y[row][lcol + 1],   cb_top,    cr_top);
            set_rgb_pixel( row + 1, lcol,     Y[row + 1][lcol],   cb_left,   cr_left);
            set_rgb_pixel( row + 1, lcol + 1, Y[row + 1][lcol + 1], cb_middle, cr_middle);
        }
    }
}
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
static void chroma_plane_upsample_neon( const uint8_t src[IMAGE_ROW_SIZE>>1][IMAGE_COL_SIZE>>1],
                                         uint8_t dst[IMAGE_ROW_SIZE][IMAGE_COL_SIZE])
{
    int row, col;
    const int ch_rows = (IMAGE_ROW_SIZE >> 1);
    const int ch_cols = (IMAGE_COL_SIZE >> 1);

    // ---- interior rows/cols, 8 columns at a time ----
    for (row = 0; row < ch_rows - 1; row++) {

        const uint8_t *src_row0 = &src[row][0];
        const uint8_t *src_row1 = &src[row + 1][0];
        uint8_t *dst_row0 = &dst[(row << 1) + 0][0];
        uint8_t *dst_row1 = &dst[(row << 1) + 1][0];

        col = 0;
        for (; col + 8 <= ch_cols - 1; col += 8) {
            chroma_upsample_neon_8( src_row0 + col, src_row1 + col,
                                    dst_row0 + (col << 1),
                                    dst_row1 + (col << 1));
        }

        // scalar remainder (interior columns left over, < 8 of them)
        for (; col < ch_cols - 1; col++) {
            int c00 = src_row0[col],     c01 = src_row0[col + 1];
            int c10 = src_row1[col],     c11 = src_row1[col + 1];

            int top    = (c00 + c01 + 1) >> 1;
            int left   = (c00 + c10 + 1) >> 1;
            int middle = (c00 + c01 + c10 + c11 + 2) >> 2;

            dst_row0[(col << 1) + 0] = (uint8_t)c00;
            dst_row0[(col << 1) + 1] = (uint8_t)top;
            dst_row1[(col << 1) + 0] = (uint8_t)left;
            dst_row1[(col << 1) + 1] = (uint8_t)middle;
        }

        // ---- last column of this row-pair: col replicated ----
        col = ch_cols - 1;
        {
            int c00 = src_row0[col], c10 = src_row1[col];
            int left = (c00 + c10 + 1) >> 1;

            dst_row0[(col << 1) + 0] = (uint8_t)c00;
            dst_row0[(col << 1) + 1] = (uint8_t)c00;   // top == c00
            dst_row1[(col << 1) + 0] = (uint8_t)left;
            dst_row1[(col << 1) + 1] = (uint8_t)left;  // middle == left
        }
    }

    // ---- last row: row replicated, cols 0..ch_cols-2 ----
    row = ch_rows - 1;
    {
        const uint8_t *src_row = &src[row][0];
        uint8_t *dst_row0 = &dst[(row << 1) + 0][0];
        uint8_t *dst_row1 = &dst[(row << 1) + 1][0];

        for (col = 0; col < ch_cols - 1; col++) {
            int c00 = src_row[col], c01 = src_row[col + 1];
            int top = (c00 + c01 + 1) >> 1;

            dst_row0[(col << 1) + 0] = (uint8_t)c00;
            dst_row0[(col << 1) + 1] = (uint8_t)top;
            dst_row1[(col << 1) + 0] = (uint8_t)c00;  // left == c00
            dst_row1[(col << 1) + 1] = (uint8_t)top;  // middle == top
        }

        // ---- bottom-right corner: single pixel replicated 4x ----
        col = ch_cols - 1;
        {
            uint8_t v = src_row[col];
            dst_row0[(col << 1) + 0] = v;
            dst_row0[(col << 1) + 1] = v;
            dst_row1[(col << 1) + 0] = v;
            dst_row1[(col << 1) + 1] = v;
        }
    }
}

// ---- Driver replacing chrominance_array_upsample() ----
static void chrominance_array_upsample_neon( void)
{
    chroma_plane_upsample_neon( Cb, Cb_temp);
    chroma_plane_upsample_neon( Cr, Cr_temp);
}


// =======
void CSC_YCC_to_RGB( void) {

  // Cb/Cr only need to be upsampled once per frame -- all three routines
  // (float, brute-force int, optimized) read from Cb_temp/Cr_temp, so this
  // is hoisted out of every per-block routine and done exactly once here.

  
  if( YCC_to_RGB_ROUTINE == 4) {
    CSC_YCC_to_RGB_neon_fused();
    //chrominance_array_upsample_neon();
    //CSC_YCC_to_RGB_neon(IMAGE_ROW_SIZE, IMAGE_COL_SIZE);
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