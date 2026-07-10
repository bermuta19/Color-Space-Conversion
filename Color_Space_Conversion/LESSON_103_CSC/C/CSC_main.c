// Copyright 2023 Mihai SIMA (mihai.sima@ieee.org).  All rights reserved.
// Color Space Conversion (CSC) in fixed-point arithmetic
// main() function

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#define GLOBAL
#include "CSC_global.h"

static FILE *open_with_fallback( const char *base_name, const char *mode) {
  const char *paths[] = {
    base_name,
    "./",
    "../",
    "../C/",
    "./C/",
    "./LESSON_103_CSC/C/",
    "./Color_Space_Conversion/LESSON_103_CSC/C/"
  };
  char full_path[512];
  size_t i;

  for( i = 0; i < (sizeof(paths) / sizeof(paths[0])); ++i) {
    FILE *fp;
    const char *path = paths[i];

    if( path[0] == '\0') {
      continue;
    }
    if( strcmp( path, "./") == 0 || strcmp( path, "../") == 0 ||
        strcmp( path, "../C/") == 0 || strcmp( path, "./C/") == 0 ||
        strcmp( path, "./LESSON_103_CSC/C/") == 0 ||
        strcmp( path, "./Color_Space_Conversion/LESSON_103_CSC/C/") == 0) {
      snprintf( full_path, sizeof(full_path), "%s%s", path, base_name);
      fp = fopen( full_path, mode);
    }
    else {
      fp = fopen( path, mode);
    }
    if( fp != NULL) {
      return fp;
    }
  }

  fprintf( stderr, "Unable to open %s with mode %s\n", base_name, mode);
  return NULL;
}

