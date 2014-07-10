	.file	"ioprocess.cpp"
	.global	standbyhdoff
	.bss
	.align	2
	.type	standbyhdoff, %object
	.size	standbyhdoff, 4
standbyhdoff:
	.space	4
	.global	usewatchdog
	.align	2
	.type	usewatchdog, %object
	.size	usewatchdog, 4
usewatchdog:
	.space	4
	.global	input_map_table
	.data
	.align	2
	.type	input_map_table, %object
	.size	input_map_table, 48
input_map_table:
	.word	1
	.word	2
	.word	4
	.word	8
	.word	16
	.word	32
	.word	1024
	.word	2048
	.word	4096
	.word	8192
	.word	16384
	.word	32768
	.global	output_map_table
	.align	2
	.type	output_map_table, %object
	.size	output_map_table, 32
output_map_table:
	.word	1
	.word	2
	.word	256
	.word	8
	.word	16
	.word	4
	.word	64
	.word	128
	.global	hdlock
	.bss
	.align	2
	.type	hdlock, %object
	.size	hdlock, 4
hdlock:
	.space	4
	.global	hdinserted
	.align	2
	.type	hdinserted, %object
	.size	hdinserted, 4
hdinserted:
	.space	4
	.global	baud_table
	.data
	.align	2
	.type	baud_table, %object
	.size	baud_table, 56
baud_table:
	.word	11
	.word	2400
	.word	12
	.word	4800
	.word	13
	.word	9600
	.word	14
	.word	19200
	.word	15
	.word	38400
	.word	4097
	.word	57600
	.word	4098
	.word	115200
	.global	p_dio_mmap
	.bss
	.align	2
	.type	p_dio_mmap, %object
	.size	p_dio_mmap, 4
p_dio_mmap:
	.space	4
	.global	dvriomap
	.data
	.align	2
	.type	dvriomap, %object
	.size	dvriomap, 256
dvriomap:
	.ascii	"/var/dvr/dvriomap\000"
	.space	238
	.global	serial_dev
	.align	2
	.type	serial_dev, %object
	.size	serial_dev, 256
serial_dev:
	.ascii	"/dev/ttyS1\000"
	.space	245
	.section	.rodata
	.align	2
.LC0:
	.ascii	"/var/dvr/ioprocess.pid\000"
	.global	pidfile
	.data
	.align	2
	.type	pidfile, %object
	.size	pidfile, 4
pidfile:
	.word	.LC0
	.global	serial_handle
	.bss
	.align	2
	.type	serial_handle, %object
	.size	serial_handle, 4
serial_handle:
	.space	4
	.global	serial_baud
	.data
	.align	2
	.type	serial_baud, %object
	.size	serial_baud, 4
serial_baud:
	.word	115200
	.global	mcupowerdelaytime
	.bss
	.align	2
	.type	mcupowerdelaytime, %object
	.size	mcupowerdelaytime, 4
mcupowerdelaytime:
	.space	4
	.global	mcu_firmware_version
	.type	mcu_firmware_version, %object
	.size	mcu_firmware_version, 80
mcu_firmware_version:
	.space	80
	.global	dvrconfigfile
	.data
	.align	2
	.type	dvrconfigfile, %object
	.size	dvrconfigfile, 18
dvrconfigfile:
	.ascii	"/etc/dvr/dvr.conf\000"
	.global	logfile
	.align	2
	.type	logfile, %object
	.size	logfile, 128
logfile:
	.ascii	"dvrlog.txt\000"
	.space	117
	.global	temp_logfile
	.bss
	.type	temp_logfile, %object
	.size	temp_logfile, 128
temp_logfile:
	.space	128
	.global	watchdogenabled
	.align	2
	.type	watchdogenabled, %object
	.size	watchdogenabled, 4
watchdogenabled:
	.space	4
	.global	watchdogtimeout
	.data
	.align	2
	.type	watchdogtimeout, %object
	.size	watchdogtimeout, 4
watchdogtimeout:
	.word	30
	.global	gpsvalid
	.bss
	.align	2
	.type	gpsvalid, %object
	.size	gpsvalid, 4
gpsvalid:
	.space	4
	.global	gforce_log_enable
	.align	2
	.type	gforce_log_enable, %object
	.size	gforce_log_enable, 4
gforce_log_enable:
	.space	4
	.global	output_inverted
	.align	2
	.type	output_inverted, %object
	.size	output_inverted, 4
output_inverted:
	.space	4
	.global	app_mode
	.align	2
	.type	app_mode, %object
	.size	app_mode, 4
app_mode:
	.space	4
	.global	panelled
	.align	2
	.type	panelled, %object
	.size	panelled, 4
panelled:
	.space	4
	.global	devicepower
	.data
	.align	2
	.type	devicepower, %object
	.size	devicepower, 4
devicepower:
	.word	65535
	.global	pid_smartftp
	.bss
	.align	2
	.type	pid_smartftp, %object
	.size	pid_smartftp, 4
pid_smartftp:
	.space	4
	.text
	.align	2
	.global	_Z11sig_handleri
	.type	_Z11sig_handleri, %function
