#!/bin/bash
gcc -O2 -std=c99 -march=armv7-a -mfpu=neon -mfloat-abi=hard CSC_main.c CSC_RGB_to_YCC_01.c CSC_YCC_to_RGB_01.c -lm -o csc_vector_demo