int main( void) {
  int row, col;
  int benchmark_rounds = 1;
  int run;
  clock_t start, finish;
  char cwd[512];
  const char *input_name = getenv( "CSC_INPUT_IMAGE");
  const char *output_prefix = getenv( "CSC_OUTPUT_PREFIX");
  char input_path[512];
  char echo_r_path[512];
  char echo_g_path[512];
  char echo_b_path[512];
  char output_y_path[512];
  char output_cb_path[512];
  char output_cr_path[512];
  char output_rgb_path[512];
  FILE *f_ID_input_RGB;
  FILE *f_ID_echo_R;
  FILE *f_ID_echo_G;
  FILE *f_ID_echo_B;
  FILE *f_ID_output_Y;
  FILE *f_ID_output_Cb;
  FILE *f_ID_output_Cr;
  FILE *f_ID_output_RGB;

  if( getenv( "CSC_BENCHMARK_ROUNDS") != NULL) {
    benchmark_rounds = atoi( getenv( "CSC_BENCHMARK_ROUNDS"));
  }
  if( benchmark_rounds < 1) {
    benchmark_rounds = 1;
  }

  if( getcwd( cwd, sizeof(cwd)) != NULL) {
    printf( "Working directory: %s\n", cwd);
  }
  printf( "Processing %d x %d image with %d round(s)\n",
          IMAGE_ROW_SIZE, IMAGE_COL_SIZE, benchmark_rounds);

  if( input_name == NULL) {
    input_name = "image_input_RGB_640_480_02.data";
  }
  if( output_prefix == NULL) {
    output_prefix = "image";
  }

  snprintf( input_path, sizeof(input_path), "%s", input_name);
  snprintf( echo_r_path, sizeof(echo_r_path), "%s_echo_R_640_480_02.data", output_prefix);
  snprintf( echo_g_path, sizeof(echo_g_path), "%s_echo_G_640_480_02.data", output_prefix);
  snprintf( echo_b_path, sizeof(echo_b_path), "%s_echo_B_640_480_02.data", output_prefix);
  snprintf( output_y_path, sizeof(output_y_path), "%s_output_Y_640_480_02.data", output_prefix);
  snprintf( output_cb_path, sizeof(output_cb_path), "%s_output_Cb_640_480_02.data", output_prefix);
  snprintf( output_cr_path, sizeof(output_cr_path), "%s_output_Cr_640_480_02.data", output_prefix);
  snprintf( output_rgb_path, sizeof(output_rgb_path), "%s_output_RGB_640_480_02.data", output_prefix);

  f_ID_input_RGB = open_with_fallback( input_path, "rb");
  if( f_ID_input_RGB == NULL) {
    fprintf( stderr, "Cannot open input file '%s'\n", input_path);
    return( 1);
  }
  printf( "Loaded input file: %s\n", input_path);

  f_ID_echo_R = fopen( echo_r_path, "wb");
  if( f_ID_echo_R == NULL) {
    fprintf( stderr, "Cannot open output file '%s'\n", echo_r_path);
    return( 1);
  }

  f_ID_echo_G = fopen( echo_g_path, "wb");
  if( f_ID_echo_G == NULL) {
    fprintf( stderr, "Cannot open output file '%s'\n", echo_g_path);
    return( 1);
  }

  f_ID_echo_B = fopen( echo_b_path, "wb");
  if( f_ID_echo_B == NULL) {
    fprintf( stderr, "Cannot open output file '%s'\n", echo_b_path);
    return( 1);
  }

  for( row=0; row < IMAGE_ROW_SIZE; row++)
  for( col=0; col < IMAGE_COL_SIZE; col++) {
    R[row][col] = (uint8_t)( fgetc( f_ID_input_RGB));
    fputc( R[row][col], f_ID_echo_R);
//
    G[row][col] = (uint8_t)( fgetc( f_ID_input_RGB));
    fputc( G[row][col], f_ID_echo_G);
//
    B[row][col] = (uint8_t)( fgetc( f_ID_input_RGB));
    fputc( B[row][col], f_ID_echo_B);
  }
  fclose( f_ID_echo_B);
  fclose( f_ID_echo_G);
  fclose( f_ID_echo_R);
  fclose( f_ID_input_RGB);

  printf( "Starting CSC conversion...\n");
  start = clock();
  for( run = 0; run < benchmark_rounds; ++run) {
    CSC_RGB_to_YCC();
    print( "CSC_RGB_to_YCC() completed\n");
    CSC_YCC_to_RGB();
    print( "CSC_YCC_to_RGB() completed\n");
  }
  finish = clock();
  printf( "CSC benchmark: %d rounds, %.3f seconds\n",
          benchmark_rounds,
          (double)(finish - start) / CLOCKS_PER_SEC);

  f_ID_output_Y = fopen( output_y_path, "wb");
  if( f_ID_output_Y == NULL) {
    fprintf( stderr, "Could not open %s\n", output_y_path);
    return( 1);
  }
  
  f_ID_output_Cb = fopen( output_cb_path, "wb");
  if( f_ID_output_Cb == NULL) {
    fprintf( stderr, "Could not open %s\n", output_cb_path);
    return( 1);
  }
  
  f_ID_output_Cr = fopen( output_cr_path, "wb");
  if( f_ID_output_Cr == NULL) {
    fprintf( stderr, "Could not open %s\n", output_cr_path);
    return( 1);
  }
  
  for( row=0; row < IMAGE_ROW_SIZE; row++)
  for( col=0; col < IMAGE_COL_SIZE; col++) {
    //fprintf( f_ID_output_Y, "%02hhx", Y[row][col]);
    fputc( Y[row][col], f_ID_output_Y);
  }

  for( row=0; row < (IMAGE_ROW_SIZE >> 1); row++)
  for( col=0; col < (IMAGE_COL_SIZE >> 1); col++) {
    //fprintf( f_ID_output_Cb, "%02hhx", Cb[row][col]);
    fputc( Cb[row][col], f_ID_output_Cb);
    //fprintf( f_ID_output_Cr, "%02hhx", Cr[row][col]);
    fputc( Cr[row][col], f_ID_output_Cr);
  }

  fclose( f_ID_output_Cr);
  fclose( f_ID_output_Cb);
  fclose( f_ID_output_Y);

  f_ID_output_RGB = fopen( output_rgb_path, "wb");
  if( f_ID_output_RGB == NULL) {
    fprintf( stderr, "Cannot open output file '%s'\n", output_rgb_path);
    return( 1);
  }

  for( row=0; row < IMAGE_ROW_SIZE; row++)
  for( col=0; col < IMAGE_COL_SIZE; col++) {
    fputc( R[row][col], f_ID_output_RGB);
    fputc( G[row][col], f_ID_output_RGB);
    fputc( B[row][col], f_ID_output_RGB);
  }
  fclose( f_ID_output_RGB);
  printf( "Completed CSC conversion. Output prefix: %s\n", output_prefix);
  return( 0);

} // END of main()

