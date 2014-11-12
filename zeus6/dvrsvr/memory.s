	.arch armv5te
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 18, 4
	.file	"memory.cpp"
	.text
	.align	2
	.global	_Z8mem_sizePv
	.type	_Z8mem_sizePv, %function
_Z8mem_sizePv:
	.fnstart
.LFB119:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI1:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI2:
	.pad #16
	sub	sp, sp, #16
.LCFI3:
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L2
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L3
.L2:
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #12
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r3, r3, #8
	ldr	r2, [r3, #0]
	ldr	r3, .L6
	cmp	r2, r3
	beq	.L4
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L3
.L4:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	sub	r3, r3, #12
	str	r3, [fp, #-28]
.L3:
	ldr	r3, [fp, #-28]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L7:
	.align	2
.L6:
	.word	1176510940
.LFE119:
	.fnend
	.size	_Z8mem_sizePv, .-_Z8mem_sizePv
	.align	2
	.global	_Z12mem_refcountPv
	.type	_Z12mem_refcountPv, %function
_Z12mem_refcountPv:
	.fnstart
.LFB121:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI4:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI5:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI6:
	.pad #16
	sub	sp, sp, #16
.LCFI7:
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L9
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L10
.L9:
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #12
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r3, r3, #8
	ldr	r2, [r3, #0]
	ldr	r3, .L13
	cmp	r2, r3
	beq	.L11
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L10
.L11:
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r3, [r3, #0]
	str	r3, [fp, #-28]
.L10:
	ldr	r3, [fp, #-28]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L14:
	.align	2
.L13:
	.word	1176510940
.LFE121:
	.fnend
	.size	_Z12mem_refcountPv, .-_Z12mem_refcountPv
	.align	2
	.global	_Z9mem_checkPv
	.type	_Z9mem_checkPv, %function
_Z9mem_checkPv:
	.fnstart
.LFB122:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI8:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI9:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI10:
	.pad #8
	sub	sp, sp, #8
.LCFI11:
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L16
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L17
.L16:
	ldr	r3, [fp, #-16]
	sub	r3, r3, #4
	ldr	r2, [r3, #0]
	ldr	r3, .L19
	cmp	r2, r3
	movne	r3, #0
	moveq	r3, #1
	str	r3, [fp, #-20]
.L17:
	ldr	r3, [fp, #-20]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L20:
	.align	2
.L19:
	.word	1176510940
.LFE122:
	.fnend
	.size	_Z9mem_checkPv, .-_Z9mem_checkPv
	.align	2
	.global	_Z7mem_cpyPvPKvj
	.type	_Z7mem_cpyPvPKvj, %function
_Z7mem_cpyPvPKvj:
	.fnstart
.LFB125:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI12:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI13:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI14:
	.pad #16
	sub	sp, sp, #16
.LCFI15:
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	ldr	r2, [fp, #-24]
	bl	memcpy
	mov	r3, r0
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.LFE125:
	.fnend
	.size	_Z7mem_cpyPvPKvj, .-_Z7mem_cpyPvPKvj
	.align	2
	.global	_Z10mem_uninitv
	.type	_Z10mem_uninitv, %function
_Z10mem_uninitv:
	.fnstart
.LFB124:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI16:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI17:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI18:
	ldr	r0, .L25
	bl	pthread_mutex_destroy
	ldmfd	sp, {fp, sp, pc}
.L26:
	.align	2
.L25:
	.word	g_mem_mutex
.LFE124:
	.fnend
	.size	_Z10mem_uninitv, .-_Z10mem_uninitv
	.align	2
	.global	_Z8mem_initv
	.type	_Z8mem_initv, %function
_Z8mem_initv:
	.fnstart
.LFB123:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI19:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI20:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI21:
	ldr	r0, .L29
	mov	r1, #0
	bl	pthread_mutex_init
	ldmfd	sp, {fp, sp, pc}
.L30:
	.align	2
.L29:
	.word	g_mem_mutex
.LFE123:
	.fnend
	.size	_Z8mem_initv, .-_Z8mem_initv
	.align	2
	.global	_Z10mem_addrefPv
	.type	_Z10mem_addrefPv, %function
_Z10mem_addrefPv:
	.fnstart
.LFB120:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI22:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI23:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI24:
	.pad #16
	sub	sp, sp, #16
.LCFI25:
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L32
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L33
.L32:
	ldr	r0, .L36
	bl	pthread_mutex_lock
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #4
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r2, [r3, #0]
	ldr	r3, .L36+4
	cmp	r2, r3
	moveq	r3, #0
	movne	r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L34
	ldr	r0, .L36
	bl	pthread_mutex_unlock
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L33
.L34:
	ldr	r3, [fp, #-16]
	sub	r3, r3, #4
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	ldr	r3, [fp, #-16]
	str	r2, [r3, #0]
	ldr	r0, .L36
	bl	pthread_mutex_unlock
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-28]
.L33:
	ldr	r3, [fp, #-28]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L37:
	.align	2
.L36:
	.word	g_mem_mutex
	.word	1176510940
.LFE120:
	.fnend
	.size	_Z10mem_addrefPv, .-_Z10mem_addrefPv
	.align	2
	.global	_Z8mem_freePv
	.type	_Z8mem_freePv, %function
_Z8mem_freePv:
	.fnstart
.LFB118:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI26:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI27:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI28:
	.pad #16
	sub	sp, sp, #16
.LCFI29:
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L42
	ldr	r0, .L43
	bl	pthread_mutex_lock
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	sub	r3, r3, #12
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r3, r3, #8
	ldr	r2, [r3, #0]
	ldr	r3, .L43+4
	cmp	r2, r3
	beq	.L40
	ldr	r0, .L43
	bl	pthread_mutex_unlock
	b	.L42
.L40:
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r2, [r3, #0]
	sub	r2, r2, #1
	str	r2, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	movgt	r3, #0
	movle	r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L41
	ldr	r3, [fp, #-16]
	add	r2, r3, #8
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r0, [fp, #-16]
	bl	free
.L41:
	ldr	r0, .L43
	bl	pthread_mutex_unlock
.L42:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L44:
	.align	2
.L43:
	.word	g_mem_mutex
	.word	1176510940
.LFE118:
	.fnend
	.size	_Z8mem_freePv, .-_Z8mem_freePv
	.section	.rodata
	.align	2
.LC0:
	.ascii	"!!!!!mem_alloc failed!\000"
	.text
	.align	2
	.global	_Z9mem_alloci
	.type	_Z9mem_alloci, %function
_Z9mem_alloci:
	.fnstart
.LFB117:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI30:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI31:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI32:
	.pad #16
	sub	sp, sp, #16
.LCFI33:
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bge	.L46
	mov	r3, #0
	str	r3, [fp, #-24]
.L46:
	ldr	r3, [fp, #-24]
	add	r3, r3, #12
	mov	r0, r3
	bl	malloc
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L47
	ldr	r0, .L50
	bl	_Z7dvr_logPKcz
	mov	r3, #0
	str	r3, [fp, #-28]
	b	.L48
.L47:
	ldr	r3, [fp, #-24]
	add	r2, r3, #12
	ldr	r3, [fp, #-16]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-16]
	add	r2, r3, #4
	mov	r3, #1
	str	r3, [r2, #0]
	ldr	r3, [fp, #-16]
	add	r2, r3, #8
	ldr	r3, .L50+4
	str	r3, [r2, #0]
	ldr	r3, [fp, #-16]
	add	r3, r3, #12
	str	r3, [fp, #-28]
.L48:
	ldr	r3, [fp, #-28]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L51:
	.align	2
.L50:
	.word	.LC0
	.word	1176510940
.LFE117:
	.fnend
	.size	_Z9mem_alloci, .-_Z9mem_alloci
	.section	.rodata
	.align	2
.LC1:
	.ascii	"/proc/sys/vm/drop_caches\000"
	.align	2
.LC2:
	.ascii	"w\000"
	.align	2
.LC3:
	.ascii	"1\000"
	.text
	.align	2
	.global	_Z14mem_dropcachesv
	.type	_Z14mem_dropcachesv, %function
_Z14mem_dropcachesv:
	.fnstart
.LFB115:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI34:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI35:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI36:
	.pad #8
	sub	sp, sp, #8
.LCFI37:
	ldr	r0, .L55
	ldr	r1, .L55+4
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L54
	ldr	r0, .L55+8
	mov	r1, #1
	mov	r2, #1
	ldr	r3, [fp, #-16]
	bl	fwrite
	ldr	r0, [fp, #-16]
	bl	fclose
.L54:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L56:
	.align	2
.L55:
	.word	.LC1
	.word	.LC2
	.word	.LC3
.LFE115:
	.fnend
	.size	_Z14mem_dropcachesv, .-_Z14mem_dropcachesv
	.section	.rodata
	.align	2
.LC4:
	.ascii	"/proc/meminfo\000"
	.align	2
.LC5:
	.ascii	"r\000"
	.align	2
.LC6:
	.ascii	"MemTotal:\000"
	.align	2
.LC7:
	.ascii	"%d\000"
	.align	2
.LC8:
	.ascii	"MemFree:\000"
	.align	2
.LC9:
	.ascii	"Cached:\000"
	.align	2
.LC10:
	.ascii	"Buffers:\000"
	.align	2
.LC11:
	.ascii	"SwapTotal:\000"
	.align	2
.LC12:
	.ascii	"SwapFree:\000"
	.align	2
.LC13:
	.ascii	"Inactive:\000"
	.text
	.align	2
	.type	_ZL12ParseMeminfov, %function
_ZL12ParseMeminfov:
	.fnstart
.LFB114:
	@ args = 0, pretend = 0, frame = 272
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI38:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI39:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI40:
	.pad #272
	sub	sp, sp, #272
.LCFI41:
	mov	r3, #0
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r0, .L69
	ldr	r1, .L69+4
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bne	.L60
	mov	r3, #0
	str	r3, [fp, #-280]
	b	.L59
.L67:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+8
	mov	r2, #9
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L61
	sub	r3, fp, #276
	add	r3, r3, #9
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+16
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L61:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+20
	mov	r2, #8
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L62
	sub	r3, fp, #276
	add	r3, r3, #8
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+24
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L62:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+28
	mov	r2, #7
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L63
	sub	r3, fp, #276
	add	r3, r3, #7
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+32
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L63:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+36
	mov	r2, #8
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L64
	sub	r3, fp, #276
	add	r3, r3, #8
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+40
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L64:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+44
	mov	r2, #10
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L65
	sub	r3, fp, #276
	add	r3, r3, #10
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+48
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L65:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+52
	mov	r2, #9
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L66
	sub	r3, fp, #276
	add	r3, r3, #9
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+56
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
	b	.L60
.L66:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, .L69+60
	mov	r2, #9
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L60
	sub	r3, fp, #276
	add	r3, r3, #9
	mov	r0, r3
	ldr	r1, .L69+12
	ldr	r2, .L69+64
	bl	sscanf
	mov	r2, r0
	ldr	r3, [fp, #-16]
	add	r3, r3, r2
	str	r3, [fp, #-16]
.L60:
	sub	r3, fp, #276
	mov	r0, r3
	mov	r1, #256
	ldr	r2, [fp, #-20]
	bl	fgets
	mov	r3, r0
	cmp	r3, #0
	moveq	r3, #0
	movne	r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	bne	.L67
	ldr	r0, [fp, #-20]
	bl	fclose
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-280]
.L59:
	ldr	r3, [fp, #-280]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L70:
	.align	2
.L69:
	.word	.LC4
	.word	.LC5
	.word	.LC6
	.word	.LC7
	.word	_ZL8MemTotal
	.word	.LC8
	.word	_ZL7MemFree
	.word	.LC9
	.word	_ZL6Cached
	.word	.LC10
	.word	_ZL7Buffers
	.word	.LC11
	.word	_ZL9SwapTotal
	.word	.LC12
	.word	_ZL8SwapFree
	.word	.LC13
	.word	_ZL8Inactive
.LFE114:
	.fnend
	.size	_ZL12ParseMeminfov, .-_ZL12ParseMeminfov
	.align	2
	.global	_Z13mem_availablev
	.type	_Z13mem_availablev, %function
_Z13mem_availablev:
	.fnstart
.LFB116:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	.movsp ip
	mov	ip, sp
.LCFI42:
	stmfd	sp!, {fp, ip, lr, pc}
	.pad #4
	.save {fp, ip, lr}
.LCFI43:
	.setfp fp, ip, #-4
	sub	fp, ip, #4
.LCFI44:
	.pad #16
	sub	sp, sp, #16
.LCFI45:
	bl	_ZL12ParseMeminfov
	mov	r3, r0
	cmp	r3, #0
	movne	r3, #0
	moveq	r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L72
	mov	r3, #0
	str	r3, [fp, #-24]
	b	.L73
.L72:
	ldr	r3, .L75
	ldr	r2, [r3, #0]
	ldr	r3, .L75+4
	ldr	r3, [r3, #0]
	add	r3, r2, r3
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-24]
.L73:
	ldr	r3, [fp, #-24]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L76:
	.align	2
.L75:
	.word	_ZL7MemFree
	.word	_ZL8Inactive
.LFE116:
	.fnend
	.size	_Z13mem_availablev, .-_Z13mem_availablev
	.global	g_mem_mutex
	.bss
	.align	2
	.type	g_mem_mutex, %object
	.size	g_mem_mutex, 24
g_mem_mutex:
	.space	24
	.local	_ZL7MemFree
	.comm	_ZL7MemFree,4,4
	.local	_ZL8Inactive
	.comm	_ZL8Inactive,4,4
	.local	_ZL8MemTotal
	.comm	_ZL8MemTotal,4,4
	.local	_ZL6Cached
	.comm	_ZL6Cached,4,4
	.local	_ZL7Buffers
	.comm	_ZL7Buffers,4,4
	.local	_ZL9SwapTotal
	.comm	_ZL9SwapTotal,4,4
	.local	_ZL8SwapFree
	.comm	_ZL8SwapFree,4,4
	.ident	"GCC: (Buildroot 2010.11) 4.3.5"
	.section	.note.GNU-stack,"",%progbits
