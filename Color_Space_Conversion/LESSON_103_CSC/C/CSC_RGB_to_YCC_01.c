// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// RGB to YCC conversion

//#include <stdio.h>
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
static void CSC_RGB_to_YCC_vectors( int row, int col, uint16x8_t y_base, uint16x8_t c_base);
// =======

static uint8_t chrominance_downsample(
    uint8_t C_pixel_1, uint8_t C_pixel_2,
    uint8_t C_pixel_3, uint8_t C_pixel_4);

// private definitions
// =======

static void CSC_RGB_to_YCC_vectors( int row, int col, uint16x8_t y_base, uint16x8_t c_base)
{
  //----------------------------------------------------------------
  //Processing row 0
  //----------------------------------------------------------------

  //load 8 pixels from the R, G, and B arrays into NEON registers
  uint8x8_t r_row0 = vld1_u8(&R[row][col]);
  uint8x8_t g_row0 = vld1_u8(&G[row][col]);
  uint8x8_t b_row0 = vld1_u8(&B[row][col]);
  
  // upgrade to 16-bit for multiplication
  uint16x8_t r0_16 = vmovl_u8(r_row0);
  uint16x8_t g0_16 = vmovl_u8(g_row0);
  uint16x8_t b0_16 = vmovl_u8(b_row0);
  
  //multiplies the R vector by C11 and adds it to the y_base vector, storing the result in y_row0
  uint16x8_t y_row0 = vmlaq_n_u16(y_base, r0_16, (uint16_t)C11); 
  y_row0 = vmlaq_n_u16(y_row0, g0_16, (uint16_t)C12); //adds the product of C12 and the G vector to the previous result
  y_row0 = vmlaq_n_u16(y_row0, b0_16, (uint16_t)C13); //adds the product of C13 and the B vector to the previous result
  
  //converts back into 8-bit representation and shifts back 8 bits to undo changes for the fixed-point representation
  uint8x8_t y_8bit_row0 = vshrn_n_u16(y_row0, K); 
  
  //store the results back into the Y array
  vst1_u8(&Y[row][col], y_8bit_row0);
  
  //multiplies the B vector by C23 and adds it to the c_base vector, storing the result in cb_row0
  uint16x8_t cb_row0 = vmlaq_n_u16(c_base, b0_16, (uint16_t)C23);
  cb_row0 = vmlsq_n_u16(cb_row0, r0_16, (uint16_t)C21);
  cb_row0 = vmlsq_n_u16(cb_row0, g0_16, (uint16_t)C22);
  
  //pairwise addition of the 16-bit values into 4 32-bit values.
  uint32x4_t cb_pairwise_row0 = vpaddlq_u16(cb_row0); 
  
  //vector multiplication and accumulation for Cr calculation | 32,896 + R*C23 - G*C21 - B*C22
  uint16x8_t cr_row0 = vmlaq_n_u16(c_base, r0_16, (uint16_t)C31);
  cr_row0 = vmlsq_n_u16(cr_row0, g0_16, (uint16_t)C32);
  cr_row0 = vmlsq_n_u16(cr_row0, b0_16, (uint16_t)C33);
  
  //pairwise addition of the 16-bit values into 4 32-bit values.
  uint32x4_t cr_pairwise_row0 = vpaddlq_u16(cr_row0); 

  //----------------------------------------------------------------
  //End of row 0 processing, now process row 1
  //----------------------------------------------------------------

  //load 8 pixels from the R, G, and B arrays into NEON registers
  uint8x8_t r_row1 = vld1_u8(&R[row + 1][col]);
  uint8x8_t g_row1 = vld1_u8(&G[row + 1][col]);
  uint8x8_t b_row1 = vld1_u8(&B[row + 1][col]);

  // upgrade to 16-bit for multiplication
  uint16x8_t r1_16 = vmovl_u8(r_row1);
  uint16x8_t g1_16 = vmovl_u8(g_row1);
  uint16x8_t b1_16 = vmovl_u8(b_row1);

  //multiplies the R vector by C11 and adds it to the y_base vector, storing the result in y_row1
  uint16x8_t y_row1 = vmlaq_n_u16(y_base, r1_16, (uint16_t)C11);
  y_row1 = vmlaq_n_u16(y_row1, g1_16, (uint16_t)C12); //adds the product of C12 and the G vector to the previous result
  y_row1 = vmlaq_n_u16(y_row1, b1_16, (uint16_t)C13); //adds the product of C13 and the B vector to the previous result

  //converts back into 8-bit representation and shifts back 8 bits to undo changes for the fixed-point representation
  uint8x8_t y_8bit_row1 = vshrn_n_u16(y_row1, K);
  
  //store the results back into the Y array
  vst1_u8(&Y[row + 1][col], y_8bit_row1);

  //vector multiplication and accumulation for Cb calculation | 32,896 + B*C23 - R*C21 - G*C22
  uint16x8_t cb_row1 = vmlaq_n_u16(c_base, b1_16, (uint16_t)C23);
  cb_row1 = vmlsq_n_u16(cb_row1, r1_16, (uint16_t)C21);
  cb_row1 = vmlsq_n_u16(cb_row1, g1_16, (uint16_t)C22);

  //pairwise addition of the 8 16-bit values into 4 32-bit values.
  uint32x4_t cb_pairwise_row1 = vpaddlq_u16(cb_row1);

  //vector multiplication and accumulation for Cr calculation | 32,896 + R*C31 - G*C32 - B*C33
  uint16x8_t cr_row1 = vmlaq_n_u16(c_base, r1_16, (uint16_t)C31);
  cr_row1 = vmlsq_n_u16(cr_row1, g1_16, (uint16_t)C32);
  cr_row1 = vmlsq_n_u16(cr_row1, b1_16, (uint16_t)C33);

  //pairwise addition of the 8 16-bit values into 4 32-bit values.
  uint32x4_t cr_pairwise_row1 = vpaddlq_u16(cr_row1);

  //----------------------------------------------------------------
  //Chrominance downsampling (average) and storing into the Cb and Cr arrays
  //----------------------------------------------------------------

  //downsampling cr
  uint32x4_t cr_combined =  vaddq_u32(cr_pairwise_row0, cr_pairwise_row1); //horizontal add of the two rows
  uint16x4_t cr_downsampled = vshrn_n_u32(cr_combined, K); //divide by K to undo offset
  uint8x8_t cr_final_8bit = vshrn_n_u16(vcombine_u16(cr_downsampled, vcreate_u16(0)), 2); //shift by 2 (divide by 4) and narrow from 16-bit down to 8-bit
  vst1_lane_u32((uint32_t*)&Cr[row>>1][col>>1], vreinterpret_u32_u8(cr_final_8bit), 0);   // store into Cr array

   //downsampling cb
  uint32x4_t cb_combined =  vaddq_u32(cb_pairwise_row0, cb_pairwise_row1); //horizontal add of the two rows
  uint16x4_t cb_downsampled = vshrn_n_u32(cb_combined, K); //divide by K to undo offset
  uint8x8_t cb_final_8bit = vshrn_n_u16(vcombine_u16(cb_downsampled, vcreate_u16(0)), 2); //shift by 2 (divide by 4) and narrow from 16-bit down to 8-bit
  vst1_lane_u32((uint32_t*)&Cb[row>>1][col>>1], vreinterpret_u32_u8(cb_final_8bit), 0); // store into Cb array

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
            CSC_RGB_to_YCC_vectors( row, col, y_base, c_base);
          }
          break;
        default:
          break;
      }
//      printf( "Luma_00  = %02hhx\n", Y[row+0][col+0]);
//      printf( "Luma_01  = %02hhx\n", Y[row+0][col+1]);
//      printf( "Luma_10  = %02hhx\n", Y[row+1][col+0]);
//      printf( "Luma_11  = %02hhx\n\n", Y[row+1][col+1]);
  }

} // END of CSC_RGB_to_YCC()

