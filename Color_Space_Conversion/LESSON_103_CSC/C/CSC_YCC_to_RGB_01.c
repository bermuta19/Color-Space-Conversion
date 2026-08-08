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
static uint8_t saturation_float( float argument);
static void CSC_YCC_to_RGB_brute_force_float( int row, int col);
static void CSC_YCC_to_RGB_brute_force_int( int row, int col);
static void chrominance_upsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4,
    uint8_t *top, uint8_t *left, uint8_t *middle);
static void chrominance_array_upsample( void);

```c
static void CSC_YCC_to_RGB_vectors(int row, int col)
{
    /*
     * Process:
     *
     *   2 rows x 8 pixels
     *
     * Chroma is 4:2:0 and MODE 1, so each chroma sample is
     * replicated horizontally and vertically.
     *
     * The important optimization here is that we keep Y/Cb/Cr
     * as signed 16-bit values and use the ARMv7 widening
     * multiply-accumulate instructions:
     *
     *     vmull_n_s16()
     *     vmlal_n_s16()
     *
     * instead of explicitly widening everything to int32 first.
     */

    // ------------------------------------------------------------
    // Load Y for both rows.
    // ------------------------------------------------------------
    uint8x8_t y_row0 = vld1_u8(&Y[row][col]);
    uint8x8_t y_row1 = vld1_u8(&Y[row + 1][col]);

    // ------------------------------------------------------------
    // Load the 4 chroma samples needed for 8 output pixels.
    //
    // Cb/Cr layout:
    //
    //   C0 C1 C2 C3
    //
    // becomes:
    //
    //   C0 C0 C1 C1 C2 C2 C3 C3
    //
    // The same replicated chroma is used for both output rows.
    // ------------------------------------------------------------
    uint8x8_t cb_src = vld1_u8(&Cb[row >> 1][col >> 1]);
    uint8x8_t cr_src = vld1_u8(&Cr[row >> 1][col >> 1]);

    uint8x8_t cb_rep = vzip_u8(cb_src, cb_src).val[0];
    uint8x8_t cr_rep = vzip_u8(cr_src, cr_src).val[0];

    // ------------------------------------------------------------
    // Convert unsigned 8-bit values to signed 16-bit offsets.
    //
    // Y  = Y  - 16
    // Cb = Cb - 128
    // Cr = Cr - 128
    // ------------------------------------------------------------
    int16x8_t y0 = vsubq_s16(
        vreinterpretq_s16_u16(vmovl_u8(y_row0)),
        vdupq_n_s16(16));

    int16x8_t y1 = vsubq_s16(
        vreinterpretq_s16_u16(vmovl_u8(y_row1)),
        vdupq_n_s16(16));

    int16x8_t cb = vsubq_s16(
        vreinterpretq_s16_u16(vmovl_u8(cb_rep)),
        vdupq_n_s16(128));

    int16x8_t cr = vsubq_s16(
        vreinterpretq_s16_u16(vmovl_u8(cr_rep)),
        vdupq_n_s16(128));


    // ============================================================
    // ROW 0
    // ============================================================

    int16x4_t y0_lo  = vget_low_s16(y0);
    int16x4_t y0_hi  = vget_high_s16(y0);

    int16x4_t cb_lo  = vget_low_s16(cb);
    int16x4_t cb_hi  = vget_high_s16(cb);

    int16x4_t cr_lo  = vget_low_s16(cr);
    int16x4_t cr_hi  = vget_high_s16(cr);


    // ------------------------------------------------------------
    // R = D1*Y + D2*Cr
    //
    // Start with a widening multiply:
    //
    //   int16 x coefficient -> int32
    //
    // Then accumulate the second term directly into int32.
    // ------------------------------------------------------------
    int32x4_t r0_lo = vmull_n_s16(y0_lo, D1);
    int32x4_t r0_hi = vmull_n_s16(y0_hi, D1);

    r0_lo = vmlal_n_s16(r0_lo, cr_lo, D2);
    r0_hi = vmlal_n_s16(r0_hi, cr_hi, D2);

    // Round and shift.
    r0_lo = vrshrq_n_s32(r0_lo, K);
    r0_hi = vrshrq_n_s32(r0_hi, K);

    // Saturate directly through int16 -> uint8.
    int16x8_t r0_16 = vcombine_s16(
        vqmovn_s32(r0_lo),
        vqmovn_s32(r0_hi));

    vst1_u8(
        &R[row][col],
        vqmovun_s16(r0_16));


    // ------------------------------------------------------------
    // G = D1*Y - D3*Cr - D4*Cb
    //
    // Again, start with a widening multiply and then perform
    // widening multiply-subtract operations.
    // ------------------------------------------------------------
    int32x4_t g0_lo = vmull_n_s16(y0_lo, D1);
    int32x4_t g0_hi = vmull_n_s16(y0_hi, D1);

    g0_lo = vmlsl_n_s16(g0_lo, cr_lo, D3);
    g0_hi = vmlsl_n_s16(g0_hi, cr_hi, D3);

    g0_lo = vmlsl_n_s16(g0_lo, cb_lo, D4);
    g0_hi = vmlsl_n_s16(g0_hi, cb_hi, D4);

    // Round and shift.
    g0_lo = vrshrq_n_s32(g0_lo, K);
    g0_hi = vrshrq_n_s32(g0_hi, K);

    int16x8_t g0_16 = vcombine_s16(
        vqmovn_s32(g0_lo),
        vqmovn_s32(g0_hi));

    vst1_u8(
        &G[row][col],
        vqmovun_s16(g0_16));


    // ------------------------------------------------------------
    // B = D1*Y + D5*Cb
    // ------------------------------------------------------------
    int32x4_t b0_lo = vmull_n_s16(y0_lo, D1);
    int32x4_t b0_hi = vmull_n_s16(y0_hi, D1);

    b0_lo = vmlal_n_s16(b0_lo, cb_lo, D5);
    b0_hi = vmlal_n_s16(b0_hi, cb_hi, D5);

    // Round and shift.
    b0_lo = vrshrq_n_s32(b0_lo, K);
    b0_hi = vrshrq_n_s32(b0_hi, K);

    int16x8_t b0_16 = vcombine_s16(
        vqmovn_s32(b0_lo),
        vqmovn_s32(b0_hi));

    vst1_u8(
        &B[row][col],
        vqmovun_s16(b0_16));


    // ============================================================
    // ROW 1
    // ============================================================

    int16x4_t y1_lo = vget_low_s16(y1);
    int16x4_t y1_hi = vget_high_s16(y1);


    // ------------------------------------------------------------
    // R = D1*Y + D2*Cr
    // ------------------------------------------------------------
    int32x4_t r1_lo = vmull_n_s16(y1_lo, D1);
    int32x4_t r1_hi = vmull_n_s16(y1_hi, D1);

    r1_lo = vmlal_n_s16(r1_lo, cr_lo, D2);
    r1_hi = vmlal_n_s16(r1_hi, cr_hi, D2);

    r1_lo = vrshrq_n_s32(r1_lo, K);
    r1_hi = vrshrq_n_s32(r1_hi, K);

    int16x8_t r1_16 = vcombine_s16(
        vqmovn_s32(r1_lo),
        vqmovn_s32(r1_hi));

    vst1_u8(
        &R[row + 1][col],
        vqmovun_s16(r1_16));


    // ------------------------------------------------------------
    // G = D1*Y - D3*Cr - D4*Cb
    // ------------------------------------------------------------
    int32x4_t g1_lo = vmull_n_s16(y1_lo, D1);
    int32x4_t g1_hi = vmull_n_s16(y1_hi, D1);

    g1_lo = vmlsl_n_s16(g1_lo, cr_lo, D3);
    g1_hi = vmlsl_n_s16(g1_hi, cr_hi, D3);

    g1_lo = vmlsl_n_s16(g1_lo, cb_lo, D4);
    g1_hi = vmlsl_n_s16(g1_hi, cb_hi, D4);

    g1_lo = vrshrq_n_s32(g1_lo, K);
    g1_hi = vrshrq_n_s32(g1_hi, K);

    int16x8_t g1_16 = vcombine_s16(
        vqmovn_s32(g1_lo),
        vqmovn_s32(g1_hi));

    vst1_u8(
        &G[row + 1][col],
        vqmovun_s16(g1_16));


    // ------------------------------------------------------------
    // B = D1*Y + D5*Cb
    // ------------------------------------------------------------
    int32x4_t b1_lo = vmull_n_s16(y1_lo, D1);
    int32x4_t b1_hi = vmull_n_s16(y1_hi, D1);

    b1_lo = vmlal_n_s16(b1_lo, cb_lo, D5);
    b1_hi = vmlal_n_s16(b1_hi, cb_hi, D5);

    b1_lo = vrshrq_n_s32(b1_lo, K);
    b1_hi = vrshrq_n_s32(b1_hi, K);

    int16x8_t b1_16 = vcombine_s16(
        vqmovn_s32(b1_lo),
        vqmovn_s32(b1_hi));

    vst1_u8(
        &B[row + 1][col],
        vqmovun_s16(b1_16));
}
```



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
  
    int row, col;
  if( YCC_to_RGB_ROUTINE == 4) {
    for( row = 0; row < IMAGE_ROW_SIZE; row += 2) {
        for( col = 0; col < IMAGE_COL_SIZE; col += 8) {
          CSC_YCC_to_RGB_vectors( row, col);
        }
      }
  } else {
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
  }
} // END of CSC_YCC_to_RGB()