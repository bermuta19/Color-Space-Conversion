#include <stdint.h>

#ifndef K
#define K 8 // bitwidth
#endif
#ifndef UNITY
#define UNITY (1 << K)
#endif
#ifndef IMAGE_ROW_SIZE
#define IMAGE_ROW_SIZE 640
#endif
#ifndef IMAGE_COL_SIZE
#define IMAGE_COL_SIZE 480
#endif
#ifndef CSC_FIXED_POINT_SHIFT
#define CSC_FIXED_POINT_SHIFT K
#endif
#ifndef CSC_USE_NEON
#define CSC_USE_NEON 1
#endif
#ifndef CSC_ENABLE_RGB_TO_YCC_OPTIMIZED
#define CSC_ENABLE_RGB_TO_YCC_OPTIMIZED 0
#endif
#ifndef CSC_ENABLE_RGB_TO_YCC_NEON
#define CSC_ENABLE_RGB_TO_YCC_NEON 0
#endif
#ifndef CSC_ENABLE_YCC_TO_RGB_OPTIMIZED
#define CSC_ENABLE_YCC_TO_RGB_OPTIMIZED 1
#endif
#ifndef CSC_ENABLE_YCC_TO_RGB_NEON
#define CSC_ENABLE_YCC_TO_RGB_NEON 1
#endif
#ifndef CSC_ENABLE_YCC_TO_RGB_ASM
#define CSC_ENABLE_YCC_TO_RGB_ASM 1
#endif
#define CSC_ROUNDING (1u << (CSC_FIXED_POINT_SHIFT - 1))

// RGB_to_YCC_ROUTINE
//     1 for CSC_RGB_to_YCC_brute_force_float()
//     2 for CSC_RGB_to_YCC_brute_force_int()
//     3 for CSC_RGB_to_YCC_optimized()
#define RGB_to_YCC_ROUTINE 2

// YCC_to_RGB_ROUTINE
//     1 for CSC_YCC_to_RGB_brute_force_float()
//     2 for CSC_YCC_to_RGB_brute_force_int()
//     3 for CSC_YCC_to_RGB_optimized()
#define YCC_to_RGB_ROUTINE 3

// CHROMINANCE_DOWNSAMPLING_MODE = 
//     0 for returning zero (no chrominance)
//     1 for discarding three pixels and keeping one
//     2 for averaging four pixels
#define CHROMINANCE_DOWNSAMPLING_MODE 1

// CHROMINANCE_UPSAMPLING_MODE = 
//     0 for returning zero (no chrominance)
//     1 for replicating one pixel into three
//     2 for interpolation with two pixels
#define CHROMINANCE_UPSAMPLING_MODE 1

// RGB-to-YCC coefficients in 8-bit representation
#define C11  66
#define C12 129
#define C13  25
#define C21  38
#define C22  74
#define C23 112
#define C31 112
#define C32  94
#define C33  18

// YCC-to-RGB coefficients in 8-bit representation
#define D1 298   // 1.164 * 256
#define D2 409   // 1.596 * 256
#define D3 208   // 0.813 * 256
#define D4 100   // 0.391 * 256
#define D5 516   // 2.018 * 256

/* choose between definition (GLOBAL is defined)      *
 * and declaration (GLOBAL is undefined)              *
 * GLOBAL is defined in exactly one file CSC_main.c)  */

#ifndef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif

/* prototypes of global functions */
void CSC_RGB_to_YCC( void);
void CSC_YCC_to_RGB( void);

/* global variables */
EXTERN uint8_t R[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Red array pointer
EXTERN uint8_t G[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Green array pointer
EXTERN uint8_t B[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Blue array pointer
EXTERN uint8_t Y[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Luminance array pointer
EXTERN uint8_t Cb[IMAGE_ROW_SIZE >> 1][IMAGE_COL_SIZE >> 1]; // Chrominance (Cb) array pointer
EXTERN uint8_t Cr[IMAGE_ROW_SIZE >> 1][IMAGE_COL_SIZE >> 1]; // Chrominance (Cr) array pointer
EXTERN uint8_t Cb_temp[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Chrominance (Cb) temp array pointer
EXTERN uint8_t Cr_temp[IMAGE_ROW_SIZE][IMAGE_COL_SIZE]; // Chrominance (Cr) temp array pointer

