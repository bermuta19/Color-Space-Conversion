// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// main() function

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
//#include <string.h>
#include <math.h>

#define GLOBAL
#include "CSC_global.h"

int main( void) {
  int i;
  FILE *f_ID_input_RGB;
  FILE *f_ID_echo_R;
  FILE *f_ID_echo_G;
  FILE *f_ID_echo_B;
  FILE *f_ID_output_Y;
  FILE *f_ID_output_Cb;
  FILE *f_ID_output_Cr;

//  R  = (uint8_t *)( calloc( IMAGE_ROW_SIZE*IMAGE_COL_SIZE, 1));
//  G  = (uint8_t *)( calloc( IMAGE_ROW_SIZE*IMAGE_COL_SIZE, 1));
//  B  = (uint8_t *)( calloc( IMAGE_ROW_SIZE*IMAGE_COL_SIZE, 1));
//  Y  = (uint8_t *)( calloc( IMAGE_ROW_SIZE*IMAGE_COL_SIZE, 1));
//  Cb = (int8_t *)( calloc( (IMAGE_ROW_SIZE*IMAGE_COL_SIZE) >> 2, 1));
//  Cr = (int8_t *)( calloc( (IMAGE_ROW_SIZE*IMAGE_COL_SIZE) >> 2, 1));

  f_ID_input_RGB = fopen( "./image_input_RGB_64_48_03.data", "rb");
  if( f_ID_input_RGB == NULL) {
    printf( "Cannot open file.\n");
    return( 1);
  }

  f_ID_echo_R = fopen( "./image_echo_R_64_48_03.data", "wb");
  if( f_ID_echo_R == NULL) {
    printf( "Cannot open file.\n");
    return( 1);
  }

  f_ID_echo_G = fopen( "./image_echo_G_64_48_03.data", "wb");
  if( f_ID_echo_G == NULL) {
    printf( "Cannot open file.\n");
    return( 1);
  }

  f_ID_echo_B = fopen( "./image_echo_B_64_48_03.data", "wb");
  if( f_ID_echo_B == NULL) {
    printf( "Cannot open file.\n");
    return( 1);
  }

  for( i=0; i < IMAGE_ROW_SIZE*IMAGE_COL_SIZE; i++) {
    //fscanf( f_ID_input_RGB, "%c", R+i);
    //fprintf( f_ID_echo_R, "%02hhx", *(R+i));
    //printf( "Red   = %02hhx\n", *(R+i));
    *(R+i) = (unsigned char)( fgetc( f_ID_input_RGB));
    fputc( *(R+i), f_ID_echo_R);
    //fscanf( f_ID_input_RGB, "%c", G+i);
    //fprintf( f_ID_echo_G, "%02hhx", *(G+i));
    //printf( "Green = %02hhx\n", *(G+i));
    *(G+i) = (unsigned char)( fgetc( f_ID_input_RGB));
    fputc( *(G+i), f_ID_echo_G);
    //fscanf( f_ID_input_RGB, "%c", B+i);
    //fprintf( f_ID_echo_B, "%02hhx", *(B+i));
    //printf( "Blue  = %02hhx\n\n", *(B+i));
    *(B+i) = (unsigned char)( fgetc( f_ID_input_RGB));
    fputc( *(B+i), f_ID_echo_B);
  }
  fclose( f_ID_echo_B);
  fclose( f_ID_echo_G);
  fclose( f_ID_echo_R);
  fclose( f_ID_input_RGB);

  CSC_RGB_to_YCC();

  f_ID_output_Y = fopen( "./image_output_Y_64_48_03.data", "wb");
  if( f_ID_output_Y == NULL) {
    fprintf( stderr, "Could not open %s\n", 
             "./image_output_Y_64_48_03.data");
    return( 1);
  }
  
  printf( "After f_ID_output_Y!\n");

  f_ID_output_Cb = fopen( "./image_output_Cb_64_48_03.data", "wb");
  printf( "Inside f_ID_output_Cb!\n");
  if( f_ID_output_Cb == NULL) {
    fprintf( stderr, "Could not open %s\n", 
             "./image_output_Cb_64_48_03.data");
    return( 1);
  }
  
  printf( "Here!\n");

  f_ID_output_Cr = fopen( "./image_output_Cr_64_48_03.data", "wb");
  if( f_ID_output_Cr == NULL) {
    fprintf( stderr, "Could not open %s\n", 
             "./image_output_Cr_64_48_03.data");
    return( 1);
  }
  
//  printf( "Here!\n");

  for( i=0; i < IMAGE_ROW_SIZE*IMAGE_COL_SIZE; i++) {
    //fprintf( f_ID_output_Y, "%02hhx", Y+i);
    fputc( *(Y+i), f_ID_output_Y);
  }

  for( i=0; i < ((IMAGE_ROW_SIZE*IMAGE_COL_SIZE) >> 2); i++) {
    //fprintf( f_ID_output_Cb, "%02hhx", Cb+i);
    fputc( *(Cb+i), f_ID_output_Cb);
    //fprintf( f_ID_output_Cr, "%02hhx", Cr+i);
    fputc( *(Cr+i), f_ID_output_Cr);
  }

  fclose( f_ID_output_Cr);
  fclose( f_ID_output_Cb);
  fclose( f_ID_output_Y);

//  free( (void *)Cr);
//  free( (void *)Cb);
//  free( (void *)Y);
//  free( (void *)B);
//  free( (void *)G);
//  free( (void *)R);

} // END of main()

