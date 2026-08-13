Before compiling the program ensure that CSC_main.c CSC_RGB_to_YCC_01.c CSC_YCC_to_RGB_01.c are all present in the directory

Use the following command to compile the program:

gcc -O3 -std=c99 -march=armv7-a -mfpu=neon -mfloat-abi=hard CSC_main.c CSC_RGB_to_YCC_01.c CSC_YCC_to_RGB_01.c -lm -o csc_demo

The code is configured to run with image_input_RGB_640_480_02.data Ensure that the file is present in the same directory

To use a different image the image name must be changed in CSC_main.c 

To use a different image size change IMAGE_ROW_SIZE and IMAGE_COL_SIZE in CSC_global.h

The program processes 8 pixels per row at two rows per cycle so ensure that the image row and column sizes are divisible 8 and 2 respectively

The specific implementation can be changed in CSC_global.h by altering #define RGB_to_YCC_ROUTINE and #define YCC_to_RGB_ROUTINE

The possible values RGB_to_YCC_ROUTINE are:

1 for brute force float implementation

2 for brute force int implementation

3 for optimized NEON implementation

The possible values YCC_to_RGB_ROUTINE are:

1 for brute force float implementation

2 for brute force int implementation

4 for optimized NEON implementation

To run the program use the command 

./csc_demo

