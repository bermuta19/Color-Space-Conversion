	.cpu arm920t
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 6
	.eabi_attribute 34, 0
	.eabi_attribute 18, 4
	.file	"CSC_RGB_to_YCC_01.c"
	.text
	.global	__aeabi_i2d
	.global	__aeabi_dmul
	.global	__aeabi_dadd
	.global	__aeabi_d2uiz
	.global	__aeabi_dsub
	.align	2
	.syntax unified
	.arm
	.fpu softvfp
	.type	CSC_RGB_to_YCC_brute_force_float, %function
CSC_RGB_to_YCC_brute_force_float:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{r4, r5, fp, lr}
	add	fp, sp, #12
	sub	sp, sp, #16
	str	r0, [fp, #-24]
	str	r1, [fp, #-28]
	ldr	r1, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+4
	ldr	r3, .L2+8
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+12
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+20
	ldr	r3, .L2+24
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+32
	ldr	r3, .L2+36
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	and	r0, r3, #255
	ldr	r1, .L2+40
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	mov	r2, r0
	strb	r2, [r3]
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+4
	ldr	r3, .L2+8
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+12
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+20
	ldr	r3, .L2+24
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+32
	ldr	r3, .L2+36
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r1, r3
	mov	r2, r4
	ldr	r3, [fp, #-28]
	add	r4, r3, #1
	mov	r0, r1
	mov	r1, r2
	bl	__aeabi_d2uiz
	mov	r3, r0
	and	r0, r3, #255
	ldr	r1, .L2+40
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r1, r3
	add	r3, r3, r4
	mov	r2, r0
	strb	r2, [r3]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+4
	ldr	r3, .L2+8
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+12
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+20
	ldr	r3, .L2+24
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+32
	ldr	r3, .L2+36
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r1, r3
	mov	r2, r4
	ldr	r3, [fp, #-24]
	add	r4, r3, #1
	mov	r0, r1
	mov	r1, r2
	bl	__aeabi_d2uiz
	mov	r3, r0
	and	r1, r3, #255
	ldr	r2, .L2+40
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #4
	add	r2, r2, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	mov	r2, r1
	strb	r2, [r3]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+4
	ldr	r3, .L2+8
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+12
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+20
	ldr	r3, .L2+24
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+32
	ldr	r3, .L2+36
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r1, r3
	mov	r2, r4
	ldr	r3, [fp, #-24]
	add	r4, r3, #1
	ldr	r3, [fp, #-28]
	add	r5, r3, #1
	mov	r0, r1
	mov	r1, r2
	bl	__aeabi_d2uiz
	mov	r3, r0
	and	r1, r3, #255
	ldr	r2, .L2+40
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #4
	add	r3, r2, r3
	add	r3, r3, r5
	mov	r2, r1
	strb	r2, [r3]
	ldr	r1, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+44
	ldr	r3, .L2+48
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r2, r3
	mov	r3, r4
	mov	r0, #0
	ldr	r1, .L2+52
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+56
	ldr	r3, .L2+60
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-13]
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+44
	ldr	r3, .L2+48
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r2, r3
	mov	r3, r4
	mov	r0, #0
	ldr	r1, .L2+52
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+56
	ldr	r3, .L2+60
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-14]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+44
	ldr	r3, .L2+48
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r2, r3
	mov	r3, r4
	mov	r0, #0
	ldr	r1, .L2+52
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+56
	ldr	r3, .L2+60
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-15]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+44
	ldr	r3, .L2+48
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r2, r3
	mov	r3, r4
	mov	r0, #0
	ldr	r1, .L2+52
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+56
	ldr	r3, .L2+60
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-16]
	ldr	r1, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+52
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+72
	ldr	r3, .L2+76
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r1, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+80
	ldr	r3, .L2+84
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-17]
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+52
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+72
	ldr	r3, .L2+76
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+80
	ldr	r3, .L2+84
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-18]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+52
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+72
	ldr	r3, .L2+76
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r1, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+80
	ldr	r3, .L2+84
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-19]
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+64
	ldr	r3, .L2+68
	bl	__aeabi_dmul
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	mov	r2, #0
	ldr	r3, .L2+52
	bl	__aeabi_dadd
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+16
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+72
	ldr	r3, .L2+76
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r5, r4
	mov	r4, r3
	ldr	r3, [fp, #-24]
	add	r2, r3, #1
	ldr	r3, [fp, #-28]
	add	r1, r3, #1
	ldr	r0, .L2+28
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	mov	r0, r3
	bl	__aeabi_i2d
	ldr	r2, .L2+80
	ldr	r3, .L2+84
	bl	__aeabi_dmul
	mov	r2, r0
	mov	r3, r1
	mov	r0, r4
	mov	r1, r5
	bl	__aeabi_dsub
	mov	r3, r0
	mov	r4, r1
	mov	r0, r3
	mov	r1, r4
	bl	__aeabi_d2uiz
	mov	r3, r0
	strb	r3, [fp, #-20]
	ldr	r3, [fp, #-24]
	asr	r4, r3, #1
	ldr	r3, [fp, #-28]
	asr	r5, r3, #1
	ldrb	r3, [fp, #-16]	@ zero_extendqisi2
	ldrb	r2, [fp, #-15]	@ zero_extendqisi2
	ldrb	r1, [fp, #-14]	@ zero_extendqisi2
	ldrb	r0, [fp, #-13]	@ zero_extendqisi2
	bl	chrominance_downsample
	b	.L3
.L4:
	.align	2
.L2:
	.word	R
	.word	549755814
	.word	1070625456
	.word	1076887552
	.word	G
	.word	-1683627180
	.word	1071653060
	.word	B
	.word	721554506
	.word	1069094535
	.word	Y
	.word	-68719477
	.word	1069740457
	.word	1080033280
	.word	1992864825
	.word	1070768062
	.word	1958505087
	.word	1071388819
	.word	-549755814
	.word	1071091023
	.word	1443109011
	.word	1068641550
	.word	Cb
	.word	Cr
.L3:
	mov	r3, r0
	mov	r1, r3
	ldr	r2, .L2+88
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #3
	add	r3, r2, r3
	add	r3, r3, r5
	mov	r2, r1
	strb	r2, [r3]
	ldr	r3, [fp, #-24]
	asr	r4, r3, #1
	ldr	r3, [fp, #-28]
	asr	r5, r3, #1
	ldrb	r3, [fp, #-20]	@ zero_extendqisi2
	ldrb	r2, [fp, #-19]	@ zero_extendqisi2
	ldrb	r1, [fp, #-18]	@ zero_extendqisi2
	ldrb	r0, [fp, #-17]	@ zero_extendqisi2
	bl	chrominance_downsample
	mov	r3, r0
	mov	r1, r3
	ldr	r2, .L2+92
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #3
	add	r3, r2, r3
	add	r3, r3, r5
	mov	r2, r1
	strb	r2, [r3]
	nop
	sub	sp, fp, #12
	@ sp needed
	pop	{r4, r5, fp, lr}
	bx	lr
	.size	CSC_RGB_to_YCC_brute_force_float, .-CSC_RGB_to_YCC_brute_force_float
	.align	2
	.syntax unified
	.arm
	.fpu softvfp
	.type	CSC_RGB_to_YCC_brute_force_int, %function
CSC_RGB_to_YCC_brute_force_int:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 104
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{r4, r5, fp, lr}
	add	fp, sp, #12
	sub	sp, sp, #104
	str	r0, [fp, #-112]
	str	r1, [fp, #-116]
	ldr	r1, .L6
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r1, .L6
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-28]
	ldr	r1, .L6+4
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6+4
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-36]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r1, .L6+4
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-40]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6+4
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-44]
	ldr	r1, .L6+8
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-48]
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6+8
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-52]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r1, .L6+8
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-56]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r0, .L6+8
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	ldrb	r3, [r3]	@ zero_extendqisi2
	str	r3, [fp, #-60]
	ldr	r2, [fp, #-16]
	mov	r3, r2
	lsl	r3, r3, #5
	add	r3, r3, r2
	lsl	r3, r3, #1
	add	r1, r3, #4096
	ldr	r2, [fp, #-32]
	mov	r3, r2
	lsl	r3, r3, #7
	add	r3, r3, r2
	add	r1, r1, r3
	ldr	r2, [fp, #-48]
	mov	r3, r2
	lsl	r3, r3, #2
	add	r3, r3, r2
	lsl	r2, r3, #2
	add	r3, r3, r2
	add	r3, r1, r3
	str	r3, [fp, #-64]
	ldr	r3, [fp, #-64]
	add	r3, r3, #128
	str	r3, [fp, #-64]
	ldr	r3, [fp, #-64]
	asr	r3, r3, #8
	str	r3, [fp, #-64]
	ldr	r2, [fp, #-20]
	mov	r3, r2
	lsl	r3, r3, #5
	add	r3, r3, r2
	lsl	r3, r3, #1
	add	r1, r3, #4096
	ldr	r2, [fp, #-36]
	mov	r3, r2
	lsl	r3, r3, #7
	add	r3, r3, r2
	add	r1, r1, r3
	ldr	r2, [fp, #-52]
	mov	r3, r2
	lsl	r3, r3, #2
	add	r3, r3, r2
	lsl	r2, r3, #2
	add	r3, r3, r2
	add	r3, r1, r3
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-68]
	add	r3, r3, #128
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-68]
	asr	r3, r3, #8
	str	r3, [fp, #-68]
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #5
	add	r3, r3, r2
	lsl	r3, r3, #1
	add	r1, r3, #4096
	ldr	r2, [fp, #-40]
	mov	r3, r2
	lsl	r3, r3, #7
	add	r3, r3, r2
	add	r1, r1, r3
	ldr	r2, [fp, #-56]
	mov	r3, r2
	lsl	r3, r3, #2
	add	r3, r3, r2
	lsl	r2, r3, #2
	add	r3, r3, r2
	add	r3, r1, r3
	str	r3, [fp, #-72]
	ldr	r3, [fp, #-72]
	add	r3, r3, #128
	str	r3, [fp, #-72]
	ldr	r3, [fp, #-72]
	asr	r3, r3, #8
	str	r3, [fp, #-72]
	ldr	r2, [fp, #-28]
	mov	r3, r2
	lsl	r3, r3, #5
	add	r3, r3, r2
	lsl	r3, r3, #1
	add	r1, r3, #4096
	ldr	r2, [fp, #-44]
	mov	r3, r2
	lsl	r3, r3, #7
	add	r3, r3, r2
	add	r1, r1, r3
	ldr	r2, [fp, #-60]
	mov	r3, r2
	lsl	r3, r3, #2
	add	r3, r3, r2
	lsl	r2, r3, #2
	add	r3, r3, r2
	add	r3, r1, r3
	str	r3, [fp, #-76]
	ldr	r3, [fp, #-76]
	add	r3, r3, #128
	str	r3, [fp, #-76]
	ldr	r3, [fp, #-76]
	asr	r3, r3, #8
	str	r3, [fp, #-76]
	ldr	r3, [fp, #-64]
	and	r0, r3, #255
	ldr	r1, .L6+12
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	mov	r2, r0
	strb	r2, [r3]
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r3, [fp, #-68]
	and	ip, r3, #255
	ldr	r0, .L6+12
	ldr	r2, [fp, #-112]
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	mov	r2, ip
	strb	r2, [r3]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r3, [fp, #-72]
	and	r0, r3, #255
	ldr	r1, .L6+12
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r1, r3
	ldr	r3, [fp, #-116]
	add	r3, r2, r3
	mov	r2, r0
	strb	r2, [r3]
	ldr	r3, [fp, #-112]
	add	r2, r3, #1
	ldr	r3, [fp, #-116]
	add	r1, r3, #1
	ldr	r3, [fp, #-76]
	and	ip, r3, #255
	ldr	r0, .L6+12
	mov	r3, r2
	lsl	r3, r3, #1
	add	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r0, r3
	add	r3, r3, r1
	mov	r2, ip
	strb	r2, [r3]
	ldr	r3, [fp, #-16]
	mvn	r2, #37
	mul	r1, r2, r3
	add	r2, r1, #32768
	ldr	r3, [fp, #-32]
	mvn	r1, #73
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-48]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r1, r3
	str	r3, [fp, #-80]
	ldr	r3, [fp, #-80]
	add	r3, r3, #128
	str	r3, [fp, #-80]
	ldr	r3, [fp, #-80]
	asr	r3, r3, #8
	str	r3, [fp, #-80]
	ldr	r3, [fp, #-20]
	mvn	r2, #37
	mul	r1, r2, r3
	add	r2, r1, #32768
	ldr	r3, [fp, #-36]
	mvn	r1, #73
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-52]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r1, r3
	str	r3, [fp, #-84]
	ldr	r3, [fp, #-84]
	add	r3, r3, #128
	str	r3, [fp, #-84]
	ldr	r3, [fp, #-84]
	asr	r3, r3, #8
	str	r3, [fp, #-84]
	ldr	r3, [fp, #-24]
	mvn	r2, #37
	mul	r1, r2, r3
	add	r2, r1, #32768
	ldr	r3, [fp, #-40]
	mvn	r1, #73
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-56]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r1, r3
	str	r3, [fp, #-88]
	ldr	r3, [fp, #-88]
	add	r3, r3, #128
	str	r3, [fp, #-88]
	ldr	r3, [fp, #-88]
	asr	r3, r3, #8
	str	r3, [fp, #-88]
	ldr	r3, [fp, #-28]
	mvn	r2, #37
	mul	r1, r2, r3
	add	r2, r1, #32768
	ldr	r3, [fp, #-44]
	mvn	r1, #73
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-60]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r3, r1, r3
	str	r3, [fp, #-92]
	ldr	r3, [fp, #-92]
	add	r3, r3, #128
	str	r3, [fp, #-92]
	ldr	r3, [fp, #-92]
	asr	r3, r3, #8
	str	r3, [fp, #-92]
	ldr	r2, [fp, #-16]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r3, #32768
	ldr	r3, [fp, #-32]
	mvn	r1, #93
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-48]
	mov	r3, r2
	lsl	r3, r3, #28
	sub	r3, r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #1
	add	r3, r1, r3
	str	r3, [fp, #-96]
	ldr	r3, [fp, #-96]
	add	r3, r3, #128
	str	r3, [fp, #-96]
	ldr	r3, [fp, #-96]
	asr	r3, r3, #8
	str	r3, [fp, #-96]
	ldr	r2, [fp, #-20]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r3, #32768
	ldr	r3, [fp, #-36]
	mvn	r1, #93
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-52]
	mov	r3, r2
	lsl	r3, r3, #28
	sub	r3, r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #1
	add	r3, r1, r3
	str	r3, [fp, #-100]
	ldr	r3, [fp, #-100]
	add	r3, r3, #128
	str	r3, [fp, #-100]
	ldr	r3, [fp, #-100]
	asr	r3, r3, #8
	str	r3, [fp, #-100]
	ldr	r2, [fp, #-24]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r3, #32768
	ldr	r3, [fp, #-40]
	mvn	r1, #93
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-56]
	mov	r3, r2
	lsl	r3, r3, #28
	sub	r3, r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #1
	add	r3, r1, r3
	str	r3, [fp, #-104]
	ldr	r3, [fp, #-104]
	add	r3, r3, #128
	str	r3, [fp, #-104]
	ldr	r3, [fp, #-104]
	asr	r3, r3, #8
	str	r3, [fp, #-104]
	ldr	r2, [fp, #-28]
	mov	r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #4
	add	r2, r3, #32768
	ldr	r3, [fp, #-44]
	mvn	r1, #93
	mul	r0, r1, r3
	add	r1, r2, r0
	ldr	r2, [fp, #-60]
	mov	r3, r2
	lsl	r3, r3, #28
	sub	r3, r3, r2
	lsl	r3, r3, #3
	sub	r3, r3, r2
	lsl	r3, r3, #1
	add	r3, r1, r3
	str	r3, [fp, #-108]
	ldr	r3, [fp, #-108]
	add	r3, r3, #128
	str	r3, [fp, #-108]
	ldr	r3, [fp, #-108]
	asr	r3, r3, #8
	str	r3, [fp, #-108]
	ldr	r3, [fp, #-80]
	and	r0, r3, #255
	ldr	r3, [fp, #-84]
	and	r1, r3, #255
	ldr	r3, [fp, #-88]
	and	r2, r3, #255
	ldr	r3, [fp, #-92]
	and	ip, r3, #255
	ldr	r3, [fp, #-112]
	asr	r4, r3, #1
	ldr	r3, [fp, #-116]
	asr	r5, r3, #1
	mov	r3, ip
	bl	chrominance_downsample
	mov	r3, r0
	mov	r1, r3
	ldr	r2, .L6+16
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #3
	add	r3, r2, r3
	add	r3, r3, r5
	mov	r2, r1
	strb	r2, [r3]
	ldr	r3, [fp, #-96]
	and	r0, r3, #255
	ldr	r3, [fp, #-100]
	and	r1, r3, #255
	ldr	r3, [fp, #-104]
	and	r2, r3, #255
	ldr	r3, [fp, #-108]
	and	ip, r3, #255
	ldr	r3, [fp, #-112]
	asr	r4, r3, #1
	ldr	r3, [fp, #-116]
	asr	r5, r3, #1
	mov	r3, ip
	bl	chrominance_downsample
	mov	r3, r0
	mov	r1, r3
	ldr	r2, .L6+20
	mov	r3, r4
	lsl	r3, r3, #1
	add	r3, r3, r4
	lsl	r3, r3, #3
	add	r3, r2, r3
	add	r3, r3, r5
	mov	r2, r1
	strb	r2, [r3]
	nop
	sub	sp, fp, #12
	@ sp needed
	pop	{r4, r5, fp, lr}
	bx	lr
.L7:
	.align	2
.L6:
	.word	R
	.word	G
	.word	B
	.word	Y
	.word	Cb
	.word	Cr
	.size	CSC_RGB_to_YCC_brute_force_int, .-CSC_RGB_to_YCC_brute_force_int
	.align	2
	.syntax unified
	.arm
	.fpu softvfp
	.type	chrominance_downsample, %function
chrominance_downsample:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	@ link register save eliminated.
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	sub	sp, sp, #12
	mov	ip, r0
	mov	r0, r1
	mov	r1, r2
	mov	r2, r3
	mov	r3, ip
	strb	r3, [fp, #-5]
	mov	r3, r0
	strb	r3, [fp, #-6]
	mov	r3, r1
	strb	r3, [fp, #-7]
	mov	r3, r2
	strb	r3, [fp, #-8]
	ldrb	r3, [fp, #-5]	@ zero_extendqisi2
	mov	r0, r3
	add	sp, fp, #0
	@ sp needed
	ldr	fp, [sp], #4
	bx	lr
	.size	chrominance_downsample, .-chrominance_downsample
	.align	2
	.global	CSC_RGB_to_YCC
	.syntax unified
	.arm
	.fpu softvfp
	.type	CSC_RGB_to_YCC, %function
CSC_RGB_to_YCC:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L11
.L14:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L12
.L13:
	ldr	r1, [fp, #-12]
	ldr	r0, [fp, #-8]
	bl	CSC_RGB_to_YCC_brute_force_float
	nop
	ldr	r3, [fp, #-12]
	add	r3, r3, #2
	str	r3, [fp, #-12]
.L12:
	ldr	r3, [fp, #-12]
	cmp	r3, #47
	ble	.L13
	ldr	r3, [fp, #-8]
	add	r3, r3, #2
	str	r3, [fp, #-8]
.L11:
	ldr	r3, [fp, #-8]
	cmp	r3, #63
	ble	.L14
	nop
	sub	sp, fp, #4
	@ sp needed
	pop	{fp, lr}
	bx	lr
	.size	CSC_RGB_to_YCC, .-CSC_RGB_to_YCC
	.ident	"GCC: (SUSE Linux) 7.5.0"