_Z11sig_handleri:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #12
	bne	.L2
	ldr	r2, .L4
	mov	r3, #8
	str	r3, [r2, #0]
	b	.L1
.L2:
	ldr	r2, .L4
	mov	r3, #0
	str	r3, [r2, #0]
.L1:
	ldmib	sp, {fp, sp, pc}
.L5:
	.align	2
.L4:
	.word	app_mode
	.size	_Z11sig_handleri, .-_Z11sig_handleri
	.align	2
	.global	_Z6getbcdh
	.type	_Z6getbcdh, %function
_Z6getbcdh:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	mov	r3, r0
	strb	r3, [fp, #-13]
	ldrb	r3, [fp, #-13]	@ zero_extendqisi2
	mov	r3, r3, lsr #4
	and	r3, r3, #255
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-20]
	add	r3, r3, r2
	mov	r2, r3, asl #1
	ldrb	r3, [fp, #-13]	@ zero_extendqisi2
	and	r3, r3, #15
	and	r3, r3, #255
	add	r2, r2, r3
	str	r2, [fp, #-20]
	ldr	r3, [fp, #-20]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z6getbcdh, .-_Z6getbcdh
	.data
	.align	2
	.type	_ZZ7dvr_logPczE11logfilename, %object
	.size	_ZZ7dvr_logPczE11logfilename, 512
_ZZ7dvr_logPczE11logfilename:
	.ascii	"\000"
	.space	511
	.section	.rodata
	.align	2
.LC1:
	.ascii	"/var/dvr/dvrcurdisk\000"
	.align	2
.LC2:
	.ascii	"r\000"
	.align	2
.LC3:
	.ascii	"%s\000"
	.align	2
.LC4:
	.ascii	"%s/_%s_/%s\000"
	.align	2
.LC5:
	.ascii	"a\000"
	.align	2
.LC6:
	.ascii	"%s:IO:\000"
	.align	2
.LC7:
	.ascii	"\n\000"
	.align	2
.LC8:
	.ascii	" *\n\000"
	.text
	.align	2
	.global	_Z7dvr_logPcz
	.type	_Z7dvr_logPcz, %function
_Z7dvr_logPcz:
	@ args = 4, pretend = 16, frame = 660
	@ frame_needed = 1, uses_anonymous_args = 1
	mov	ip, sp
	stmfd	sp!, {r0, r1, r2, r3}
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #20
	sub	sp, sp, #664
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r0, .L16
	ldr	r1, .L16+4
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-660]
	ldr	r3, [fp, #-660]
	cmp	r3, #0
	beq	.L8
	sub	r3, fp, #528
	ldr	r0, [fp, #-660]
	ldr	r1, .L16+8
	mov	r2, r3
	bl	fscanf
	mov	r3, r0
	cmp	r3, #1
	bne	.L9
	sub	r3, fp, #656
	mov	r0, r3
	mov	r1, #128
	bl	gethostname
	sub	r2, fp, #528
	sub	ip, fp, #656
	ldr	r3, .L16+12
	str	r3, [sp, #0]
	ldr	r0, .L16+16
	ldr	r1, .L16+20
	mov	r3, ip
	bl	sprintf
.L9:
	ldr	r0, [fp, #-660]
	bl	fclose
.L8:
	mov	r0, #0
	bl	time
	mov	r3, r0
	str	r3, [fp, #-664]
	sub	r3, fp, #664
	sub	r2, fp, #528
	mov	r0, r3
	mov	r1, r2
	bl	ctime_r
	sub	r3, fp, #528
	mov	r0, r3
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-668]
	mvn	r2, #516
	ldr	r3, [fp, #-668]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #10
	bne	.L10
	mvn	r2, #516
	ldr	r3, [fp, #-668]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r2, r3, r2
	mov	r3, #0
	strb	r3, [r2, #0]
.L10:
	ldr	r0, .L16+16
	ldr	r1, .L16+24
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-660]
	ldr	r3, [fp, #-660]
	cmp	r3, #0
	beq	.L11
	sub	r3, fp, #528
	ldr	r0, [fp, #-660]
	ldr	r1, .L16+28
	mov	r2, r3
	bl	fprintf
	add	r3, fp, #8
	str	r3, [fp, #-672]
	ldr	r0, [fp, #-660]
	ldr	r1, [fp, #4]
	ldr	r2, [fp, #-672]
	bl	vfprintf
	ldr	r0, [fp, #-660]
	ldr	r1, .L16+32
	bl	fprintf
	ldr	r0, [fp, #-660]
	bl	fclose
	mov	r3, r0
	cmp	r3, #0
	bne	.L11
	mov	r3, #1
	str	r3, [fp, #-16]
.L11:
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L13
	ldr	r0, .L16+36
	ldr	r1, .L16+24
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-660]
	ldr	r3, [fp, #-660]
	cmp	r3, #0
	beq	.L13
	sub	r3, fp, #528
	ldr	r0, [fp, #-660]
	ldr	r1, .L16+28
	mov	r2, r3
	bl	fprintf
	add	r3, fp, #8
	str	r3, [fp, #-672]
	ldr	r0, [fp, #-660]
	ldr	r1, [fp, #4]
	ldr	r2, [fp, #-672]
	bl	vfprintf
	ldr	r0, [fp, #-660]
	ldr	r1, .L16+40
	bl	fprintf
	ldr	r0, [fp, #-660]
	bl	fclose
	mov	r3, r0
	cmp	r3, #0
	bne	.L13
	mov	r3, #1
	str	r3, [fp, #-16]
.L13:
	ldr	r3, [fp, #-16]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L17:
	.align	2
.L16:
	.word	.LC1
	.word	.LC2
	.word	.LC3
	.word	logfile
	.word	_ZZ7dvr_logPczE11logfilename
	.word	.LC4
	.word	.LC5
	.word	.LC6
	.word	.LC7
	.word	temp_logfile
	.word	.LC8
	.size	_Z7dvr_logPcz, .-_Z7dvr_logPcz
	.global	_Unwind_SjLj_Resume
	.global	__gxx_personality_sj0
	.global	_Unwind_SjLj_Register
	.global	_Unwind_SjLj_Unregister
	.section	.rodata
	.align	2
.LC9:
	.ascii	"system\000"
	.align	2
.LC10:
	.ascii	"shutdowndelay\000"
	.text
	.align	2
	.global	_Z20getshutdowndelaytimev
	.type	_Z20getshutdowndelaytimev, %function
_Z20getshutdowndelaytimev:
	@ args = 0, pretend = 0, frame = 96
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #96
	ldr	r3, .L26
	str	r3, [fp, #-96]
	ldr	r3, .L26+4
	str	r3, [fp, #-92]
	sub	r3, fp, #88
	sub	r2, fp, #40
	str	r2, [r3, #0]
	ldr	r2, .L26+8
	str	r2, [r3, #4]
	str	sp, [r3, #8]
	sub	r3, fp, #120
	mov	r0, r3
	bl	_Unwind_SjLj_Register
	sub	r2, fp, #64
	mvn	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r1, .L26+12
	bl	_ZN6configC1EPc
	sub	r2, fp, #64
	mov	r3, #1
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r1, .L26+16
	ldr	r2, .L26+20
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-68]
	cmp	r3, #9
	bgt	.L19
	mov	r3, #10
	str	r3, [fp, #-68]
.L19:
	ldr	r2, [fp, #-68]
	mov	r3, #7168
	add	r3, r3, #32
	cmp	r2, r3
	ble	.L20
	mov	r3, #7168
	add	r3, r3, #32
	str	r3, [fp, #-68]
.L20:
	ldr	r3, [fp, #-68]
	str	r3, [fp, #-128]
	sub	r2, fp, #64
	mvn	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r3, .L26+24
	mov	lr, pc
	mov	pc, r3
	ldr	r2, [fp, #-128]
	str	r2, [fp, #-124]
	b	.L18
.L25:
	add	fp, fp, #40
	ldr	r3, [fp, #-112]
	str	r3, [fp, #-136]
.L21:
	ldr	r2, [fp, #-136]
	str	r2, [fp, #-132]
	sub	r2, fp, #64
	mov	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r3, .L26+24
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-132]
	str	r3, [fp, #-136]
.L23:
	mvn	r3, #0
	str	r3, [fp, #-116]
	ldr	r0, [fp, #-136]
	bl	_Unwind_SjLj_Resume
.L18:
	sub	r3, fp, #120
	mov	r0, r3
	bl	_Unwind_SjLj_Unregister
	ldr	r0, [fp, #-124]
	sub	sp, fp, #40
	ldmfd	sp, {r4, r5, r6, r7, r8, r9, sl, fp, sp, pc}
.L27:
	.align	2
.L26:
	.word	__gxx_personality_sj0
	.word	.LLSDA64
	.word	.L25
	.word	dvrconfigfile
	.word	.LC9
	.word	.LC10
	.word	_ZN6configD1Ev
	.size	_Z20getshutdowndelaytimev, .-_Z20getshutdowndelaytimev
	.section	.gcc_except_table,"a",%progbits
.LLSDA64:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE64-.LLSDACSB64
.LLSDACSB64:
	.uleb128 0x0
	.uleb128 0x0
.LLSDACSE64:
	.text
	.section	.gnu.linkonce.t._ZN6configD1Ev,"ax",%progbits
	.align	2
	.weak	_ZN6configD1Ev
	.type	_ZN6configD1Ev, %function
_ZN6configD1Ev:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
.L30:
	ldr	r3, [fp, #-16]
	add	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L37
	mov	lr, pc
	mov	pc, r3
.L32:
	ldr	r3, [fp, #-16]
	add	r3, r3, #12
	mov	r0, r3
	ldr	r3, .L37
	mov	lr, pc
	mov	pc, r3
.L34:
	ldr	r0, [fp, #-16]
	bl	_ZN5arrayI6stringED1Ev
	ldmib	sp, {fp, sp, pc}
.L38:
	.align	2
.L37:
	.word	_ZN6stringD1Ev
.L28:
	.size	_ZN6configD1Ev, .-_ZN6configD1Ev
	.section	.gnu.linkonce.t._ZN6stringD1Ev,"ax",%progbits
	.align	2
	.weak	_ZN6stringD1Ev
	.type	_ZN6stringD1Ev, %function
_ZN6stringD1Ev:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L39
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	_ZdlPv
.L39:
	ldmib	sp, {fp, sp, pc}
	.size	_ZN6stringD1Ev, .-_ZN6stringD1Ev
	.section	.rodata
	.align	2
.LC12:
	.ascii	"standbytime\000"
	.text
	.align	2
	.global	_Z14getstandbytimev
	.type	_Z14getstandbytimev, %function
_Z14getstandbytimev:
	@ args = 0, pretend = 0, frame = 96
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #96
	ldr	r3, .L51
	str	r3, [fp, #-96]
	ldr	r3, .L51+4
	str	r3, [fp, #-92]
	sub	r3, fp, #88
	sub	r2, fp, #40
	str	r2, [r3, #0]
	ldr	r2, .L51+8
	str	r2, [r3, #4]
	str	sp, [r3, #8]
	sub	r3, fp, #120
	mov	r0, r3
	bl	_Unwind_SjLj_Register
	sub	r2, fp, #64
	mvn	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r1, .L51+12
	bl	_ZN6configC1EPc
	sub	r2, fp, #64
	mov	r3, #1
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r1, .L51+16
	ldr	r2, .L51+20
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-68]
	cmp	r3, #9
	bgt	.L44
	mov	r3, #10
	str	r3, [fp, #-68]
.L44:
	ldr	r2, [fp, #-68]
	mov	r3, #43008
	add	r3, r3, #192
	cmp	r2, r3
	ble	.L45
	mov	r3, #43008
	add	r3, r3, #192
	str	r3, [fp, #-68]
.L45:
	ldr	r3, [fp, #-68]
	str	r3, [fp, #-128]
	sub	r2, fp, #64
	mvn	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r3, .L51+24
	mov	lr, pc
	mov	pc, r3
	ldr	r2, [fp, #-128]
	str	r2, [fp, #-124]
	b	.L43
.L50:
	add	fp, fp, #40
	ldr	r3, [fp, #-112]
	str	r3, [fp, #-136]
.L46:
	ldr	r2, [fp, #-136]
	str	r2, [fp, #-132]
	sub	r2, fp, #64
	mov	r3, #0
	str	r3, [fp, #-116]
	mov	r0, r2
	ldr	r3, .L51+24
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-132]
	str	r3, [fp, #-136]
.L48:
	mvn	r3, #0
	str	r3, [fp, #-116]
	ldr	r0, [fp, #-136]
	bl	_Unwind_SjLj_Resume
.L43:
	sub	r3, fp, #120
	mov	r0, r3
	bl	_Unwind_SjLj_Unregister
	ldr	r0, [fp, #-124]
	sub	sp, fp, #40
	ldmfd	sp, {r4, r5, r6, r7, r8, r9, sl, fp, sp, pc}
.L52:
	.align	2
.L51:
	.word	__gxx_personality_sj0
	.word	.LLSDA68
	.word	.L50
	.word	dvrconfigfile
	.word	.LC9
	.word	.LC12
	.word	_ZN6configD1Ev
	.size	_Z14getstandbytimev, .-_Z14getstandbytimev
	.section	.gcc_except_table
.LLSDA68:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE68-.LLSDACSB68
.LLSDACSB68:
	.uleb128 0x0
	.uleb128 0x0
.LLSDACSE68:
	.text
	.local	starttime
	.comm	starttime,4,4
	.align	2
	.global	_Z11initruntimev
	.type	_Z11initruntimev, %function
_Z11initruntimev:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	ldr	r2, .L54
	ldr	r3, [fp, #-20]
	str	r3, [r2, #0]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L55:
	.align	2
.L54:
	.word	starttime
	.size	_Z11initruntimev, .-_Z11initruntimev
	.align	2
	.global	_Z10getruntimev
	.type	_Z10getruntimev, %function
_Z10getruntimev:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	ldr	r3, .L57
	ldr	r2, [fp, #-20]
	ldr	r3, [r3, #0]
	rsb	r2, r3, r2
	str	r2, [fp, #-24]
	ldr	r3, [fp, #-24]
	mov	r3, r3, asl #5
	ldr	r2, [fp, #-24]
	rsb	r3, r2, r3
	mov	r3, r3, asl #2
	ldr	r2, [fp, #-24]
	add	r3, r3, r2
	mov	r0, r3, asl #3
	ldr	r1, [fp, #-16]
	ldr	r3, .L57+4
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #6
	mov	r3, r1, asr #31
	rsb	r3, r3, r2
	add	r0, r0, r3
	str	r0, [fp, #-24]
	ldr	r3, [fp, #-24]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L58:
	.align	2
.L57:
	.word	starttime
	.word	274877907
	.size	_Z10getruntimev, .-_Z10getruntimev
	.align	2
	.global	_Z11atomic_swapPii
	.type	_Z11atomic_swapPii, %function
_Z11atomic_swapPii:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-20]
#APP
	swp  r3, r3, [r2]
	
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z11atomic_swapPii, .-_Z11atomic_swapPii
	.align	2
	.global	_Z8dio_lockv
	.type	_Z8dio_lockv, %function
_Z8dio_lockv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
.L61:
	ldr	r3, .L63
	ldr	r3, [r3, #0]
	add	r3, r3, #4
	mov	r0, r3
	mov	r1, #1
	bl	_Z11atomic_swapPii
	mov	r3, r0
	cmp	r3, #0
	beq	.L60
	bl	sched_yield
	b	.L61
.L60:
	ldmfd	sp, {fp, sp, pc}
.L64:
	.align	2
.L63:
	.word	p_dio_mmap
	.size	_Z8dio_lockv, .-_Z8dio_lockv
	.align	2
	.global	_Z10dio_unlockv
	.type	_Z10dio_unlockv, %function
_Z10dio_unlockv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r3, .L66
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #4]
	ldmfd	sp, {fp, sp, pc}
.L67:
	.align	2
.L66:
	.word	p_dio_mmap
	.size	_Z10dio_unlockv, .-_Z10dio_unlockv
	.section	.rodata
	.align	2
.LC14:
	.ascii	"/dev/rtc\000"
	.text
	.align	2
	.global	_Z7rtc_setl
	.type	_Z7rtc_setl, %function
_Z7rtc_setl:
	@ args = 0, pretend = 0, frame = 52
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #52
	str	r0, [fp, #-16]
	ldr	r0, .L70
	mov	r1, #1
	bl	open
	mov	r3, r0
	str	r3, [fp, #-64]
	ldr	r3, [fp, #-64]
	cmp	r3, #0
	ble	.L68
	sub	r3, fp, #16
	sub	r2, fp, #60
	mov	r0, r3
	mov	r1, r2
	bl	gmtime_r
	sub	r3, fp, #60
	ldr	r0, [fp, #-64]
	mov	r1, #1073741834
	add	r1, r1, #2375680
	add	r1, r1, #12288
	mov	r2, r3
	bl	ioctl
	ldr	r0, [fp, #-64]
	bl	close
.L68:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L71:
	.align	2
.L70:
	.word	.LC14
	.size	_Z7rtc_setl, .-_Z7rtc_setl
	.align	2
	.global	_Z16serial_datareadyi
	.type	_Z16serial_datareadyi, %function
_Z16serial_datareadyi:
	@ args = 0, pretend = 0, frame = 152
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #156
	str	r0, [fp, #-16]
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bgt	.L73
	mov	r0, #0
	str	r0, [fp, #-164]
	b	.L72
.L73:
	mvn	r3, #11
	sub	r1, fp, #12
	add	r0, r1, r3
	ldr	r1, [fp, #-16]
	ldr	r3, .L80+4
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #18
	mov	r3, r1, asr #31
	rsb	r3, r3, r2
	str	r3, [r0, #0]
	mvn	r3, #11
	sub	r0, fp, #12
	add	ip, r0, r3
	ldr	r0, [fp, #-16]
	ldr	r3, .L80+4
	smull	r1, r3, r0, r3
	mov	r2, r3, asr #18
	mov	r3, r0, asr #31
	rsb	r1, r3, r2
	mov	r2, r1
	mov	r2, r2, asl #5
	rsb	r2, r1, r2
	mov	r3, r2, asl #6
	rsb	r3, r2, r3
	mov	r3, r3, asl #3
	add	r3, r3, r1
	mov	r3, r3, asl #6
	rsb	r3, r3, r0
	str	r3, [ip, #4]
	sub	r3, fp, #152
	str	r3, [fp, #-160]
	mov	r3, #0
	str	r3, [fp, #-156]
.L75:
	ldr	r3, [fp, #-156]
	cmp	r3, #31
	bhi	.L74
	ldr	r1, [fp, #-160]
	ldr	r2, [fp, #-156]
	mov	r3, #0
	str	r3, [r1, r2, asl #2]
	ldr	r3, [fp, #-156]
	add	r3, r3, #1
	str	r3, [fp, #-156]
	b	.L75
.L74:
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	mov	r2, r3, lsr #5
	mvn	r1, #139
	mov	r3, r2
	mov	r3, r3, asl #2
	sub	r0, fp, #12
	add	r3, r3, r0
	add	r0, r3, r1
	mvn	r1, #139
	mov	r3, r2
	mov	r3, r3, asl #2
	sub	r2, fp, #12
	add	r3, r3, r2
	add	r1, r3, r1
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	and	r2, r3, #31
	mov	r3, #1
	mov	r2, r3, asl r2
	ldr	r3, [r1, #0]
	orr	r3, r3, r2
	str	r3, [r0, #0]
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	add	r2, r3, #1
	sub	r1, fp, #152
	sub	r3, fp, #24
	str	r3, [sp, #0]
	mov	r0, r2
	mov	r2, #0
	mov	r3, #0
	bl	select
	mov	r3, r0
	cmp	r3, #0
	ble	.L78
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #5
	mvn	r2, #139
	mov	r3, r3, asl #2
	sub	r0, fp, #12
	add	r3, r3, r0
	add	r1, r3, r2
	ldr	r3, .L80
	ldr	r3, [r3, #0]
	and	r2, r3, #31
	ldr	r3, [r1, #0]
	mov	r3, r3, asr r2
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [fp, #-164]
	b	.L72
.L78:
	mov	r1, #0
	str	r1, [fp, #-164]
.L72:
	ldr	r0, [fp, #-164]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L81:
	.align	2
.L80:
	.word	serial_handle
	.word	1125899907
	.size	_Z16serial_datareadyi, .-_Z16serial_datareadyi
	.align	2
	.global	_Z11serial_readPvj
	.type	_Z11serial_readPvj, %function
_Z11serial_readPvj:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L84
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L83
	ldr	r3, .L84
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-20]
	bl	read
	mov	r3, r0
	str	r3, [fp, #-24]
	b	.L82
.L83:
	mov	r3, #0
	str	r3, [fp, #-24]
.L82:
	ldr	r0, [fp, #-24]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L85:
	.align	2
.L84:
	.word	serial_handle
	.size	_Z11serial_readPvj, .-_Z11serial_readPvj
	.align	2
	.global	_Z12serial_writePvj
	.type	_Z12serial_writePvj, %function
_Z12serial_writePvj:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L88
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L87
	ldr	r3, .L88
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-16]
	ldr	r2, [fp, #-20]
	bl	write
	mov	r3, r0
	str	r3, [fp, #-24]
	b	.L86
.L87:
	mov	r3, #0
	str	r3, [fp, #-24]
.L86:
	ldr	r0, [fp, #-24]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L89:
	.align	2
.L88:
	.word	serial_handle
	.size	_Z12serial_writePvj, .-_Z12serial_writePvj
	.align	2
	.global	_Z12serial_cleari
	.type	_Z12serial_cleari, %function
_Z12serial_cleari:
	@ args = 0, pretend = 0, frame = 1008
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #1008
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
.L91:
	ldr	r2, [fp, #-20]
	mov	r3, #996
	add	r3, r3, #3
	cmp	r2, r3
	bgt	.L90
	ldr	r0, [fp, #-16]
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L90
	sub	r3, fp, #1020
	mov	r0, r3
	mov	r1, #1000
	bl	_Z11serial_readPvj
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
	b	.L91
.L90:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z12serial_cleari, .-_Z12serial_cleari
	.section	.rodata
	.align	2
.LC15:
	.ascii	"/dev/ttyS1\000"
	.align	2
.LC16:
	.ascii	"Serial port failed!\n\000"
	.text
	.align	2
	.global	_Z11serial_initv
	.type	_Z11serial_initv, %function
_Z11serial_initv:
	@ args = 0, pretend = 0, frame = 72
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #80
	mov	r3, #0
	str	r3, [fp, #-20]
	ldr	r3, .L107
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L97
	ldr	r3, .L107
	ldr	r0, [r3, #0]
	bl	close
	ldr	r2, .L107
	mov	r3, #0
	str	r3, [r2, #0]
.L97:
	ldr	r0, .L107+4
	ldr	r1, .L107+8
	bl	strcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L98
	mov	r3, #1
	str	r3, [fp, #-20]
.L98:
	ldr	r0, .L107+4
	mov	r1, #2304
	add	r1, r1, #2
	bl	open
	mov	r2, r0
	ldr	r3, .L107
	str	r2, [r3, #0]
	ldr	r3, .L107
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L99
	ldr	r3, .L107
	ldr	r0, [r3, #0]
	mov	r1, #4
	mov	r2, #0
	bl	fcntl
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	bne	.L100
	ldr	r2, .L107
	ldr	r1, .L107+12
	mov	r3, #0
	str	r3, [sp, #0]
	mov	r3, #0
	str	r3, [sp, #4]
	ldr	r0, [r2, #0]
	ldr	r1, [r1, #0]
	mov	r2, #3
	mov	r3, #1
	bl	InitRS485
	b	.L96
.L100:
	ldr	r3, .L107
	sub	r2, fp, #80
	ldr	r0, [r3, #0]
	mov	r1, r2
	bl	tcgetattr
	mov	r3, #2224
	str	r3, [fp, #-72]
	mov	r3, #4
	str	r3, [fp, #-80]
	mov	r3, #0
	str	r3, [fp, #-76]
	mov	r3, #0
	str	r3, [fp, #-68]
	mov	r3, #0
	strb	r3, [fp, #-57]
	mov	r3, #2
	strb	r3, [fp, #-58]
	mov	r3, #4096
	add	r3, r3, #2
	str	r3, [fp, #-84]
	mov	r3, #0
	str	r3, [fp, #-16]
.L102:
	ldr	r3, [fp, #-16]
	cmp	r3, #6
	bgt	.L103
	ldr	r2, .L107+16
	ldr	r3, [fp, #-16]
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldr	r2, .L107+12
	ldr	r1, [r3, #0]
	ldr	r3, [r2, #0]
	cmp	r1, r3
	bne	.L104
	ldr	r3, .L107+16
	ldr	r2, [fp, #-16]
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-84]
	b	.L103
.L104:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L102
.L103:
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r1, [fp, #-84]
	bl	cfsetispeed
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r1, [fp, #-84]
	bl	cfsetospeed
	ldr	r3, .L107
	ldr	r0, [r3, #0]
	mov	r1, #0
	bl	tcflush
	ldr	r3, .L107
	sub	r2, fp, #80
	ldr	r0, [r3, #0]
	mov	r1, #0
	bl	tcsetattr
	b	.L96
.L99:
	ldr	r0, .L107+20
	bl	printf
.L96:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L108:
	.align	2
.L107:
	.word	serial_handle
	.word	serial_dev
	.word	.LC15
	.word	serial_baud
	.word	baud_table
	.word	.LC16
	.size	_Z11serial_initv, .-_Z11serial_initv
	.global	mcu_doutputmap
	.bss
	.align	2
	.type	mcu_doutputmap, %object
	.size	mcu_doutputmap, 4
mcu_doutputmap:
	.space	4
	.text
	.align	2
	.global	_Z12mcu_checksunPc
	.type	_Z12mcu_checksunPc, %function
_Z12mcu_checksunPc:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #4
	bls	.L111
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #40
	bhi	.L111
	b	.L110
.L111:
	mvn	r3, #0
	str	r3, [fp, #-28]
	b	.L109
.L110:
	mov	r3, #0
	strb	r3, [fp, #-17]
	mov	r3, #0
	str	r3, [fp, #-24]
.L112:
	ldr	r3, [fp, #-16]
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	ble	.L113
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	ldrb	r2, [fp, #-17]
	ldrb	r3, [r3, #0]
	add	r3, r2, r3
	strb	r3, [fp, #-17]
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
	b	.L112
.L113:
	ldrb	r3, [fp, #-17]	@ zero_extendqisi2
	str	r3, [fp, #-28]
.L109:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z12mcu_checksunPc, .-_Z12mcu_checksunPc
	.align	2
	.global	_Z15mcu_calchecksunPc
	.type	_Z15mcu_calchecksunPc, %function
_Z15mcu_calchecksunPc:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	str	r0, [fp, #-16]
	mov	r3, #0
	strb	r3, [fp, #-17]
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	str	r3, [fp, #-24]
.L116:
	ldr	r3, [fp, #-24]
	sub	r3, r3, #1
	str	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L117
	sub	r0, fp, #16
	ldr	r2, [r0, #0]
	mov	r3, r2
	ldrb	r1, [fp, #-17]
	ldrb	r3, [r3, #0]
	rsb	r3, r3, r1
	add	r2, r2, #1
	str	r2, [r0, #0]
	strb	r3, [fp, #-17]
	b	.L116
.L117:
	ldr	r2, [fp, #-16]
	ldrb	r3, [fp, #-17]
	strb	r3, [r2, #0]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z15mcu_calchecksunPc, .-_Z15mcu_calchecksunPc
	.align	2
	.global	_Z8mcu_sendPc
	.type	_Z8mcu_sendPc, %function
_Z8mcu_sendPc:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #4
	bls	.L120
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #40
	bhi	.L120
	b	.L119
.L120:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L118
.L119:
	ldr	r0, [fp, #-16]
	bl	_Z15mcu_calchecksunPc
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	ldr	r0, [fp, #-16]
	mov	r1, r3
	bl	_Z12serial_writePvj
	mov	r3, r0
	str	r3, [fp, #-20]
.L118:
	ldr	r0, [fp, #-20]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z8mcu_sendPc, .-_Z8mcu_sendPc
	.local	mcu_recverror
	.comm	mcu_recverror,4,4
	.align	2
	.global	_Z11mcu_recvmsgPci
	.type	_Z11mcu_recvmsgPci, %function
_Z11mcu_recvmsgPci:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r0, [fp, #-16]
	mov	r1, #1
	bl	_Z11serial_readPvj
	mov	r3, r0
	cmp	r3, #0
	beq	.L122
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #4
	ble	.L122
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L122
	ldr	r3, [fp, #-16]
	add	r2, r3, #1
	ldr	r3, [fp, #-24]
	sub	r3, r3, #1
	mov	r0, r2
	mov	r1, r3
	bl	_Z11serial_readPvj
	mov	r3, r0
	add	r3, r3, #1
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-16]
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bne	.L122
	mov	r2, #1
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L122
	mov	r2, #2
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L122
	ldr	r0, [fp, #-16]
	bl	_Z12mcu_checksunPc
	mov	r3, r0
	cmp	r3, #0
	bne	.L122
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-28]
	b	.L121
.L122:
	mov	r0, #100
	bl	_Z12serial_cleari
	mov	r3, #0
	str	r3, [fp, #-28]
.L121:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z11mcu_recvmsgPci, .-_Z11mcu_recvmsgPci
	.align	2
	.global	_Z8mcu_recvPcii
	.type	_Z8mcu_recvPcii, %function
_Z8mcu_recvPcii:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	ldr	r3, [fp, #-20]
	cmp	r3, #4
	bgt	.L127
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L125
.L127:
	ldr	r0, [fp, #-24]
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L128
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	bl	_Z11mcu_recvmsgPci
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	ble	.L128
	ldr	r2, .L135
	mov	r3, #0
	str	r3, [r2, #0]
	mov	r2, #4
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L130
	ldr	r0, [fp, #-16]
	bl	_Z17mcu_checkinputbufPc
	b	.L127
.L130:
	mov	r2, #3
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #31
	bne	.L132
	b	.L127
.L132:
	ldr	r3, [fp, #-28]
	str	r3, [fp, #-32]
	b	.L125
.L128:
	ldr	r2, .L135
	ldr	r3, .L135
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #5
	ble	.L134
	mov	r0, #1
	bl	sleep
	bl	_Z11serial_initv
	ldr	r2, .L135
	mov	r3, #0
	str	r3, [r2, #0]
.L134:
	mov	r3, #0
	str	r3, [fp, #-32]
.L125:
	ldr	r0, [fp, #-32]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L136:
	.align	2
.L135:
	.word	mcu_recverror
	.size	_Z8mcu_recvPcii, .-_Z8mcu_recvPcii
	.section	.rodata
	.align	2
.LC17:
	.ascii	"Enter a command\000"
	.text
	.align	2
	.global	_Z22mcu_recv_enteracommandv
	.type	_Z22mcu_recv_enteracommandv, %function
_Z22mcu_recv_enteracommandv:
	@ args = 0, pretend = 0, frame = 204
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #204
	mov	r3, #0
	str	r3, [fp, #-16]
.L138:
	mov	r0, #99328
	add	r0, r0, #672
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L139
	sub	r3, fp, #216
	mov	r0, r3
	mov	r1, #0
	mov	r2, #200
	bl	memset
	sub	r3, fp, #216
	mov	r0, r3
	mov	r1, #200
	bl	_Z11serial_readPvj
	sub	r3, fp, #216
	mov	r0, r3
	ldr	r1, .L141
	bl	strcasestr
	mov	r3, r0
	cmp	r3, #0
	beq	.L138
	mov	r3, #1
	str	r3, [fp, #-16]
	b	.L138
.L139:
	ldr	r3, [fp, #-16]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L142:
	.align	2
.L141:
	.word	.LC17
	.size	_Z22mcu_recv_enteracommandv, .-_Z22mcu_recv_enteracommandv
	.data
	.type	g_convert, %object
	.size	g_convert, 216
g_convert:
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	-1
	.byte	-1
	.byte	0
	.byte	0
	.byte	0
	.byte	1
	.byte	0
	.type	direction_table, %object
	.size	direction_table, 48
direction_table:
	.byte	0
	.byte	2
	.byte	0
	.byte	3
	.byte	0
	.byte	4
	.byte	0
	.byte	5
	.byte	1
	.byte	2
	.byte	1
	.byte	3
	.byte	1
	.byte	4
	.byte	1
	.byte	5
	.byte	2
	.byte	0
	.byte	2
	.byte	1
	.byte	2
	.byte	4
	.byte	2
	.byte	5
	.byte	3
	.byte	0
	.byte	3
	.byte	1
	.byte	3
	.byte	4
	.byte	3
	.byte	5
	.byte	4
	.byte	0
	.byte	4
	.byte	1
	.byte	4
	.byte	2
	.byte	4
	.byte	3
	.byte	5
	.byte	0
	.byte	5
	.byte	1
	.byte	5
	.byte	2
	.byte	5
	.byte	3
	.align	2
	.type	gsensor_direction, %object
	.size	gsensor_direction, 4
gsensor_direction:
	.word	7
	.global	g_sensor_available
	.bss
	.align	2
	.type	g_sensor_available, %object
	.size	g_sensor_available, 4
g_sensor_available:
	.space	4
	.global	g_sensor_trigger_forward
	.align	2
	.type	g_sensor_trigger_forward, %object
	.size	g_sensor_trigger_forward, 4
g_sensor_trigger_forward:
	.space	4
	.global	g_sensor_trigger_backward
	.align	2
	.type	g_sensor_trigger_backward, %object
	.size	g_sensor_trigger_backward, 4
g_sensor_trigger_backward:
	.space	4
	.global	g_sensor_trigger_right
	.align	2
	.type	g_sensor_trigger_right, %object
	.size	g_sensor_trigger_right, 4
g_sensor_trigger_right:
	.space	4
	.global	g_sensor_trigger_left
	.align	2
	.type	g_sensor_trigger_left, %object
	.size	g_sensor_trigger_left, 4
g_sensor_trigger_left:
	.space	4
	.global	g_sensor_trigger_down
	.align	2
	.type	g_sensor_trigger_down, %object
	.size	g_sensor_trigger_down, 4
g_sensor_trigger_down:
	.space	4
	.global	g_sensor_trigger_up
	.align	2
	.type	g_sensor_trigger_up, %object
	.size	g_sensor_trigger_up, 4
g_sensor_trigger_up:
	.space	4
	.data
	.align	2
	.type	_ZZ17mcu_checkinputbufPcE10rsp_dinput, %object
	.size	_ZZ17mcu_checkinputbufPcE10rsp_dinput, 7
_ZZ17mcu_checkinputbufPcE10rsp_dinput:
	.ascii	"\006\001\000\034\003\314\000"
	.align	2
	.type	_ZZ17mcu_checkinputbufPcE12rsp_poweroff, %object
	.size	_ZZ17mcu_checkinputbufPcE12rsp_poweroff, 10
_ZZ17mcu_checkinputbufPcE12rsp_poweroff:
	.ascii	"\b\001\000\b\003\000\000\314\000"
	.space	1
	.align	2
	.type	_ZZ17mcu_checkinputbufPcE11rsp_poweron, %object
	.size	_ZZ17mcu_checkinputbufPcE11rsp_poweron, 10
_ZZ17mcu_checkinputbufPcE11rsp_poweron:
	.ascii	"\007\001\000\t\003\000\314\000"
	.space	2
	.align	2
	.type	_ZZ17mcu_checkinputbufPcE9rsp_accel, %object
	.size	_ZZ17mcu_checkinputbufPcE9rsp_accel, 10
_ZZ17mcu_checkinputbufPcE9rsp_accel:
	.ascii	"\006\001\000@\003\314\000"
	.space	3
	.global	__floatsisf
	.global	__divsf3
	.global	__mulsf3
	.global	__addsf3
	.text
	.align	2
	.global	_Z17mcu_checkinputbufPc
	.type	_Z17mcu_checkinputbufPc, %function
_Z17mcu_checkinputbufPc:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #48
	str	r0, [fp, #-20]
	mov	r2, #4
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L144
	mov	r3, #3
	ldr	r2, [fp, #-20]
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	str	r3, [fp, #-64]
	ldr	r3, [fp, #-64]
	cmp	r3, #9
	beq	.L152
	ldr	r3, [fp, #-64]
	cmp	r3, #9
	bgt	.L159
	ldr	r3, [fp, #-64]
	cmp	r3, #8
	beq	.L151
	b	.L145
.L159:
	ldr	r3, [fp, #-64]
	cmp	r3, #28
	beq	.L146
	ldr	r3, [fp, #-64]
	cmp	r3, #64
	beq	.L153
	b	.L145
.L146:
	ldr	r0, .L160
	bl	_Z8mcu_sendPc
	mov	r2, #5
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrb	r1, [r3, #0]	@ zero_extendqisi2
	mov	r2, #6
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r3, r3, asl #8
	add	r3, r1, r3
	str	r3, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-28]
	mov	r3, #0
	str	r3, [fp, #-32]
.L147:
	ldr	r3, [fp, #-32]
	cmp	r3, #11
	bgt	.L148
	ldr	r2, .L160+4
	ldr	r3, [fp, #-32]
	ldr	r2, [r2, r3, asl #2]
	ldr	r3, [fp, #-24]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L149
	mov	r2, #1
	ldr	r3, [fp, #-32]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-28]
	orr	r3, r3, r2
	str	r3, [fp, #-28]
.L149:
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
	b	.L147
.L148:
	ldr	r2, .L160+8
	ldr	r3, [fp, #-24]
	mov	r3, r3, lsr #7
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [r2, #0]
	ldr	r2, .L160+12
	ldr	r3, [fp, #-24]
	mov	r3, r3, lsr #6
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [r2, #0]
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	str	r3, [r2, #24]
	b	.L145
.L151:
	ldr	r0, .L160+20
	bl	_Z8mcu_sendPc
	ldr	r2, .L160+24
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	mov	r3, #1
	str	r3, [r2, #44]
	b	.L145
.L152:
	ldr	r2, .L160+28
	ldr	r3, .L160+32
	ldr	r3, [r3, #0]
	strb	r3, [r2, #5]
	ldr	r0, .L160+28
	bl	_Z8mcu_sendPc
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #44]
	b	.L145
.L153:
	ldr	r0, .L160+36
	bl	_Z8mcu_sendPc
	mov	r2, #5
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L160+40	@ float
	bl	__divsf3
	mov	r3, r0
	str	r3, [fp, #-40]	@ float
	mov	r2, #6
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L160+40	@ float
	bl	__divsf3
	mov	r3, r0
	str	r3, [fp, #-36]	@ float
	mov	r2, #7
	ldr	r3, [fp, #-20]
	add	r3, r2, r3
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L160+40	@ float
	bl	__divsf3
	mov	r3, r0
	str	r3, [fp, #-44]	@ float
	ldr	r1, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-36]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #1
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-40]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #2
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-44]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-48]	@ float
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #3
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-36]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #4
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-40]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #5
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-44]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-52]	@ float
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #6
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-36]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #7
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-40]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L160+44
	ldr	r3, .L160+48
	ldr	r2, [r3, #0]
	mov	r1, #8
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	mov	r0, r3
	ldr	r1, [fp, #-44]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-56]	@ float
	ldr	r3, .L160+16
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #136]
	cmp	r3, #0
	bne	.L154
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-52]	@ float
	str	r3, [r2, #140]	@ float
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-48]	@ float
	str	r3, [r2, #144]	@ float
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-56]	@ float
	str	r3, [r2, #148]	@ float
	ldr	r3, .L160+52
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L145
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	mov	r3, #1
	str	r3, [r2, #136]
	b	.L145
.L154:
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-52]	@ float
	str	r3, [r2, #156]	@ float
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-48]	@ float
	str	r3, [r2, #160]	@ float
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-56]	@ float
	str	r3, [r2, #164]	@ float
	ldr	r3, .L160+52
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L145
	ldr	r3, .L160+16
	ldr	r2, [r3, #0]
	mov	r3, #1
	str	r3, [r2, #152]
.L145:
	mov	r3, #1
	str	r3, [fp, #-60]
	b	.L143
.L144:
	mov	r3, #0
	str	r3, [fp, #-60]
.L143:
	ldr	r0, [fp, #-60]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L161:
	.align	2
.L160:
	.word	_ZZ17mcu_checkinputbufPcE10rsp_dinput
	.word	input_map_table
	.word	hdlock
	.word	hdinserted
	.word	p_dio_mmap
	.word	_ZZ17mcu_checkinputbufPcE12rsp_poweroff
	.word	mcupowerdelaytime
	.word	_ZZ17mcu_checkinputbufPcE11rsp_poweron
	.word	watchdogenabled
	.word	_ZZ17mcu_checkinputbufPcE9rsp_accel
	.word	1096810496
	.word	g_convert
	.word	gsensor_direction
	.word	gforce_log_enable
	.size	_Z17mcu_checkinputbufPc, .-_Z17mcu_checkinputbufPc
	.align	2
	.global	_Z9mcu_inputi
	.type	_Z9mcu_inputi, %function
_Z9mcu_inputi:
	@ args = 0, pretend = 0, frame = 72
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #72
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-84]
	mov	r3, #0
	str	r3, [fp, #-28]
.L163:
	ldr	r3, [fp, #-28]
	cmp	r3, #9
	bgt	.L164
	ldr	r0, [fp, #-84]
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L164
	sub	r3, fp, #80
	mov	r0, r3
	mov	r1, #50
	bl	_Z11mcu_recvmsgPci
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	ble	.L165
	mov	r3, #10
	str	r3, [fp, #-84]
	ldrb	r3, [fp, #-77]	@ zero_extendqisi2
	cmp	r3, #31
	beq	.L165
	sub	r3, fp, #80
	mov	r0, r3
	bl	_Z17mcu_checkinputbufPc
	mov	r3, r0
	str	r3, [fp, #-20]
.L165:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L163
.L164:
	ldr	r3, [fp, #-20]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z9mcu_inputi, .-_Z9mcu_inputi
	.data
	.align	2
	.type	_ZZ15mcu_bootupreadyvE15cmd_bootupready, %object
	.size	_ZZ15mcu_bootupreadyvE15cmd_bootupready, 8
_ZZ15mcu_bootupreadyvE15cmd_bootupready:
	.ascii	"\007\001\000\021\002\000\314\000"
	.text
	.align	2
	.global	_Z15mcu_bootupreadyv
	.type	_Z15mcu_bootupreadyv, %function
_Z15mcu_bootupreadyv:
	@ args = 0, pretend = 0, frame = 60
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #60
	mov	r3, #10
	str	r3, [fp, #-68]
.L171:
	ldr	r3, [fp, #-68]
	sub	r3, r3, #1
	str	r3, [fp, #-68]
	cmp	r3, #0
	ble	.L172
	mov	r0, #100
	bl	_Z12serial_cleari
	ldr	r0, .L176
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L173
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L173
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #17
	bne	.L173
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L173
	mov	r3, #1
	str	r3, [fp, #-72]
	b	.L170
.L173:
	mov	r0, #1
	bl	sleep
	b	.L171
.L172:
	mov	r3, #0
	str	r3, [fp, #-72]
.L170:
	ldr	r0, [fp, #-72]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L177:
	.align	2
.L176:
	.word	_ZZ15mcu_bootupreadyvE15cmd_bootupready
	.size	_Z15mcu_bootupreadyv, .-_Z15mcu_bootupreadyv
	.global	code
	.bss
	.align	2
	.type	code, %object
	.size	code, 4
code:
	.space	4
	.data
	.align	2
	.type	_ZZ12mcu_readcodevE12cmd_readcode, %object
	.size	_ZZ12mcu_readcodevE12cmd_readcode, 8
_ZZ12mcu_readcodevE12cmd_readcode:
	.ascii	"\006\001\000A\002\314\000"
	.space	1
	.section	.rodata
	.align	2
.LC18:
	.ascii	"MCU shutdown code: %02x at %04d-%02d-%02d %02d:%02d"
	.ascii	":%02d\000"
	.align	2
.LC19:
	.ascii	"Read MCU shutdown code failed.\000"
	.text
	.align	2
	.global	_Z12mcu_readcodev
	.type	_Z12mcu_readcodev, %function
_Z12mcu_readcodev:
	@ args = 0, pretend = 0, frame = 104
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #120
	ldr	r0, .L184
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L179
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L179
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #65
	bne	.L179
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L179
	ldrb	r3, [fp, #-64]	@ zero_extendqisi2
	cmp	r3, #8
	bls	.L179
	ldrb	r3, [fp, #-59]	@ zero_extendqisi2
	cmp	r3, #255
	beq	.L182
	ldr	r2, .L184+4
	ldrb	r3, [fp, #-59]	@ zero_extendqisi2
	str	r3, [r2, #0]
	sub	r3, fp, #108
	mov	r0, r3
	mov	r1, #0
	mov	r2, #44
	bl	memset
	ldrb	r3, [fp, #-52]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	mov	r3, r0
	add	r3, r3, #100
	str	r3, [fp, #-88]
	ldrb	r3, [fp, #-53]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	mov	r3, r0
	sub	r3, r3, #1
	str	r3, [fp, #-92]
	ldrb	r3, [fp, #-54]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-96]
	ldrb	r3, [fp, #-56]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-100]
	ldrb	r3, [fp, #-57]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-104]
	ldrb	r3, [fp, #-58]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-108]
	mvn	r3, #0
	str	r3, [fp, #-76]
	sub	r3, fp, #108
	mov	r0, r3
	bl	timegm
	mov	r3, r0
	str	r3, [fp, #-112]
	ldr	r3, [fp, #-112]
	cmp	r3, #0
	ble	.L182
	sub	r3, fp, #112
	sub	r2, fp, #108
	mov	r0, r3
	mov	r1, r2
	bl	localtime_r
	ldr	r1, .L184+4
	ldr	r3, [fp, #-88]
	add	r2, r3, #1888
	add	r2, r2, #12
	ldr	r3, [fp, #-92]
	add	ip, r3, #1
	ldr	r3, [fp, #-96]
	str	r3, [sp, #0]
	ldr	r3, [fp, #-100]
	str	r3, [sp, #4]
	ldr	r3, [fp, #-104]
	str	r3, [sp, #8]
	ldr	r3, [fp, #-108]
	str	r3, [sp, #12]
	ldr	r0, .L184+8
	ldr	r1, [r1, #0]
	mov	r3, ip
	bl	_Z7dvr_logPcz
.L182:
	mov	r3, #1
	str	r3, [fp, #-116]
	b	.L178
.L179:
	ldr	r0, .L184+12
	bl	_Z7dvr_logPcz
	mov	r3, #0
	str	r3, [fp, #-116]
.L178:
	ldr	r0, [fp, #-116]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L185:
	.align	2
.L184:
	.word	_ZZ12mcu_readcodevE12cmd_readcode
	.word	code
	.word	.LC18
	.word	.LC19
	.size	_Z12mcu_readcodev, .-_Z12mcu_readcodev
	.data
	.align	2
	.type	_ZZ10mcu_rebootvE10cmd_reboot, %object
	.size	_ZZ10mcu_rebootvE10cmd_reboot, 8
_ZZ10mcu_rebootvE10cmd_reboot:
	.ascii	"\006\001\000\022\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z10mcu_rebootv
	.type	_Z10mcu_rebootv, %function
_Z10mcu_rebootv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r0, .L187
	bl	_Z8mcu_sendPc
	mov	r3, #0
	mov	r0, r3
	ldmfd	sp, {fp, sp, pc}
.L188:
	.align	2
.L187:
	.word	_ZZ10mcu_rebootvE10cmd_reboot
	.size	_Z10mcu_rebootv, .-_Z10mcu_rebootv
	.data
	.align	2
	.type	_ZZ15mcu_gsensorinitvE15cmd_gsensorinit, %object
	.size	_ZZ15mcu_gsensorinitvE15cmd_gsensorinit, 16
_ZZ15mcu_gsensorinitvE15cmd_gsensorinit:
	.ascii	"\f\001\0004\002\001\002\003\004\005\006\314\000"
	.space	3
	.global	__gesf2
	.global	__fixsfsi
	.text
	.align	2
	.global	_Z15mcu_gsensorinitv
	.type	_Z15mcu_gsensorinitv, %function
_Z15mcu_gsensorinitv:
	@ args = 0, pretend = 0, frame = 84
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #84
	mov	r3, #10
	str	r3, [fp, #-72]
	ldr	r3, .L205
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L190
	mov	r3, #0
	str	r3, [fp, #-100]
	b	.L189
.L190:
	ldr	r1, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+12
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #3
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+16
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #6
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+20
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-76]	@ float
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #1
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+12
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #4
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+16
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #7
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+20
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-84]	@ float
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #2
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+12
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #5
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+16
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #8
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+20
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-92]	@ float
	ldr	r1, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+24
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #3
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+28
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #6
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+32
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-80]	@ float
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #1
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+24
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #4
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+28
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #7
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+32
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-88]	@ float
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #2
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+24
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #5
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+28
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r4, r0
	ldr	r0, .L205+4
	ldr	r3, .L205+8
	ldr	r2, [r3, #0]
	mov	r1, #8
	mov	r3, r2
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r0
	add	r3, r3, r1
	ldrsb	r3, [r3, #0]
	mov	r0, r3
	bl	__floatsisf
	mov	r3, r0
	ldr	r2, .L205+32
	mov	r0, r3
	ldr	r1, [r2, #0]	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__addsf3
	mov	r3, r0
	str	r3, [fp, #-96]	@ float
	ldr	r0, [fp, #-84]	@ float
	ldr	r1, [fp, #-88]	@ float
	bl	__gesf2
	mov	r3, r0
	cmp	r3, #0
	bge	.L192
	b	.L191
.L192:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-84]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #5]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-88]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #6]
	b	.L193
.L191:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-88]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #5]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-84]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #6]
.L193:
	ldr	r0, [fp, #-76]	@ float
	ldr	r1, [fp, #-80]	@ float
	bl	__gesf2
	mov	r3, r0
	cmp	r3, #0
	bge	.L195
	b	.L194
.L195:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-76]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #7]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-80]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #8]
	b	.L196
.L194:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-80]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #7]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-76]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #8]
.L196:
	ldr	r0, [fp, #-92]	@ float
	ldr	r1, [fp, #-96]	@ float
	bl	__gesf2
	mov	r3, r0
	cmp	r3, #0
	bge	.L198
	b	.L197
.L198:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-92]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #9]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-96]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #10]
	b	.L199
.L197:
	ldr	r4, .L205+36
	ldr	r0, [fp, #-96]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #9]
	ldr	r4, .L205+36
	ldr	r0, [fp, #-92]	@ float
	ldr	r1, .L205+40	@ float
	bl	__mulsf3
	mov	r3, r0
	mov	r0, r3
	bl	__fixsfsi
	mov	r3, r0
	strb	r3, [r4, #10]
.L199:
	ldr	r2, .L205+44
	mov	r3, #0
	str	r3, [r2, #0]
.L200:
	ldr	r3, [fp, #-72]
	sub	r3, r3, #1
	str	r3, [fp, #-72]
	cmp	r3, #0
	ble	.L201
	mov	r0, #100
	bl	_Z12serial_cleari
	ldr	r0, .L205+36
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L202
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L202
	ldrb	r3, [fp, #-65]	@ zero_extendqisi2
	cmp	r3, #52
	bne	.L202
	ldrb	r3, [fp, #-64]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L202
	ldrb	r3, [fp, #-68]	@ zero_extendqisi2
	cmp	r3, #6
	bls	.L202
	ldr	r2, .L205+44
	ldrb	r3, [fp, #-63]	@ zero_extendqisi2
	str	r3, [r2, #0]
	ldr	r3, .L205+44
	ldr	r3, [r3, #0]
	str	r3, [fp, #-100]
	b	.L189
.L202:
	mov	r0, #1
	bl	sleep
	b	.L200
.L201:
	mov	r3, #0
	str	r3, [fp, #-100]
.L189:
	ldr	r0, [fp, #-100]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L206:
	.align	2
.L205:
	.word	gforce_log_enable
	.word	g_convert
	.word	gsensor_direction
	.word	g_sensor_trigger_forward
	.word	g_sensor_trigger_right
	.word	g_sensor_trigger_down
	.word	g_sensor_trigger_backward
	.word	g_sensor_trigger_left
	.word	g_sensor_trigger_up
	.word	_ZZ15mcu_gsensorinitvE15cmd_gsensorinit
	.word	1096810496
	.word	g_sensor_available
	.size	_Z15mcu_gsensorinitv, .-_Z15mcu_gsensorinitv
	.data
	.align	2
	.type	_ZZ11mcu_versionPcE19cmd_firmwareversion, %object
	.size	_ZZ11mcu_versionPcE19cmd_firmwareversion, 8
_ZZ11mcu_versionPcE19cmd_firmwareversion:
	.ascii	"\006\001\000-\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z11mcu_versionPc
	.type	_Z11mcu_versionPc, %function
_Z11mcu_versionPc:
	@ args = 0, pretend = 0, frame = 68
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #68
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-76]
.L208:
	ldr	r3, [fp, #-76]
	cmp	r3, #9
	bgt	.L209
	ldr	r0, .L214
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L210
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	str	r3, [fp, #-72]
	ldr	r3, [fp, #-72]
	cmp	r3, #0
	ble	.L210
	ldrb	r3, [fp, #-65]	@ zero_extendqisi2
	cmp	r3, #45
	bne	.L210
	ldrb	r3, [fp, #-64]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L210
	sub	r3, fp, #68
	add	r2, r3, #5
	ldr	r3, [fp, #-72]
	sub	r3, r3, #6
	ldr	r0, [fp, #-16]
	mov	r1, r2
	mov	r2, r3
	bl	memcpy
	mvn	r1, #5
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-72]
	add	r3, r2, r3
	add	r2, r3, r1
	mov	r3, #0
	strb	r3, [r2, #0]
	ldr	r3, [fp, #-72]
	sub	r3, r3, #6
	str	r3, [fp, #-80]
	b	.L207
.L210:
	ldr	r3, [fp, #-76]
	add	r3, r3, #1
	str	r3, [fp, #-76]
	b	.L208
.L209:
	mov	r3, #0
	str	r3, [fp, #-80]
.L207:
	ldr	r0, [fp, #-80]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L215:
	.align	2
.L214:
	.word	_ZZ11mcu_versionPcE19cmd_firmwareversion
	.size	_Z11mcu_versionPc, .-_Z11mcu_versionPc
	.data
	.align	2
	.type	_ZZ11mcu_doutputvE11cmd_doutput, %object
	.size	_ZZ11mcu_doutputvE11cmd_doutput, 8
_ZZ11mcu_doutputvE11cmd_doutput:
	.ascii	"\007\001\0001\002\000\314\000"
	.text
	.align	2
	.global	_Z11mcu_doutputv
	.type	_Z11mcu_doutputv, %function
_Z11mcu_doutputv:
	@ args = 0, pretend = 0, frame = 68
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #68
	ldr	r3, .L225
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	str	r3, [fp, #-68]
	ldr	r3, .L225+4
	ldr	r2, [fp, #-68]
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bne	.L217
	mov	r3, #1
	str	r3, [fp, #-80]
	b	.L216
.L217:
	ldr	r3, .L225+8
	ldr	r2, [fp, #-68]
	ldr	r3, [r3, #0]
	eor	r3, r2, r3
	str	r3, [fp, #-72]
	ldr	r2, .L225+12
	mov	r3, #0
	strb	r3, [r2, #5]
	mov	r3, #0
	str	r3, [fp, #-76]
.L218:
	ldr	r3, [fp, #-76]
	cmp	r3, #7
	bgt	.L219
	ldr	r2, .L225+16
	ldr	r3, [fp, #-76]
	ldr	r2, [r2, r3, asl #2]
	ldr	r3, [fp, #-72]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L220
	ldr	r0, .L225+12
	ldr	r1, .L225+12
	mov	r2, #1
	ldr	r3, [fp, #-76]
	mov	r3, r2, asl r3
	ldrb	r2, [r1, #5]
	orr	r3, r2, r3
	strb	r3, [r0, #5]
.L220:
	ldr	r3, [fp, #-76]
	add	r3, r3, #1
	str	r3, [fp, #-76]
	b	.L218
.L219:
	ldr	r0, .L225+12
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L222
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L222
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #49
	bne	.L222
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L222
	ldr	r2, .L225+4
	ldr	r3, [fp, #-68]
	str	r3, [r2, #0]
.L222:
	mov	r3, #1
	str	r3, [fp, #-80]
.L216:
	ldr	r0, [fp, #-80]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L226:
	.align	2
.L225:
	.word	p_dio_mmap
	.word	mcu_doutputmap
	.word	output_inverted
	.word	_ZZ11mcu_doutputvE11cmd_doutput
	.word	output_map_table
	.size	_Z11mcu_doutputv, .-_Z11mcu_doutputv
	.data
	.align	2
	.type	_ZZ10mcu_dinputvE10cmd_dinput, %object
	.size	_ZZ10mcu_dinputvE10cmd_dinput, 8
_ZZ10mcu_dinputvE10cmd_dinput:
	.ascii	"\006\001\000\035\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z10mcu_dinputv
	.type	_Z10mcu_dinputv, %function
_Z10mcu_dinputv:
	@ args = 0, pretend = 0, frame = 68
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #68
	mov	r0, #9984
	add	r0, r0, #16
	bl	_Z9mcu_inputi
	ldr	r0, .L235
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L228
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L228
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #29
	bne	.L228
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L228
	ldrb	r2, [fp, #-59]	@ zero_extendqisi2
	ldrb	r3, [fp, #-58]	@ zero_extendqisi2
	mov	r3, r3, asl #8
	add	r3, r2, r3
	str	r3, [fp, #-68]
	mov	r3, #0
	str	r3, [fp, #-72]
	mov	r3, #0
	str	r3, [fp, #-76]
.L231:
	ldr	r3, [fp, #-76]
	cmp	r3, #11
	bgt	.L232
	ldr	r2, .L235+4
	ldr	r3, [fp, #-76]
	ldr	r2, [r2, r3, asl #2]
	ldr	r3, [fp, #-68]
	and	r3, r2, r3
	cmp	r3, #0
	beq	.L233
	mov	r2, #1
	ldr	r3, [fp, #-76]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-72]
	orr	r3, r3, r2
	str	r3, [fp, #-72]
.L233:
	ldr	r3, [fp, #-76]
	add	r3, r3, #1
	str	r3, [fp, #-76]
	b	.L231
.L232:
	ldr	r3, .L235+8
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-72]
	str	r3, [r2, #24]
	ldr	r2, .L235+12
	ldr	r3, [fp, #-68]
	mov	r3, r3, lsr #7
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [r2, #0]
	ldr	r2, .L235+16
	ldr	r3, [fp, #-68]
	mov	r3, r3, lsr #6
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [r2, #0]
	mov	r3, #1
	str	r3, [fp, #-80]
	b	.L227
.L228:
	mov	r3, #0
	str	r3, [fp, #-80]
.L227:
	ldr	r0, [fp, #-80]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L236:
	.align	2
.L235:
	.word	_ZZ10mcu_dinputvE10cmd_dinput
	.word	input_map_table
	.word	p_dio_mmap
	.word	hdlock
	.word	hdinserted
	.size	_Z10mcu_dinputv, .-_Z10mcu_dinputv
	.data
	.align	2
	.type	delay_inc, %object
	.size	delay_inc, 4
delay_inc:
	.word	10
	.align	2
	.type	_ZZ17mcu_poweroffdelayvE21cmd_poweroffdelaytime, %object
	.size	_ZZ17mcu_poweroffdelayvE21cmd_poweroffdelaytime, 10
_ZZ17mcu_poweroffdelayvE21cmd_poweroffdelaytime:
	.ascii	"\b\001\000\b\002\000\000\314\000"
	.space	1
	.text
	.align	2
	.global	_Z17mcu_poweroffdelayv
	.type	_Z17mcu_poweroffdelayv, %function
_Z17mcu_poweroffdelayv:
	@ args = 0, pretend = 0, frame = 52
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #52
	ldr	r3, .L243
	ldr	r3, [r3, #0]
	cmp	r3, #49
	bgt	.L238
	ldr	r2, .L243+4
	ldr	r3, .L243+8
	ldr	r3, [r3, #0]
	strb	r3, [r2, #6]
	b	.L239
.L238:
	ldr	r3, .L243+4
	mov	r2, #0
	strb	r2, [r3, #6]
.L239:
	ldr	r0, .L243+4
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L237
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L237
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #8
	bne	.L237
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L237
	ldr	r1, .L243
	ldrb	r3, [fp, #-59]	@ zero_extendqisi2
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-58]	@ zero_extendqisi2
	add	r3, r2, r3
	str	r3, [r1, #0]
.L237:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L244:
	.align	2
.L243:
	.word	mcupowerdelaytime
	.word	_ZZ17mcu_poweroffdelayvE21cmd_poweroffdelaytime
	.word	delay_inc
	.size	_Z17mcu_poweroffdelayv, .-_Z17mcu_poweroffdelayv
	.data
	.align	2
	.type	_ZZ18mcu_watchdogenablevE19cmd_watchdogtimeout, %object
	.size	_ZZ18mcu_watchdogenablevE19cmd_watchdogtimeout, 10
_ZZ18mcu_watchdogenablevE19cmd_watchdogtimeout:
	.ascii	"\b\001\000\031\002\000\000\314\000"
	.space	1
	.align	2
	.type	_ZZ18mcu_watchdogenablevE18cmd_watchdogenable, %object
	.size	_ZZ18mcu_watchdogenablevE18cmd_watchdogenable, 10
_ZZ18mcu_watchdogenablevE18cmd_watchdogenable:
	.ascii	"\006\001\000\032\002\314\000"
	.space	3
	.text
	.align	2
	.global	_Z18mcu_watchdogenablev
	.type	_Z18mcu_watchdogenablev, %function
_Z18mcu_watchdogenablev:
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #56
	ldr	r1, .L256
	ldr	r3, .L256+4
	ldr	r2, [r3, #0]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #24
	add	r3, r2, r3
	mov	r3, r3, asr #8
	strb	r3, [r1, #5]
	ldr	r1, .L256
	ldr	r3, .L256+4
	ldr	r2, [r3, #0]
	mov	r3, r2, asr #31
	mov	r3, r3, lsr #24
	add	r3, r2, r3
	mov	r3, r3, asr #8
	mov	r3, r3, asl #8
	rsb	r3, r3, r2
	strb	r3, [r1, #6]
	mov	r3, #0
	str	r3, [fp, #-16]
.L246:
	ldr	r3, [fp, #-16]
	cmp	r3, #4
	bgt	.L247
	ldr	r0, .L256
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L248
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L248
	b	.L247
.L248:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L246
.L247:
	mov	r3, #0
	str	r3, [fp, #-16]
.L251:
	ldr	r3, [fp, #-16]
	cmp	r3, #4
	bgt	.L252
	ldr	r0, .L256+8
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L253
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L253
	b	.L252
.L253:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L251
.L252:
	ldr	r2, .L256+12
	mov	r3, #1
	str	r3, [r2, #0]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L257:
	.align	2
.L256:
	.word	_ZZ18mcu_watchdogenablevE19cmd_watchdogtimeout
	.word	watchdogtimeout
	.word	_ZZ18mcu_watchdogenablevE18cmd_watchdogenable
	.word	watchdogenabled
	.size	_Z18mcu_watchdogenablev, .-_Z18mcu_watchdogenablev
	.data
	.align	2
	.type	_ZZ19mcu_watchdogdisablevE19cmd_watchdogdisable, %object
	.size	_ZZ19mcu_watchdogdisablevE19cmd_watchdogdisable, 10
_ZZ19mcu_watchdogdisablevE19cmd_watchdogdisable:
	.ascii	"\006\001\000\033\002\314\000"
	.space	3
	.text
	.align	2
	.global	_Z19mcu_watchdogdisablev
	.type	_Z19mcu_watchdogdisablev, %function
_Z19mcu_watchdogdisablev:
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #56
	mov	r3, #0
	str	r3, [fp, #-16]
.L259:
	ldr	r3, [fp, #-16]
	cmp	r3, #4
	bgt	.L260
	ldr	r0, .L264
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L261
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L261
	b	.L260
.L261:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L259
.L260:
	ldr	r2, .L264+4
	mov	r3, #0
	str	r3, [r2, #0]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L265:
	.align	2
.L264:
	.word	_ZZ19mcu_watchdogdisablevE19cmd_watchdogdisable
	.word	watchdogenabled
	.size	_Z19mcu_watchdogdisablev, .-_Z19mcu_watchdogdisablev
	.data
	.align	2
	.type	_ZZ16mcu_watchdogkickvE16cmd_kickwatchdog, %object
	.size	_ZZ16mcu_watchdogkickvE16cmd_kickwatchdog, 10
_ZZ16mcu_watchdogkickvE16cmd_kickwatchdog:
	.ascii	"\006\001\000\030\002\314\000"
	.space	3
	.text
	.align	2
	.global	_Z16mcu_watchdogkickv
	.type	_Z16mcu_watchdogkickv, %function
_Z16mcu_watchdogkickv:
	@ args = 0, pretend = 0, frame = 60
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #60
	mov	r3, #0
	str	r3, [fp, #-16]
.L267:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	bgt	.L268
	ldr	r0, .L273
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L269
	sub	r3, fp, #68
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L269
	ldrb	r3, [fp, #-65]	@ zero_extendqisi2
	cmp	r3, #24
	bne	.L269
	ldrb	r3, [fp, #-64]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L269
	mov	r3, #1
	str	r3, [fp, #-72]
	b	.L266
.L269:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L267
.L268:
	mov	r3, #0
	str	r3, [fp, #-72]
.L266:
	ldr	r0, [fp, #-72]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L274:
	.align	2
.L273:
	.word	_ZZ16mcu_watchdogkickvE16cmd_kickwatchdog
	.size	_Z16mcu_watchdogkickv, .-_Z16mcu_watchdogkickv
	.data
	.align	2
	.type	_ZZ17mcu_iotemperaturevE17cmd_iotemperature, %object
	.size	_ZZ17mcu_iotemperaturevE17cmd_iotemperature, 8
_ZZ17mcu_iotemperaturevE17cmd_iotemperature:
	.ascii	"\006\001\000\013\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z17mcu_iotemperaturev
	.type	_Z17mcu_iotemperaturev, %function
_Z17mcu_iotemperaturev:
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #56
	ldr	r0, .L279
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L276
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L276
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #11
	bne	.L276
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L276
	ldrsb	r3, [fp, #-59]
	str	r3, [fp, #-68]
	b	.L275
.L276:
	mvn	r3, #127
	str	r3, [fp, #-68]
.L275:
	ldr	r0, [fp, #-68]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L280:
	.align	2
.L279:
	.word	_ZZ17mcu_iotemperaturevE17cmd_iotemperature
	.size	_Z17mcu_iotemperaturev, .-_Z17mcu_iotemperaturev
	.data
	.align	2
	.type	_ZZ17mcu_hdtemperaturevE17cmd_hdtemperature, %object
	.size	_ZZ17mcu_hdtemperaturevE17cmd_hdtemperature, 8
_ZZ17mcu_hdtemperaturevE17cmd_hdtemperature:
	.ascii	"\006\001\000\f\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z17mcu_hdtemperaturev
	.type	_Z17mcu_hdtemperaturev, %function
_Z17mcu_hdtemperaturev:
	@ args = 0, pretend = 0, frame = 56
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #56
	ldr	r0, .L285
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L282
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	ble	.L282
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #12
	bne	.L282
	ldrb	r3, [fp, #-60]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L282
	ldrsb	r3, [fp, #-59]
	str	r3, [fp, #-68]
	b	.L281
.L282:
	mvn	r3, #127
	str	r3, [fp, #-68]
.L281:
	ldr	r0, [fp, #-68]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L286:
	.align	2
.L285:
	.word	_ZZ17mcu_hdtemperaturevE17cmd_hdtemperature
	.size	_Z17mcu_hdtemperaturev, .-_Z17mcu_hdtemperaturev
	.data
	.align	2
	.type	_ZZ13mcu_hdpoweronvE13cmd_hdpoweron, %object
	.size	_ZZ13mcu_hdpoweronvE13cmd_hdpoweron, 10
_ZZ13mcu_hdpoweronvE13cmd_hdpoweron:
	.ascii	"\006\001\000(\002\314\000"
	.space	3
	.text
	.align	2
	.global	_Z13mcu_hdpoweronv
	.type	_Z13mcu_hdpoweronv, %function
_Z13mcu_hdpoweronv:
	@ args = 0, pretend = 0, frame = 52
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #52
	ldr	r0, .L289
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L287
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
.L287:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L290:
	.align	2
.L289:
	.word	_ZZ13mcu_hdpoweronvE13cmd_hdpoweron
	.size	_Z13mcu_hdpoweronv, .-_Z13mcu_hdpoweronv
	.data
	.align	2
	.type	_ZZ14mcu_hdpoweroffvE14cmd_hdpoweroff, %object
	.size	_ZZ14mcu_hdpoweroffvE14cmd_hdpoweroff, 10
_ZZ14mcu_hdpoweroffvE14cmd_hdpoweroff:
	.ascii	"\006\001\000)\002\314\000"
	.space	3
	.text
	.align	2
	.global	_Z14mcu_hdpoweroffv
	.type	_Z14mcu_hdpoweroffv, %function
_Z14mcu_hdpoweroffv:
	@ args = 0, pretend = 0, frame = 52
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #52
	ldr	r0, .L293
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L291
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
.L291:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L294:
	.align	2
.L293:
	.word	_ZZ14mcu_hdpoweroffvE14cmd_hdpoweroff
	.size	_Z14mcu_hdpoweroffv, .-_Z14mcu_hdpoweroffv
	.data
	.align	2
	.type	_ZZ9mcu_r_rtcP2tmE11cmd_readrtc, %object
	.size	_ZZ9mcu_r_rtcP2tmE11cmd_readrtc, 8
_ZZ9mcu_r_rtcP2tmE11cmd_readrtc:
	.ascii	"\006\001\000\006\002\314\000"
	.space	1
	.section	.rodata
	.align	2
.LC20:
	.ascii	"Read clock from MCU failed!\000"
	.text
	.align	2
	.global	_Z9mcu_r_rtcP2tm
	.type	_Z9mcu_r_rtcP2tm, %function
_Z9mcu_r_rtcP2tm:
	@ args = 0, pretend = 0, frame = 112
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #112
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
.L296:
	ldr	r3, [fp, #-20]
	cmp	r3, #4
	bgt	.L297
	ldr	r0, .L305
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L298
	sub	r3, fp, #120
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L298
	ldrb	r3, [fp, #-117]	@ zero_extendqisi2
	cmp	r3, #6
	bne	.L298
	ldrb	r3, [fp, #-116]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L298
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #0
	mov	r2, #44
	bl	memset
	ldrb	r3, [fp, #-109]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	mov	r3, r0
	add	r3, r3, #100
	str	r3, [fp, #-44]
	ldrb	r3, [fp, #-110]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	mov	r3, r0
	sub	r3, r3, #1
	str	r3, [fp, #-48]
	ldrb	r3, [fp, #-111]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-52]
	ldrb	r3, [fp, #-113]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-56]
	ldrb	r3, [fp, #-114]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-60]
	ldrb	r3, [fp, #-115]	@ zero_extendqisi2
	mov	r0, r3
	bl	_Z6getbcdh
	str	r0, [fp, #-64]
	mvn	r3, #0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-44]
	cmp	r3, #105
	ble	.L298
	ldr	r3, [fp, #-44]
	cmp	r3, #134
	bgt	.L298
	ldr	r3, [fp, #-48]
	cmp	r3, #0
	blt	.L298
	ldr	r3, [fp, #-48]
	cmp	r3, #11
	bgt	.L298
	ldr	r3, [fp, #-52]
	cmp	r3, #0
	ble	.L298
	ldr	r3, [fp, #-52]
	cmp	r3, #31
	bgt	.L298
	ldr	r3, [fp, #-56]
	cmp	r3, #0
	blt	.L298
	ldr	r3, [fp, #-56]
	cmp	r3, #24
	bgt	.L298
	ldr	r3, [fp, #-60]
	cmp	r3, #0
	blt	.L298
	ldr	r3, [fp, #-60]
	cmp	r3, #60
	bgt	.L298
	ldr	r3, [fp, #-64]
	cmp	r3, #0
	blt	.L298
	ldr	r3, [fp, #-64]
	cmp	r3, #60
	bgt	.L298
	sub	r3, fp, #64
	mov	r0, r3
	bl	timegm
	mov	r3, r0
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-68]
	cmp	r3, #0
	ble	.L298
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L304
	ldr	r3, [fp, #-16]
	mov	lr, r3
	sub	ip, fp, #64
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip, {r0, r1, r2}
	stmia	lr, {r0, r1, r2}
.L304:
	ldr	r3, [fp, #-68]
	str	r3, [fp, #-124]
	b	.L295
.L298:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
	b	.L296
.L297:
	ldr	r0, .L305+4
	bl	_Z7dvr_logPcz
	mvn	r3, #0
	str	r3, [fp, #-124]
.L295:
	ldr	r0, [fp, #-124]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L306:
	.align	2
.L305:
	.word	_ZZ9mcu_r_rtcP2tmE11cmd_readrtc
	.word	.LC20
	.size	_Z9mcu_r_rtcP2tm, .-_Z9mcu_r_rtcP2tm
	.align	2
	.global	_Z11mcu_readrtcv
	.type	_Z11mcu_readrtcv, %function
_Z11mcu_readrtcv:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #48
	sub	r3, fp, #60
	mov	r0, r3
	bl	_Z9mcu_r_rtcP2tm
	mov	r3, r0
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	ble	.L308
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-40]
	add	r3, r3, #1888
	add	r3, r3, #12
	strh	r3, [r2, #56]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-44]
	add	r3, r3, #1
	strh	r3, [r2, #58]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-48]
	strh	r3, [r2, #60]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-52]
	strh	r3, [r2, #62]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-56]
	strh	r3, [r2, #64]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-60]
	strh	r3, [r2, #66]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	mov	r3, #0
	strh	r3, [r2, #68]	@ movhi 
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #72]
	b	.L307
.L308:
	ldr	r3, .L309
	ldr	r2, [r3, #0]
	mvn	r3, #0
	str	r3, [r2, #72]
.L307:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L310:
	.align	2
.L309:
	.word	p_dio_mmap
	.size	_Z11mcu_readrtcv, .-_Z11mcu_readrtcv
	.data
	.align	2
	.type	_ZZ7mcu_lediiE11cmd_ledctrl, %object
	.size	_ZZ7mcu_lediiE11cmd_ledctrl, 10
_ZZ7mcu_lediiE11cmd_ledctrl:
	.ascii	"\b\001\000/\002\000\000\314\000"
	.space	1
	.text
	.align	2
	.global	_Z7mcu_ledii
	.type	_Z7mcu_ledii, %function
_Z7mcu_ledii:
	@ args = 0, pretend = 0, frame = 60
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #60
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, .L312
	ldr	r3, [fp, #-16]
	strb	r3, [r2, #5]
	ldr	r2, .L312
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	moveq	r3, #0
	movne	r3, #1
	strb	r3, [r2, #6]
	ldr	r0, .L312
	bl	_Z8mcu_sendPc
	sub	r3, fp, #72
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L313:
	.align	2
.L312:
	.word	_ZZ7mcu_lediiE11cmd_ledctrl
	.size	_Z7mcu_ledii, .-_Z7mcu_ledii
	.data
	.align	2
	.type	_ZZ15mcu_devicepoweriiE15cmd_devicepower, %object
	.size	_ZZ15mcu_devicepoweriiE15cmd_devicepower, 10
_ZZ15mcu_devicepoweriiE15cmd_devicepower:
	.ascii	"\b\001\000.\002\000\000\314\000"
	.space	1
	.text
	.align	2
	.global	_Z15mcu_devicepowerii
	.type	_Z15mcu_devicepowerii, %function
_Z15mcu_devicepowerii:
	@ args = 0, pretend = 0, frame = 60
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #60
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, .L315
	ldr	r3, [fp, #-16]
	strb	r3, [r2, #5]
	ldr	r2, .L315
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	moveq	r3, #0
	movne	r3, #1
	strb	r3, [r2, #6]
	ldr	r0, .L315
	bl	_Z8mcu_sendPc
	sub	r3, fp, #72
	mov	r0, r3
	mov	r1, #50
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L316:
	.align	2
.L315:
	.word	_ZZ15mcu_devicepoweriiE15cmd_devicepower
	.size	_Z15mcu_devicepowerii, .-_Z15mcu_devicepowerii
	.data
	.align	2
	.type	_ZZ9mcu_resetvE9cmd_reset, %object
	.size	_ZZ9mcu_resetvE9cmd_reset, 8
_ZZ9mcu_resetvE9cmd_reset:
	.ascii	"\006\001\000\000\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z9mcu_resetv
	.type	_Z9mcu_resetv, %function
_Z9mcu_resetv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r0, .L321
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L318
	mov	r0, #14942208
	add	r0, r0, #57600
	add	r0, r0, #192
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L318
	bl	_Z22mcu_recv_enteracommandv
	mov	r3, r0
	cmp	r3, #0
	beq	.L318
	mov	r3, #1
	str	r3, [fp, #-16]
	b	.L317
.L318:
	mov	r3, #0
	str	r3, [fp, #-16]
.L317:
	ldr	r0, [fp, #-16]
	ldmib	sp, {fp, sp, pc}
.L322:
	.align	2
.L321:
	.word	_ZZ9mcu_resetvE9cmd_reset
	.size	_Z9mcu_resetv, .-_Z9mcu_resetv
	.data
	.align	2
	.type	_ZZ19mcu_update_firmwarePcE18cmd_updatefirmware, %object
	.size	_ZZ19mcu_update_firmwarePcE18cmd_updatefirmware, 8
_ZZ19mcu_update_firmwarePcE18cmd_updatefirmware:
	.ascii	"\005\001\000\001\002\314\000"
	.space	1
	.section	.rodata
	.align	2
.LC21:
	.ascii	"rb\000"
	.align	2
.LC22:
	.ascii	"Start update MCU firmware: %s\n\000"
	.align	2
.LC23:
	.ascii	"Reset MCU.\n\000"
	.align	2
.LC24:
	.ascii	"Failed\n\000"
	.align	2
.LC25:
	.ascii	"Done.\n\000"
	.align	2
.LC26:
	.ascii	"Erasing.\n\000"
	.align	2
.LC27:
	.ascii	"Failed.\n\000"
	.align	2
.LC28:
	.ascii	"Programming.\n\000"
	.text
	.align	2
	.global	_Z19mcu_update_firmwarePc
	.type	_Z19mcu_update_firmwarePc, %function
_Z19mcu_update_firmwarePc:
	@ args = 0, pretend = 0, frame = 232
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #232
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
	ldr	r0, [fp, #-16]
	ldr	r1, .L351
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L324
	mov	r1, #0
	str	r1, [fp, #-240]
	b	.L323
.L324:
	ldr	r0, .L351+4
	ldr	r1, [fp, #-16]
	bl	printf
	ldr	r0, .L351+8
	bl	printf
	bl	_Z9mcu_resetv
	mov	r3, r0
	cmp	r3, #0
	bne	.L325
	ldr	r0, .L351+12
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-240]
	b	.L323
.L325:
	ldr	r0, .L351+16
	bl	printf
	sub	r3, fp, #232
	mov	r0, r3
	mov	r1, #0
	mov	r2, #200
	bl	memset
	sub	r3, fp, #232
	mov	r0, r3
	mov	r1, #200
	bl	_Z12serial_writePvj
	mov	r0, #999424
	add	r0, r0, #576
	bl	_Z12serial_cleari
	ldr	r0, .L351+20
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-32]
	ldr	r0, .L351+24
	mov	r1, #5
	bl	_Z12serial_writePvj
	mov	r3, r0
	cmp	r3, #0
	beq	.L326
	mov	r0, #9961472
	add	r0, r0, #38400
	add	r0, r0, #128
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L326
	sub	r3, fp, #232
	mov	r0, r3
	mov	r1, #200
	bl	_Z11serial_readPvj
	mov	r3, r0
	str	r3, [fp, #-32]
.L326:
	ldr	r3, [fp, #-32]
	cmp	r3, #4
	ble	.L328
	mvn	r2, #224
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #5
	bne	.L328
	mvn	r2, #223
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L328
	mvn	r2, #222
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L328
	mvn	r2, #221
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L328
	mvn	r2, #220
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L328
	ldr	r0, .L351+16
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L329
.L328:
	ldr	r0, .L351+28
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-240]
	b	.L323
.L329:
	ldr	r0, .L351+32
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-20]
.L330:
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-236]
	ldr	r3, [fp, #-236]
	ldr	r3, [r3, #52]
	cmp	r3, #0
	beq	.L332
	ldr	r3, [fp, #-236]
	ldr	r2, [fp, #-236]
	ldr	r1, [r3, #16]
	ldr	r3, [r2, #24]
	cmp	r1, r3
	bcs	.L334
	ldr	r3, [fp, #-236]
	add	r1, r3, #16
	ldr	r3, [r1, #0]
	mov	r2, r3
	ldrb	r2, [r2, #0]	@ zero_extendqisi2
	str	r2, [fp, #-244]
	add	r3, r3, #1
	str	r3, [r1, #0]
	b	.L333
.L334:
	ldr	r0, [fp, #-236]
	bl	__fgetc_unlocked
	str	r0, [fp, #-244]
	b	.L333
.L332:
	ldr	r0, [fp, #-236]
	bl	fgetc
	str	r0, [fp, #-244]
.L333:
	ldr	r3, [fp, #-244]
	str	r3, [fp, #-28]
	cmn	r3, #1
	beq	.L331
	sub	r3, fp, #28
	mov	r0, r3
	mov	r1, #1
	bl	_Z12serial_writePvj
	mov	r3, r0
	cmp	r3, #1
	beq	.L336
	b	.L331
.L336:
	ldr	r3, [fp, #-28]
	cmp	r3, #10
	bne	.L330
	mov	r0, #0
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L330
	sub	r3, fp, #232
	mov	r0, r3
	mov	r1, #200
	bl	_Z11serial_readPvj
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #4
	ble	.L338
	ldrb	r3, [fp, #-232]	@ zero_extendqisi2
	cmp	r3, #5
	bne	.L338
	ldrb	r3, [fp, #-231]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L338
	ldrb	r3, [fp, #-230]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L338
	ldrb	r3, [fp, #-229]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L338
	ldrb	r3, [fp, #-228]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L338
	mov	r3, #1
	str	r3, [fp, #-20]
	b	.L331
.L338:
	ldr	r3, [fp, #-32]
	cmp	r3, #4
	ble	.L340
	mvn	r2, #224
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #5
	bne	.L340
	mvn	r2, #223
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L340
	mvn	r2, #222
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L340
	mvn	r2, #221
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L340
	mvn	r2, #220
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L340
	mov	r3, #1
	str	r3, [fp, #-20]
	b	.L331
.L340:
	ldr	r3, [fp, #-32]
	cmp	r3, #5
	ble	.L342
	ldrb	r3, [fp, #-232]	@ zero_extendqisi2
	cmp	r3, #6
	bne	.L342
	ldrb	r3, [fp, #-231]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L342
	ldrb	r3, [fp, #-230]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L342
	ldrb	r3, [fp, #-229]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L342
	ldrb	r3, [fp, #-228]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L342
	b	.L331
.L342:
	ldr	r3, [fp, #-32]
	cmp	r3, #5
	ble	.L330
	mvn	r2, #225
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #6
	bne	.L330
	mvn	r2, #224
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L330
	mvn	r2, #223
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L330
	mvn	r2, #222
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L330
	mvn	r2, #221
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L330
.L331:
	ldr	r0, [fp, #-24]
	bl	fclose
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bne	.L345
	mov	r0, #4980736
	add	r0, r0, #19200
	add	r0, r0, #64
	bl	_Z16serial_datareadyi
	mov	r3, r0
	cmp	r3, #0
	beq	.L345
	sub	r3, fp, #232
	mov	r0, r3
	mov	r1, #200
	bl	_Z11serial_readPvj
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #4
	ble	.L346
	ldrb	r3, [fp, #-232]	@ zero_extendqisi2
	cmp	r3, #5
	bne	.L346
	ldrb	r3, [fp, #-231]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L346
	ldrb	r3, [fp, #-230]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L346
	ldrb	r3, [fp, #-229]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L346
	ldrb	r3, [fp, #-228]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L346
	mov	r3, #1
	str	r3, [fp, #-20]
	b	.L345
.L346:
	ldr	r3, [fp, #-32]
	cmp	r3, #4
	ble	.L345
	mvn	r2, #224
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #5
	bne	.L345
	mvn	r2, #223
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L345
	mvn	r2, #222
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #1
	bne	.L345
	mvn	r2, #221
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L345
	mvn	r2, #220
	ldr	r3, [fp, #-32]
	sub	r1, fp, #12
	add	r3, r1, r3
	add	r3, r3, r2
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L345
	mov	r3, #1
	str	r3, [fp, #-20]
.L345:
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	beq	.L349
	ldr	r0, .L351+16
	bl	printf
	b	.L350
.L349:
	ldr	r0, .L351+28
	bl	printf
.L350:
	ldr	r3, [fp, #-20]
	str	r3, [fp, #-240]
.L323:
	ldr	r0, [fp, #-240]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L352:
	.align	2
.L351:
	.word	.LC21
	.word	.LC22
	.word	.LC23
	.word	.LC24
	.word	.LC25
	.word	.LC26
	.word	_ZZ19mcu_update_firmwarePcE18cmd_updatefirmware
	.word	.LC27
	.word	.LC28
	.size	_Z19mcu_update_firmwarePc, .-_Z19mcu_update_firmwarePc
	.align	2
	.type	_Z3bcdi, %function
_Z3bcdi:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r1, [fp, #-16]
	ldr	r3, .L354
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #2
	mov	r3, r1, asr #31
	rsb	r1, r3, r2
	ldr	r3, .L354
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #2
	mov	r3, r1, asr #31
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	rsb	r3, r3, r1
	mov	r0, r3, asl #4
	ldr	r1, [fp, #-16]
	ldr	r3, .L354
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #2
	mov	r3, r1, asr #31
	rsb	r2, r3, r2
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	rsb	r3, r3, r1
	mov	r2, r0
	add	r3, r2, r3
	and	r3, r3, #255
	mov	r0, r3
	ldmib	sp, {fp, sp, pc}
.L355:
	.align	2
.L354:
	.word	1717986919
	.size	_Z3bcdi, .-_Z3bcdi
	.data
	.align	2
	.type	_ZZ9mcu_w_rtclE10cmd_setrtc, %object
	.size	_ZZ9mcu_w_rtclE10cmd_setrtc, 16
_ZZ9mcu_w_rtclE10cmd_setrtc:
	.ascii	"\r\001\000\007\002\000"
	.space	10
	.text
	.align	2
	.global	_Z9mcu_w_rtcl
	.type	_Z9mcu_w_rtcl, %function
_Z9mcu_w_rtcl:
	@ args = 0, pretend = 0, frame = 72
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #72
	str	r0, [fp, #-20]
	sub	r3, fp, #20
	sub	r2, fp, #84
	mov	r0, r3
	mov	r1, r2
	bl	gmtime_r
	ldr	r4, .L360
	ldr	r0, [fp, #-84]
	bl	_Z3bcdi
	strb	r0, [r4, #5]
	ldr	r4, .L360
	ldr	r0, [fp, #-80]
	bl	_Z3bcdi
	strb	r0, [r4, #6]
	ldr	r4, .L360
	ldr	r0, [fp, #-76]
	bl	_Z3bcdi
	strb	r0, [r4, #7]
	ldr	r4, .L360
	ldr	r0, [fp, #-60]
	bl	_Z3bcdi
	strb	r0, [r4, #8]
	ldr	r4, .L360
	ldr	r0, [fp, #-72]
	bl	_Z3bcdi
	strb	r0, [r4, #9]
	ldr	r4, .L360
	ldr	r3, [fp, #-68]
	add	r3, r3, #1
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #10]
	ldr	r4, .L360
	ldr	r0, [fp, #-64]
	bl	_Z3bcdi
	strb	r0, [r4, #11]
	ldr	r0, .L360
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L357
	sub	r3, fp, #40
	mov	r0, r3
	mov	r1, #20
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L357
	ldrb	r3, [fp, #-37]	@ zero_extendqisi2
	cmp	r3, #7
	bne	.L357
	ldrb	r3, [fp, #-36]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L357
	mov	r3, #1
	str	r3, [fp, #-88]
	b	.L356
.L357:
	mov	r3, #0
	str	r3, [fp, #-88]
.L356:
	ldr	r0, [fp, #-88]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L361:
	.align	2
.L360:
	.word	_ZZ9mcu_w_rtclE10cmd_setrtc
	.size	_Z9mcu_w_rtcl, .-_Z9mcu_w_rtcl
	.data
	.align	2
	.type	_ZZ10mcu_setrtcvE10cmd_setrtc, %object
	.size	_ZZ10mcu_setrtcvE10cmd_setrtc, 16
_ZZ10mcu_setrtcvE10cmd_setrtc:
	.ascii	"\r\001\000\007\002\000"
	.space	10
	.text
	.align	2
	.global	_Z10mcu_setrtcv
	.type	_Z10mcu_setrtcv, %function
_Z10mcu_setrtcv:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #66]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #5]
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #64]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #6]
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #62]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #7]
	ldr	r2, .L366
	mov	r3, #0
	strb	r3, [r2, #8]
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #60]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #9]
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #58]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #10]
	ldr	r4, .L366
	ldr	r3, .L366+4
	ldr	r3, [r3, #0]
	ldrh	r3, [r3, #56]
	mov	r0, r3
	bl	_Z3bcdi
	strb	r0, [r4, #11]
	ldr	r0, .L366
	bl	_Z8mcu_sendPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L363
	sub	r3, fp, #36
	mov	r0, r3
	mov	r1, #20
	mov	r2, #99328
	add	r2, r2, #672
	bl	_Z8mcu_recvPcii
	mov	r3, r0
	cmp	r3, #0
	beq	.L363
	ldrb	r3, [fp, #-33]	@ zero_extendqisi2
	cmp	r3, #7
	bne	.L363
	ldrb	r3, [fp, #-32]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L363
	ldr	r3, .L366+4
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #72]
	b	.L362
.L363:
	ldr	r3, .L366+4
	ldr	r2, [r3, #0]
	mvn	r3, #0
	str	r3, [r2, #72]
.L362:
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L367:
	.align	2
.L366:
	.word	_ZZ10mcu_setrtcvE10cmd_setrtc
	.word	p_dio_mmap
	.size	_Z10mcu_setrtcv, .-_Z10mcu_setrtcv
	.align	2
	.global	_Z11mcu_syncrtcv
	.type	_Z11mcu_syncrtcv, %function
_Z11mcu_syncrtcv:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	sub	r3, fp, #24
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	mvn	r3, #11
	sub	r2, fp, #12
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	str	r3, [fp, #-16]
	ldr	r0, [fp, #-16]
	bl	_Z9mcu_w_rtcl
	mov	r3, r0
	cmp	r3, #0
	beq	.L369
	ldr	r3, .L371
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #72]
	b	.L368
.L369:
	ldr	r3, .L371
	ldr	r2, [r3, #0]
	mvn	r3, #0
	str	r3, [r2, #72]
.L368:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L372:
	.align	2
.L371:
	.word	p_dio_mmap
	.size	_Z11mcu_syncrtcv, .-_Z11mcu_syncrtcv
	.align	2
	.global	_Z12mcu_rtctosysv
	.type	_Z12mcu_rtctosysv, %function
_Z12mcu_rtctosysv:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	mov	r3, #0
	str	r3, [fp, #-24]
.L374:
	ldr	r3, [fp, #-24]
	cmp	r3, #9
	bgt	.L373
	mov	r0, #0
	bl	_Z9mcu_r_rtcP2tm
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	ble	.L376
	ldr	r3, [fp, #-28]
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	settimeofday
	ldr	r0, [fp, #-20]
	bl	_Z7rtc_setl
	b	.L373
.L376:
	ldr	r3, [fp, #-24]
	add	r3, r3, #1
	str	r3, [fp, #-24]
	b	.L374
.L373:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z12mcu_rtctosysv, .-_Z12mcu_rtctosysv
	.global	__fixdfsi
	.section	.rodata
	.align	2
.LC29:
	.ascii	"Time sync use GPS time.\000"
	.text
	.align	2
	.global	_Z12time_syncgpsv
	.type	_Z12time_syncgpsv, %function
_Z12time_syncgpsv:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	bl	_Z8dio_lockv
	ldr	r3, .L385
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #76]
	cmp	r3, #0
	beq	.L379
	ldr	r3, .L385
	ldr	r2, [r3, #0]
	mov	r3, #112
	add	r3, r2, r3
	ldmia	r3, {r0-r1}
	bl	__fixdfsi
	mov	r3, r0
	str	r3, [fp, #-28]
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-20]
	rsb	r3, r3, r2
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #5
	bgt	.L381
	ldr	r3, [fp, #-24]
	cmn	r3, #5
	blt	.L381
	b	.L380
.L381:
	ldr	r3, [fp, #-28]
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	settimeofday
	ldr	r0, .L385+4
	bl	_Z7dvr_logPcz
	b	.L382
.L380:
	ldr	r3, [fp, #-24]
	cmp	r3, #1
	bgt	.L384
	ldr	r3, [fp, #-24]
	cmn	r3, #1
	blt	.L384
	b	.L382
.L384:
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-16]
	sub	r3, fp, #20
	mov	r0, r3
	mov	r1, #0
	bl	adjtime
.L382:
	ldr	r0, [fp, #-28]
	bl	_Z9mcu_w_rtcl
	ldr	r0, [fp, #-28]
	bl	_Z7rtc_setl
.L379:
	bl	_Z10dio_unlockv
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L386:
	.align	2
.L385:
	.word	p_dio_mmap
	.word	.LC29
	.size	_Z12time_syncgpsv, .-_Z12time_syncgpsv
	.section	.rodata
	.align	2
.LC30:
	.ascii	"Time sync use MCU time.\000"
	.text
	.align	2
	.global	_Z12time_syncmcuv
	.type	_Z12time_syncmcuv, %function
_Z12time_syncmcuv:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	mov	r0, #0
	bl	_Z9mcu_r_rtcP2tm
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	ble	.L387
	sub	r3, fp, #24
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	mvn	r3, #11
	sub	r1, fp, #12
	add	r3, r1, r3
	ldr	r2, [fp, #-28]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #5
	bgt	.L390
	ldr	r3, [fp, #-16]
	cmn	r3, #5
	blt	.L390
	b	.L389
.L390:
	mvn	r3, #11
	sub	r1, fp, #12
	add	r2, r1, r3
	ldr	r3, [fp, #-28]
	str	r3, [r2, #0]
	mvn	r3, #11
	sub	r1, fp, #12
	add	r2, r1, r3
	mov	r3, #0
	str	r3, [r2, #4]
	sub	r3, fp, #24
	mov	r0, r3
	mov	r1, #0
	bl	settimeofday
	ldr	r0, .L394
	bl	_Z7dvr_logPcz
	b	.L391
.L389:
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bgt	.L393
	ldr	r3, [fp, #-16]
	cmn	r3, #1
	blt	.L393
	b	.L391
.L393:
	mvn	r3, #11
	sub	r1, fp, #12
	add	r2, r1, r3
	ldr	r3, [fp, #-16]
	str	r3, [r2, #0]
	mvn	r3, #11
	sub	r1, fp, #12
	add	r2, r1, r3
	mov	r3, #0
	str	r3, [r2, #4]
	sub	r3, fp, #24
	mov	r0, r3
	mov	r1, #0
	bl	adjtime
.L391:
	ldr	r0, [fp, #-28]
	bl	_Z7rtc_setl
.L387:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L395:
	.align	2
.L394:
	.word	.LC30
	.size	_Z12time_syncmcuv, .-_Z12time_syncmcuv
	.align	2
	.global	_Z11dvrsvr_downv
	.type	_Z11dvrsvr_downv, %function
_Z11dvrsvr_downv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r3, .L400
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	str	r3, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	ble	.L396
	ldr	r0, [fp, #-16]
	mov	r1, #10
	bl	kill
.L398:
	ldr	r3, .L400
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	beq	.L399
	ldr	r3, .L400
	ldr	r2, [r3, #0]
	ldr	r3, .L400
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	eor	r3, r3, #16
	str	r3, [r2, #32]
	mov	r0, #199680
	add	r0, r0, #320
	bl	_Z9mcu_inputi
	bl	_Z11mcu_doutputv
	b	.L398
.L399:
	bl	sync
.L396:
	ldmib	sp, {fp, sp, pc}
.L401:
	.align	2
.L400:
	.word	p_dio_mmap
	.size	_Z11dvrsvr_downv, .-_Z11dvrsvr_downv
	.local	_ZZ10setnetworkvE11pid_network
	.comm	_ZZ10setnetworkvE11pid_network,4,4
	.data
	.align	2
	.type	_ZZ10setnetworkvE7snwpath, %object
	.size	_ZZ10setnetworkvE7snwpath, 24
_ZZ10setnetworkvE7snwpath:
	.ascii	"/davinci/dvr/setnetwork\000"
	.text
	.align	2
	.global	_Z10setnetworkv
	.type	_Z10setnetworkv, %function
_Z10setnetworkv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r3, .L405
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L403
	ldr	r3, .L405
	ldr	r0, [r3, #0]
	mov	r1, #15
	bl	kill
	ldr	r3, .L405
	sub	r2, fp, #16
	ldr	r0, [r3, #0]
	mov	r1, r2
	mov	r2, #0
	bl	waitpid
.L403:
	bl	fork
	mov	r2, r0
	ldr	r3, .L405
	str	r2, [r3, #0]
	ldr	r3, .L405
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L402
	ldr	r0, .L405+4
	ldr	r1, .L405+4
	mov	r2, #0
	bl	execl
	mov	r0, #0
	bl	exit
.L402:
	ldmib	sp, {fp, sp, pc}
.L406:
	.align	2
.L405:
	.word	_ZZ10setnetworkvE11pid_network
	.word	_ZZ10setnetworkvE7snwpath
	.size	_Z10setnetworkv, .-_Z10setnetworkv
	.local	buzzer_timer
	.comm	buzzer_timer,4,4
	.local	buzzer_count
	.comm	buzzer_count,4,4
	.local	buzzer_on
	.comm	buzzer_on,4,4
	.local	buzzer_ontime
	.comm	buzzer_ontime,4,4
	.local	buzzer_offtime
	.comm	buzzer_offtime,4,4
	.align	2
	.global	_Z6buzzeriii
	.type	_Z6buzzeriii, %function
_Z6buzzeriii:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	str	r0, [fp, #-20]
	str	r1, [fp, #-24]
	str	r2, [fp, #-28]
	ldr	r3, .L409
	ldr	r2, [fp, #-20]
	ldr	r3, [r3, #0]
	cmp	r2, r3
	blt	.L407
	ldr	r4, .L409+4
	bl	_Z10getruntimev
	str	r0, [r4, #0]
	ldr	r2, .L409
	ldr	r3, [fp, #-20]
	str	r3, [r2, #0]
	ldr	r2, .L409+8
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r2, .L409+12
	ldr	r3, [fp, #-24]
	str	r3, [r2, #0]
	ldr	r2, .L409+16
	ldr	r3, [fp, #-28]
	str	r3, [r2, #0]
.L407:
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L410:
	.align	2
.L409:
	.word	buzzer_count
	.word	buzzer_timer
	.word	buzzer_on
	.word	buzzer_ontime
	.word	buzzer_offtime
	.size	_Z6buzzeriii, .-_Z6buzzeriii
	.align	2
	.global	_Z10buzzer_runi
	.type	_Z10buzzer_runi, %function
_Z10buzzer_runi:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r3, .L415
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L411
	ldr	r3, .L415+4
	ldr	r2, [fp, #-16]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	cmp	r3, #0
	blt	.L411
	ldr	r3, .L415+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L413
	ldr	r2, .L415
	ldr	r3, .L415
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	str	r3, [r2, #0]
	ldr	r1, .L415+4
	ldr	r3, .L415+12
	ldr	r2, [fp, #-16]
	ldr	r3, [r3, #0]
	add	r3, r2, r3
	str	r3, [r1, #0]
	ldr	r3, .L415+16
	ldr	r2, [r3, #0]
	ldr	r3, .L415+16
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #256
	str	r3, [r2, #32]
	b	.L414
.L413:
	ldr	r1, .L415+4
	ldr	r3, .L415+20
	ldr	r2, [fp, #-16]
	ldr	r3, [r3, #0]
	add	r3, r2, r3
	str	r3, [r1, #0]
	ldr	r3, .L415+16
	ldr	r2, [r3, #0]
	ldr	r3, .L415+16
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	orr	r3, r3, #256
	str	r3, [r2, #32]
.L414:
	ldr	r2, .L415+8
	ldr	r3, .L415+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	movne	r3, #0
	moveq	r3, #1
	str	r3, [r2, #0]
.L411:
	ldmib	sp, {fp, sp, pc}
.L416:
	.align	2
.L415:
	.word	buzzer_count
	.word	buzzer_timer
	.word	buzzer_on
	.word	buzzer_offtime
	.word	p_dio_mmap
	.word	buzzer_ontime
	.size	_Z10buzzer_runi, .-_Z10buzzer_runi
	.global	smartftp_retry
	.bss
	.align	2
	.type	smartftp_retry, %object
	.size	smartftp_retry, 4
smartftp_retry:
	.space	4
	.global	smartftp_disable
	.align	2
	.type	smartftp_disable, %object
	.size	smartftp_disable, 4
smartftp_disable:
	.space	4
	.global	smartftp_reporterror
	.align	2
	.type	smartftp_reporterror, %object
	.size	smartftp_reporterror, 4
smartftp_reporterror:
	.space	4
	.section	.rodata
	.align	2
.LC31:
	.ascii	"Start smart server uploading.\000"
	.align	2
.LC32:
	.ascii	"/davinci/dvr/setnetwork\000"
	.align	2
.LC33:
	.ascii	"TZ\000"
	.align	2
.LC36:
	.ascii	"247SECURITY\000"
	.align	2
.LC37:
	.ascii	"/var/dvr/disks\000"
	.align	2
.LC38:
	.ascii	"510\000"
	.align	2
.LC39:
	.ascii	"247ftp\000"
	.align	2
.LC40:
	.ascii	"0\000"
	.align	2
.LC41:
	.ascii	"Y\000"
	.align	2
.LC42:
	.ascii	"N\000"
	.align	2
.LC34:
	.ascii	"/davinci/dvr/smartftp\000"
	.align	2
.LC35:
	.ascii	"rausb0\000"
	.align	2
.LC43:
	.ascii	"Start smart server uploading failed!\000"
	.text
	.align	2
	.global	_Z14smartftp_startv
	.type	_Z14smartftp_startv, %function
_Z14smartftp_startv:
	@ args = 0, pretend = 0, frame = 140
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #176
	ldr	r0, .L423
	bl	_Z7dvr_logPcz
	bl	fork
	mov	r2, r0
	ldr	r3, .L423+4
	str	r2, [r3, #0]
	ldr	r3, .L423+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L418
	sub	r3, fp, #140
	mov	r0, r3
	mov	r1, #128
	bl	gethostname
	ldr	r0, .L423+8
	bl	system
	ldr	r0, .L423+12
	bl	getenv
	str	r0, [fp, #-144]
	sub	r3, fp, #140
	str	r3, [fp, #-148]
	ldr	r3, .L423+16
	str	r3, [sp, #0]
	ldr	r3, .L423+20
	str	r3, [sp, #4]
	ldr	r3, .L423+24
	str	r3, [sp, #8]
	ldr	r3, .L423+28
	str	r3, [sp, #12]
	ldr	r3, .L423+16
	str	r3, [sp, #16]
	ldr	r3, .L423+32
	str	r3, [sp, #20]
	ldr	r3, .L423+36
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L419
	ldr	r3, .L423+40
	str	r3, [fp, #-152]
	b	.L420
.L419:
	ldr	r3, .L423+44
	str	r3, [fp, #-152]
.L420:
	ldr	r3, [fp, #-152]
	str	r3, [sp, #24]
	ldr	r3, [fp, #-144]
	str	r3, [sp, #28]
	mov	r3, #0
	str	r3, [sp, #32]
	ldr	r0, .L423+48
	ldr	r1, .L423+48
	ldr	r2, .L423+52
	ldr	r3, [fp, #-148]
	bl	execl
	ldr	r0, .L423+56
	bl	_Z7dvr_logPcz
	mov	r0, #101
	bl	exit
.L418:
	ldr	r3, .L423+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bge	.L417
	ldr	r2, .L423+4
	mov	r3, #0
	str	r3, [r2, #0]
.L417:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L424:
	.align	2
.L423:
	.word	.LC31
	.word	pid_smartftp
	.word	.LC32
	.word	.LC33
	.word	.LC36
	.word	.LC37
	.word	.LC38
	.word	.LC39
	.word	.LC40
	.word	smartftp_reporterror
	.word	.LC41
	.word	.LC42
	.word	.LC34
	.word	.LC35
	.word	.LC43
	.size	_Z14smartftp_startv, .-_Z14smartftp_startv
	.section	.rodata
	.align	2
.LC44:
	.ascii	"\"smartftp\" exit. (code:%d)\000"
	.align	2
.LC45:
	.ascii	"\"smartftp\" failed, retry...\000"
	.align	2
.LC46:
	.ascii	"\"smartftp\" killed by signal %d.\000"
	.align	2
.LC47:
	.ascii	"\"smartftp\" aborted.\000"
	.text
	.align	2
	.global	_Z13smartftp_waitv
	.type	_Z13smartftp_waitv, %function
_Z13smartftp_waitv:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	mov	r3, #0
	str	r3, [fp, #-16]
	mvn	r3, #0
	str	r3, [fp, #-24]
	ldr	r3, .L435
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L425
	ldr	r3, .L435
	sub	r2, fp, #16
	ldr	r0, [r3, #0]
	mov	r1, r2
	mov	r2, #1
	bl	waitpid
	mov	r3, r0
	str	r3, [fp, #-20]
	ldr	r3, .L435
	ldr	r2, [fp, #-20]
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bne	.L425
	ldr	r2, .L435
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r3, [fp, #-16]
	and	r3, r3, #127
	cmp	r3, #0
	bne	.L428
	ldr	r3, [fp, #-16]
	and	r3, r3, #65280
	mov	r3, r3, asr #8
	str	r3, [fp, #-24]
	ldr	r0, .L435+4
	ldr	r1, [fp, #-24]
	bl	_Z7dvr_logPcz
	ldr	r3, [fp, #-24]
	cmp	r3, #3
	beq	.L430
	ldr	r3, [fp, #-24]
	cmp	r3, #6
	beq	.L430
	b	.L425
.L430:
	ldr	r2, .L435+8
	ldr	r3, .L435+8
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #0
	ble	.L425
	ldr	r0, .L435+12
	bl	_Z7dvr_logPcz
	bl	_Z14smartftp_startv
	b	.L425
.L428:
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-28]
	mov	r3, #0
	strb	r3, [fp, #-29]
	ldr	r3, [fp, #-28]
	and	r3, r3, #255
	cmp	r3, #127
	beq	.L433
	ldr	r3, [fp, #-28]
	and	r3, r3, #127
	cmp	r3, #0
	beq	.L433
	mov	r3, #1
	strb	r3, [fp, #-29]
.L433:
	ldrb	r3, [fp, #-29]
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L432
	ldr	r3, [fp, #-16]
	and	r3, r3, #127
	ldr	r0, .L435+16
	mov	r1, r3
	bl	_Z7dvr_logPcz
	b	.L425
.L432:
	ldr	r0, .L435+20
	bl	_Z7dvr_logPcz
.L425:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L436:
	.align	2
.L435:
	.word	pid_smartftp
	.word	.LC44
	.word	smartftp_retry
	.word	.LC45
	.word	.LC46
	.word	.LC47
	.size	_Z13smartftp_waitv, .-_Z13smartftp_waitv
	.section	.rodata
	.align	2
.LC48:
	.ascii	"Kill \"smartftp\".\000"
	.text
	.align	2
	.global	_Z13smartftp_killv
	.type	_Z13smartftp_killv, %function
_Z13smartftp_killv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r3, .L439
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L437
	ldr	r3, .L439
	ldr	r0, [r3, #0]
	mov	r1, #15
	bl	kill
	ldr	r0, .L439+4
	bl	_Z7dvr_logPcz
	ldr	r2, .L439
	mov	r3, #0
	str	r3, [r2, #0]
.L437:
	ldmfd	sp, {fp, sp, pc}
.L440:
	.align	2
.L439:
	.word	pid_smartftp
	.word	.LC48
	.size	_Z13smartftp_killv, .-_Z13smartftp_killv
	.global	g_syncrtc
	.bss
	.align	2
	.type	g_syncrtc, %object
	.size	g_syncrtc, 4
g_syncrtc:
	.space	4
	.local	_ZZ8cpuusagevE9s_cputime
	.comm	_ZZ8cpuusagevE9s_cputime,4,4
	.local	_ZZ8cpuusagevE10s_idletime
	.comm	_ZZ8cpuusagevE10s_idletime,4,4
	.global	__subsf3
	.section	.rodata
	.align	2
.LC49:
	.ascii	"/proc/uptime\000"
	.align	2
.LC50:
	.ascii	"%f %f\000"
	.text
	.align	2
	.global	_Z8cpuusagev
	.type	_Z8cpuusagev, %function
_Z8cpuusagev:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	ldr	r3, .L443	@ float
	str	r3, [fp, #-28]	@ float
	ldr	r0, .L443+4
	ldr	r1, .L443+8
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	beq	.L442
	sub	r3, fp, #20
	sub	ip, fp, #24
	ldr	r0, [fp, #-32]
	ldr	r1, .L443+12
	mov	r2, r3
	mov	r3, ip
	bl	fscanf
	ldr	r0, [fp, #-32]
	bl	fclose
	ldr	r3, .L443+16
	ldr	r0, [fp, #-24]	@ float
	ldr	r1, [r3, #0]	@ float
	bl	__subsf3
	mov	r4, r0
	ldr	r3, .L443+20
	ldr	r0, [fp, #-20]	@ float
	ldr	r1, [r3, #0]	@ float
	bl	__subsf3
	mov	r3, r0
	mov	r0, r4
	mov	r1, r3
	bl	__divsf3
	mov	r3, r0
	ldr	r0, .L443+24	@ float
	mov	r1, r3
	bl	__subsf3
	mov	r3, r0
	str	r3, [fp, #-28]	@ float
	ldr	r2, .L443+20
	ldr	r3, [fp, #-20]	@ float
	str	r3, [r2, #0]	@ float
	ldr	r2, .L443+16
	ldr	r3, [fp, #-24]	@ float
	str	r3, [r2, #0]	@ float
.L442:
	ldr	r3, [fp, #-28]	@ float
	mov	r0, r3
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L444:
	.align	2
.L443:
	.word	0
	.word	.LC49
	.word	.LC2
	.word	.LC50
	.word	_ZZ8cpuusagevE10s_idletime
	.word	_ZZ8cpuusagevE9s_cputime
	.word	1065353216
	.size	_Z8cpuusagev, .-_Z8cpuusagev
	.section	.rodata
	.align	2
.LC51:
	.ascii	"timezone\000"
	.align	2
.LC52:
	.ascii	"timezones\000"
	.align	2
.LC53:
	.ascii	"logfile\000"
	.align	2
.LC54:
	.ascii	"temp_logfile\000"
	.align	2
.LC55:
	.ascii	"iomapfile\000"
	.align	2
.LC56:
	.ascii	"Can't create io map file!\n\000"
	.align	2
.LC57:
	.ascii	"IO memory map failed!\000"
	.align	2
.LC58:
	.ascii	"io\000"
	.align	2
.LC59:
	.ascii	"inputnum\000"
	.align	2
.LC60:
	.ascii	"outputnum\000"
	.align	2
.LC61:
	.ascii	"output%d_inverted\000"
	.align	2
.LC62:
	.ascii	"syncrtc\000"
	.align	2
.LC63:
	.ascii	"serialport\000"
	.align	2
.LC64:
	.ascii	"serialbaudrate\000"
	.align	2
.LC65:
	.ascii	"smartftp_disable\000"
	.align	2
.LC66:
	.ascii	"MCU UP!\n\000"
	.align	2
.LC67:
	.ascii	"MCU ready.\000"
	.align	2
.LC68:
	.ascii	"MCU failed!\n\000"
	.align	2
.LC69:
	.ascii	"MCU failed.\000"
	.align	2
.LC70:
	.ascii	"glog\000"
	.align	2
.LC71:
	.ascii	"gforce_log_enable\000"
	.align	2
.LC72:
	.ascii	"gsensor_forward\000"
	.align	2
.LC73:
	.ascii	"gsensor_upward\000"
	.align	2
.LC74:
	.ascii	"gsensor_forward_trigger\000"
	.align	2
.LC75:
	.ascii	"%f\000"
	.align	2
.LC76:
	.ascii	"gsensor_backward_trigger\000"
	.align	2
.LC77:
	.ascii	"gsensor_right_trigger\000"
	.align	2
.LC78:
	.ascii	"gsensor_left_trigger\000"
	.align	2
.LC79:
	.ascii	"gsensor_down_trigger\000"
	.align	2
.LC80:
	.ascii	"gsensor_up_trigger\000"
	.align	2
.LC81:
	.ascii	"G sensor detected!\000"
	.align	2
.LC82:
	.ascii	"/var/dvr/gsensor\000"
	.align	2
.LC83:
	.ascii	"w\000"
	.align	2
.LC84:
	.ascii	"1\000"
	.align	2
.LC85:
	.ascii	"usewatchdog\000"
	.align	2
.LC86:
	.ascii	"watchdogtimeout\000"
	.align	2
.LC87:
	.ascii	"%d\000"
	.text
	.align	2
	.global	_Z7appinitv
	.type	_Z7appinitv, %function
_Z7appinitv:
	@ args = 0, pretend = 0, frame = 204
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #212
	ldr	r3, .L507
	str	r3, [fp, #-184]
	ldr	r3, .L507+4
	str	r3, [fp, #-180]
	sub	r3, fp, #176
	sub	r2, fp, #40
	str	r2, [r3, #0]
	ldr	r2, .L507+8
	str	r2, [r3, #4]
	str	sp, [r3, #8]
	sub	r3, fp, #208
	mov	r0, r3
	bl	_Unwind_SjLj_Register
	sub	r2, fp, #76
	mvn	r3, #0
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+12
	bl	_ZN6configC1EPc
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+16
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+16
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+16
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+20
	ldr	r2, .L507+24
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #88
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L446
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r2, r0
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+40
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #92
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L447
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	mov	r1, #32
	bl	strchr
	mov	r3, r0
	str	r3, [fp, #-52]
	ldr	r3, [fp, #-52]
	cmp	r3, #0
	beq	.L448
	ldr	r3, [fp, #-52]
	mov	r2, #0
	strb	r2, [r3, #0]
.L448:
	sub	r3, fp, #92
	mov	r2, #1
	str	r2, [fp, #-204]
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	mov	r1, #9
	bl	strchr
	mov	r3, r0
	str	r3, [fp, #-52]
	ldr	r3, [fp, #-52]
	cmp	r3, #0
	beq	.L449
	ldr	r3, [fp, #-52]
	mov	r2, #0
	strb	r2, [r3, #0]
.L449:
	sub	r3, fp, #92
	mov	r2, #1
	str	r2, [fp, #-204]
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L507+44
	mov	r1, r3
	mov	r2, #1
	bl	setenv
	b	.L446
.L447:
	sub	r3, fp, #88
	mov	r2, #1
	str	r2, [fp, #-204]
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L507+44
	mov	r1, r3
	mov	r2, #1
	bl	setenv
.L446:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+20
	ldr	r2, .L507+48
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L451
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L507+52
	mov	r1, r3
	mov	r2, #128
	bl	strncpy
.L451:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+20
	ldr	r2, .L507+56
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L452
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L507+60
	mov	r1, r3
	mov	r2, #128
	bl	strncpy
.L452:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+20
	ldr	r2, .L507+64
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	str	r3, [fp, #-96]
	ldr	r3, [fp, #-96]
	cmp	r3, #0
	beq	.L453
	ldr	r3, [fp, #-96]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L453
	ldr	r0, .L507+68
	ldr	r1, [fp, #-96]
	mov	r2, #256
	bl	strncpy
.L453:
	ldr	r0, .L507+68
	mov	r1, #66
	mov	r2, #448
	bl	open
	mov	r3, r0
	str	r3, [fp, #-48]
	ldr	r3, [fp, #-48]
	cmp	r3, #0
	bgt	.L454
	mov	r3, #1
	str	r3, [fp, #-204]
	ldr	r0, .L507+72
	bl	printf
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #76
	mvn	r3, #0
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r3, .L507+292
	mov	lr, pc
	mov	pc, r3
	mov	r3, #0
	str	r3, [fp, #-212]
	b	.L445
.L454:
	ldr	r0, [fp, #-48]
	mov	r1, #172
	bl	ftruncate
	ldr	r3, [fp, #-48]
	str	r3, [sp, #0]
	mov	r3, #0
	str	r3, [sp, #4]
	mov	r0, #0
	mov	r1, #172
	mov	r2, #3
	mov	r3, #1
	bl	mmap
	str	r0, [fp, #-52]
	ldr	r0, [fp, #-48]
	bl	close
	ldr	r3, [fp, #-52]
	cmn	r3, #1
	beq	.L456
	ldr	r3, [fp, #-52]
	cmp	r3, #0
	bne	.L455
.L456:
	mov	r3, #1
	str	r3, [fp, #-204]
	ldr	r0, .L507+76
	bl	printf
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #76
	mvn	r3, #0
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r3, .L507+292
	mov	lr, pc
	mov	pc, r3
	mov	r2, #0
	str	r2, [fp, #-212]
	b	.L445
.L455:
	ldr	r2, .L507+80
	ldr	r3, [fp, #-52]
	str	r3, [r2, #0]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bgt	.L457
	ldr	r3, .L507+80
	ldr	r0, [r3, #0]
	mov	r1, #0
	mov	r2, #172
	bl	memset
.L457:
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #8]
	cmp	r3, #0
	ble	.L458
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #8]
	str	r3, [fp, #-100]
	ldr	r0, [fp, #-100]
	mov	r1, #15
	bl	kill
	mov	r3, r0
	cmp	r3, #0
	bne	.L459
	ldr	r0, [fp, #-100]
	mov	r1, #0
	mov	r2, #0
	bl	waitpid
	bl	sync
	mov	r0, #2
	bl	sleep
	b	.L458
.L459:
	mov	r0, #5
	bl	sleep
.L458:
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	str	r3, [fp, #-216]
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+88
	bl	_ZN6config11getvalueintEPcS0_
	ldr	r3, [fp, #-216]
	str	r0, [r3, #20]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #20]
	cmp	r3, #0
	ble	.L462
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #20]
	cmp	r3, #32
	bgt	.L462
	b	.L461
.L462:
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #6
	str	r3, [r2, #20]
.L461:
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	str	r3, [fp, #-220]
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+92
	bl	_ZN6config11getvalueintEPcS0_
	ldr	r2, [fp, #-220]
	str	r0, [r2, #28]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #28]
	cmp	r3, #0
	ble	.L464
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #28]
	cmp	r3, #32
	bgt	.L464
	b	.L463
.L464:
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #6
	str	r3, [r2, #28]
.L463:
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #120]
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #65280
	add	r3, r3, #255
	str	r3, [r2, #124]
	ldr	r3, .L507+80
	ldr	r3, [r3, #0]
	str	r3, [fp, #-224]
	bl	getpid
	ldr	r3, [fp, #-224]
	str	r0, [r3, #8]
	ldr	r2, .L507+96
	mov	r3, #0
	str	r3, [r2, #0]
	mov	r3, #0
	str	r3, [fp, #-84]
.L465:
	ldr	r3, [fp, #-84]
	cmp	r3, #31
	bgt	.L466
	sub	r2, fp, #152
	ldr	r3, [fp, #-84]
	add	r3, r3, #1
	mov	r0, r2
	ldr	r1, .L507+100
	mov	r2, r3
	bl	sprintf
	sub	r2, fp, #76
	sub	ip, fp, #152
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	mov	r2, ip
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	cmp	r3, #0
	beq	.L467
	ldr	r1, .L507+96
	mov	r2, #1
	ldr	r3, [fp, #-84]
	mov	r2, r2, asl r3
	ldr	r3, .L507+96
	ldr	r3, [r3, #0]
	orr	r3, r2, r3
	str	r3, [r1, #0]
.L467:
	ldr	r3, [fp, #-84]
	add	r3, r3, #1
	str	r3, [fp, #-84]
	b	.L465
.L466:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+104
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L507+108
	str	r3, [r2, #0]
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+84
	ldr	r2, .L507+112
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L469
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L507+116
	mov	r1, r3
	bl	strcpy
.L469:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+120
	bl	_ZN6config11getvalueintEPcS0_
	mov	r2, r0
	ldr	r3, .L507+124
	str	r2, [r3, #0]
	ldr	r3, .L507+124
	ldr	r2, [r3, #0]
	mov	r3, #2384
	add	r3, r3, #15
	cmp	r2, r3
	ble	.L471
	ldr	r3, .L507+124
	ldr	r2, [r3, #0]
	mov	r3, #114688
	add	r3, r3, #512
	cmp	r2, r3
	bgt	.L471
	b	.L470
.L471:
	ldr	r2, .L507+124
	mov	r3, #114688
	add	r3, r3, #512
	str	r3, [r2, #0]
.L470:
	mov	r3, #1
	str	r3, [fp, #-204]
	bl	_Z11serial_initv
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+20
	ldr	r2, .L507+128
	bl	_ZN6config11getvalueintEPcS0_
	mov	r2, r0
	ldr	r3, .L507+132
	str	r2, [r3, #0]
	bl	_Z15mcu_bootupreadyv
	mov	r3, r0
	cmp	r3, #0
	beq	.L472
	ldr	r0, .L507+136
	bl	printf
	ldr	r0, .L507+140
	bl	_Z7dvr_logPcz
	bl	_Z12mcu_readcodev
	b	.L473
.L472:
	mov	r3, #1
	str	r3, [fp, #-204]
	ldr	r0, .L507+144
	bl	printf
	ldr	r0, .L507+148
	bl	_Z7dvr_logPcz
.L473:
	ldr	r3, .L507+108
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L474
	mov	r3, #1
	str	r3, [fp, #-204]
	bl	_Z12mcu_rtctosysv
.L474:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+152
	ldr	r2, .L507+156
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L507+160
	str	r3, [r2, #0]
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+84
	ldr	r2, .L507+164
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-100]
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+84
	ldr	r2, .L507+168
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-156]
	ldr	r2, .L507+172
	mov	r3, #7
	str	r3, [r2, #0]
	mov	r3, #0
	str	r3, [fp, #-84]
.L475:
	ldr	r3, [fp, #-84]
	cmp	r3, #23
	bgt	.L476
	ldr	r2, .L507+176
	ldr	r3, [fp, #-84]
	ldrb	r2, [r2, r3, asl #1]	@ zero_extendqisi2
	ldr	r3, [fp, #-100]
	cmp	r2, r3
	bne	.L477
	ldr	r2, .L507+176
	ldr	r3, [fp, #-84]
	mov	r1, #1
	mov	r3, r3, asl #1
	add	r3, r3, r2
	add	r3, r3, r1
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r3, [fp, #-156]
	cmp	r2, r3
	bne	.L477
	ldr	r2, .L507+172
	ldr	r3, [fp, #-84]
	str	r3, [r2, #0]
	b	.L476
.L477:
	ldr	r3, [fp, #-84]
	add	r3, r3, #1
	str	r3, [fp, #-84]
	b	.L475
.L476:
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #136]
	ldr	r3, .L507+80
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #152]
	ldr	r2, .L507+180
	ldr	r3, .L507+184	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+188
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L479
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+180
	bl	sscanf
.L479:
	ldr	r2, .L507+196
	ldr	r3, .L507+200	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+204
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L480
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+196
	bl	sscanf
.L480:
	ldr	r2, .L507+208
	ldr	r3, .L507+184	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+212
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L481
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+208
	bl	sscanf
.L481:
	ldr	r2, .L507+216
	ldr	r3, .L507+200	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+220
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L482
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+216
	bl	sscanf
.L482:
	ldr	r2, .L507+224
	ldr	r3, .L507+228	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+232
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L483
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+224
	bl	sscanf
.L483:
	ldr	r2, .L507+236
	ldr	r3, .L507+240	@ float
	str	r3, [r2, #0]	@ float
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+244
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #80
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L507+28
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L484
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L507+192
	ldr	r2, .L507+236
	bl	sscanf
.L484:
	mov	r3, #1
	str	r3, [fp, #-204]
	bl	_Z15mcu_gsensorinitv
	mov	r3, r0
	cmp	r3, #0
	beq	.L485
	ldr	r0, .L507+248
	bl	_Z7dvr_logPcz
	ldr	r0, .L507+252
	ldr	r1, .L507+256
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-44]
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	beq	.L485
	ldr	r0, [fp, #-44]
	ldr	r1, .L507+260
	bl	fprintf
	ldr	r0, [fp, #-44]
	bl	fclose
.L485:
	sub	r2, fp, #76
	mov	r3, #1
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r1, .L507+84
	ldr	r2, .L507+264
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L507+268
	str	r3, [r2, #0]
	sub	r3, fp, #76
	mov	r0, r3
	ldr	r1, .L507+84
	ldr	r2, .L507+272
	bl	_ZN6config11getvalueintEPcS0_
	mov	r2, r0
	ldr	r3, .L507+276
	str	r2, [r3, #0]
	ldr	r3, .L507+276
	ldr	r3, [r3, #0]
	cmp	r3, #9
	bgt	.L487
	ldr	r2, .L507+276
	mov	r3, #10
	str	r3, [r2, #0]
.L487:
	ldr	r3, .L507+276
	ldr	r3, [r3, #0]
	cmp	r3, #200
	ble	.L488
	ldr	r2, .L507+276
	mov	r3, #200
	str	r3, [r2, #0]
.L488:
	ldr	r2, .L507+280
	mov	r3, #1
	str	r3, [fp, #-204]
	ldr	r0, [r2, #0]
	ldr	r1, .L507+256
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-44]
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	beq	.L489
	bl	getpid
	mov	r3, r0
	ldr	r0, [fp, #-44]
	ldr	r1, .L507+284
	mov	r2, r3
	bl	fprintf
	ldr	r0, [fp, #-44]
	bl	fclose
.L489:
	bl	_Z11initruntimev
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #76
	mvn	r3, #0
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r3, .L507+292
	mov	lr, pc
	mov	pc, r3
	mov	r2, #1
	str	r2, [fp, #-212]
	b	.L445
.L508:
	.align	2
.L507:
	.word	__gxx_personality_sj0
	.word	.LLSDA124
	.word	.L506
	.word	dvrconfigfile
	.word	_ZN6stringC1Ev
	.word	.LC9
	.word	.LC51
	.word	_ZN6stringaSERS_
	.word	_ZN6string6lengthEv
	.word	_ZN6string9getstringEv
	.word	.LC52
	.word	.LC33
	.word	.LC53
	.word	logfile
	.word	.LC54
	.word	temp_logfile
	.word	.LC55
	.word	dvriomap
	.word	.LC56
	.word	.LC57
	.word	p_dio_mmap
	.word	.LC58
	.word	.LC59
	.word	.LC60
	.word	output_inverted
	.word	.LC61
	.word	.LC62
	.word	g_syncrtc
	.word	.LC63
	.word	serial_dev
	.word	.LC64
	.word	serial_baud
	.word	.LC65
	.word	smartftp_disable
	.word	.LC66
	.word	.LC67
	.word	.LC68
	.word	.LC69
	.word	.LC70
	.word	.LC71
	.word	gforce_log_enable
	.word	.LC72
	.word	.LC73
	.word	gsensor_direction
	.word	direction_table
	.word	g_sensor_trigger_forward
	.word	1056964608
	.word	.LC74
	.word	.LC75
	.word	g_sensor_trigger_backward
	.word	-1090519040
	.word	.LC76
	.word	g_sensor_trigger_right
	.word	.LC77
	.word	g_sensor_trigger_left
	.word	.LC78
	.word	g_sensor_trigger_down
	.word	1080033280
	.word	.LC79
	.word	g_sensor_trigger_up
	.word	-1077936128
	.word	.LC80
	.word	.LC81
	.word	.LC82
	.word	.LC83
	.word	.LC84
	.word	.LC85
	.word	usewatchdog
	.word	.LC86
	.word	watchdogtimeout
	.word	pidfile
	.word	.LC87
	.word	_ZN6stringD1Ev
	.word	_ZN6configD1Ev
.L506:
	add	fp, fp, #40
	ldr	r3, [fp, #-200]
	str	r3, [fp, #-232]
.L490:
	ldr	r2, [fp, #-232]
	str	r2, [fp, #-228]
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-228]
	str	r3, [fp, #-232]
.L492:
.L494:
	ldr	r2, [fp, #-232]
	str	r2, [fp, #-236]
	sub	r3, fp, #88
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-236]
	str	r3, [fp, #-232]
.L496:
.L498:
	ldr	r2, [fp, #-232]
	str	r2, [fp, #-240]
	sub	r3, fp, #80
	mov	r0, r3
	ldr	r3, .L507+288
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-240]
	str	r3, [fp, #-232]
.L500:
.L502:
	ldr	r2, [fp, #-232]
	str	r2, [fp, #-244]
	sub	r2, fp, #76
	mov	r3, #0
	str	r3, [fp, #-204]
	mov	r0, r2
	ldr	r3, .L507+292
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-244]
	str	r3, [fp, #-232]
.L504:
	mvn	r3, #0
	str	r3, [fp, #-204]
	ldr	r0, [fp, #-232]
	bl	_Unwind_SjLj_Resume
.L445:
	sub	r3, fp, #208
	mov	r0, r3
	bl	_Unwind_SjLj_Unregister
	ldr	r0, [fp, #-212]
	sub	sp, fp, #40
	ldmfd	sp, {r4, r5, r6, r7, r8, r9, sl, fp, sp, pc}
	.size	_Z7appinitv, .-_Z7appinitv
	.section	.gcc_except_table
.LLSDA124:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE124-.LLSDACSB124
.LLSDACSB124:
	.uleb128 0x0
	.uleb128 0x0
.LLSDACSE124:
	.text
	.section	.gnu.linkonce.t._ZN6string9getstringEv,"ax",%progbits
	.align	2
	.weak	_ZN6string9getstringEv
	.type	_ZN6string9getstringEv, %function
_ZN6string9getstringEv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-20]
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L510
	ldr	r4, [fp, #-20]
	mov	r0, #4
	bl	_Znaj
	str	r0, [r4, #0]
	ldr	r3, [fp, #-20]
	ldr	r2, [r3, #0]
	mov	r3, #0
	strb	r3, [r2, #0]
.L510:
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	mov	r0, r3
	ldmib	sp, {r4, fp, sp, pc}
	.size	_ZN6string9getstringEv, .-_ZN6string9getstringEv
	.section	.gnu.linkonce.t._ZN6string6lengthEv,"ax",%progbits
	.align	2
	.weak	_ZN6string6lengthEv
	.type	_ZN6string6lengthEv, %function
_ZN6string6lengthEv:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L512
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-20]
	b	.L511
.L512:
	mov	r3, #0
	str	r3, [fp, #-20]
.L511:
	ldr	r0, [fp, #-20]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_ZN6string6lengthEv, .-_ZN6string6lengthEv
	.section	.gnu.linkonce.t._ZN6stringaSERS_,"ax",%progbits
	.align	2
	.weak	_ZN6stringaSERS_
	.type	_ZN6stringaSERS_, %function
_ZN6stringaSERS_:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-20]
	str	r1, [fp, #-24]
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L515
	ldr	r3, [fp, #-20]
	ldr	r0, [r3, #0]
	bl	_ZdlPv
	ldr	r2, [fp, #-20]
	mov	r3, #0
	str	r3, [r2, #0]
.L515:
	ldr	r0, [fp, #-24]
	ldr	r3, .L517
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	beq	.L516
	ldr	r0, [fp, #-32]
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r4, [fp, #-20]
	ldr	r3, [fp, #-28]
	add	r3, r3, #4
	mov	r0, r3
	bl	_Znaj
	str	r0, [r4, #0]
	ldr	r3, [fp, #-20]
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-32]
	bl	strcpy
.L516:
	ldr	r3, [fp, #-20]
	mov	r0, r3
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L518:
	.align	2
.L517:
	.word	_ZN6string9getstringEv
	.size	_ZN6stringaSERS_, .-_ZN6stringaSERS_
	.section	.gnu.linkonce.t._ZN6stringC1Ev,"ax",%progbits
	.align	2
	.weak	_ZN6stringC1Ev
	.type	_ZN6stringC1Ev, %function
_ZN6stringC1Ev:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r2, [fp, #-16]
	mov	r3, #0
	str	r3, [r2, #0]
	ldmib	sp, {fp, sp, pc}
	.size	_ZN6stringC1Ev, .-_ZN6stringC1Ev
	.text
	.align	2
	.global	_Z9appfinishv
	.type	_Z9appfinishv, %function
_Z9appfinishv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r3, .L525
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L521
	ldr	r3, .L525
	ldr	r0, [r3, #0]
	bl	close
	ldr	r2, .L525
	mov	r3, #0
	str	r3, [r2, #0]
.L521:
	ldr	r3, .L525+4
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #8]
	ldr	r3, .L525+4
	ldr	r2, [r3, #0]
	ldr	r3, .L525+4
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L525+4
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bgt	.L522
	mov	r3, #1
	str	r3, [fp, #-16]
	b	.L523
.L522:
	mov	r3, #0
	str	r3, [fp, #-16]
.L523:
	ldr	r3, .L525+4
	ldr	r0, [r3, #0]
	mov	r1, #172
	bl	munmap
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	beq	.L524
	ldr	r0, .L525+8
	bl	unlink
.L524:
	ldr	r3, .L525+12
	ldr	r0, [r3, #0]
	bl	unlink
	ldmib	sp, {fp, sp, pc}
.L526:
	.align	2
.L525:
	.word	serial_handle
	.word	p_dio_mmap
	.word	dvriomap
	.word	pidfile
	.size	_Z9appfinishv, .-_Z9appfinishv
	.local	_ZZ4mainE14cpuusage_timer
	.comm	_ZZ4mainE14cpuusage_timer,4,4
	.local	_ZZ4mainE13usage_counter
	.comm	_ZZ4mainE13usage_counter,4,4
	.global	__extendsfdf2
	.global	__gtdf2
	.local	_ZZ4mainE17temperature_timer
	.comm	_ZZ4mainE17temperature_timer,4,4
	.local	_ZZ4mainE10saveiotemp
	.comm	_ZZ4mainE10saveiotemp,4,4
	.local	_ZZ4mainE10savehdtemp
	.comm	_ZZ4mainE10savehdtemp,4,4
	.local	_ZZ4mainE10hitempbeep
	.comm	_ZZ4mainE10hitempbeep,4,4
	.local	_ZZ4mainE17hightemp_norecord
	.comm	_ZZ4mainE17hightemp_norecord,4,4
	.local	_ZZ4mainE13appmode_timer
	.comm	_ZZ4mainE13appmode_timer,4,4
	.local	_ZZ4mainE12app_run_bell
	.comm	_ZZ4mainE12app_run_bell,4,4
	.local	_ZZ4mainE12reboot_begin
	.comm	_ZZ4mainE12reboot_begin,4,4
	.local	_ZZ4mainE14nodatawatchdog
	.comm	_ZZ4mainE14nodatawatchdog,4,4
	.local	_ZZ4mainE8hd_timer
	.comm	_ZZ4mainE8hd_timer,4,4
	.local	_ZZ4mainE14hd_dvrpid_save
	.comm	_ZZ4mainE14hd_dvrpid_save,4,4
	.local	_ZZ4mainE11panel_timer
	.comm	_ZZ4mainE11panel_timer,4,4
	.local	_ZZ4mainE13videolostbell
	.comm	_ZZ4mainE13videolostbell,4,4
	.local	_ZZ4mainE13adjtime_timer
	.comm	_ZZ4mainE13adjtime_timer,4,4
	.section	.rodata
	.align	2
.LC89:
	.ascii	"MCU: %s\n\000"
	.align	2
.LC90:
	.ascii	"/var/dvr/mcuversion\000"
	.align	2
.LC91:
	.ascii	"-fw\000"
	.align	2
.LC92:
	.ascii	"-reboot\000"
	.align	2
.LC93:
	.ascii	"DVR IO process started!\n\000"
	.align	2
.LC94:
	.ascii	"CPU usage at 100% for 60 seconds, system reset.\000"
	.align	2
.LC95:
	.ascii	"High system temperature: %d\000"
	.align	2
.LC96:
	.ascii	"High hard disk temperature: %d\000"
	.align	2
.LC97:
	.ascii	"Stop DVR recording on high temperature.\000"
	.align	2
.LC98:
	.ascii	"Resume DVR recording on lower temperature.\000"
	.align	2
.LC99:
	.ascii	"Power off switch, enter shutdown delay (mode %d).\000"
	.align	2
.LC100:
	.ascii	"Shutdown delay timeout, to stop recording (mode %d)"
	.ascii	".\000"
	.align	2
.LC102:
	.ascii	"Recording stopped.\000"
	.align	2
.LC103:
	.ascii	"Enter standby mode. (mode %d).\000"
	.align	2
.LC104:
	.ascii	"Standby timeout, system shutdown. (mode %d).\000"
	.align	2
.LC101:
	.ascii	"Power on switch, set to running mode. (mode %d)\000"
	.align	2
.LC105:
	.ascii	"Hardware shutdown failed. Try reboot by software!\000"
	.align	2
.LC106:
	.ascii	"/bin/reboot\000"
	.align	2
.LC107:
	.ascii	"IO re-initialize.\000"
	.align	2
.LC108:
	.ascii	"Error! Enter undefined mode %d.\000"
	.align	2
.LC109:
	.ascii	"Dvr watchdog failed, try restart DVR!!!\000"
	.align	2
.LC110:
	.ascii	"Dvr watchdog failed, system reboot.\000"
	.align	2
.LC111:
	.ascii	"One or more camera not working, system reset.\000"
	.align	2
.LC112:
	.ascii	"Hard drive failed, system reset!\000"
	.align	2
.LC113:
	.ascii	"/davinci/dvr/umountdisks\000"
	.align	2
.LC114:
	.ascii	"ifconfig eth0:247 2.4.7.247\000"
	.align	2
.LC115:
	.ascii	"DVR IO process ended!\n\000"
	.text
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #40
	str	r0, [fp, #-20]
	str	r1, [fp, #-24]
	mov	r3, #0
	str	r3, [fp, #-32]
	mov	r3, #0
	str	r3, [fp, #-36]
	mov	r3, #0
	str	r3, [fp, #-40]
	mov	r3, #0
	str	r3, [fp, #-44]
	bl	_Z7appinitv
	mov	r3, r0
	cmp	r3, #0
	bne	.L528
	mov	r3, #1
	str	r3, [fp, #-56]
	b	.L527
.L528:
	ldr	r0, .L686+8
	bl	_Z11mcu_versionPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L529
	ldr	r0, .L686+12
	ldr	r1, .L686+8
	bl	printf
	ldr	r0, .L686+16
	ldr	r1, .L686+20
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-48]
	ldr	r3, [fp, #-48]
	cmp	r3, #0
	beq	.L529
	ldr	r0, [fp, #-48]
	ldr	r1, .L686+24
	ldr	r2, .L686+8
	bl	fprintf
	ldr	r0, [fp, #-48]
	bl	fclose
.L529:
	ldr	r3, [fp, #-20]
	cmp	r3, #1
	ble	.L531
	mov	r3, #1
	str	r3, [fp, #-28]
.L532:
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bge	.L531
	ldr	r3, [fp, #-28]
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r1, .L686+28
	bl	strcasecmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L535
	ldr	r3, [fp, #-28]
	mov	r1, #4
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	add	r3, r3, r1
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L536
	ldr	r3, [fp, #-28]
	mov	r1, #4
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	add	r3, r3, r1
	ldr	r0, [r3, #0]
	bl	_Z19mcu_update_firmwarePc
	mov	r3, r0
	cmp	r3, #0
	beq	.L536
	bl	_Z9appfinishv
	mov	r3, #0
	str	r3, [fp, #-56]
	b	.L527
.L536:
	bl	_Z9appfinishv
	mov	r3, #1
	str	r3, [fp, #-56]
	b	.L527
.L535:
	ldr	r3, [fp, #-28]
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	ldr	r0, [r3, #0]
	ldr	r1, .L686+32
	bl	strcasecmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L534
	mov	r3, #5
	str	r3, [fp, #-48]
	ldr	r3, [fp, #-28]
	mov	r1, #4
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	add	r3, r3, r1
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L540
	ldr	r3, [fp, #-28]
	mov	r1, #4
	mov	r2, r3, asl #2
	ldr	r3, [fp, #-24]
	add	r3, r2, r3
	add	r3, r3, r1
	ldr	r0, [r3, #0]
	bl	atoi
	mov	r3, r0
	str	r3, [fp, #-48]
.L540:
	ldr	r3, [fp, #-48]
	cmp	r3, #4
	bgt	.L541
	mov	r3, #5
	str	r3, [fp, #-48]
	b	.L542
.L541:
	ldr	r3, [fp, #-48]
	cmp	r3, #100
	ble	.L542
	mov	r3, #100
	str	r3, [fp, #-48]
.L542:
	ldr	r2, .L686+152
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	bl	_Z18mcu_watchdogenablev
	ldr	r3, [fp, #-48]
	add	r3, r3, #20
	mov	r0, r3
	bl	sleep
	bl	_Z10mcu_rebootv
	mov	r3, #1
	str	r3, [fp, #-56]
	b	.L527
.L534:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L532
.L531:
	ldr	r2, .L686+232
	mov	r3, #1
	str	r3, [r2, #0]
	mov	r0, #3
	ldr	r1, .L686+36
	bl	signal
	mov	r0, #2
	ldr	r1, .L686+36
	bl	signal
	mov	r0, #15
	ldr	r1, .L686+36
	bl	signal
	mov	r0, #12
	ldr	r1, .L686+36
	bl	signal
	ldr	r0, .L686+40
	bl	printf
	bl	_Z10mcu_dinputv
	ldr	r2, .L686+44
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	mvn	r3, r3
	str	r3, [r2, #0]
	ldr	r2, .L686+236
	mov	r3, #255
	str	r3, [r2, #0]
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #120]
	ldr	r2, .L686+244
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #65280
	add	r3, r3, #255
	str	r3, [r2, #124]
.L544:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L545
	bl	_Z11mcu_doutputv
	mov	r0, #4992
	add	r0, r0, #8
	bl	_Z9mcu_inputi
	bl	_Z10getruntimev
	mov	r3, r0
	str	r3, [fp, #-48]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #72]
	cmp	r3, #0
	beq	.L546
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #72]
	cmp	r3, #1
	bne	.L547
	bl	_Z11mcu_readrtcv
	b	.L546
.L547:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #72]
	cmp	r3, #2
	bne	.L549
	bl	_Z10mcu_setrtcv
	b	.L546
.L549:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #72]
	cmp	r3, #3
	bne	.L551
	bl	_Z11mcu_syncrtcv
	b	.L546
.L551:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mvn	r3, #0
	str	r3, [r2, #72]
.L546:
	ldr	r3, .L686+48
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r2, r3, r2
	mov	r3, #4992
	add	r3, r3, #8
	cmp	r2, r3
	bls	.L553
	ldr	r2, .L686+48
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	bl	_Z8cpuusagev
	mov	r3, r0
	mov	r0, r3
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L686
	ldmia	r2, {r2-r3}
	bl	__gtdf2
	mov	r3, r0
	cmp	r3, #0
	bgt	.L555
	b	.L554
.L555:
	ldr	r2, .L686+52
	ldr	r3, .L686+52
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #12
	ble	.L553
	mov	r0, #10
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r0, .L686+56
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+232
	mov	r3, #7
	str	r3, [r2, #0]
	b	.L553
.L554:
	ldr	r2, .L686+52
	mov	r3, #0
	str	r3, [r2, #0]
.L553:
	ldr	r0, [fp, #-48]
	bl	_Z10buzzer_runi
	ldr	r3, .L686+60
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r2, r3, r2
	mov	r3, #9984
	add	r3, r3, #16
	cmp	r2, r3
	bls	.L558
	ldr	r2, .L686+60
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	bl	_Z17mcu_iotemperaturev
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmn	r3, #127
	ble	.L559
	ldr	r3, [fp, #-28]
	cmp	r3, #126
	bgt	.L559
	ldr	r3, [fp, #-28]
	cmp	r3, #74
	ble	.L560
	ldr	r3, .L686+64
	ldr	r2, [fp, #-28]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	cmp	r3, #2
	ble	.L560
	ldr	r0, .L686+68
	ldr	r1, [fp, #-28]
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+64
	ldr	r3, [fp, #-28]
	str	r3, [r2, #0]
	bl	sync
.L560:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	str	r3, [r2, #128]
	b	.L561
.L559:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mvn	r3, #127
	str	r3, [r2, #128]
.L561:
	bl	_Z17mcu_hdtemperaturev
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmn	r3, #127
	ble	.L562
	ldr	r3, [fp, #-28]
	cmp	r3, #126
	bgt	.L562
	ldr	r3, [fp, #-28]
	cmp	r3, #74
	ble	.L563
	ldr	r3, .L686+72
	ldr	r2, [fp, #-28]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	cmp	r3, #2
	ble	.L563
	ldr	r0, .L686+76
	ldr	r1, [fp, #-28]
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+72
	ldr	r3, [fp, #-28]
	str	r3, [r2, #0]
	bl	sync
.L563:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	str	r3, [r2, #132]
	b	.L564
.L562:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mvn	r3, #127
	str	r3, [r2, #132]
.L564:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #128]
	cmp	r3, #66
	bgt	.L566
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #132]
	cmp	r3, #66
	bgt	.L566
	b	.L565
.L566:
	ldr	r2, .L686+80
	ldr	r3, .L686+80
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #3
	ble	.L565
	mov	r0, #4
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r2, .L686+80
	mov	r3, #0
	str	r3, [r2, #0]
.L565:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #128]
	cmp	r3, #83
	bgt	.L569
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #132]
	cmp	r3, #83
	bgt	.L569
	b	.L568
.L569:
	ldr	r3, .L686+88
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L558
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #36]
	cmp	r3, #0
	bne	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L558
	ldr	r0, .L686+84
	bl	_Z7dvr_logPcz
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #3
	str	r3, [r2, #36]
	ldr	r2, .L686+88
	mov	r3, #1
	str	r3, [r2, #0]
	b	.L558
.L687:
	.align	2
.L686:
	.word	1072682762
	.word	1030792151
	.word	mcu_firmware_version
	.word	.LC89
	.word	.LC90
	.word	.LC83
	.word	.LC3
	.word	.LC91
	.word	.LC92
	.word	_Z11sig_handleri
	.word	.LC93
	.word	mcu_doutputmap
	.word	_ZZ4mainE14cpuusage_timer
	.word	_ZZ4mainE13usage_counter
	.word	.LC94
	.word	_ZZ4mainE17temperature_timer
	.word	_ZZ4mainE10saveiotemp
	.word	.LC95
	.word	_ZZ4mainE10savehdtemp
	.word	.LC96
	.word	_ZZ4mainE10hitempbeep
	.word	.LC97
	.word	_ZZ4mainE17hightemp_norecord
	.word	.LC98
	.word	_ZZ4mainE13appmode_timer
	.word	_ZZ4mainE12app_run_bell
	.word	.LC99
	.word	.LC100
	.word	.LC102
	.word	smartftp_disable
	.word	smartftp_reporterror
	.word	smartftp_retry
	.word	.LC103
	.word	standbyhdoff
	.word	.LC104
	.word	.LC101
	.word	pid_smartftp
	.word	.LC105
	.word	watchdogtimeout
	.word	_ZZ4mainE12reboot_begin
	.word	.LC106
	.word	.LC107
	.word	.LC108
	.word	usewatchdog
	.word	watchdogenabled
	.word	.LC109
	.word	.LC110
	.word	.LC111
	.word	_ZZ4mainE14nodatawatchdog
	.word	_ZZ4mainE8hd_timer
	.word	.LC112
	.word	hdinserted
	.word	.LC113
	.word	_ZZ4mainE14hd_dvrpid_save
	.word	.LC114
	.word	hdlock
	.word	_ZZ4mainE11panel_timer
	.word	_ZZ4mainE13videolostbell
	.word	app_mode
	.word	panelled
	.word	p_dio_mmap
	.word	devicepower
.L568:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #128]
	cmp	r3, #74
	bgt	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #132]
	cmp	r3, #74
	bgt	.L558
	ldr	r3, .L686+88
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L558
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #36]
	cmp	r3, #0
	bne	.L558
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #1
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L558
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #4
	str	r3, [r2, #36]
	ldr	r2, .L686+88
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r0, .L686+92
	bl	_Z7dvr_logPcz
.L558:
	ldr	r3, .L686+96
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r2, r3, r2
	mov	r3, #2992
	add	r3, r3, #8
	cmp	r2, r3
	bls	.L574
	ldr	r2, .L686+96
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L575
	ldr	r3, .L686+100
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L576
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L576
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L576
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L576
	ldr	r2, .L686+100
	mov	r3, #1
	str	r3, [r2, #0]
	mov	r0, #1
	mov	r1, #1000
	mov	r2, #100
	bl	_Z6buzzeriii
.L576:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	beq	.L578
	bl	_Z20getshutdowndelaytimev
	mov	r2, r0
	mov	r3, r2
	mov	r3, r3, asl #5
	rsb	r3, r2, r3
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #3
	ldr	r3, [fp, #-48]
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r2, .L686+232
	mov	r3, #2
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r0, .L686+104
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	b	.L578
.L575:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #2
	bne	.L579
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	beq	.L580
	bl	_Z17mcu_poweroffdelayv
	ldr	r2, [fp, #-48]
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bls	.L578
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L582
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L582
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #36]
	cmp	r3, #0
	bne	.L582
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #3
	str	r3, [r2, #36]
.L582:
	bl	sync
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	ble	.L583
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #16]
	mov	r1, #10
	bl	kill
.L583:
	ldr	r3, [fp, #-48]
	add	r3, r3, #119808
	add	r3, r3, #192
	str	r3, [fp, #-40]
	ldr	r2, .L686+232
	mov	r3, #3
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r0, .L686+108
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	b	.L578
.L580:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L585
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L585
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #36]
	cmp	r3, #0
	bne	.L585
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #4
	str	r3, [r2, #36]
.L585:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #65280
	add	r3, r3, #255
	str	r3, [r2, #124]
	ldr	r2, .L686+232
	mov	r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r0, .L686+140
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	b	.L578
.L579:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #3
	bne	.L587
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	beq	.L588
	bl	_Z17mcu_poweroffdelayv
.L588:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L589
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L589
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #1
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L589
	ldr	r0, .L686+112
	bl	_Z7dvr_logPcz
	ldr	r3, [fp, #-48]
	str	r3, [fp, #-40]
.L589:
	bl	sync
	ldr	r2, [fp, #-48]
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bcc	.L578
	ldr	r2, .L686+232
	mov	r3, #5
	str	r3, [r2, #0]
	bl	_Z14getstandbytimev
	mov	r2, r0
	mov	r3, r2
	mov	r3, r3, asl #5
	rsb	r3, r2, r3
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r2, r3, asl #3
	ldr	r3, [fp, #-48]
	add	r3, r2, r3
	str	r3, [fp, #-40]
	ldr	r3, .L686+116
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L591
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	bic	r3, r3, #32512
	bic	r3, r3, #251
	mov	r3, r3, asl #16
	mov	r3, r3, lsr #16
	cmp	r3, #0
	bne	.L592
	ldr	r2, .L686+120
	mov	r3, #0
	str	r3, [r2, #0]
	b	.L593
.L592:
	ldr	r2, .L686+120
	mov	r3, #1
	str	r3, [r2, #0]
.L593:
	ldr	r2, .L686+124
	mov	r3, #3
	str	r3, [r2, #0]
	bl	_Z14smartftp_startv
.L591:
	ldr	r3, .L686+232
	ldr	r0, .L686+128
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	mov	r0, #3
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	b	.L578
.L587:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #5
	bne	.L595
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	eor	r3, r3, #16
	str	r3, [r2, #32]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	beq	.L596
	bl	_Z17mcu_poweroffdelayv
.L596:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L597
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L597
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #4
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L597
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #65280
	add	r3, r3, #255
	str	r3, [r2, #124]
	b	.L598
.L597:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #124]
.L598:
	ldr	r3, .L686+144
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L599
	bl	_Z13smartftp_waitv
.L599:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	beq	.L600
	ldr	r3, .L686+132
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L601
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #16
	str	r3, [r2, #32]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	beq	.L601
	mov	r3, #0
	str	r3, [fp, #-32]
	bl	_Z14mcu_hdpoweroffv
.L601:
	ldr	r2, [fp, #-48]
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bls	.L578
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #124]
	ldr	r3, [fp, #-48]
	add	r3, r3, #89088
	add	r3, r3, #912
	str	r3, [fp, #-40]
	ldr	r2, .L686+232
	mov	r3, #6
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r0, .L686+136
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	mov	r0, #5
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L604
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #12]
	mov	r1, #15
	bl	kill
.L604:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	ble	.L605
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #16]
	mov	r1, #15
	bl	kill
.L605:
	ldr	r3, .L686+144
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L578
	bl	_Z13smartftp_killv
	b	.L578
.L600:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L608
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L608
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #36]
	cmp	r3, #0
	bne	.L608
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #4
	str	r3, [r2, #36]
.L608:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	ble	.L609
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #16]
	mov	r1, #12
	bl	kill
.L609:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #65280
	add	r3, r3, #255
	str	r3, [r2, #124]
	ldr	r2, .L686+232
	mov	r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L686+232
	ldr	r0, .L686+140
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	ldr	r3, .L686+144
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L578
	bl	_Z13smartftp_killv
	b	.L578
.L595:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #6
	bne	.L612
	bl	sync
	ldr	r2, [fp, #-48]
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bls	.L578
	ldr	r0, .L686+148
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+232
	mov	r3, #7
	str	r3, [r2, #0]
	b	.L578
.L612:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #7
	bne	.L615
	bl	sync
	ldr	r3, .L686+156
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L616
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L617
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #12]
	mov	r1, #15
	bl	kill
.L617:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	ble	.L618
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #16]
	mov	r1, #15
	bl	kill
.L618:
	ldr	r2, .L686+152
	mov	r3, #10
	str	r3, [r2, #0]
	bl	_Z18mcu_watchdogenablev
	ldr	r2, .L686+172
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r2, .L686+176
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r2, .L686+156
	mov	r3, #1
	str	r3, [r2, #0]
	ldr	r3, [fp, #-48]
	add	r3, r3, #19968
	add	r3, r3, #32
	str	r3, [fp, #-40]
	b	.L578
.L616:
	ldr	r2, [fp, #-48]
	ldr	r3, [fp, #-40]
	cmp	r2, r3
	bls	.L578
	bl	_Z10mcu_rebootv
	ldr	r0, .L686+160
	bl	system
	ldr	r2, .L686+232
	mov	r3, #0
	str	r3, [r2, #0]
	b	.L578
.L615:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #8
	bne	.L622
	ldr	r0, .L686+164
	bl	_Z7dvr_logPcz
	bl	_Z9appfinishv
	bl	_Z7appinitv
	ldr	r2, .L686+232
	mov	r3, #1
	str	r3, [r2, #0]
	b	.L578
.L622:
	ldr	r3, .L686+232
	ldr	r0, .L686+168
	ldr	r1, [r3, #0]
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+232
	mov	r3, #1
	str	r3, [r2, #0]
.L578:
	ldr	r3, .L686+172
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L624
	ldr	r3, .L686+176
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L624
	bl	_Z18mcu_watchdogenablev
.L624:
	ldr	r3, .L686+176
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L625
	bl	_Z16mcu_watchdogkickv
.L625:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L574
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #52]
	cmp	r3, #0
	blt	.L574
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #52]
	add	r3, r3, #1
	str	r3, [r2, #52]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #52]
	cmp	r3, #50
	bne	.L627
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #12]
	mov	r1, #12
	bl	kill
	mov	r3, r0
	cmp	r3, #0
	beq	.L628
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #110
	str	r3, [r2, #52]
	b	.L630
.L628:
	ldr	r0, .L686+180
	bl	_Z7dvr_logPcz
	b	.L630
.L627:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #52]
	cmp	r3, #110
	ble	.L630
	mov	r0, #10
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r0, .L686+184
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+232
	mov	r3, #7
	str	r3, [r2, #0]
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	mov	r3, #1
	str	r3, [r2, #52]
.L630:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L632
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L632
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L632
	ldr	r2, .L686+192
	ldr	r3, .L686+192
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #20
	ble	.L574
	mov	r0, #10
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r0, .L686+188
	bl	_Z7dvr_logPcz
	ldr	r2, .L686+232
	mov	r3, #7
	str	r3, [r2, #0]
	b	.L574
.L632:
	ldr	r2, .L686+192
	mov	r3, #0
	str	r3, [r2, #0]
.L574:
	ldr	r3, .L686+196
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	cmp	r3, #500
	bls	.L635
	ldr	r2, .L686+196
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	ldr	r3, .L686+220
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L636
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	bgt	.L636
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	bne	.L637
	bl	_Z13mcu_hdpoweronv
.L637:
	mov	r3, #0
	str	r3, [fp, #-36]
	ldr	r3, .L686+204
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L638
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L638
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #52]
	cmp	r3, #0
	blt	.L638
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L638
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #5
	eor	r3, r3, #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L638
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	eor	r3, r3, #16
	str	r3, [r2, #32]
	ldr	r3, [fp, #-32]
	add	r3, r3, #1
	str	r3, [fp, #-32]
	cmp	r3, #100
	ble	.L635
	ldr	r0, .L686+200
	bl	_Z7dvr_logPcz
	mov	r0, #10
	mov	r1, #250
	mov	r2, #250
	bl	_Z6buzzeriii
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #16
	str	r3, [r2, #32]
	bl	_Z14mcu_hdpoweroffv
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L635
.L638:
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #2
	bgt	.L641
	ldr	r3, .L686+204
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L642
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	orr	r3, r3, #16
	str	r3, [r2, #32]
	b	.L641
.L642:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #16
	str	r3, [r2, #32]
.L641:
	mov	r3, #1
	str	r3, [fp, #-32]
	b	.L635
.L636:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	eor	r3, r3, #16
	str	r3, [r2, #32]
	ldr	r3, [fp, #-36]
	cmp	r3, #9
	bgt	.L645
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	ldr	r2, .L686+212
	mov	r3, #0
	str	r3, [r2, #0]
	b	.L635
.L645:
	ldr	r3, [fp, #-36]
	cmp	r3, #10
	bne	.L647
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L635
	ldr	r2, .L686+212
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	str	r3, [r2, #0]
	ldr	r3, .L686+212
	ldr	r0, [r3, #0]
	mov	r1, #10
	bl	kill
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	orr	r3, r3, #16
	str	r3, [r2, #32]
	b	.L635
.L647:
	ldr	r3, [fp, #-36]
	cmp	r3, #11
	bne	.L650
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	bgt	.L635
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	bl	sync
	ldr	r0, .L686+208
	bl	system
	b	.L635
.L650:
	ldr	r3, [fp, #-36]
	cmp	r3, #12
	bne	.L653
	bl	sync
	bl	_Z14mcu_hdpoweroffv
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	b	.L635
.L653:
	ldr	r3, [fp, #-36]
	cmp	r3, #19
	bgt	.L655
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	b	.L635
.L655:
	ldr	r3, [fp, #-36]
	cmp	r3, #20
	bne	.L657
	ldr	r3, [fp, #-36]
	add	r3, r3, #1
	str	r3, [fp, #-36]
	ldr	r3, .L686+212
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L658
	ldr	r3, .L686+212
	ldr	r0, [r3, #0]
	mov	r1, #12
	bl	kill
	ldr	r2, .L686+212
	mov	r3, #0
	str	r3, [r2, #0]
.L658:
	ldr	r3, .L686+240
	ldr	r2, [r3, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #16
	str	r3, [r2, #32]
	ldr	r0, .L686+216
	bl	system
	b	.L635
.L657:
	ldr	r3, .L686+220
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L635
	mov	r3, #0
	str	r3, [fp, #-36]
.L635:
	ldr	r3, .L686+224
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	cmp	r3, #2000
	bls	.L661
	ldr	r2, .L686+224
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #120]
	str	r3, [fp, #-52]
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L662
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L662
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #2
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L663
	ldr	r3, [fp, #-52]
	orr	r3, r3, #4
	str	r3, [fp, #-52]
	ldr	r3, .L686+228
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L662
	mov	r0, #5
	mov	r1, #500
	mov	r2, #500
	bl	_Z6buzzeriii
	ldr	r2, .L686+228
	mov	r3, #1
	str	r3, [r2, #0]
	b	.L662
.L663:
	ldr	r2, .L686+228
	mov	r3, #0
	str	r3, [r2, #0]
.L662:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #12]
	cmp	r3, #0
	ble	.L666
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #40]
	mov	r3, r3, lsr #6
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L666
	ldr	r3, .L686+232
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L666
	ldr	r3, [fp, #-52]
	orr	r3, r3, #2
	str	r3, [fp, #-52]
.L666:
	ldr	r3, .L686+236
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-52]
	cmp	r2, r3
	beq	.L667
	mov	r3, #0
	str	r3, [fp, #-28]
.L668:
	ldr	r3, [fp, #-28]
	cmp	r3, #2
	bgt	.L669
	ldr	r3, .L686+236
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-52]
	eor	r1, r2, r3
	mov	r2, #1
	ldr	r3, [fp, #-28]
	mov	r3, r2, asl r3
	and	r3, r1, r3
	cmp	r3, #0
	beq	.L670
	mov	r2, #1
	ldr	r3, [fp, #-28]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-52]
	and	r3, r2, r3
	ldr	r0, [fp, #-28]
	mov	r1, r3
	bl	_Z7mcu_ledii
.L670:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L668
.L669:
	ldr	r2, .L686+236
	ldr	r3, [fp, #-52]
	str	r3, [r2, #0]
.L667:
	ldr	r3, .L686+240
	ldr	r3, [r3, #0]
	ldr	r2, .L686+244
	ldr	r1, [r3, #124]
	ldr	r3, [r2, #0]
	cmp	r1, r3
	beq	.L661
	mov	r3, #0
	str	r3, [fp, #-28]
.L673:
	ldr	r3, [fp, #-28]
	cmp	r3, #4
	bgt	.L674
	ldr	r3, .L688
	ldr	r3, [r3, #0]
	ldr	r2, .L688+4
	ldr	r1, [r3, #124]
	ldr	r3, [r2, #0]
	eor	r1, r1, r3
	mov	r2, #1
	ldr	r3, [fp, #-28]
	mov	r3, r2, asl r3
	and	r3, r1, r3
	cmp	r3, #0
	beq	.L675
	ldr	r3, .L688
	ldr	r1, [r3, #0]
	mov	r2, #1
	ldr	r3, [fp, #-28]
	mov	r2, r2, asl r3
	ldr	r3, [r1, #124]
	and	r3, r2, r3
	ldr	r0, [fp, #-28]
	mov	r1, r3
	bl	_Z15mcu_devicepowerii
.L675:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L673
.L674:
	ldr	r2, .L688+4
	ldr	r3, .L688
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #124]
	str	r3, [r2, #0]
	bl	_Z10setnetworkv
.L661:
	ldr	r3, .L688
	ldr	r3, [r3, #0]
	ldr	r2, .L688+8
	ldr	r1, [r3, #76]
	ldr	r3, [r2, #0]
	cmp	r1, r3
	beq	.L677
	ldr	r2, .L688+8
	ldr	r3, .L688
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #76]
	str	r3, [r2, #0]
	ldr	r3, .L688+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L677
	bl	_Z12time_syncgpsv
	ldr	r3, .L688+12
	ldr	r2, [fp, #-48]
	str	r2, [r3, #0]
	mov	r3, #1
	str	r3, [fp, #-44]
.L677:
	ldr	r3, .L688+12
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	rsb	r2, r3, r2
	mov	r3, #598016
	add	r3, r3, #1984
	cmp	r2, r3
	bhi	.L680
	ldr	r3, .L688+12
	ldr	r2, [fp, #-48]
	ldr	r3, [r3, #0]
	cmp	r2, r3
	bcc	.L680
	b	.L544
.L680:
	ldr	r3, .L688+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L544
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	beq	.L682
	ldr	r3, .L688+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L544
	bl	_Z12time_syncgpsv
	ldr	r2, .L688+12
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	b	.L544
.L682:
	bl	_Z12time_syncmcuv
	ldr	r2, .L688+12
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
	b	.L544
.L545:
	ldr	r3, .L688+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L685
	bl	_Z19mcu_watchdogdisablev
.L685:
	bl	_Z9appfinishv
	ldr	r0, .L688+24
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-56]
.L527:
	ldr	r0, [fp, #-56]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L689:
	.align	2
.L688:
	.word	p_dio_mmap
	.word	devicepower
	.word	gpsvalid
	.word	_ZZ4mainE13adjtime_timer
	.word	g_syncrtc
	.word	watchdogenabled
	.word	.LC115
	.size	main, .-main
	.section	.gnu.linkonce.t._ZN5arrayI6stringED1Ev,"ax",%progbits
	.align	2
	.weak	_ZN5arrayI6stringED1Ev
	.type	_ZN5arrayI6stringED1Ev, %function
_ZN5arrayI6stringED1Ev:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #8]
	cmp	r3, #0
	ble	.L690
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L690
	ldr	r1, [fp, #-16]
	ldr	r3, [fp, #-16]
	mvn	r2, #3
	ldr	r3, [r3, #0]
	add	r3, r2, r3
	ldr	r3, [r3, #0]
	mov	r2, r3, asl #2
	ldr	r3, [r1, #0]
	add	r2, r2, r3
	str	r2, [fp, #-20]
.L695:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-20]
	cmp	r3, r2
	beq	.L696
	ldr	r3, [fp, #-20]
	sub	r3, r3, #4
	str	r3, [fp, #-20]
	ldr	r0, [fp, #-20]
	ldr	r3, .L698
	mov	lr, pc
	mov	pc, r3
	b	.L695
.L696:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	sub	r3, r3, #4
	mov	r0, r3
	bl	_ZdaPv
.L690:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L699:
	.align	2
.L698:
	.word	_ZN6stringD1Ev
	.size	_ZN5arrayI6stringED1Ev, .-_ZN5arrayI6stringED1Ev
	.ident	"GCC: (GNU) 3.4.6"
