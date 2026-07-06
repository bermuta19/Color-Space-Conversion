% RGB-to-YCC and YCC-to-RGB conversion in floating-point arithmetic
%

clear all

r = 0;
g = 255;
b = 0;

rgb = [r; g; b]
CSC_rgb_to_ycc_matrix = [+0.257 +0.504 +0.098;...
                         -0.148 -0.291 +0.439;...
                         +0.439 -0.368 -0.071];

ycbcr = CSC_rgb_to_ycc_matrix * rgb;
ycbcr = ycbcr + [16; 128; 128];
ycbcr = round( ycbcr)

%ycbcr = [16; 240; 240]

CSC_ycc_to_rgb_matrix = [+1.164  0     +1.596;...
                         +1.164 -0.391 -0.813;...
                         +1.164 +2.018  0];

rgb_reconstructed = CSC_ycc_to_rgb_matrix * (ycbcr - [16; 128; 128]);
rgb_reconstructed = round( rgb_reconstructed)

