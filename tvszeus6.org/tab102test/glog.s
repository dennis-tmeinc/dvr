	.file	"glog.cpp"
	.global	baud_table
	.data
	.align	2
	.type	baud_table, %object
	.size	baud_table, 96
baud_table:
	.word	4099
	.word	230400
	.word	4098
	.word	115200
	.word	4097
	.word	57600
	.word	15
	.word	38400
	.word	14
	.word	19200
	.word	13
	.word	9600
	.word	12
	.word	4800
	.word	11
	.word	2400
	.word	10
	.word	1800
	.word	9
	.word	1200
	.word	8
	.word	600
	.word	0
	.word	0
	.global	speed_table
	.bss
	.align	2
	.type	speed_table, %object
	.size	speed_table, 80
speed_table:
	.space	80
	.global	max_distance
	.data
	.align	2
	.type	max_distance, %object
	.size	max_distance, 4
max_distance:
	.word	1140457472
	.global	degree1km
	.align	2
	.type	degree1km, %object
	.size	degree1km, 4
degree1km:
	.word	1011129254
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
	.global	gpslogfilename
	.bss
	.type	gpslogfilename, %object
	.size	gpslogfilename, 512
gpslogfilename:
	.space	512
	.global	glog_poweroff
	.data
	.align	2
	.type	glog_poweroff, %object
	.size	glog_poweroff, 4
glog_poweroff:
	.word	1
	.global	dvrcurdisk
	.bss
	.align	2
	.type	dvrcurdisk, %object
	.size	dvrcurdisk, 4
dvrcurdisk:
	.space	4
	.global	glog_ok
	.align	2
	.type	glog_ok, %object
	.size	glog_ok, 4
glog_ok:
	.space	4
	.global	gps_port_disable
	.align	2
	.type	gps_port_disable, %object
	.size	gps_port_disable, 4
gps_port_disable:
	.space	4
	.global	gps_port_handle
	.data
	.align	2
	.type	gps_port_handle, %object
	.size	gps_port_handle, 4
gps_port_handle:
	.word	-1
	.global	gps_port_dev
	.align	2
	.type	gps_port_dev, %object
	.size	gps_port_dev, 256
gps_port_dev:
	.ascii	"/dev/ttyS0\000"
	.space	245
	.global	gps_port_baudrate
	.align	2
	.type	gps_port_baudrate, %object
	.size	gps_port_baudrate, 4
gps_port_baudrate:
	.word	4800
	.global	gforce_log_enable
	.bss
	.align	2
	.type	gforce_log_enable, %object
	.size	gforce_log_enable, 4
gforce_log_enable:
	.space	4
	.global	tab102b_enable
	.align	2
	.type	tab102b_enable, %object
	.size	tab102b_enable, 4
tab102b_enable:
	.space	4
	.global	tab102b_port_handle
	.data
	.align	2
	.type	tab102b_port_handle, %object
	.size	tab102b_port_handle, 4
tab102b_port_handle:
	.word	-1
	.global	tab102b_port_dev
	.align	2
	.type	tab102b_port_dev, %object
	.size	tab102b_port_dev, 256
tab102b_port_dev:
	.ascii	"/dev/ttyUSB1\000"
	.space	243
	.global	tab102b_port_baudrate
	.align	2
	.type	tab102b_port_baudrate, %object
	.size	tab102b_port_baudrate, 4
tab102b_port_baudrate:
	.word	19200
	.global	tab102b_data_enable
	.bss
	.align	2
	.type	tab102b_data_enable, %object
	.size	tab102b_data_enable, 4
tab102b_data_enable:
	.space	4
	.global	inputdebounce
	.data
	.align	2
	.type	inputdebounce, %object
	.size	inputdebounce, 4
inputdebounce:
	.word	3
	.global	sensorname
	.bss
	.align	2
	.type	sensorname, %object
	.size	sensorname, 128
sensorname:
	.space	128
	.global	sensor_invert
	.align	2
	.type	sensor_invert, %object
	.size	sensor_invert, 128
sensor_invert:
	.space	128
	.global	sensor_value
	.align	2
	.type	sensor_value, %object
	.size	sensor_value, 128
sensor_value:
	.space	128
	.global	sensorbouncetime
	.align	2
	.type	sensorbouncetime, %object
	.size	sensorbouncetime, 256
sensorbouncetime:
	.space	256
	.global	sensorbouncevalue
	.align	2
	.type	sensorbouncevalue, %object
	.size	sensorbouncevalue, 256
sensorbouncevalue:
	.space	256
	.global	dvrconfigfile
	.data
	.align	2
	.type	dvrconfigfile, %object
	.size	dvrconfigfile, 18
dvrconfigfile:
	.ascii	"/etc/dvr/dvr.conf\000"
	.section	.rodata
	.align	2
.LC0:
	.ascii	"/var/dvr/glog.pid\000"
	.global	pidfile
	.data
	.align	2
	.type	pidfile, %object
	.size	pidfile, 4
pidfile:
	.word	.LC0
	.global	app_state
	.bss
	.align	2
	.type	app_state, %object
	.size	app_state, 4
app_state:
	.space	4
	.global	sigcap
	.align	2
	.type	sigcap, %object
	.size	sigcap, 4
sigcap:
	.space	4
	.global	disksroot
	.data
	.align	2
	.type	disksroot, %object
	.size	disksroot, 128
disksroot:
	.ascii	"/var/dvr/disks\000"
	.space	113
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
	cmp	r3, #10
	bne	.L2
	ldr	r2, .L8
	ldr	r3, .L8
	ldr	r3, [r3, #0]
	orr	r3, r3, #1
	str	r3, [r2, #0]
	b	.L1
.L2:
	ldr	r3, [fp, #-16]
	cmp	r3, #12
	bne	.L4
	ldr	r2, .L8
	ldr	r3, .L8
	ldr	r3, [r3, #0]
	orr	r3, r3, #2
	str	r3, [r2, #0]
	b	.L1
.L4:
	ldr	r3, [fp, #-16]
	cmp	r3, #13
	bne	.L6
	ldr	r2, .L8
	ldr	r3, .L8
	ldr	r3, [r3, #0]
	orr	r3, r3, #3
	str	r3, [r2, #0]
	b	.L1
.L6:
	ldr	r2, .L8
	ldr	r3, .L8
	ldr	r3, [r3, #0]
	orr	r3, r3, #32768
	str	r3, [r2, #0]
.L1:
	ldmib	sp, {fp, sp, pc}
.L9:
	.align	2
.L8:
	.word	sigcap
	.size	_Z11sig_handleri, .-_Z11sig_handleri
	.align	2
	.global	_Z10atomic_addPii
	.type	_Z10atomic_addPii, %function
_Z10atomic_addPii:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-20]
#APP
		ldrex r3, [r2]
	add   r3, r3
	strex r2, r3, [r2]

	str	r3, [fp, #-24]
	str	r2, [fp, #-28]
	ldr	r3, [fp, #-24]
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z10atomic_addPii, .-_Z10atomic_addPii
	.data
	.align	2
	.type	log_mutex, %object
	.size	log_mutex, 24
log_mutex:
	.word	0
	.word	0
	.word	0
	.word	1
	.word	0
	.word	0
	.text
	.align	2
	.global	_Z8log_lockv
	.type	_Z8log_lockv, %function
_Z8log_lockv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r0, .L12
	bl	pthread_mutex_lock
	ldmfd	sp, {fp, sp, pc}
.L13:
	.align	2
.L12:
	.word	log_mutex
	.size	_Z8log_lockv, .-_Z8log_lockv
	.align	2
	.global	_Z10log_unlockv
	.type	_Z10log_unlockv, %function
_Z10log_unlockv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r0, .L15
	bl	pthread_mutex_unlock
	ldmfd	sp, {fp, sp, pc}
.L16:
	.align	2
.L15:
	.word	log_mutex
	.size	_Z10log_unlockv, .-_Z10log_unlockv
	.align	2
	.global	_Z8dio_lockv
	.type	_Z8dio_lockv, %function
_Z8dio_lockv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	mov	r3, #0
	str	r3, [fp, #-16]
.L18:
	ldr	r2, [fp, #-16]
	mov	r3, #996
	add	r3, r3, #3
	cmp	r2, r3
	bgt	.L19
	ldr	r3, .L23
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #4]
	cmp	r3, #0
	ble	.L19
	mov	r0, #1
	bl	usleep
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L18
.L19:
	ldr	r3, .L23
	ldr	r2, [r3, #0]
	mov	r3, #1
	str	r3, [r2, #4]
	ldmib	sp, {fp, sp, pc}
.L24:
	.align	2
.L23:
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
	ldr	r3, .L26
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #4]
	ldmfd	sp, {fp, sp, pc}
.L27:
	.align	2
.L26:
	.word	p_dio_mmap
	.size	_Z10dio_unlockv, .-_Z10dio_unlockv
	.align	2
	.global	_Z13buzzer_threadPv
	.type	_Z13buzzer_threadPv, %function
_Z13buzzer_threadPv:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-20]
.L29:
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	str	r3, [r2, #0]
	cmn	r3, #1
	beq	.L30
	ldr	r3, .L31
	ldr	r2, [r3, #0]
	ldr	r3, .L31
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	orr	r3, r3, #256
	str	r3, [r2, #32]
	ldr	r3, [fp, #-20]
	ldr	r2, [r3, #4]
	mov	r3, r2
	mov	r3, r3, asl #5
	rsb	r3, r2, r3
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r0, r3
	bl	usleep
	ldr	r3, .L31
	ldr	r2, [r3, #0]
	ldr	r3, .L31
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #32]
	bic	r3, r3, #256
	str	r3, [r2, #32]
	ldr	r3, [fp, #-20]
	ldr	r2, [r3, #8]
	mov	r3, r2
	mov	r3, r3, asl #5
	rsb	r3, r2, r3
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #3
	mov	r0, r3
	bl	usleep
	b	.L29
.L30:
	ldr	r0, [fp, #-20]
	bl	_ZdlPv
	mov	r3, #0
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L32:
	.align	2
.L31:
	.word	p_dio_mmap
	.size	_Z13buzzer_threadPv, .-_Z13buzzer_threadPv
	.align	2
	.global	_Z6buzzeriii
	.type	_Z6buzzeriii, %function
_Z6buzzeriii:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	mov	r0, #12
	bl	_Znwj
	str	r0, [fp, #-32]
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-16]
	str	r3, [r2, #0]
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-20]
	str	r3, [r2, #4]
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-24]
	str	r3, [r2, #8]
	sub	r3, fp, #28
	mov	r0, r3
	mov	r1, #0
	ldr	r2, .L36
	ldr	r3, [fp, #-32]
	bl	pthread_create
	mov	r3, r0
	cmp	r3, #0
	bne	.L34
	ldr	r0, [fp, #-28]
	bl	pthread_detach
	b	.L33
.L34:
	ldr	r0, [fp, #-32]
	bl	_ZdlPv
.L33:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L37:
	.align	2
.L36:
	.word	_Z13buzzer_threadPv
	.size	_Z6buzzeriii, .-_Z6buzzeriii
	.section	.rodata
	.align	2
.LC1:
	.ascii	"Stdin match serail port!\n\000"
	.text
	.align	2
	.global	_Z11serial_openPiPci
	.type	_Z11serial_openPiPci, %function
_Z11serial_openPiPci:
	@ args = 0, pretend = 0, frame = 280
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #280
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	blt	.L39
	ldr	r3, [fp, #-32]
	str	r3, [fp, #-284]
	b	.L38
.L39:
	sub	r3, fp, #120
	mov	r0, #0
	mov	r1, r3
	bl	fstat
	mov	r3, r0
	str	r3, [fp, #-212]
	sub	r3, fp, #208
	ldr	r0, [fp, #-20]
	mov	r1, r3
	bl	stat
	mov	r3, r0
	str	r3, [fp, #-216]
	ldr	r3, [fp, #-212]
	cmp	r3, #0
	bne	.L40
	ldr	r3, [fp, #-216]
	cmp	r3, #0
	bne	.L40
	mvn	r3, #107
	sub	r1, fp, #12
	add	r1, r1, r3
	str	r1, [fp, #-288]
	mvn	r3, #195
	sub	r2, fp, #12
	add	r2, r2, r3
	str	r2, [fp, #-292]
	ldr	r3, [fp, #-288]
	ldr	r2, [r3, #0]
	ldr	r1, [fp, #-292]
	ldr	r3, [r1, #0]
	cmp	r2, r3
	bne	.L40
	ldr	r3, [fp, #-288]
	ldr	r2, [r3, #4]
	ldr	r1, [fp, #-292]
	ldr	r3, [r1, #4]
	cmp	r2, r3
	bne	.L40
	ldr	r2, [fp, #-108]
	ldr	r3, [fp, #-196]
	cmp	r2, r3
	bne	.L40
	ldr	r0, .L47
	bl	printf
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L41
.L40:
	ldr	r0, [fp, #-20]
	mov	r1, #2304
	add	r1, r1, #2
	bl	open
	mov	r3, r0
	str	r3, [fp, #-32]
.L41:
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	blt	.L42
	ldr	r0, [fp, #-32]
	mov	r1, #4
	mov	r2, #0
	bl	fcntl
	sub	r3, fp, #276
	mov	r0, r3
	mov	r1, #0
	mov	r2, #60
	bl	memset
	sub	r3, fp, #276
	ldr	r0, [fp, #-32]
	mov	r1, r3
	bl	tcgetattr
	mov	r3, #2224
	str	r3, [fp, #-268]
	mov	r3, #4
	str	r3, [fp, #-276]
	mov	r3, #0
	str	r3, [fp, #-272]
	mov	r3, #0
	str	r3, [fp, #-264]
	mov	r3, #0
	strb	r3, [fp, #-253]
	mov	r3, #0
	strb	r3, [fp, #-254]
	mov	r3, #4096
	add	r3, r3, #2
	str	r3, [fp, #-280]
	mov	r3, #0
	str	r3, [fp, #-28]
.L43:
	ldr	r3, [fp, #-28]
	cmp	r3, #11
	bgt	.L44
	ldr	r2, .L47+4
	ldr	r3, [fp, #-28]
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bgt	.L45
	ldr	r3, .L47+4
	ldr	r2, [fp, #-28]
	ldr	r3, [r3, r2, asl #3]
	str	r3, [fp, #-280]
	b	.L44
.L45:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L43
.L44:
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, [fp, #-280]
	bl	cfsetispeed
	sub	r3, fp, #276
	mov	r0, r3
	ldr	r1, [fp, #-280]
	bl	cfsetospeed
	ldr	r0, [fp, #-32]
	mov	r1, #2
	bl	tcflush
	sub	r3, fp, #276
	ldr	r0, [fp, #-32]
	mov	r1, #0
	mov	r2, r3
	bl	tcsetattr
	ldr	r0, [fp, #-32]
	mov	r1, #2
	bl	tcflush
.L42:
	ldr	r3, [fp, #-16]
	ldr	r2, [fp, #-32]
	str	r2, [r3, #0]
	ldr	r3, [fp, #-32]
	str	r3, [fp, #-284]
.L38:
	ldr	r0, [fp, #-284]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L48:
	.align	2
.L47:
	.word	.LC1
	.word	baud_table
	.size	_Z11serial_openPiPci, .-_Z11serial_openPiPci
	.align	2
	.global	_Z12serial_closePi
	.type	_Z12serial_closePi, %function
_Z12serial_closePi:
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
	ble	.L50
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	mov	r1, #0
	bl	tcflush
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	close
.L50:
	ldr	r2, [fp, #-16]
	mvn	r3, #0
	str	r3, [r2, #0]
	ldmib	sp, {fp, sp, pc}
	.size	_Z12serial_closePi, .-_Z12serial_closePi
	.align	2
	.global	_Z16serial_datareadyii
	.type	_Z16serial_datareadyii, %function
_Z16serial_datareadyii:
	@ args = 0, pretend = 0, frame = 156
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #160
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bge	.L52
	mov	r0, #0
	str	r0, [fp, #-168]
	b	.L51
.L52:
	mvn	r3, #15
	sub	r1, fp, #12
	add	r0, r1, r3
	ldr	r1, [fp, #-20]
	ldr	r3, .L59
	smull	r2, r3, r1, r3
	mov	r2, r3, asr #18
	mov	r3, r1, asr #31
	rsb	r3, r3, r2
	str	r3, [r0, #0]
	mvn	r3, #15
	sub	r0, fp, #12
	add	ip, r0, r3
	ldr	r0, [fp, #-20]
	ldr	r3, .L59
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
	sub	r3, fp, #156
	str	r3, [fp, #-164]
	mov	r3, #0
	str	r3, [fp, #-160]
.L54:
	ldr	r3, [fp, #-160]
	cmp	r3, #31
	bhi	.L53
	ldr	r1, [fp, #-164]
	ldr	r2, [fp, #-160]
	mov	r3, #0
	str	r3, [r1, r2, asl #2]
	ldr	r3, [fp, #-160]
	add	r3, r3, #1
	str	r3, [fp, #-160]
	b	.L54
.L53:
	ldr	r3, [fp, #-16]
	mov	r2, r3, lsr #5
	mvn	r1, #143
	mov	r3, r2
	mov	r3, r3, asl #2
	sub	r0, fp, #12
	add	r3, r3, r0
	add	r0, r3, r1
	mvn	r1, #143
	mov	r3, r2
	mov	r3, r3, asl #2
	sub	r2, fp, #12
	add	r3, r3, r2
	add	r1, r3, r1
	ldr	r3, [fp, #-16]
	and	r2, r3, #31
	mov	r3, #1
	mov	r2, r3, asl r2
	ldr	r3, [r1, #0]
	orr	r3, r3, r2
	str	r3, [r0, #0]
	ldr	r3, [fp, #-16]
	add	r2, r3, #1
	sub	r1, fp, #156
	sub	r3, fp, #28
	str	r3, [sp, #0]
	mov	r0, r2
	mov	r2, #0
	mov	r3, #0
	bl	select
	mov	r3, r0
	cmp	r3, #0
	ble	.L57
	ldr	r3, [fp, #-16]
	mov	r3, r3, lsr #5
	mvn	r2, #143
	mov	r3, r3, asl #2
	sub	r0, fp, #12
	add	r3, r3, r0
	add	r1, r3, r2
	ldr	r3, [fp, #-16]
	and	r2, r3, #31
	ldr	r3, [r1, #0]
	mov	r3, r3, asr r2
	and	r3, r3, #1
	and	r3, r3, #255
	str	r3, [fp, #-168]
	b	.L51
.L57:
	mov	r1, #0
	str	r1, [fp, #-168]
.L51:
	ldr	r0, [fp, #-168]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L60:
	.align	2
.L59:
	.word	1125899907
	.size	_Z16serial_datareadyii, .-_Z16serial_datareadyii
	.align	2
	.global	_Z11serial_readiPvj
	.type	_Z11serial_readiPvj, %function
_Z11serial_readiPvj:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	blt	.L62
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	ldr	r2, [fp, #-24]
	bl	read
	mov	r3, r0
	str	r3, [fp, #-28]
	b	.L61
.L62:
	mov	r3, #0
	str	r3, [fp, #-28]
.L61:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z11serial_readiPvj, .-_Z11serial_readiPvj
	.align	2
	.global	_Z12serial_writeiPvj
	.type	_Z12serial_writeiPvj, %function
_Z12serial_writeiPvj:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	str	r2, [fp, #-24]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	blt	.L64
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	ldr	r2, [fp, #-24]
	bl	write
	mov	r3, r0
	str	r3, [fp, #-28]
	b	.L63
.L64:
	mov	r3, #0
	str	r3, [fp, #-28]
.L63:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z12serial_writeiPvj, .-_Z12serial_writeiPvj
	.align	2
	.global	_Z12serial_cleari
	.type	_Z12serial_cleari, %function
_Z12serial_cleari:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	blt	.L66
.L67:
	ldr	r0, [fp, #-16]
	mov	r1, #1000
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L66
	sub	r3, fp, #32
	ldr	r0, [fp, #-16]
	mov	r1, r3
	mov	r2, #16
	bl	_Z11serial_readiPvj
	b	.L67
.L66:
	mov	r3, #0
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z12serial_cleari, .-_Z12serial_cleari
	.align	2
	.global	_Z12gps_readlinePci
	.type	_Z12gps_readlinePci, %function
_Z12gps_readlinePci:
	@ args = 0, pretend = 0, frame = 20
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #20
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L83
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L70
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L69
.L70:
	ldr	r3, .L83+4
	ldr	r0, .L83+8
	ldr	r1, .L83+12
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L71
	mov	r0, #1
	bl	sleep
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L69
.L71:
	mov	r3, #0
	str	r3, [fp, #-24]
.L72:
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bge	.L73
	ldr	r3, .L83+8
	ldr	r0, [r3, #0]
	mov	r1, #1490944
	add	r1, r1, #9024
	add	r1, r1, #32
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L73
	ldr	r3, .L83+8
	sub	r2, fp, #25
	ldr	r0, [r3, #0]
	mov	r1, r2
	mov	r2, #1
	bl	_Z11serial_readiPvj
	mov	r3, r0
	cmp	r3, #1
	bne	.L73
	ldrb	r3, [fp, #-25]	@ zero_extendqisi2
	cmp	r3, #10
	bne	.L76
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-24]
	add	r2, r2, r3
	mov	r3, #0
	strb	r3, [r2, #0]
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-32]
	b	.L69
.L76:
	ldrb	r3, [fp, #-25]	@ zero_extendqisi2
	cmp	r3, #1
	bls	.L73
	ldrb	r3, [fp, #-25]	@ zero_extendqisi2
	cmp	r3, #126
	bhi	.L73
	sub	r0, fp, #24
	ldr	r2, [r0, #0]
	mov	r1, r2
	ldr	r3, [fp, #-16]
	add	r1, r1, r3
	ldrb	r3, [fp, #-25]
	strb	r3, [r1, #0]
	add	r2, r2, #1
	str	r2, [r0, #0]
	b	.L72
.L73:
	ldr	r0, .L83+8
	bl	_Z12serial_closePi
	mov	r0, #9984
	add	r0, r0, #16
	bl	usleep
	mov	r3, #0
	str	r3, [fp, #-32]
.L69:
	ldr	r0, [fp, #-32]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L84:
	.align	2
.L83:
	.word	gps_port_disable
	.word	gps_port_baudrate
	.word	gps_port_handle
	.word	gps_port_dev
	.size	_Z12gps_readlinePci, .-_Z12gps_readlinePci
	.section	.rodata
	.align	2
.LC2:
	.ascii	"%2x\000"
	.text
	.align	2
	.global	_Z12gps_checksumPc
	.type	_Z12gps_checksumPc, %function
_Z12gps_checksumPc:
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #24
	str	r0, [fp, #-16]
	mov	r3, #85
	str	r3, [fp, #-20]
	mov	r3, #0
	strb	r3, [fp, #-21]
	mov	r3, #1
	str	r3, [fp, #-28]
.L86:
	ldr	r2, [fp, #-28]
	mov	r3, #396
	add	r3, r3, #3
	cmp	r2, r3
	bgt	.L87
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L90
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #13
	beq	.L90
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #10
	beq	.L90
	b	.L89
.L90:
	mov	r2, #0
	str	r2, [fp, #-32]
	b	.L85
.L89:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #42
	bne	.L91
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	add	r3, r3, #1
	sub	r2, fp, #20
	mov	r0, r3
	ldr	r1, .L93
	bl	sscanf
	ldrb	r3, [fp, #-21]	@ zero_extendqisi2
	str	r3, [fp, #-36]
	ldr	r3, [fp, #-20]
	ldr	r2, [fp, #-36]
	cmp	r2, r3
	movne	r3, #0
	moveq	r3, #1
	str	r3, [fp, #-36]
	ldr	r2, [fp, #-36]
	str	r2, [fp, #-32]
	b	.L85
.L91:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	ldrb	r2, [fp, #-21]
	ldrb	r3, [r3, #0]
	eor	r3, r2, r3
	strb	r3, [fp, #-21]
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L86
.L87:
	mov	r3, #0
	str	r3, [fp, #-32]
.L85:
	ldr	r0, [fp, #-32]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L94:
	.align	2
.L93:
	.word	.LC2
	.size	_Z12gps_checksumPc, .-_Z12gps_checksumPc
	.global	__floatsidf
	.global	__extendsfdf2
	.global	__divdf3
	.global	__adddf3
	.global	__truncdfsf2
	.local	_ZZ12gps_readdataP8gps_dataE7gpsfifo
	.comm	_ZZ12gps_readdataP8gps_dataE7gpsfifo,4,4
	.section	.rodata
	.align	2
.LC3:
	.ascii	"$GPRMC,\000"
	.align	2
.LC4:
	.ascii	"%2d%2d%f,%c,%2d%f,%c,%3d%f,%c,%f,%f,%2d%2d%d\000"
	.align	2
.LC5:
	.ascii	"/davinci/gpsfifo\000"
	.text
	.align	2
	.global	_Z12gps_readdataP8gps_data
	.type	_Z12gps_readdataP8gps_data, %function
_Z12gps_readdataP8gps_data:
	@ args = 0, pretend = 0, frame = 328
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #380
	str	r0, [fp, #-32]
	sub	r3, fp, #332
	mov	r0, r3
	mov	r1, #300
	bl	_Z12gps_readlinePci
	mov	r3, r0
	str	r3, [fp, #-336]
	ldr	r3, [fp, #-336]
	cmp	r3, #10
	ble	.L96
	sub	r3, fp, #332
	mov	r0, r3
	ldr	r1, .L107+8
	mov	r2, #7
	bl	memcmp
	mov	r3, r0
	cmp	r3, #0
	bne	.L97
	sub	r3, fp, #332
	mov	r0, r3
	bl	_Z12gps_checksumPc
	mov	r3, r0
	cmp	r3, #0
	beq	.L97
	mvn	r2, #308
	mov	r3, #0
	sub	r1, fp, #28
	strb	r3, [r1, r2]
	sub	r3, fp, #332
	add	r2, r3, #7
	ldr	r3, [fp, #-32]
	add	ip, r3, #16
	ldr	r3, [fp, #-32]
	add	lr, r3, #20
	ldr	r3, [fp, #-32]
	add	r3, r3, #24
	str	r3, [sp, #0]
	sub	r3, fp, #336
	sub	r3, r3, #1
	str	r3, [sp, #4]
	sub	r3, fp, #344
	str	r3, [sp, #8]
	ldr	r3, [fp, #-32]
	add	r3, r3, #28
	str	r3, [sp, #12]
	sub	r3, fp, #348
	sub	r3, r3, #1
	str	r3, [sp, #16]
	sub	r3, fp, #348
	str	r3, [sp, #20]
	ldr	r3, [fp, #-32]
	add	r3, r3, #32
	str	r3, [sp, #24]
	sub	r3, fp, #348
	sub	r3, r3, #2
	str	r3, [sp, #28]
	ldr	r3, [fp, #-32]
	add	r3, r3, #36
	str	r3, [sp, #32]
	ldr	r3, [fp, #-32]
	add	r3, r3, #40
	str	r3, [sp, #36]
	ldr	r3, [fp, #-32]
	add	r3, r3, #12
	str	r3, [sp, #40]
	ldr	r3, [fp, #-32]
	add	r3, r3, #8
	str	r3, [sp, #44]
	ldr	r3, [fp, #-32]
	add	r3, r3, #4
	str	r3, [sp, #48]
	mov	r0, r2
	ldr	r1, .L107+12
	mov	r2, ip
	mov	r3, lr
	bl	sscanf
	ldr	r2, [fp, #-32]
	mvn	r3, #308
	sub	r1, fp, #28
	ldrb	r3, [r1, r3]	@ zero_extendqisi2
	cmp	r3, #65
	movne	r3, #0
	moveq	r3, #1
	str	r3, [r2, #0]
	ldr	r7, [fp, #-32]
	ldr	r0, [fp, #-344]
	bl	__floatsidf
	mov	r6, r1
	mov	r5, r0
	ldr	r3, [fp, #-32]
	ldr	r0, [r3, #28]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L107
	ldmia	r2, {r2-r3}
	bl	__divdf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r6
	mov	r0, r5
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [r7, #28]	@ float
	mvn	r3, #320
	sub	r2, fp, #28
	ldrb	r3, [r2, r3]	@ zero_extendqisi2
	cmp	r3, #78
	beq	.L98
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, #28]
	eor	r3, r3, #-2147483648
	str	r3, [r2, #28]	@ float
.L98:
	ldr	r7, [fp, #-32]
	ldr	r0, [fp, #-348]
	bl	__floatsidf
	mov	r6, r1
	mov	r5, r0
	ldr	r3, [fp, #-32]
	ldr	r0, [r3, #32]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L107
	ldmia	r2, {r2-r3}
	bl	__divdf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r6
	mov	r0, r5
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [r7, #32]	@ float
	mvn	r3, #320
	sub	r3, r3, #1
	sub	r1, fp, #28
	ldrb	r3, [r1, r3]	@ zero_extendqisi2
	cmp	r3, #69
	beq	.L99
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, #32]
	eor	r3, r3, #-2147483648
	str	r3, [r2, #32]	@ float
.L99:
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, #4]
	cmp	r3, #79
	ble	.L100
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, #4]
	add	r3, r3, #1888
	add	r3, r3, #12
	str	r3, [r2, #4]
	b	.L101
.L100:
	ldr	r2, [fp, #-32]
	ldr	r3, [fp, #-32]
	ldr	r3, [r3, #4]
	add	r3, r3, #2000
	str	r3, [r2, #4]
.L101:
	ldr	r3, .L107+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bgt	.L102
	ldr	r0, .L107+20
	mov	r1, #2048
	add	r1, r1, #1
	bl	open
	mov	r2, r0
	ldr	r3, .L107+16
	str	r2, [r3, #0]
.L102:
	ldr	r3, .L107+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L103
	ldr	r3, .L107+16
	sub	r2, fp, #332
	ldr	r0, [r3, #0]
	mov	r1, r2
	ldr	r2, [fp, #-336]
	bl	write
	mov	r2, r0
	ldr	r3, [fp, #-336]
	cmp	r2, r3
	beq	.L103
	ldr	r3, .L107+16
	ldr	r0, [r3, #0]
	bl	close
	ldr	r2, .L107+16
	mov	r3, #0
	str	r3, [r2, #0]
.L103:
	mov	r2, #1
	str	r2, [fp, #-356]
	b	.L95
.L97:
	mov	r3, #2
	str	r3, [fp, #-356]
	b	.L95
.L96:
	ldr	r2, [fp, #-32]
	mov	r3, #0
	str	r3, [r2, #0]
	mov	r1, #0
	str	r1, [fp, #-356]
.L95:
	ldr	r0, [fp, #-356]
	sub	sp, fp, #28
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.L108:
	.align	2
.L107:
	.word	1078853632
	.word	0
	.word	.LC3
	.word	.LC4
	.word	_ZZ12gps_readdataP8gps_dataE7gpsfifo
	.word	.LC5
	.size	_Z12gps_readdataP8gps_data, .-_Z12gps_readdataP8gps_data
	.global	gps_logfile
	.bss
	.align	2
	.type	gps_logfile, %object
	.size	gps_logfile, 4
gps_logfile:
	.space	4
	.local	gps_logday
	.comm	gps_logday,4,4
	.global	validgpsdata
	.align	2
	.type	validgpsdata, %object
	.size	validgpsdata, 44
validgpsdata:
	.space	44
	.local	gps_close_fine
	.comm	gps_close_fine,4,4
	.text
	.align	2
	.global	_Z12gps_logclosev
	.type	_Z12gps_logclosev, %function
_Z12gps_logclosev:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r3, .L111
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L109
	ldr	r3, .L111
	ldr	r0, [r3, #0]
	bl	fclose
	ldr	r2, .L111
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r2, .L111+4
	mov	r3, #1
	str	r3, [r2, #0]
.L109:
	ldmfd	sp, {fp, sp, pc}
.L112:
	.align	2
.L111:
	.word	gps_logfile
	.word	gps_close_fine
	.size	_Z12gps_logclosev, .-_Z12gps_logclosev
	.global	_Unwind_SjLj_Resume
	.global	__gxx_personality_sj0
	.global	_Unwind_SjLj_Register
	.global	_Unwind_SjLj_Unregister
	.section	.rodata
	.align	2
.LC6:
	.ascii	"%s/smartlog/%s_%04d%02d%02d_N.001\000"
	.align	2
.LC7:
	.ascii	"r\000"
	.align	2
.LC8:
	.ascii	"%s/smartlog\000"
	.align	2
.LC9:
	.ascii	"%s/smartlog/%s_%04d%02d%02d_L.001\000"
	.align	2
.LC10:
	.ascii	"a\000"
	.align	2
.LC11:
	.ascii	"%04d-%02d-%02d\n"
	.ascii	"99\000"
	.align	2
.LC12:
	.ascii	",%s\000"
	.align	2
.LC13:
	.ascii	"\n\000"
	.text
	.align	2
	.global	_Z11gps_logopenP2tm
	.type	_Z11gps_logopenP2tm, %function
_Z11gps_logopenP2tm:
	@ args = 0, pretend = 0, frame = 9108
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #9088
	sub	sp, sp, #32
	str	r0, [fp, #-44]
	mvn	r3, #9024
	sub	r3, r3, #47
	ldr	r2, .L141
	sub	r0, fp, #40
	str	r2, [r0, r3]
	mvn	r3, #9024
	sub	r3, r3, #43
	ldr	r2, .L141+4
	sub	r1, fp, #40
	str	r2, [r1, r3]
	sub	r3, fp, #9024
	sub	r3, r3, #40
	sub	r3, r3, #40
	sub	r2, fp, #40
	str	r2, [r3, #0]
	ldr	r2, .L141+8
	str	r2, [r3, #4]
	str	sp, [r3, #8]
	sub	r3, fp, #9088
	sub	r3, r3, #40
	sub	r3, r3, #8
	mov	r0, r3
	bl	_Unwind_SjLj_Register
	ldr	r3, [fp, #-44]
	ldr	r2, .L141+12
	ldr	r1, [r3, #12]
	ldr	r3, [r2, #0]
	cmp	r1, r3
	beq	.L114
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	ip, fp, #40
	str	r2, [ip, r3]
	bl	_Z12gps_logclosev
	ldr	r2, .L141+12
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #12]
	str	r3, [r2, #0]
	ldr	r2, .L141+16
	mov	r3, #0
	strb	r3, [r2, #0]
.L114:
	ldr	r3, .L141+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L115
	ldr	r3, .L141+16
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L116
	sub	r3, fp, #428
	mov	r0, r3
	mov	r1, #127
	bl	gethostname
	sub	r1, fp, #9024
	sub	r1, r1, #40
	sub	r1, r1, #16
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	r0, fp, #40
	str	r2, [r0, r3]
	mov	r0, r1
	ldr	r1, .L141+24
	ldr	r3, .L141+28
	mov	lr, pc
	mov	pc, r3
.L117:
	sub	r1, fp, #9024
	sub	r1, r1, #40
	sub	r1, r1, #16
	mvn	r3, #9088
	sub	r3, r3, #3
	mov	r2, #1
	sub	ip, fp, #40
	str	r2, [ip, r3]
	mov	r0, r1
	ldr	r3, .L141+32
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	beq	.L118
	sub	r3, fp, #9024
	sub	r3, r3, #40
	sub	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L141+36
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	beq	.L117
	sub	r3, fp, #9024
	sub	r3, r3, #40
	sub	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L141+40
	mov	lr, pc
	mov	pc, r3
	mov	r2, r0
	sub	ip, fp, #428
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #20]
	add	r3, r3, #1888
	add	r3, r3, #12
	str	r3, [sp, #0]
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #16]
	add	r3, r3, #1
	str	r3, [sp, #4]
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #12]
	str	r3, [sp, #8]
	ldr	r0, .L141+16
	ldr	r1, .L141+44
	mov	r3, ip
	bl	sprintf
	sub	r3, fp, #524
	ldr	r0, .L141+16
	mov	r1, r3
	bl	stat
	mov	r3, r0
	cmp	r3, #0
	bne	.L120
	ldr	r3, [fp, #-508]
	and	r3, r3, #61440
	cmp	r3, #32768
	bne	.L120
	sub	r3, fp, #300
	mov	r0, r3
	ldr	r1, .L141+16
	mov	r2, #256
	bl	strncpy
	ldr	r0, .L141+16
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-436]
	ldr	r2, .L141+16
	mvn	r1, #4
	ldr	r3, [fp, #-436]
	add	r3, r2, r3
	add	r2, r3, r1
	mov	r3, #76
	strb	r3, [r2, #0]
	sub	r3, fp, #300
	mov	r0, r3
	ldr	r1, .L141+16
	bl	rename
	b	.L118
.L120:
	ldr	r0, .L141+16
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-436]
	ldr	r2, .L141+16
	mvn	r1, #4
	ldr	r3, [fp, #-436]
	add	r3, r2, r3
	add	r2, r3, r1
	mov	r3, #76
	strb	r3, [r2, #0]
	sub	r3, fp, #524
	ldr	r0, .L141+16
	mov	r1, r3
	bl	stat
	mov	r3, r0
	cmp	r3, #0
	bne	.L122
	ldr	r3, [fp, #-508]
	and	r3, r3, #61440
	cmp	r3, #32768
	bne	.L122
	b	.L118
.L122:
	ldr	r3, .L141+16
	mov	r2, #0
	strb	r2, [r3, #0]
	b	.L117
.L118:
	ldr	r3, .L141+16
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L128
	mvn	r3, #9088
	sub	r3, r3, #3
	mov	r2, #1
	sub	r0, fp, #40
	str	r2, [r0, r3]
	ldr	r0, .L141+48
	ldr	r3, .L141+52
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L141+56
	bl	fopen
	mov	r2, r0
	mvn	r3, #9024
	sub	r3, r3, #19
	sub	r1, fp, #40
	str	r2, [r1, r3]
	mvn	r3, #9024
	sub	r3, r3, #19
	sub	r2, fp, #40
	ldr	r3, [r2, r3]
	cmp	r3, #0
	beq	.L128
	sub	r2, fp, #300
	mvn	r3, #9024
	sub	r3, r3, #19
	mov	r0, r2
	mov	r1, #1
	mov	r2, #255
	sub	ip, fp, #40
	ldr	r3, [ip, r3]
	bl	fread
	str	r0, [fp, #-432]
	mvn	r3, #9024
	sub	r3, r3, #19
	sub	r1, fp, #40
	ldr	r0, [r1, r3]
	bl	fclose
	ldr	r3, [fp, #-432]
	cmp	r3, #2
	ble	.L128
	mov	r3, #-1090519040
	mov	r3, r3, asr #22
	ldr	r2, [fp, #-432]
	sub	ip, fp, #40
	add	r2, ip, r2
	add	r2, r2, r3
	mov	r3, #0
	strb	r3, [r2, #0]
	sub	r3, fp, #300
	ldr	r0, .L141+16
	ldr	r1, .L141+60
	mov	r2, r3
	bl	sprintf
	ldr	r0, .L141+16
	mov	r1, #508
	add	r1, r1, #3
	bl	mkdir
	sub	r3, fp, #428
	mov	r0, r3
	mov	r1, #127
	bl	gethostname
	sub	r2, fp, #300
	sub	ip, fp, #428
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #20]
	add	r3, r3, #1888
	add	r3, r3, #12
	str	r3, [sp, #0]
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #16]
	add	r3, r3, #1
	str	r3, [sp, #4]
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #12]
	str	r3, [sp, #8]
	ldr	r0, .L141+16
	ldr	r1, .L141+64
	mov	r3, ip
	bl	sprintf
	b	.L128
.L140:
	add	fp, fp, #40
	mov	r3, #-1191182336
	mov	r3, r3, asr #17
	sub	r0, fp, #40
	ldr	r3, [r0, r3]
	sub	r0, fp, #8192
	str	r3, [r0, #-956]
.L127:
	sub	r1, fp, #8192
	ldr	r2, [r1, #-956]
	sub	r1, fp, #8192
	str	r2, [r1, #-952]
	sub	r3, fp, #9024
	sub	r3, r3, #40
	sub	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L141+68
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #8192
	ldr	ip, [r3, #-952]
	sub	r3, fp, #8192
	str	ip, [r3, #-956]
.L129:
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	r0, fp, #40
	str	r2, [r0, r3]
	sub	r1, fp, #8192
	ldr	r0, [r1, #-956]
	bl	_Unwind_SjLj_Resume
.L128:
	sub	r3, fp, #9024
	sub	r3, r3, #40
	sub	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L141+68
	mov	lr, pc
	mov	pc, r3
.L116:
	ldr	r3, .L141+16
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L131
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	ip, fp, #40
	str	r2, [ip, r3]
	ldr	r0, .L141+16
	ldr	r1, .L141+72
	bl	fopen
	mov	r2, r0
	ldr	r3, .L141+20
	str	r2, [r3, #0]
.L131:
	ldr	r3, .L141+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L132
	ldr	r3, .L141+20
	ldr	r0, [r3, #0]
	mov	r1, #0
	mov	r2, #2
	bl	fseek
	ldr	r1, .L141+20
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	r0, fp, #40
	str	r2, [r0, r3]
	ldr	r0, [r1, #0]
	bl	ftell
	mov	r3, r0
	cmp	r3, #9
	bgt	.L133
	ldr	r3, .L141+20
	ldr	r0, [r3, #0]
	mov	r1, #0
	mov	r2, #0
	bl	fseek
	ldr	r1, .L141+20
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #20]
	add	r2, r3, #1888
	add	r2, r2, #12
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #16]
	add	ip, r3, #1
	ldr	r3, [fp, #-44]
	ldr	r3, [r3, #12]
	str	r3, [sp, #0]
	ldr	r0, [r1, #0]
	ldr	r1, .L141+76
	mov	r3, ip
	bl	fprintf
	mov	r3, #0
	str	r3, [fp, #-436]
.L134:
	ldr	r3, [fp, #-436]
	cmp	r3, #31
	bgt	.L135
	ldr	r3, .L141+80
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #20]
	ldr	r3, [fp, #-436]
	cmp	r2, r3
	ble	.L135
	ldr	r3, [fp, #-436]
	mov	r2, r3, asl #2
	ldr	r3, .L141+84
	add	r1, r2, r3
	mvn	r3, #9088
	sub	r3, r3, #3
	mvn	r2, #0
	sub	ip, fp, #40
	str	r2, [ip, r3]
	mov	r0, r1
	ldr	r3, .L141+52
	mov	lr, pc
	mov	pc, r3
	mov	r2, r0
	ldr	r3, .L141+20
	ldr	r0, [r3, #0]
	ldr	r1, .L141+88
	bl	fprintf
	ldr	r3, [fp, #-436]
	add	r3, r3, #1
	str	r3, [fp, #-436]
	b	.L134
.L135:
	ldr	r3, .L141+20
	mvn	r2, #9088
	sub	r2, r2, #3
	mvn	r1, #0
	sub	r0, fp, #40
	str	r1, [r0, r2]
	ldr	r0, [r3, #0]
	ldr	r1, .L141+92
	bl	fprintf
	b	.L115
.L133:
	ldr	r3, .L141+96
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L115
	ldr	r3, .L141+20
	mvn	r2, #9088
	sub	r2, r2, #3
	mvn	r1, #0
	sub	ip, fp, #40
	str	r1, [ip, r2]
	ldr	r0, [r3, #0]
	ldr	r1, .L141+92
	bl	fprintf
	b	.L115
.L132:
	ldr	r3, .L141+16
	mov	r2, #0
	strb	r2, [r3, #0]
.L115:
	ldr	r3, .L141+20
	ldr	r3, [r3, #0]
	sub	r0, fp, #8192
	str	r3, [r0, #-948]
.L113:
	sub	r3, fp, #9088
	sub	r3, r3, #40
	sub	r3, r3, #8
	mov	r0, r3
	bl	_Unwind_SjLj_Unregister
	sub	r1, fp, #8192
	ldr	r0, [r1, #-948]
	sub	sp, fp, #40
	ldmfd	sp, {r4, r5, r6, r7, r8, r9, sl, fp, sp, pc}
.L142:
	.align	2
.L141:
	.word	__gxx_personality_sj0
	.word	.LLSDA97
	.word	.L140
	.word	gps_logday
	.word	gpslogfilename
	.word	gps_logfile
	.word	disksroot
	.word	_ZN8dir_findC1EPKc
	.word	_ZN8dir_find4findEv
	.word	_ZN8dir_find5isdirEv
	.word	_ZN8dir_find8pathnameEv
	.word	.LC6
	.word	dvrcurdisk
	.word	_ZN6string9getstringEv
	.word	.LC7
	.word	.LC8
	.word	.LC9
	.word	_ZN8dir_findD1Ev
	.word	.LC10
	.word	.LC11
	.word	p_dio_mmap
	.word	sensorname
	.word	.LC12
	.word	.LC13
	.word	gps_close_fine
	.size	_Z11gps_logopenP2tm, .-_Z11gps_logopenP2tm
	.section	.gcc_except_table,"a",%progbits
.LLSDA97:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE97-.LLSDACSB97
.LLSDACSB97:
	.uleb128 0x0
	.uleb128 0x0
.LLSDACSE97:
	.text
	.section	.gnu.linkonce.t._ZN8dir_findD1Ev,"ax",%progbits
	.align	2
	.weak	_ZN8dir_findD1Ev
	.type	_ZN8dir_findD1Ev, %function
_ZN8dir_findD1Ev:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r0, [fp, #-16]
	ldr	r3, .L146
	mov	lr, pc
	mov	pc, r3
	ldmib	sp, {fp, sp, pc}
.L147:
	.align	2
.L146:
	.word	_ZN8dir_find5closeEv
	.size	_ZN8dir_findD1Ev, .-_ZN8dir_findD1Ev
	.section	.gnu.linkonce.t._ZN8dir_find5closeEv,"ax",%progbits
	.align	2
	.weak	_ZN8dir_find5closeEv
	.type	_ZN8dir_find5closeEv, %function
_ZN8dir_find5closeEv:
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
	beq	.L148
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	closedir
	ldr	r2, [fp, #-16]
	mov	r3, #0
	str	r3, [r2, #0]
.L148:
	ldmib	sp, {fp, sp, pc}
	.size	_ZN8dir_find5closeEv, .-_ZN8dir_find5closeEv
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
	bne	.L151
	ldr	r4, [fp, #-20]
	mov	r0, #4
	bl	_Znaj
	str	r0, [r4, #0]
	ldr	r3, [fp, #-20]
	ldr	r2, [r3, #0]
	mov	r3, #0
	strb	r3, [r2, #0]
.L151:
	ldr	r3, [fp, #-20]
	ldr	r3, [r3, #0]
	mov	r0, r3
	ldmib	sp, {r4, fp, sp, pc}
	.size	_ZN6string9getstringEv, .-_ZN6string9getstringEv
	.section	.gnu.linkonce.t._ZN8dir_find8pathnameEv,"ax",%progbits
	.align	2
	.weak	_ZN8dir_find8pathnameEv
	.type	_ZN8dir_find8pathnameEv, %function
_ZN8dir_find8pathnameEv:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L153
	ldr	r3, [fp, #-16]
	add	r3, r3, #4352
	add	r3, r3, #20
	ldr	r2, [fp, #-16]
	add	r2, r2, #276
	mov	r0, r3
	mov	r1, r2
	bl	strcpy
	ldr	r3, [fp, #-16]
	add	r2, r3, #4352
	add	r2, r2, #20
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	add	r3, r3, #11
	mov	r0, r2
	mov	r1, r3
	bl	strcat
	ldr	r3, [fp, #-16]
	add	r3, r3, #4352
	add	r3, r3, #20
	str	r3, [fp, #-20]
	b	.L152
.L153:
	mov	r3, #0
	str	r3, [fp, #-20]
.L152:
	ldr	r0, [fp, #-20]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_ZN8dir_find8pathnameEv, .-_ZN8dir_find8pathnameEv
	.section	.gnu.linkonce.t._ZN8dir_find5isdirEv,"ax",%progbits
	.align	2
	.weak	_ZN8dir_find5isdirEv
	.type	_ZN8dir_find5isdirEv, %function
_ZN8dir_find5isdirEv:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	cmp	r3, #0
	beq	.L155
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #10]	@ zero_extendqisi2
	cmp	r3, #4
	movne	r3, #0
	moveq	r3, #1
	str	r3, [fp, #-20]
	b	.L154
.L155:
	mov	r3, #0
	str	r3, [fp, #-20]
.L154:
	ldr	r0, [fp, #-20]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_ZN8dir_find5isdirEv, .-_ZN8dir_find5isdirEv
	.section	.gnu.linkonce.t._ZN8dir_find4findEv,"ax",%progbits
	.align	2
	.weak	_ZN8dir_find4findEv
	.type	_ZN8dir_find4findEv, %function
_ZN8dir_find4findEv:
	@ args = 0, pretend = 0, frame = 96
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #96
	str	r0, [fp, #-16]
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L158
.L159:
	ldr	r2, [fp, #-16]
	ldr	r3, [fp, #-16]
	add	r1, r3, #8
	ldr	r3, [fp, #-16]
	add	r3, r3, #4
	ldr	r0, [r2, #0]
	mov	r2, r3
	bl	readdir_r
	mov	r3, r0
	cmp	r3, #0
	bne	.L158
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	cmp	r3, #0
	bne	.L161
	b	.L158
.L161:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #11]	@ zero_extendqisi2
	cmp	r3, #46
	bne	.L164
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #12]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L164
	b	.L159
.L164:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #11]	@ zero_extendqisi2
	cmp	r3, #46
	bne	.L162
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #12]	@ zero_extendqisi2
	cmp	r3, #46
	bne	.L162
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #13]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L162
	b	.L159
.L162:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	ldrb	r3, [r3, #10]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L165
	ldr	r3, [fp, #-16]
	add	r3, r3, #4352
	add	r3, r3, #20
	ldr	r2, [fp, #-16]
	add	r2, r2, #276
	mov	r0, r3
	mov	r1, r2
	bl	strcpy
	ldr	r3, [fp, #-16]
	add	r2, r3, #4352
	add	r2, r2, #20
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #4]
	add	r3, r3, #11
	mov	r0, r2
	mov	r1, r3
	bl	strcat
	ldr	r3, [fp, #-16]
	add	r3, r3, #4352
	add	r3, r3, #20
	sub	r2, fp, #104
	mov	r0, r3
	mov	r1, r2
	bl	stat
	mov	r3, r0
	cmp	r3, #0
	bne	.L165
	ldr	r3, [fp, #-88]
	and	r3, r3, #61440
	cmp	r3, #16384
	bne	.L167
	ldr	r3, [fp, #-16]
	ldr	r2, [r3, #4]
	mov	r3, #4
	strb	r3, [r2, #10]
	b	.L165
.L167:
	ldr	r3, [fp, #-88]
	and	r3, r3, #61440
	cmp	r3, #32768
	bne	.L165
	ldr	r3, [fp, #-16]
	ldr	r2, [r3, #4]
	mov	r3, #8
	strb	r3, [r2, #10]
.L165:
	mov	r3, #1
	str	r3, [fp, #-108]
	b	.L157
.L158:
	ldr	r2, [fp, #-16]
	mov	r3, #0
	str	r3, [r2, #4]
	mov	r3, #0
	str	r3, [fp, #-108]
.L157:
	ldr	r0, [fp, #-108]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_ZN8dir_find4findEv, .-_ZN8dir_find4findEv
	.section	.gnu.linkonce.t._ZN8dir_findC1EPKc,"ax",%progbits
	.align	2
	.weak	_ZN8dir_findC1EPKc
	.type	_ZN8dir_findC1EPKc, %function
_ZN8dir_findC1EPKc:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, [fp, #-16]
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	ldr	r3, .L171
	mov	lr, pc
	mov	pc, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L172:
	.align	2
.L171:
	.word	_ZN8dir_find4openEPKc
	.size	_ZN8dir_findC1EPKc, .-_ZN8dir_findC1EPKc
	.section	.gnu.linkonce.t._ZN8dir_find4openEPKc,"ax",%progbits
	.align	2
	.weak	_ZN8dir_find4openEPKc
	.type	_ZN8dir_find4openEPKc, %function
_ZN8dir_find4openEPKc:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	str	r0, [fp, #-20]
	str	r1, [fp, #-24]
	ldr	r0, [fp, #-20]
	ldr	r3, .L176
	mov	lr, pc
	mov	pc, r3
	ldr	r2, [fp, #-20]
	mov	r3, #0
	str	r3, [r2, #4]
	ldr	r3, [fp, #-20]
	add	r3, r3, #276
	mov	r0, r3
	ldr	r1, [fp, #-24]
	mov	r2, #4096
	bl	strncpy
	ldr	r3, [fp, #-20]
	add	r3, r3, #276
	mov	r0, r3
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	ble	.L174
	ldr	r1, [fp, #-20]
	mov	r3, #272
	add	r3, r3, #3
	ldr	r2, [fp, #-28]
	add	r2, r1, r2
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #47
	beq	.L174
	ldr	r2, [fp, #-20]
	mov	r1, #276
	ldr	r3, [fp, #-28]
	add	r3, r2, r3
	add	r2, r3, r1
	mov	r3, #47
	strb	r3, [r2, #0]
	ldr	r1, [fp, #-20]
	mov	r3, #276
	add	r3, r3, #1
	ldr	r2, [fp, #-28]
	add	r2, r1, r2
	add	r2, r2, r3
	mov	r3, #0
	strb	r3, [r2, #0]
.L174:
	ldr	r4, [fp, #-20]
	ldr	r3, [fp, #-20]
	add	r3, r3, #276
	mov	r0, r3
	bl	opendir
	str	r0, [r4, #0]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L177:
	.align	2
.L176:
	.word	_ZN8dir_find5closeEv
	.size	_ZN8dir_find4openEPKc, .-_ZN8dir_find4openEPKc
	.global	__ltsf2
	.global	__muldf3
	.section	.rodata
	.align	2
.LC15:
	.ascii	"%02d,%02d:%02d:%02d,%09.6f%c%010.6f%c%.1fD%06.2f\000"
	.text
	.align	2
	.global	_Z13gps_logprintfiPcz
	.type	_Z13gps_logprintfiPcz, %function
_Z13gps_logprintfiPcz:
	@ args = 4, pretend = 12, frame = 80
	@ frame_needed = 1, uses_anonymous_args = 1
	mov	ip, sp
	stmfd	sp!, {r1, r2, r3}
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #16
	sub	sp, sp, #128
	str	r0, [fp, #-20]
	mov	r0, #0
	bl	time
	mov	r3, r0
	str	r3, [fp, #-76]
	sub	r3, fp, #76
	sub	r2, fp, #72
	mov	r0, r3
	mov	r1, r2
	bl	localtime_r
	ldr	r3, [fp, #-52]
	cmp	r3, #104
	ble	.L180
	ldr	r3, [fp, #-52]
	cmp	r3, #150
	bgt	.L180
	b	.L179
.L180:
	mov	r2, #0
	str	r2, [fp, #-80]
	b	.L178
.L179:
	bl	_Z8log_lockv
	sub	r3, fp, #72
	mov	r0, r3
	bl	_Z11gps_logopenP2tm
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	bne	.L181
	bl	_Z10log_unlockv
	ldr	r2, .L194+8
	mov	r3, #0
	str	r3, [r2, #0]
	mov	r3, #0
	str	r3, [fp, #-80]
	b	.L178
.L181:
	ldr	r3, [fp, #-68]
	str	r3, [sp, #0]
	ldr	r3, [fp, #-72]
	str	r3, [sp, #4]
	ldr	r3, .L194+12
	ldr	r3, [r3, #28]	@ float
	str	r3, [fp, #-84]	@ float
	ldr	r3, .L194+12
	ldr	r0, [r3, #28]	@ float
	ldr	r1, .L194+16	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L183
	b	.L182
.L183:
	ldr	r2, [fp, #-84]
	eor	r3, r2, #-2147483648
	str	r3, [fp, #-84]	@ float
.L182:
	ldr	r0, [fp, #-84]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #8
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	ldr	r3, .L194+12
	ldr	r0, [r3, #28]	@ float
	ldr	r1, .L194+16	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L186
	b	.L184
.L186:
	mov	r3, #83
	str	r3, [fp, #-88]
	b	.L185
.L184:
	mov	r2, #78
	str	r2, [fp, #-88]
.L185:
	ldr	r3, [fp, #-88]
	str	r3, [sp, #16]
	ldr	r3, .L194+12
	ldr	r3, [r3, #32]	@ float
	str	r3, [fp, #-92]	@ float
	ldr	r3, .L194+12
	ldr	r0, [r3, #32]	@ float
	ldr	r1, .L194+16	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L188
	b	.L187
.L188:
	ldr	r2, [fp, #-92]
	eor	r3, r2, #-2147483648
	str	r3, [fp, #-92]	@ float
.L187:
	ldr	r0, [fp, #-92]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #20
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	ldr	r3, .L194+12
	ldr	r0, [r3, #32]	@ float
	ldr	r1, .L194+16	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L191
	b	.L189
.L191:
	mov	r3, #87
	str	r3, [fp, #-96]
	b	.L190
.L189:
	mov	r2, #69
	str	r2, [fp, #-96]
.L190:
	ldr	r3, [fp, #-96]
	str	r3, [sp, #28]
	ldr	r3, .L194+12
	ldr	r0, [r3, #36]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L194
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	mov	r0, r3
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #32
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	ldr	r3, .L194+12
	ldr	r0, [r3, #40]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #40
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	ldr	r0, [fp, #-24]
	ldr	r1, .L194+20
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-64]
	bl	fprintf
	add	r3, fp, #8
	str	r3, [fp, #-28]
	ldr	r0, [fp, #-24]
	ldr	r1, [fp, #4]
	ldr	r2, [fp, #-28]
	bl	vfprintf
	ldr	r0, [fp, #-24]
	ldr	r1, .L194+24
	bl	fprintf
	ldr	r3, .L194+12
	ldr	r0, [r3, #36]	@ float
	ldr	r1, .L194+28	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L193
	b	.L192
.L193:
	bl	_Z12gps_logclosev
.L192:
	bl	_Z10log_unlockv
	ldr	r2, .L194+8
	mov	r3, #1
	str	r3, [r2, #0]
	mov	r2, #1
	str	r2, [fp, #-80]
.L178:
	ldr	r0, [fp, #-80]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L195:
	.align	2
.L194:
	.word	1073586634
	.word	-1065151889
	.word	glog_ok
	.word	validgpsdata
	.word	0
	.word	.LC15
	.word	.LC13
	.word	1065353216
	.size	_Z13gps_logprintfiPcz, .-_Z13gps_logprintfiPcz
	.local	_ZZ7gps_logvE7logtime
	.comm	_ZZ7gps_logvE7logtime,4,4
	.local	_ZZ7gps_logvE4stop
	.comm	_ZZ7gps_logvE4stop,4,4
	.global	__subsf3
	.global	__ltdf2
	.global	__gtsf2
	.section	.rodata
	.align	2
.LC16:
	.ascii	"\000"
	.text
	.align	2
	.global	_Z7gps_logv
	.type	_Z7gps_logv, %function
_Z7gps_logv:
	@ args = 0, pretend = 0, frame = 12
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #12
	mov	r3, #0
	str	r3, [fp, #-36]
	ldr	r3, .L208+24
	ldr	r0, [r3, #16]
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L208
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r6, r1
	mov	r5, r0
	ldr	r3, .L208+24
	ldr	r0, [r3, #20]
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L208+8
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r6
	mov	r0, r5
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r6, r1
	mov	r5, r0
	ldr	r3, .L208+24
	ldr	r0, [r3, #24]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r6
	mov	r0, r5
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [fp, #-28]	@ float
	ldr	r3, .L208+28
	ldr	r0, [fp, #-28]	@ float
	ldr	r1, [r3, #0]	@ float
	bl	__subsf3
	mov	r3, r0
	str	r3, [fp, #-32]	@ float
	ldr	r0, [fp, #-32]	@ float
	ldr	r1, .L208+32	@ float
	bl	__ltsf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L198
	b	.L197
.L198:
	ldr	r3, .L208+36	@ float
	str	r3, [fp, #-32]	@ float
.L197:
	ldr	r3, .L208+24
	ldr	r0, [r3, #36]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L208+16
	ldmia	r2, {r2-r3}
	bl	__ltdf2
	mov	r3, r0
	cmp	r3, #0
	blt	.L200
	b	.L199
.L200:
	ldr	r3, .L208+40
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L202
	ldr	r0, [fp, #-32]	@ float
	ldr	r1, .L208+44	@ float
	bl	__gtsf2
	mov	r3, r0
	cmp	r3, #0
	bgt	.L202
	b	.L203
.L202:
	ldr	r3, .L208+40
	mov	r2, #1
	str	r2, [r3, #0]
	mov	r3, #1
	str	r3, [fp, #-36]
	b	.L203
.L199:
	ldr	r0, [fp, #-32]	@ float
	ldr	r1, .L208+48	@ float
	bl	__gtsf2
	mov	r3, r0
	cmp	r3, #0
	bgt	.L205
	b	.L203
.L205:
	ldr	r3, .L208+40
	mov	r2, #0
	str	r2, [r3, #0]
	mov	r3, #1
	str	r3, [fp, #-36]
.L203:
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	beq	.L206
	mov	r0, #1
	ldr	r1, .L208+52
	bl	_Z13gps_logprintfiPcz
	mov	r3, r0
	cmp	r3, #0
	beq	.L206
	ldr	r2, .L208+28
	ldr	r3, [fp, #-28]	@ float
	str	r3, [r2, #0]	@ float
.L206:
	mov	r3, #1
	mov	r0, r3
	sub	sp, fp, #24
	ldmfd	sp, {r4, r5, r6, fp, sp, pc}
.L209:
	.align	2
.L208:
	.word	1085022208
	.word	0
	.word	1078853632
	.word	0
	.word	1070176665
	.word	-1717986918
	.word	validgpsdata
	.word	_ZZ7gps_logvE7logtime
	.word	0
	.word	1148846080
	.word	_ZZ7gps_logvE4stop
	.word	1114636288
	.word	1077936128
	.word	.LC16
	.size	_Z7gps_logv, .-_Z7gps_logv
	.global	__subdf3
	.global	__gtdf2
	.global	__nedf2
	.section	.rodata
	.align	2
.LC17:
	.ascii	",%d\000"
	.align	2
.LC18:
	.ascii	",%.1f,%.1f,%.1f\000"
	.text
	.align	2
	.global	_Z10sensor_logv
	.type	_Z10sensor_logv, %function
_Z10sensor_logv:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #64
	ldr	r3, .L228+24
	ldr	r2, [r3, #4]
	mov	r3, #1888
	add	r3, r3, #11
	cmp	r2, r3
	bgt	.L211
	mov	r1, #0
	str	r1, [fp, #-60]
	b	.L210
.L211:
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r2, .L228+28
	ldr	r1, [r3, #44]
	ldr	r3, [r2, #0]
	cmp	r1, r3
	beq	.L212
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	cmp	r3, #0
	movne	r3, #0
	moveq	r3, #1
	mov	r0, #2
	ldr	r1, .L228+32
	mov	r2, r3
	bl	_Z13gps_logprintfiPcz
	mov	r3, r0
	cmp	r3, #0
	beq	.L213
	ldr	r2, .L228+28
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #44]
	str	r3, [r2, #0]
	b	.L212
.L213:
	mov	r2, #0
	str	r2, [fp, #-60]
	b	.L210
.L212:
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #24]
	str	r3, [fp, #-32]
	mov	r3, #3
	str	r3, [fp, #-36]
	sub	r3, fp, #52
	mov	r0, r3
	mov	r1, #0
	bl	gettimeofday
	mvn	r3, #19
	sub	r1, fp, #24
	add	r1, r1, r3
	str	r1, [fp, #-64]
	mvn	r3, #27
	sub	r2, fp, #24
	add	r4, r2, r3
	ldr	r0, [r4, #0]
	bl	__floatsidf
	sub	r3, fp, #64
	stmdb	r3, {r0-r1}
	ldr	r3, [r4, #0]
	cmp	r3, #0
	bge	.L215
	sub	r2, fp, #64
	ldmdb	r2, {r0-r1}
	adr	r2, .L228
	ldmia	r2, {r2-r3}
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	sub	r1, fp, #64
	stmdb	r1, {r3-r4}
.L215:
	mvn	r3, #27
	sub	r2, fp, #24
	add	r3, r2, r3
	ldr	r0, [r3, #4]
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L228+8
	ldmia	r2, {r2-r3}
	bl	__divdf3
	mov	r4, r1
	mov	r3, r0
	sub	r2, fp, #64
	ldmdb	r2, {r0-r1}
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	ldr	r1, [fp, #-64]
	stmia	r1, {r3-r4}
	mov	r3, #0
	str	r3, [fp, #-28]
.L216:
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #20]
	ldr	r3, [fp, #-28]
	cmp	r2, r3
	ble	.L217
	mov	r2, #1
	ldr	r3, [fp, #-28]
	mov	r2, r2, asl r3
	ldr	r3, [fp, #-32]
	and	r3, r2, r3
	cmp	r3, #0
	moveq	r3, #0
	movne	r3, #1
	str	r3, [fp, #-56]
	ldr	r2, .L228+36
	ldr	r3, [fp, #-28]
	ldr	r3, [r2, r3, asl #2]
	cmp	r3, #0
	beq	.L219
	ldr	r3, [fp, #-56]
	cmp	r3, #0
	movne	r3, #0
	moveq	r3, #1
	str	r3, [fp, #-56]
.L219:
	ldr	r2, .L228+40
	ldr	r3, [fp, #-28]
	ldr	r2, [r2, r3, asl #2]
	ldr	r3, [fp, #-56]
	cmp	r2, r3
	beq	.L220
	mvn	r3, #19
	sub	r2, fp, #24
	add	r1, r2, r3
	ldr	r2, .L228+44
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #3
	add	r3, r3, r2
	ldmia	r1, {r0-r1}
	ldmia	r3, {r2-r3}
	bl	__subdf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L228+16
	ldmia	r2, {r2-r3}
	bl	__gtdf2
	mov	r3, r0
	cmp	r3, #0
	bgt	.L221
	b	.L220
.L221:
	ldr	r3, [fp, #-56]
	cmp	r3, #0
	movne	r2, #0
	moveq	r2, #1
	ldr	r3, [fp, #-28]
	mov	r1, r3, asl #1
	ldr	r3, [fp, #-36]
	add	r3, r1, r3
	add	r2, r2, r3
	mov	r0, r2
	ldr	r1, .L228+48
	bl	_Z13gps_logprintfiPcz
	mov	r3, r0
	cmp	r3, #0
	beq	.L220
	ldr	r1, .L228+40
	ldr	r2, [fp, #-28]
	ldr	r3, [fp, #-56]
	str	r3, [r1, r2, asl #2]
.L220:
	ldr	r0, [fp, #-56]
	bl	__floatsidf
	mov	r2, r1
	mov	r1, r0
	ldr	r0, .L228+52
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #3
	add	r3, r3, r0
	mov	r0, r1
	mov	r1, r2
	ldmia	r3, {r2-r3}
	bl	__nedf2
	mov	r3, r0
	cmp	r3, #0
	bne	.L224
	b	.L218
.L224:
	ldr	r2, .L228+44
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #3
	add	r2, r3, r2
	mvn	r3, #19
	sub	r1, fp, #24
	add	r3, r1, r3
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r2, .L228+52
	ldr	r3, [fp, #-28]
	mov	r3, r3, asl #3
	add	r5, r3, r2
	ldr	r0, [fp, #-56]
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	stmia	r5, {r3-r4}
.L218:
	ldr	r3, [fp, #-28]
	add	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L216
.L229:
	.align	2
.L228:
	.word	1106247680
	.word	0
	.word	1093567616
	.word	0
	.word	1074003968
	.word	0
	.word	validgpsdata
	.word	glog_poweroff
	.word	.LC17
	.word	sensor_invert
	.word	sensor_value
	.word	sensorbouncetime
	.word	.LC16
	.word	sensorbouncevalue
	.word	gforce_log_enable
	.word	.LC18
	.word	p_dio_mmap
.L217:
	ldr	r3, .L228+56
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L225
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #136]
	cmp	r3, #0
	beq	.L226
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #144]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r6, r1
	mov	r5, r0
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #140]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	sp, {r3-r4}
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #148]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #8
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	mov	r0, #16
	ldr	r1, .L228+60
	mov	r3, r6
	mov	r2, r5
	bl	_Z13gps_logprintfiPcz
	ldr	r3, .L228+64
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #136]
.L226:
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #152]
	cmp	r3, #0
	beq	.L225
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #160]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r6, r1
	mov	r5, r0
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #156]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	sp, {r3-r4}
	ldr	r3, .L228+64
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #164]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #8
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	mov	r0, #16
	ldr	r1, .L228+60
	mov	r3, r6
	mov	r2, r5
	bl	_Z13gps_logprintfiPcz
	ldr	r3, .L228+64
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #152]
.L225:
	mov	r2, #1
	str	r2, [fp, #-60]
.L210:
	ldr	r0, [fp, #-60]
	sub	sp, fp, #24
	ldmfd	sp, {r4, r5, r6, fp, sp, pc}
	.size	_Z10sensor_logv, .-_Z10sensor_logv
	.align	2
	.type	_Z16sensorlog_threadPv, %function
_Z16sensorlog_threadPv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
.L231:
	ldr	r3, .L234
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L232
	ldr	r3, .L234
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L233
	bl	_Z10sensor_logv
.L233:
	mov	r0, #19968
	add	r0, r0, #32
	bl	usleep
	b	.L231
.L232:
	mov	r3, #0
	mov	r0, r3
	ldmib	sp, {fp, sp, pc}
.L235:
	.align	2
.L234:
	.word	app_state
	.size	_Z16sensorlog_threadPv, .-_Z16sensorlog_threadPv
	.align	2
	.global	_Z16tab102b_checksunPh
	.type	_Z16tab102b_checksunPh, %function
_Z16tab102b_checksunPh:
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
	bls	.L238
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #40
	bhi	.L238
	b	.L237
.L238:
	mvn	r3, #0
	str	r3, [fp, #-28]
	b	.L236
.L237:
	mov	r3, #0
	strb	r3, [fp, #-17]
	mov	r3, #0
	str	r3, [fp, #-24]
.L239:
	ldr	r3, [fp, #-16]
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	ble	.L240
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
	b	.L239
.L240:
	ldrb	r3, [fp, #-17]	@ zero_extendqisi2
	str	r3, [fp, #-28]
.L236:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z16tab102b_checksunPh, .-_Z16tab102b_checksunPh
	.align	2
	.global	_Z19tab102b_calchecksunPh
	.type	_Z19tab102b_calchecksunPh, %function
_Z19tab102b_calchecksunPh:
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
.L243:
	ldr	r3, [fp, #-24]
	sub	r3, r3, #1
	str	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L244
	sub	r0, fp, #16
	ldr	r2, [r0, #0]
	mov	r3, r2
	ldrb	r1, [fp, #-17]
	ldrb	r3, [r3, #0]
	rsb	r3, r3, r1
	add	r2, r2, #1
	str	r2, [r0, #0]
	strb	r3, [fp, #-17]
	b	.L243
.L244:
	ldr	r2, [fp, #-16]
	ldrb	r3, [fp, #-17]
	strb	r3, [r2, #0]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z19tab102b_calchecksunPh, .-_Z19tab102b_calchecksunPh
	.align	2
	.global	_Z16tab102b_senddataPh
	.type	_Z16tab102b_senddataPh, %function
_Z16tab102b_senddataPh:
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
	bls	.L247
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #50
	bhi	.L247
	b	.L246
.L247:
	mov	r3, #0
	str	r3, [fp, #-20]
	b	.L245
.L246:
	ldr	r0, [fp, #-16]
	bl	_Z19tab102b_calchecksunPh
	ldr	r2, .L248
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	ldr	r0, [r2, #0]
	ldr	r1, [fp, #-16]
	mov	r2, r3
	bl	_Z12serial_writeiPvj
	mov	r3, r0
	str	r3, [fp, #-20]
.L245:
	ldr	r0, [fp, #-20]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L249:
	.align	2
.L248:
	.word	tab102b_port_handle
	.size	_Z16tab102b_senddataPh, .-_Z16tab102b_senddataPh
	.align	2
	.global	_Z15tab102b_recvmsgPhi
	.type	_Z15tab102b_recvmsgPhi, %function
_Z15tab102b_recvmsgPhi:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r3, .L254
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-16]
	mov	r2, #1
	bl	_Z11serial_readiPvj
	mov	r3, r0
	cmp	r3, #0
	beq	.L251
	ldr	r3, [fp, #-16]
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #4
	ble	.L251
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-20]
	cmp	r2, r3
	bgt	.L251
	ldr	r2, .L254
	ldr	r3, [fp, #-16]
	add	r1, r3, #1
	ldr	r3, [fp, #-24]
	sub	r3, r3, #1
	ldr	r0, [r2, #0]
	mov	r2, r3
	bl	_Z11serial_readiPvj
	mov	r3, r0
	add	r3, r3, #1
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-16]
	ldrb	r2, [r3, #0]	@ zero_extendqisi2
	ldr	r3, [fp, #-24]
	cmp	r2, r3
	bne	.L251
	mov	r2, #1
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	bne	.L251
	mov	r2, #2
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #4
	bne	.L251
	ldr	r0, [fp, #-16]
	bl	_Z16tab102b_checksunPh
	mov	r3, r0
	cmp	r3, #0
	bne	.L251
	ldr	r3, [fp, #-24]
	str	r3, [fp, #-28]
	b	.L250
.L251:
	ldr	r3, .L254
	ldr	r0, [r3, #0]
	bl	_Z12serial_cleari
	mov	r3, #0
	str	r3, [fp, #-28]
.L250:
	ldr	r0, [fp, #-28]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L255:
	.align	2
.L254:
	.word	tab102b_port_handle
	.size	_Z15tab102b_recvmsgPhi, .-_Z15tab102b_recvmsgPhi
	.global	tab102b_x_0g
	.bss
	.align	2
	.type	tab102b_x_0g, %object
	.size	tab102b_x_0g, 4
tab102b_x_0g:
	.space	4
	.global	tab102b_y_0g
	.align	2
	.type	tab102b_y_0g, %object
	.size	tab102b_y_0g, 4
tab102b_y_0g:
	.space	4
	.global	tab102b_z_0g
	.align	2
	.type	tab102b_z_0g, %object
	.size	tab102b_z_0g, 4
tab102b_z_0g:
	.space	4
	.global	tab102b_st
	.align	2
	.type	tab102b_st, %object
	.size	tab102b_st, 4
tab102b_st:
	.space	4
	.text
	.align	2
	.global	_Z11tab102b_logiii
	.type	_Z11tab102b_logiii, %function
_Z11tab102b_logiii:
	@ args = 0, pretend = 0, frame = 24
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #40
	str	r0, [fp, #-28]
	str	r1, [fp, #-32]
	str	r2, [fp, #-36]
	ldr	r3, .L259+8
	ldr	r2, [fp, #-28]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	mov	r0, r3
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L259
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [fp, #-40]	@ float
	ldr	r3, .L259+12
	ldr	r2, [fp, #-32]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	mov	r0, r3
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L259
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [fp, #-44]	@ float
	ldr	r3, .L259+16
	ldr	r2, [fp, #-36]
	ldr	r3, [r3, #0]
	rsb	r3, r3, r2
	mov	r0, r3
	bl	__floatsidf
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L259
	ldmia	r2, {r2-r3}
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	str	r3, [fp, #-48]	@ float
	ldr	r3, .L259+20
	ldr	r2, [r3, #4]
	mov	r3, #1888
	add	r3, r3, #11
	cmp	r2, r3
	bgt	.L257
	b	.L256
.L257:
	ldr	r3, .L259+24
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L256
	ldr	r3, [fp, #-40]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r6, r1
	mov	r5, r0
	ldr	r3, [fp, #-44]
	eor	r3, r3, #-2147483648
	mov	r0, r3
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	sp, {r3-r4}
	ldr	r0, [fp, #-48]	@ float
	bl	__extendsfdf2
	mov	r2, r1
	mov	r1, r0
	mov	r3, #8
	add	r3, sp, r3
	stmia	r3, {r1-r2}
	mov	r0, #16
	ldr	r1, .L259+28
	mov	r3, r6
	mov	r2, r5
	bl	_Z13gps_logprintfiPcz
.L256:
	sub	sp, fp, #24
	ldmfd	sp, {r4, r5, r6, fp, sp, pc}
.L260:
	.align	2
.L259:
	.word	1067155456
	.word	0
	.word	tab102b_x_0g
	.word	tab102b_y_0g
	.word	tab102b_z_0g
	.word	validgpsdata
	.word	gforce_log_enable
	.word	.LC18
	.size	_Z11tab102b_logiii, .-_Z11tab102b_logiii
	.align	2
	.global	_Z18tab102b_checkinputPh
	.type	_Z18tab102b_checkinputPh, %function
_Z18tab102b_checkinputPh:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	mov	r2, #3
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #30
	beq	.L263
	b	.L261
.L263:
	mov	r2, #5
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r1, r3, asl #8
	mov	r2, #6
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	add	r3, r1, r3
	str	r3, [fp, #-20]
	mov	r2, #7
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r1, r3, asl #8
	mov	r2, #8
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	add	r3, r1, r3
	str	r3, [fp, #-24]
	mov	r2, #9
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	mov	r1, r3, asl #8
	mov	r2, #10
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	add	r3, r1, r3
	str	r3, [fp, #-28]
	ldr	r0, [fp, #-20]
	ldr	r1, [fp, #-24]
	ldr	r2, [fp, #-28]
	bl	_Z11tab102b_logiii
.L261:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
	.size	_Z18tab102b_checkinputPh, .-_Z18tab102b_checkinputPh
	.global	tab102b_error
	.bss
	.align	2
	.type	tab102b_error, %object
	.size	tab102b_error, 4
tab102b_error:
	.space	4
	.text
	.align	2
	.global	_Z16tab102b_responsePhii
	.type	_Z16tab102b_responsePhii, %function
_Z16tab102b_responsePhii:
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
	bgt	.L267
	mov	r3, #0
	str	r3, [fp, #-32]
	b	.L265
.L267:
	ldr	r3, .L276
	ldr	r0, [r3, #0]
	ldr	r1, [fp, #-24]
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L268
	ldr	r0, [fp, #-16]
	ldr	r1, [fp, #-20]
	bl	_Z15tab102b_recvmsgPhi
	mov	r3, r0
	str	r3, [fp, #-28]
	ldr	r3, [fp, #-28]
	cmp	r3, #0
	ble	.L268
	mov	r2, #4
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #2
	bne	.L270
	ldr	r2, .L276+4
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r0, [fp, #-16]
	bl	_Z18tab102b_checkinputPh
	b	.L267
.L270:
	mov	r2, #4
	ldr	r3, [fp, #-16]
	add	r3, r2, r3
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #3
	bne	.L268
	ldr	r3, .L276+4
	mov	r2, #0
	str	r2, [r3, #0]
	ldr	r3, [fp, #-28]
	str	r3, [fp, #-32]
	b	.L265
.L268:
	ldr	r2, .L276+4
	ldr	r3, .L276+4
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	cmp	r3, #5
	ble	.L275
	ldr	r0, .L276
	bl	_Z12serial_closePi
	mov	r0, #1
	bl	sleep
	ldr	r3, .L276+8
	ldr	r0, .L276
	ldr	r1, .L276+12
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
.L275:
	mov	r3, #0
	str	r3, [fp, #-32]
.L265:
	ldr	r0, [fp, #-32]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L277:
	.align	2
.L276:
	.word	tab102b_port_handle
	.word	tab102b_error
	.word	tab102b_port_baudrate
	.word	tab102b_port_dev
	.size	_Z16tab102b_responsePhii, .-_Z16tab102b_responsePhii
	.align	2
	.type	_Z5bcd_vh, %function
_Z5bcd_vh:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	mov	r3, r0
	strb	r3, [fp, #-13]
	ldrb	r2, [fp, #-13]	@ zero_extendqisi2
	ldr	r3, .L279
	umull	r1, r3, r2, r3
	mov	r3, r3, lsr #3
	and	r3, r3, #255
	mov	r1, r3, asl #4
	ldrb	r0, [fp, #-13]	@ zero_extendqisi2
	ldr	r3, .L279
	umull	r2, r3, r0, r3
	mov	r2, r3, lsr #3
	mov	r3, r2
	mov	r3, r3, asl #2
	add	r3, r3, r2
	mov	r3, r3, asl #1
	rsb	r3, r3, r0
	mov	r2, r1
	add	r3, r2, r3
	and	r3, r3, #255
	mov	r0, r3
	ldmib	sp, {fp, sp, pc}
.L280:
	.align	2
.L279:
	.word	-858993459
	.size	_Z5bcd_vh, .-_Z5bcd_vh
	.data
	.align	2
	.type	_ZZ20tab102b_cmd_sync_rtcvE12cmd_sync_rtc, %object
	.size	_ZZ20tab102b_cmd_sync_rtcvE12cmd_sync_rtc, 15
_ZZ20tab102b_cmd_sync_rtcvE12cmd_sync_rtc:
	.ascii	"\r\004\000\007\002\001\002\003\004\005\006\007\314\000"
	.space	1
	.text
	.align	2
	.global	_Z20tab102b_cmd_sync_rtcv
	.type	_Z20tab102b_cmd_sync_rtcv, %function
_Z20tab102b_cmd_sync_rtcv:
	@ args = 0, pretend = 0, frame = 96
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #96
	ldr	r3, .L287
	ldr	r0, .L287+4
	ldr	r1, .L287+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L282
	mov	r3, #0
	str	r3, [fp, #-112]
	b	.L281
.L282:
	mov	r0, #0
	bl	time
	mov	r3, r0
	str	r3, [fp, #-108]
	sub	r3, fp, #108
	sub	r2, fp, #104
	mov	r0, r3
	mov	r1, r2
	bl	localtime_r
	ldr	r4, .L287+12
	ldr	r3, [fp, #-104]
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #5]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-100]
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #6]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-96]
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #7]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-80]
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #8]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-92]
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #9]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-88]
	add	r3, r3, #1
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #10]
	ldr	r4, .L287+12
	ldr	r3, [fp, #-84]
	sub	r3, r3, #100
	and	r3, r3, #255
	mov	r0, r3
	bl	_Z5bcd_vh
	strb	r0, [r4, #11]
	mov	r3, #0
	str	r3, [fp, #-20]
.L283:
	ldr	r3, [fp, #-20]
	cmp	r3, #9
	bgt	.L284
	ldr	r0, .L287+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #60
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L285
	b	.L284
.L285:
	ldr	r3, [fp, #-20]
	add	r3, r3, #1
	str	r3, [fp, #-20]
	b	.L283
.L284:
	mov	r3, #1
	str	r3, [fp, #-112]
.L281:
	ldr	r0, [fp, #-112]
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L288:
	.align	2
.L287:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ20tab102b_cmd_sync_rtcvE12cmd_sync_rtc
	.size	_Z20tab102b_cmd_sync_rtcv, .-_Z20tab102b_cmd_sync_rtcv
	.data
	.align	2
	.type	_ZZ30tab102b_datauploadconfirmationiE13cmd_data_conf, %object
	.size	_ZZ30tab102b_datauploadconfirmationiE13cmd_data_conf, 15
_ZZ30tab102b_datauploadconfirmationiE13cmd_data_conf:
	.ascii	"\007\004\000\032\002\335\314\000"
	.space	7
	.text
	.align	2
	.global	_Z30tab102b_datauploadconfirmationi
	.type	_Z30tab102b_datauploadconfirmationi, %function
_Z30tab102b_datauploadconfirmationi:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #48
	str	r0, [fp, #-16]
	ldr	r3, .L292
	ldr	r0, .L292+4
	ldr	r1, .L292+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L290
	mov	r3, #0
	str	r3, [fp, #-60]
	b	.L289
.L290:
	ldr	r2, .L292+12
	ldr	r3, [fp, #-16]
	strb	r3, [r2, #5]
	ldr	r0, .L292+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #56
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L291
	mov	r3, #1
	str	r3, [fp, #-60]
	b	.L289
.L291:
	mov	r3, #0
	str	r3, [fp, #-60]
.L289:
	ldr	r0, [fp, #-60]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L293:
	.align	2
.L292:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ30tab102b_datauploadconfirmationiE13cmd_data_conf
	.size	_Z30tab102b_datauploadconfirmationi, .-_Z30tab102b_datauploadconfirmationi
	.section	.rodata
	.align	2
.LC19:
	.ascii	"%s/_%s_/%04d%02d%02d/%s_%04d%02d%02d%02d%02d%02d_TA"
	.ascii	"B102log_L.log\000"
	.align	2
.LC20:
	.ascii	"wb\000"
	.text
	.align	2
	.global	_Z15tab102b_logdataPhi
	.type	_Z15tab102b_logdataPhi, %function
_Z15tab102b_logdataPhi:
	@ args = 0, pretend = 0, frame = 708
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #748
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	mov	r3, #0
	str	r3, [fp, #-716]
	ldr	r0, .L298
	ldr	r3, .L298+4
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L298+8
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-720]
	ldr	r3, [fp, #-720]
	cmp	r3, #0
	beq	.L295
	sub	r3, fp, #712
	mov	r0, r3
	mov	r1, #1
	mov	r2, #255
	ldr	r3, [fp, #-720]
	bl	fread
	str	r0, [fp, #-24]
	ldr	r0, [fp, #-720]
	bl	fclose
	ldr	r3, [fp, #-24]
	cmp	r3, #2
	ble	.L295
	mvn	r3, #696
	sub	r3, r3, #3
	ldr	r2, [fp, #-24]
	sub	r1, fp, #12
	add	r2, r1, r2
	add	r2, r2, r3
	mov	r3, #0
	strb	r3, [r2, #0]
	mov	r0, #0
	bl	time
	mov	r3, r0
	str	r3, [fp, #-456]
	sub	r3, fp, #456
	sub	r2, fp, #452
	mov	r0, r3
	mov	r1, r2
	bl	localtime_r
	sub	r3, fp, #152
	mov	r0, r3
	mov	r1, #127
	bl	gethostname
	sub	r2, fp, #408
	sub	ip, fp, #712
	sub	lr, fp, #152
	ldr	r3, [fp, #-432]
	add	r3, r3, #1888
	add	r3, r3, #12
	str	r3, [sp, #0]
	ldr	r3, [fp, #-436]
	add	r3, r3, #1
	str	r3, [sp, #4]
	ldr	r3, [fp, #-440]
	str	r3, [sp, #8]
	sub	r3, fp, #152
	str	r3, [sp, #12]
	ldr	r3, [fp, #-432]
	add	r3, r3, #1888
	add	r3, r3, #12
	str	r3, [sp, #16]
	ldr	r3, [fp, #-436]
	add	r3, r3, #1
	str	r3, [sp, #20]
	ldr	r3, [fp, #-440]
	str	r3, [sp, #24]
	ldr	r3, [fp, #-444]
	str	r3, [sp, #28]
	ldr	r3, [fp, #-448]
	str	r3, [sp, #32]
	ldr	r3, [fp, #-452]
	str	r3, [sp, #36]
	mov	r0, r2
	ldr	r1, .L298+12
	mov	r2, ip
	mov	r3, lr
	bl	sprintf
	sub	r3, fp, #408
	mov	r0, r3
	ldr	r1, .L298+16
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-716]
.L295:
	ldr	r3, [fp, #-716]
	cmp	r3, #0
	beq	.L297
	ldr	r0, [fp, #-16]
	mov	r1, #1
	ldr	r2, [fp, #-20]
	ldr	r3, [fp, #-716]
	bl	fwrite
	ldr	r0, [fp, #-716]
	bl	fclose
.L297:
	mov	r3, #1
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L299:
	.align	2
.L298:
	.word	dvrcurdisk
	.word	_ZN6string9getstringEv
	.word	.LC7
	.word	.LC19
	.word	.LC20
	.size	_Z15tab102b_logdataPhi, .-_Z15tab102b_logdataPhi
	.data
	.align	2
	.type	_ZZ20tab102b_cmd_req_datavE12cmd_req_data, %object
	.size	_ZZ20tab102b_cmd_req_datavE12cmd_req_data, 8
_ZZ20tab102b_cmd_req_datavE12cmd_req_data:
	.ascii	"\006\004\000\031\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z20tab102b_cmd_req_datav
	.type	_Z20tab102b_cmd_req_datav, %function
_Z20tab102b_cmd_req_datav:
	@ args = 0, pretend = 0, frame = 68
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #68
	mov	r3, #0
	str	r3, [fp, #-16]
	ldr	r3, .L317
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L301
	mov	r3, #0
	str	r3, [fp, #-80]
	b	.L300
.L301:
	ldr	r3, .L317+4
	ldr	r0, .L317+8
	ldr	r1, .L317+12
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L302
	mov	r3, #0
	str	r3, [fp, #-80]
	b	.L300
.L302:
	ldr	r0, .L317+16
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #64
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L303
	ldrb	r3, [fp, #-61]	@ zero_extendqisi2
	cmp	r3, #25
	bne	.L303
	ldrb	r3, [fp, #-59]	@ zero_extendqisi2
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-58]	@ zero_extendqisi2
	add	r3, r2, r3
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-57]	@ zero_extendqisi2
	add	r3, r2, r3
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-56]	@ zero_extendqisi2
	add	r3, r2, r3
	str	r3, [fp, #-20]
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	ble	.L303
	ldr	r2, [fp, #-20]
	mvn	r3, #-16777216
	sub	r3, r3, #15728640
	cmp	r2, r3
	bgt	.L303
	ldr	r3, [fp, #-20]
	add	r3, r3, #18
	mov	r0, r3
	bl	malloc
	mov	r3, r0
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-24]
	cmp	r3, #0
	beq	.L303
	mov	r3, #0
	str	r3, [fp, #-68]
	ldr	r3, [fp, #-20]
	add	r3, r3, #18
	str	r3, [fp, #-72]
.L307:
	ldr	r3, .L317+8
	ldr	r0, [r3, #0]
	mov	r1, #9961472
	add	r1, r1, #38400
	add	r1, r1, #128
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L308
	ldr	r3, .L317+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L308
	ldr	r0, .L317+8
	ldr	r2, [fp, #-24]
	ldr	r3, [fp, #-68]
	add	r1, r2, r3
	ldr	r2, [fp, #-72]
	ldr	r3, [fp, #-68]
	rsb	r3, r3, r2
	ldr	r0, [r0, #0]
	mov	r2, r3
	bl	_Z11serial_readiPvj
	mov	r3, r0
	str	r3, [fp, #-76]
	ldr	r3, [fp, #-76]
	cmp	r3, #0
	ble	.L308
	ldr	r2, [fp, #-68]
	ldr	r3, [fp, #-76]
	add	r3, r2, r3
	str	r3, [fp, #-68]
	ldr	r2, [fp, #-68]
	ldr	r3, [fp, #-72]
	cmp	r2, r3
	blt	.L307
	mov	r3, #1
	str	r3, [fp, #-16]
.L308:
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bne	.L312
	ldr	r0, [fp, #-24]
	ldr	r1, [fp, #-72]
	bl	_Z15tab102b_logdataPhi
.L312:
	ldr	r0, [fp, #-24]
	bl	free
.L303:
	ldr	r3, [fp, #-16]
	cmp	r3, #0
	bne	.L313
.L314:
	ldr	r3, .L317+8
	ldr	r0, [r3, #0]
	mov	r1, #999424
	add	r1, r1, #576
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L313
	ldr	r3, .L317+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L313
	ldr	r3, .L317+8
	ldr	r0, [r3, #0]
	bl	_Z12serial_cleari
	b	.L314
.L313:
	ldr	r3, .L317+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L316
	ldr	r0, [fp, #-16]
	bl	_Z30tab102b_datauploadconfirmationi
.L316:
	ldr	r3, [fp, #-16]
	str	r3, [fp, #-80]
.L300:
	ldr	r0, [fp, #-80]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L318:
	.align	2
.L317:
	.word	tab102b_data_enable
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ20tab102b_cmd_req_datavE12cmd_req_data
	.word	app_state
	.size	_Z20tab102b_cmd_req_datav, .-_Z20tab102b_cmd_req_datav
	.data
	.align	2
	.type	_ZZ18tab102b_cmd_get_0gvE10cmd_get_0g, %object
	.size	_ZZ18tab102b_cmd_get_0gvE10cmd_get_0g, 8
_ZZ18tab102b_cmd_get_0gvE10cmd_get_0g:
	.ascii	"\006\004\000\030\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z18tab102b_cmd_get_0gv
	.type	_Z18tab102b_cmd_get_0gv, %function
_Z18tab102b_cmd_get_0gv:
	@ args = 0, pretend = 0, frame = 44
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #44
	ldr	r3, .L323
	ldr	r0, .L323+4
	ldr	r1, .L323+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L320
	mov	r3, #0
	str	r3, [fp, #-56]
	b	.L319
.L320:
	ldr	r0, .L323+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #52
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L321
	ldrb	r3, [fp, #-49]	@ zero_extendqisi2
	cmp	r3, #24
	bne	.L321
	ldr	r1, .L323+16
	ldrb	r3, [fp, #-47]	@ zero_extendqisi2
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-46]	@ zero_extendqisi2
	add	r3, r2, r3
	str	r3, [r1, #0]
	ldr	r1, .L323+20
	ldrb	r3, [fp, #-45]	@ zero_extendqisi2
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-44]	@ zero_extendqisi2
	add	r3, r2, r3
	str	r3, [r1, #0]
	ldr	r1, .L323+24
	ldrb	r3, [fp, #-43]	@ zero_extendqisi2
	mov	r2, r3, asl #8
	ldrb	r3, [fp, #-42]	@ zero_extendqisi2
	add	r3, r2, r3
	str	r3, [r1, #0]
	ldr	r2, .L323+28
	ldrb	r3, [fp, #-41]	@ zero_extendqisi2
	str	r3, [r2, #0]
.L321:
	mov	r3, #1
	str	r3, [fp, #-56]
.L319:
	ldr	r0, [fp, #-56]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L324:
	.align	2
.L323:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ18tab102b_cmd_get_0gvE10cmd_get_0g
	.word	tab102b_x_0g
	.word	tab102b_y_0g
	.word	tab102b_z_0g
	.word	tab102b_st
	.size	_Z18tab102b_cmd_get_0gv, .-_Z18tab102b_cmd_get_0gv
	.data
	.align	2
	.type	_ZZ17tab102b_cmd_readyvE9cmd_ready, %object
	.size	_ZZ17tab102b_cmd_readyvE9cmd_ready, 8
_ZZ17tab102b_cmd_readyvE9cmd_ready:
	.ascii	"\006\004\000\b\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z17tab102b_cmd_readyv
	.type	_Z17tab102b_cmd_readyv, %function
_Z17tab102b_cmd_readyv:
	@ args = 0, pretend = 0, frame = 48
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #48
	ldr	r3, .L331
	ldr	r0, .L331+4
	ldr	r1, .L331+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L326
	mov	r3, #0
	str	r3, [fp, #-60]
	b	.L325
.L326:
	mov	r3, #0
	str	r3, [fp, #-16]
.L327:
	ldr	r3, [fp, #-16]
	cmp	r3, #9
	bgt	.L328
	ldr	r0, .L331+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #56
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L329
	mov	r3, #1
	str	r3, [fp, #-60]
	b	.L325
.L329:
	ldr	r3, [fp, #-16]
	add	r3, r3, #1
	str	r3, [fp, #-16]
	b	.L327
.L328:
	mov	r3, #0
	str	r3, [fp, #-60]
.L325:
	ldr	r0, [fp, #-60]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L332:
	.align	2
.L331:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ17tab102b_cmd_readyvE9cmd_ready
	.size	_Z17tab102b_cmd_readyv, .-_Z17tab102b_cmd_readyv
	.data
	.align	2
	.type	_ZZ22tab102b_cmd_enablepeakvE14cmd_enablepeak, %object
	.size	_ZZ22tab102b_cmd_enablepeakvE14cmd_enablepeak, 8
_ZZ22tab102b_cmd_enablepeakvE14cmd_enablepeak:
	.ascii	"\006\004\000\017\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z22tab102b_cmd_enablepeakv
	.type	_Z22tab102b_cmd_enablepeakv, %function
_Z22tab102b_cmd_enablepeakv:
	@ args = 0, pretend = 0, frame = 44
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #44
	ldr	r3, .L336
	ldr	r0, .L336+4
	ldr	r1, .L336+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L334
	mov	r3, #0
	str	r3, [fp, #-56]
	b	.L333
.L334:
	ldr	r0, .L336+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #52
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L335
	mov	r3, #1
	str	r3, [fp, #-56]
	b	.L333
.L335:
	mov	r3, #0
	str	r3, [fp, #-56]
.L333:
	ldr	r0, [fp, #-56]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L337:
	.align	2
.L336:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ22tab102b_cmd_enablepeakvE14cmd_enablepeak
	.size	_Z22tab102b_cmd_enablepeakv, .-_Z22tab102b_cmd_enablepeakv
	.data
	.align	2
	.type	_ZZ23tab102b_cmd_disablepeakvE15cmd_disablepeak, %object
	.size	_ZZ23tab102b_cmd_disablepeakvE15cmd_disablepeak, 8
_ZZ23tab102b_cmd_disablepeakvE15cmd_disablepeak:
	.ascii	"\006\004\000\020\002\314\000"
	.space	1
	.text
	.align	2
	.global	_Z23tab102b_cmd_disablepeakv
	.type	_Z23tab102b_cmd_disablepeakv, %function
_Z23tab102b_cmd_disablepeakv:
	@ args = 0, pretend = 0, frame = 44
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #44
	ldr	r3, .L341
	ldr	r0, .L341+4
	ldr	r1, .L341+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L339
	mov	r3, #0
	str	r3, [fp, #-56]
	b	.L338
.L339:
	ldr	r0, .L341+12
	bl	_Z16tab102b_senddataPh
	sub	r3, fp, #52
	mov	r0, r3
	mov	r1, #40
	mov	r2, #499712
	add	r2, r2, #288
	bl	_Z16tab102b_responsePhii
	mov	r3, r0
	cmp	r3, #0
	ble	.L340
	mov	r3, #1
	str	r3, [fp, #-56]
	b	.L338
.L340:
	mov	r3, #0
	str	r3, [fp, #-56]
.L338:
	ldr	r0, [fp, #-56]
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L342:
	.align	2
.L341:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	_ZZ23tab102b_cmd_disablepeakvE15cmd_disablepeak
	.size	_Z23tab102b_cmd_disablepeakv, .-_Z23tab102b_cmd_disablepeakv
	.align	2
	.global	_Z12tab102b_initv
	.type	_Z12tab102b_initv, %function
_Z12tab102b_initv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r3, .L346
	ldr	r0, .L346+4
	ldr	r1, .L346+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L344
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L343
.L344:
	bl	_Z20tab102b_cmd_sync_rtcv
	bl	_Z20tab102b_cmd_req_datav
	bl	_Z18tab102b_cmd_get_0gv
	bl	_Z17tab102b_cmd_readyv
	ldr	r3, .L346+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L345
	bl	_Z22tab102b_cmd_enablepeakv
.L345:
	mov	r3, #1
	str	r3, [fp, #-16]
.L343:
	ldr	r0, [fp, #-16]
	ldmib	sp, {fp, sp, pc}
.L347:
	.align	2
.L346:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.word	gforce_log_enable
	.size	_Z12tab102b_initv, .-_Z12tab102b_initv
	.align	2
	.global	_Z14tab102b_finishv
	.type	_Z14tab102b_finishv, %function
_Z14tab102b_finishv:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	ldr	r3, .L350
	ldr	r0, .L350+4
	ldr	r1, .L350+8
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	mov	r3, r0
	cmp	r3, #0
	bge	.L349
	mov	r3, #0
	str	r3, [fp, #-16]
	b	.L348
.L349:
	bl	_Z23tab102b_cmd_disablepeakv
	ldr	r0, .L350+4
	bl	_Z12serial_closePi
	mov	r3, #1
	str	r3, [fp, #-16]
.L348:
	ldr	r0, [fp, #-16]
	ldmib	sp, {fp, sp, pc}
.L351:
	.align	2
.L350:
	.word	tab102b_port_baudrate
	.word	tab102b_port_handle
	.word	tab102b_port_dev
	.size	_Z14tab102b_finishv, .-_Z14tab102b_finishv
	.align	2
	.type	_Z14tab102b_threadPv, %function
_Z14tab102b_threadPv:
	@ args = 0, pretend = 0, frame = 60
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #60
	str	r0, [fp, #-16]
	mov	r3, #0
	str	r3, [fp, #-20]
.L353:
	ldr	r3, .L367
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L354
	ldr	r3, .L367
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L355
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	bne	.L356
	ldr	r3, .L367+4
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L357
	bl	_Z12tab102b_initv
	mov	r3, #1
	str	r3, [fp, #-20]
.L357:
	mov	r0, #99328
	add	r0, r0, #672
	bl	usleep
	b	.L353
.L356:
	ldr	r3, .L367+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bge	.L359
	ldr	r3, .L367+12
	ldr	r0, .L367+8
	ldr	r1, .L367+16
	ldr	r2, [r3, #0]
	bl	_Z11serial_openPiPci
	b	.L353
.L359:
	ldr	r3, .L367+8
	ldr	r0, [r3, #0]
	mov	r1, #1998848
	add	r1, r1, #1152
	bl	_Z16serial_datareadyii
	mov	r3, r0
	cmp	r3, #0
	beq	.L353
	sub	r3, fp, #72
	mov	r0, r3
	mov	r1, #50
	mov	r2, #999424
	add	r2, r2, #576
	bl	_Z16tab102b_responsePhii
	b	.L353
.L355:
	ldr	r3, .L367
	ldr	r3, [r3, #0]
	cmp	r3, #2
	bne	.L363
	ldr	r3, .L367+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	ble	.L363
	ldr	r3, .L367+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L364
	bl	_Z23tab102b_cmd_disablepeakv
.L364:
	bl	_Z20tab102b_cmd_req_datav
	ldr	r3, .L367+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L363
	bl	_Z22tab102b_cmd_enablepeakv
.L363:
	ldr	r0, .L367+8
	bl	_Z12serial_closePi
	mov	r0, #9984
	add	r0, r0, #16
	bl	usleep
	b	.L353
.L354:
	ldr	r3, [fp, #-20]
	cmp	r3, #0
	beq	.L366
	bl	_Z14tab102b_finishv
.L366:
	mov	r3, #0
	mov	r0, r3
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L368:
	.align	2
.L367:
	.word	app_state
	.word	glog_ok
	.word	tab102b_port_handle
	.word	tab102b_port_baudrate
	.word	tab102b_port_dev
	.word	gforce_log_enable
	.size	_Z14tab102b_threadPv, .-_Z14tab102b_threadPv
	.global	sensorthreadid
	.bss
	.align	2
	.type	sensorthreadid, %object
	.size	sensorthreadid, 4
sensorthreadid:
	.space	4
	.global	tab102b_threadid
	.align	2
	.type	tab102b_threadid, %object
	.size	tab102b_threadid, 4
tab102b_threadid:
	.space	4
	.section	.rodata
	.align	2
.LC22:
	.ascii	"iomapfile\000"
	.align	2
.LC23:
	.ascii	"GLOG: IO module not started!\000"
	.align	2
.LC24:
	.ascii	"GLOG:IO memory map failed!\000"
	.align	2
.LC26:
	.ascii	"gpsdisable\000"
	.align	2
.LC27:
	.ascii	"serialport\000"
	.align	2
.LC28:
	.ascii	"serialbaudrate\000"
	.align	2
.LC29:
	.ascii	"tab102b_port\000"
	.align	2
.LC30:
	.ascii	"tab102b_baudrate\000"
	.align	2
.LC21:
	.ascii	"system\000"
	.align	2
.LC31:
	.ascii	"timezone\000"
	.align	2
.LC32:
	.ascii	"timezones\000"
	.align	2
.LC33:
	.ascii	"TZ\000"
	.align	2
.LC34:
	.ascii	"degree1km\000"
	.align	2
.LC25:
	.ascii	"glog\000"
	.align	2
.LC36:
	.ascii	"dist_max\000"
	.align	2
.LC35:
	.ascii	"%f\000"
	.align	2
.LC37:
	.ascii	"speed%d\000"
	.align	2
.LC38:
	.ascii	"dist%d\000"
	.align	2
.LC39:
	.ascii	"inputdebounce\000"
	.align	2
.LC41:
	.ascii	"gforce_log_enable\000"
	.align	2
.LC42:
	.ascii	"tab102b_enable\000"
	.align	2
.LC43:
	.ascii	"tab102b_data_enable\000"
	.align	2
.LC44:
	.ascii	"currentdisk\000"
	.align	2
.LC45:
	.ascii	"sensor%d\000"
	.align	2
.LC46:
	.ascii	"name\000"
	.align	2
.LC47:
	.ascii	"inverted\000"
	.align	2
.LC48:
	.ascii	"w\000"
	.align	2
.LC40:
	.ascii	"%d\000"
	.align	2
.LC49:
	.ascii	"GLOG: started!\n\000"
	.text
	.align	2
	.global	_Z7appinitv
	.type	_Z7appinitv, %function
_Z7appinitv:
	@ args = 0, pretend = 0, frame = 384
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #392
	ldr	r3, .L433
	str	r3, [fp, #-328]
	ldr	r3, .L433+4
	str	r3, [fp, #-324]
	sub	r3, fp, #320
	sub	r2, fp, #40
	str	r2, [r3, #0]
	ldr	r2, .L433+8
	str	r2, [r3, #4]
	str	sp, [r3, #8]
	sub	r3, fp, #352
	mov	r0, r3
	bl	_Unwind_SjLj_Register
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+80
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L433+80
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+80
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #92
	mov	r3, #3
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+12
	bl	_ZN6configC1EPc
	sub	r2, fp, #92
	mov	r3, #2
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+84
	ldr	r2, .L433+16
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #64
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	ldr	r2, .L433+36
	mov	r3, #0
	str	r3, [r2, #0]
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	bne	.L370
	sub	r3, fp, #92
	mov	r2, #3
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+20
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	mov	r3, #0
	str	r3, [fp, #-356]
	b	.L369
.L370:
	mov	r3, #0
	str	r3, [fp, #-48]
.L371:
	ldr	r3, [fp, #-48]
	cmp	r3, #9
	bgt	.L372
	sub	r3, fp, #64
	mov	r2, #2
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	mov	r1, #2
	bl	open
	mov	r3, r0
	str	r3, [fp, #-44]
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	ble	.L374
	b	.L372
.L374:
	mov	r0, #1
	bl	sleep
	ldr	r3, [fp, #-48]
	add	r3, r3, #1
	str	r3, [fp, #-48]
	b	.L371
.L372:
	ldr	r3, [fp, #-44]
	cmp	r3, #0
	bgt	.L375
	mov	r3, #2
	str	r3, [fp, #-348]
	ldr	r0, .L433+28
	bl	printf
	sub	r3, fp, #92
	mov	r2, #3
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+20
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	mov	r2, #0
	str	r2, [fp, #-356]
	b	.L369
.L375:
	ldr	r3, [fp, #-44]
	str	r3, [sp, #0]
	mov	r3, #0
	str	r3, [sp, #4]
	mov	r0, #0
	mov	r1, #172
	mov	r2, #3
	mov	r3, #1
	bl	mmap
	str	r0, [fp, #-52]
	ldr	r0, [fp, #-44]
	bl	close
	ldr	r3, [fp, #-52]
	cmn	r3, #1
	beq	.L377
	ldr	r3, [fp, #-52]
	cmp	r3, #0
	bne	.L376
.L377:
	mov	r3, #2
	str	r3, [fp, #-348]
	ldr	r0, .L433+32
	bl	printf
	sub	r3, fp, #92
	mov	r2, #3
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+20
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+24
	mov	lr, pc
	mov	pc, r3
	mov	r3, #0
	str	r3, [fp, #-356]
	b	.L369
.L376:
	ldr	r2, .L433+36
	ldr	r3, [fp, #-52]
	str	r3, [r2, #0]
	ldr	r3, .L433+36
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	ble	.L378
	ldr	r3, .L433+36
	ldr	r3, [r3, #0]
	ldr	r0, [r3, #16]
	mov	r1, #15
	bl	kill
	mov	r3, #0
	str	r3, [fp, #-48]
.L379:
	ldr	r3, [fp, #-48]
	cmp	r3, #9
	bgt	.L378
	ldr	r3, .L433+36
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #16]
	cmp	r3, #0
	bgt	.L381
	b	.L378
.L381:
	ldr	r3, [fp, #-48]
	add	r3, r3, #1
	str	r3, [fp, #-48]
	b	.L379
.L434:
	.align	2
.L433:
	.word	__gxx_personality_sj0
	.word	.LLSDA121
	.word	.L432
	.word	dvrconfigfile
	.word	.LC22
	.word	_ZN6configD1Ev
	.word	_ZN6stringD1Ev
	.word	.LC23
	.word	.LC24
	.word	p_dio_mmap
	.word	.LC26
	.word	gps_port_disable
	.word	.LC27
	.word	gps_port_dev
	.word	.LC28
	.word	gps_port_baudrate
	.word	.LC29
	.word	tab102b_port_dev
	.word	.LC30
	.word	tab102b_port_baudrate
	.word	_ZN6stringC1Ev
	.word	.LC21
	.word	.LC31
	.word	.LC32
	.word	.LC33
	.word	.LC34
	.word	.LC25
	.word	.LC36
	.word	_ZN6stringaSERS_
	.word	_ZN6string6lengthEv
	.word	_ZN6string9getstringEv
	.word	.LC35
	.word	1140457472
	.word	max_distance
	.word	degree1km
	.word	0
	.word	0
.L378:
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	ldr	r3, .L433+36
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	add	r3, r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L433+36
	ldr	r3, [r3, #0]
	str	r3, [fp, #-360]
	bl	getpid
	ldr	r2, [fp, #-360]
	str	r0, [r2, #16]
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #76]
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #80
	add	r2, r2, r3
	adr	r3, .L433+140
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #88
	add	r2, r2, r3
	adr	r3, .L433+140
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #96
	add	r2, r2, r3
	adr	r3, .L433+140
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #104
	add	r2, r2, r3
	adr	r3, .L433+140
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L433+36
	ldr	r2, [r3, #0]
	mov	r3, #112
	add	r2, r2, r3
	adr	r3, .L433+140
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	sub	r2, fp, #92
	mov	r3, #2
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+40
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L433+44
	str	r3, [r2, #0]
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r1, .L433+104
	ldr	r2, .L433+48
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #60
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L383
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L433+52
	mov	r1, r3
	bl	strcpy
.L383:
	sub	r2, fp, #92
	mov	r3, #2
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+56
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-48]
	ldr	r2, [fp, #-48]
	mov	r3, #1184
	add	r3, r3, #15
	cmp	r2, r3
	ble	.L384
	ldr	r2, [fp, #-48]
	mov	r3, #114688
	add	r3, r3, #512
	cmp	r2, r3
	bgt	.L384
	ldr	r2, .L433+60
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
.L384:
	sub	r2, fp, #92
	mov	r3, #2
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+64
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #60
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L385
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L433+68
	mov	r1, r3
	bl	strcpy
.L385:
	sub	r2, fp, #92
	mov	r3, #2
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+72
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	str	r3, [fp, #-48]
	ldr	r2, [fp, #-48]
	mov	r3, #1184
	add	r3, r3, #15
	cmp	r2, r3
	ble	.L386
	ldr	r2, [fp, #-48]
	mov	r3, #114688
	add	r3, r3, #512
	cmp	r2, r3
	bgt	.L386
	ldr	r2, .L433+76
	ldr	r3, [fp, #-48]
	str	r3, [r2, #0]
.L386:
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r3, .L433+80
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #100
	mov	r0, r3
	ldr	r3, .L433+80
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #92
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+84
	ldr	r2, .L433+88
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #96
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L387
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r2, r0
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r1, .L433+92
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #100
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #100
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L388
	sub	r3, fp, #100
	mov	r0, r3
	ldr	r3, .L433+120
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
	beq	.L389
	ldr	r3, [fp, #-52]
	mov	r2, #0
	strb	r2, [r3, #0]
.L389:
	sub	r3, fp, #100
	mov	r2, #1
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+120
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
	beq	.L390
	ldr	r3, [fp, #-52]
	mov	r2, #0
	strb	r2, [r3, #0]
.L390:
	sub	r3, fp, #100
	mov	r2, #1
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L433+96
	mov	r1, r3
	mov	r2, #1
	bl	setenv
	b	.L387
.L388:
	sub	r3, fp, #96
	mov	r2, #1
	str	r2, [fp, #-348]
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	ldr	r0, .L433+96
	mov	r1, r3
	mov	r2, #1
	bl	setenv
.L387:
	sub	r2, fp, #92
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+100
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #68
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L392
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L433+124
	ldr	r2, .L433+136
	bl	sscanf
.L392:
	sub	r2, fp, #92
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L433+104
	ldr	r2, .L433+108
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #68
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L433+112
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+116
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L393
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L433+120
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L433+124
	ldr	r2, .L433+132
	bl	sscanf
	b	.L394
.L393:
	ldr	r2, .L433+132
	ldr	r3, .L433+128	@ float
	str	r3, [r2, #0]	@ float
.L394:
	ldr	r3, .L433+132
	str	r3, [fp, #-364]
	ldr	r3, .L433+132
	ldr	r0, [r3, #0]	@ float
	bl	__extendsfdf2
	sub	r2, fp, #368
	stmda	r2, {r0-r1}
	ldr	r3, .L433+136
	ldr	r0, [r3, #0]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L435
	ldmia	r2, {r2-r3}
	bl	__divdf3
	mov	r4, r1
	mov	r3, r0
	sub	r2, fp, #368
	ldmda	r2, {r0-r1}
	mov	r2, r3
	mov	r3, r4
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	ldr	r2, [fp, #-364]
	str	r3, [r2, #0]	@ float
	mov	r3, #0
	str	r3, [fp, #-48]
.L395:
	ldr	r3, [fp, #-48]
	cmp	r3, #9
	bgt	.L396
	sub	r3, fp, #300
	mov	r0, r3
	ldr	r1, .L435+8
	ldr	r2, [fp, #-48]
	bl	sprintf
	sub	r2, fp, #92
	sub	ip, fp, #300
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L435+68
	mov	r2, ip
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #68
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L435+108
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+44
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L398
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+48
	mov	lr, pc
	mov	pc, r3
	mov	r1, r0
	ldr	r3, [fp, #-48]
	mov	r2, r3, asl #3
	ldr	r3, .L435+12
	add	r3, r2, r3
	mov	r0, r1
	ldr	r1, .L435+16
	mov	r2, r3
	bl	sscanf
	sub	r3, fp, #300
	mov	r0, r3
	ldr	r1, .L435+20
	ldr	r2, [fp, #-48]
	bl	sprintf
	sub	r3, fp, #92
	sub	r2, fp, #300
	mov	r0, r3
	ldr	r1, .L435+68
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #68
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L435+108
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+44
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L399
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+48
	mov	lr, pc
	mov	pc, r3
	mov	r1, r0
	ldr	r3, [fp, #-48]
	mov	r2, r3, asl #3
	ldr	r3, .L435+24
	add	r3, r2, r3
	mov	r0, r1
	ldr	r1, .L435+16
	mov	r2, r3
	bl	sscanf
	b	.L400
.L399:
	ldr	r2, .L435+12
	ldr	r3, [fp, #-48]
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r2, r3, r1
	ldr	r3, .L435+28	@ float
	str	r3, [r2, #0]	@ float
.L400:
	ldr	r2, .L435+12
	ldr	r3, [fp, #-48]
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	str	r3, [fp, #-376]
	ldr	r2, .L435+12
	ldr	r3, [fp, #-48]
	mov	r1, #4
	mov	r3, r3, asl #3
	add	r3, r3, r2
	add	r3, r3, r1
	ldr	r0, [r3, #0]	@ float
	bl	__extendsfdf2
	sub	r3, fp, #384
	stmia	r3, {r0-r1}
	ldr	r3, .L435+32
	ldr	r0, [r3, #0]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	adr	r2, .L435
	ldmia	r2, {r2-r3}
	bl	__divdf3
	mov	r4, r1
	mov	r3, r0
	sub	r2, fp, #384
	ldmia	r2, {r0-r1}
	mov	r2, r3
	mov	r3, r4
	bl	__muldf3
	mov	r4, r1
	mov	r3, r0
	mov	r1, r4
	mov	r0, r3
	bl	__truncdfsf2
	mov	r3, r0
	ldr	r2, [fp, #-376]
	str	r3, [r2, #0]	@ float
	b	.L397
.L398:
	ldr	r1, .L435+12
	ldr	r2, [fp, #-48]
	ldr	r3, .L435+36	@ float
	str	r3, [r1, r2, asl #3]	@ float
	b	.L396
.L397:
	ldr	r3, [fp, #-48]
	add	r3, r3, #1
	str	r3, [fp, #-48]
	b	.L395
.L436:
	.align	2
.L435:
	.word	1083129856
	.word	0
	.word	.LC37
	.word	speed_table
	.word	.LC35
	.word	.LC38
	.word	speed_table+4
	.word	1120403456
	.word	degree1km
	.word	0
	.word	.LC39
	.word	_ZN6string6lengthEv
	.word	_ZN6string9getstringEv
	.word	inputdebounce
	.word	.LC41
	.word	gforce_log_enable
	.word	.LC42
	.word	.LC25
	.word	.LC43
	.word	tab102b_data_enable
	.word	.LC21
	.word	.LC44
	.word	dvrcurdisk
	.word	p_dio_mmap
	.word	.LC45
	.word	.LC46
	.word	sensorname
	.word	_ZN6stringaSERS_
	.word	sensor_invert
	.word	.LC47
	.word	gpslogfilename
	.word	pidfile
	.word	.LC48
	.word	.LC40
	.word	sensorthreadid
	.word	_Z16sensorlog_threadPv
	.word	tab102b_enable
	.word	tab102b_threadid
	.word	_Z14tab102b_threadPv
	.word	.LC49
	.word	_ZN6configD1Ev
	.word	_ZN6stringD1Ev
.L396:
	sub	r2, fp, #92
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L435+68
	ldr	r2, .L435+40
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	sub	r2, fp, #68
	mov	r0, r2
	mov	r1, r3
	ldr	r3, .L435+108
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+44
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	cmp	r3, #0
	ble	.L402
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+48
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	mov	r0, r3
	ldr	r1, .L435+132
	ldr	r2, .L435+52
	bl	sscanf
.L402:
	sub	r2, fp, #92
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r1, .L435+68
	ldr	r2, .L435+56
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L435+60
	str	r3, [r2, #0]
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r1, .L435+68
	ldr	r2, .L435+64
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L435+144
	str	r3, [r2, #0]
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r1, .L435+68
	ldr	r2, .L435+72
	bl	_ZN6config11getvalueintEPcS0_
	mov	r3, r0
	ldr	r2, .L435+76
	str	r3, [r2, #0]
	sub	r3, fp, #92
	mov	r0, r3
	ldr	r1, .L435+80
	ldr	r2, .L435+84
	bl	_ZN6config8getvalueEPcS0_
	mov	r3, r0
	ldr	r0, .L435+88
	mov	r1, r3
	ldr	r3, .L435+108
	mov	lr, pc
	mov	pc, r3
	mov	r3, #0
	str	r3, [fp, #-48]
.L403:
	ldr	r3, [fp, #-48]
	cmp	r3, #31
	bgt	.L404
	ldr	r3, .L435+92
	ldr	r3, [r3, #0]
	ldr	r2, [r3, #20]
	ldr	r3, [fp, #-48]
	cmp	r2, r3
	ble	.L404
	sub	r2, fp, #300
	ldr	r3, [fp, #-48]
	add	r3, r3, #1
	mov	r0, r2
	ldr	r1, .L435+96
	mov	r2, r3
	bl	sprintf
	sub	r2, fp, #92
	sub	r1, fp, #300
	mov	r3, #1
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r2, .L435+100
	bl	_ZN6config8getvalueEPcS0_
	mov	r1, r0
	ldr	r3, [fp, #-48]
	mov	r2, r3, asl #2
	ldr	r3, .L435+104
	add	r3, r2, r3
	mov	r0, r3
	ldr	r3, .L435+108
	mov	lr, pc
	mov	pc, r3
	ldr	r3, .L435+112
	str	r3, [fp, #-388]
	ldr	r2, [fp, #-48]
	str	r2, [fp, #-392]
	sub	r3, fp, #92
	sub	r2, fp, #300
	mov	r0, r3
	mov	r1, r2
	ldr	r2, .L435+116
	bl	_ZN6config11getvalueintEPcS0_
	ldr	r3, [fp, #-392]
	ldr	r2, [fp, #-388]
	str	r0, [r2, r3, asl #2]
	ldr	r3, [fp, #-48]
	add	r3, r3, #1
	str	r3, [fp, #-48]
	b	.L403
.L404:
	ldr	r2, .L435+120
	mov	r3, #0
	strb	r3, [r2, #0]
	ldr	r2, .L435+124
	mov	r3, #1
	str	r3, [fp, #-348]
	ldr	r0, [r2, #0]
	ldr	r1, .L435+128
	bl	fopen
	mov	r3, r0
	str	r3, [fp, #-56]
	ldr	r3, [fp, #-56]
	cmp	r3, #0
	beq	.L406
	bl	getpid
	mov	r3, r0
	ldr	r0, [fp, #-56]
	ldr	r1, .L435+132
	mov	r2, r3
	bl	fprintf
	ldr	r0, [fp, #-56]
	bl	fclose
.L406:
	ldr	r0, .L435+136
	mov	r1, #0
	ldr	r2, .L435+140
	mov	r3, #0
	bl	pthread_create
	ldr	r3, .L435+144
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L407
	ldr	r0, .L435+148
	mov	r1, #0
	ldr	r2, .L435+152
	mov	r3, #0
	bl	pthread_create
.L407:
	mov	r3, #1
	str	r3, [fp, #-348]
	ldr	r0, .L435+156
	bl	printf
	sub	r3, fp, #100
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	sub	r2, fp, #92
	mov	r3, #3
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r3, .L435+160
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	mov	r3, #1
	str	r3, [fp, #-356]
	b	.L369
.L432:
	add	fp, fp, #40
	ldr	r2, [fp, #-348]
	str	r2, [fp, #-424]
	ldr	r3, [fp, #-344]
	str	r3, [fp, #-400]
	ldr	r2, [fp, #-424]
	cmp	r2, #1
	beq	.L416
	ldr	r3, [fp, #-424]
	cmp	r3, #2
	beq	.L420
.L408:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-396]
	sub	r3, fp, #100
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-396]
	str	r3, [fp, #-400]
.L410:
.L412:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-404]
	sub	r3, fp, #96
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-404]
	str	r3, [fp, #-400]
.L414:
.L416:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-408]
	sub	r2, fp, #92
	mov	r3, #0
	str	r3, [fp, #-348]
	mov	r0, r2
	ldr	r3, .L435+160
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-408]
	str	r3, [fp, #-400]
.L418:
.L420:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-412]
	sub	r3, fp, #68
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-412]
	str	r3, [fp, #-400]
.L422:
.L424:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-416]
	sub	r3, fp, #64
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-416]
	str	r3, [fp, #-400]
.L426:
.L428:
	ldr	r2, [fp, #-400]
	str	r2, [fp, #-420]
	sub	r3, fp, #60
	mov	r0, r3
	ldr	r3, .L435+164
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-420]
	str	r3, [fp, #-400]
.L430:
	mvn	r3, #0
	str	r3, [fp, #-348]
	ldr	r0, [fp, #-400]
	bl	_Unwind_SjLj_Resume
.L369:
	sub	r3, fp, #352
	mov	r0, r3
	bl	_Unwind_SjLj_Unregister
	ldr	r0, [fp, #-356]
	sub	sp, fp, #40
	ldmfd	sp, {r4, r5, r6, r7, r8, r9, sl, fp, sp, pc}
	.size	_Z7appinitv, .-_Z7appinitv
	.section	.gcc_except_table
.LLSDA121:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE121-.LLSDACSB121
.LLSDACSB121:
	.uleb128 0x0
	.uleb128 0x0
	.uleb128 0x1
	.uleb128 0x0
	.uleb128 0x2
	.uleb128 0x0
.LLSDACSE121:
	.text
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
	beq	.L438
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	strlen
	mov	r3, r0
	str	r3, [fp, #-20]
	b	.L437
.L438:
	mov	r3, #0
	str	r3, [fp, #-20]
.L437:
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
	beq	.L441
	ldr	r3, [fp, #-20]
	ldr	r0, [r3, #0]
	bl	_ZdlPv
	ldr	r2, [fp, #-20]
	mov	r3, #0
	str	r3, [r2, #0]
.L441:
	ldr	r0, [fp, #-24]
	ldr	r3, .L443
	mov	lr, pc
	mov	pc, r3
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	beq	.L442
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
.L442:
	ldr	r3, [fp, #-20]
	mov	r0, r3
	sub	sp, fp, #16
	ldmfd	sp, {r4, fp, sp, pc}
.L444:
	.align	2
.L443:
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
	beq	.L446
	ldr	r3, [fp, #-16]
	ldr	r0, [r3, #0]
	bl	_ZdlPv
.L446:
	ldmib	sp, {fp, sp, pc}
	.size	_ZN6stringD1Ev, .-_ZN6stringD1Ev
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
.L452:
	ldr	r3, [fp, #-16]
	add	r3, r3, #16
	mov	r0, r3
	ldr	r3, .L459
	mov	lr, pc
	mov	pc, r3
.L454:
	ldr	r3, [fp, #-16]
	add	r3, r3, #12
	mov	r0, r3
	ldr	r3, .L459
	mov	lr, pc
	mov	pc, r3
.L456:
	ldr	r0, [fp, #-16]
	bl	_ZN5arrayI6stringED1Ev
	ldmib	sp, {fp, sp, pc}
.L460:
	.align	2
.L459:
	.word	_ZN6stringD1Ev
.L450:
	.size	_ZN6configD1Ev, .-_ZN6configD1Ev
	.section	.rodata
	.align	2
.LC51:
	.ascii	"GPS logging process ended!\n\000"
	.text
	.align	2
	.global	_Z9appfinishv
	.type	_Z9appfinishv, %function
_Z9appfinishv:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, fp, ip, lr, pc}
	sub	fp, ip, #4
	ldr	r3, .L464+8
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L462
	ldr	r3, .L464+8
	ldr	r0, [r3, #0]
	mov	r1, #0
	bl	pthread_join
.L462:
	ldr	r2, .L464+8
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r3, .L464+12
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L463
	ldr	r3, .L464+12
	ldr	r0, [r3, #0]
	mov	r1, #0
	bl	pthread_join
.L463:
	ldr	r2, .L464+12
	mov	r3, #0
	str	r3, [r2, #0]
	ldr	r0, .L464+16
	bl	_Z12serial_closePi
	bl	_Z12gps_logclosev
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #76]
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #80
	add	r2, r2, r3
	adr	r3, .L464
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #88
	add	r2, r2, r3
	adr	r3, .L464
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #96
	add	r2, r2, r3
	adr	r3, .L464
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #104
	add	r2, r2, r3
	adr	r3, .L464
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #112
	add	r2, r2, r3
	adr	r3, .L464
	ldmia	r3, {r3-r4}
	stmia	r2, {r3-r4}
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #16]
	ldr	r3, .L464+20
	ldr	r2, [r3, #0]
	ldr	r3, .L464+20
	ldr	r3, [r3, #0]
	ldr	r3, [r3, #0]
	sub	r3, r3, #1
	str	r3, [r2, #0]
	ldr	r3, .L464+20
	ldr	r0, [r3, #0]
	mov	r1, #172
	bl	munmap
	ldr	r3, .L464+24
	ldr	r0, [r3, #0]
	bl	unlink
	ldr	r0, .L464+28
	bl	printf
	ldmfd	sp, {r4, fp, sp, pc}
.L465:
	.align	2
.L464:
	.word	0
	.word	0
	.word	sensorthreadid
	.word	tab102b_threadid
	.word	gps_port_handle
	.word	p_dio_mmap
	.word	pidfile
	.word	.LC51
	.size	_Z9appfinishv, .-_Z9appfinishv
	.align	2
	.global	main
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 100
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #100
	mov	r3, #0
	str	r3, [fp, #-36]
	ldr	r2, .L486
	mov	r3, #1
	str	r3, [r2, #0]
	mov	r0, #3
	ldr	r1, .L486+4
	bl	signal
	mov	r0, #2
	ldr	r1, .L486+4
	bl	signal
	mov	r0, #15
	ldr	r1, .L486+4
	bl	signal
	mov	r0, #12
	ldr	r1, .L486+4
	bl	signal
	mov	r0, #10
	ldr	r1, .L486+4
	bl	signal
	mov	r0, #13
	ldr	r1, .L486+4
	bl	signal
	bl	_Z7appinitv
	mov	r3, r0
	cmp	r3, #0
	bne	.L467
	mov	r3, #1
	str	r3, [fp, #-128]
	b	.L466
.L467:
	ldr	r2, .L486+8
	mov	r3, #1
	str	r3, [r2, #0]
	sub	r3, fp, #80
	mov	r0, r3
	mov	r1, #0
	mov	r2, #44
	bl	memset
	ldr	r3, .L486+12
	mov	lr, r3
	sub	ip, fp, #80
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip, {r0, r1, r2}
	stmia	lr, {r0, r1, r2}
.L468:
	ldr	r3, .L486
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L469
	ldr	r3, .L486+16
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L470
	ldr	r3, .L486+16
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #15
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L471
	ldr	r2, .L486
	mov	r3, #0
	str	r3, [r2, #0]
	b	.L472
.L471:
	ldr	r3, .L486+16
	ldr	r3, [r3, #0]
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L473
	ldr	r2, .L486
	mov	r3, #2
	str	r3, [r2, #0]
.L473:
	ldr	r3, .L486+16
	ldr	r3, [r3, #0]
	mov	r3, r3, lsr #1
	and	r3, r3, #1
	and	r3, r3, #255
	cmp	r3, #0
	beq	.L472
	ldr	r3, .L486
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L475
	ldr	r2, .L486
	mov	r3, #0
	str	r3, [r2, #0]
	bl	_Z9appfinishv
	bl	_Z7appinitv
.L475:
	ldr	r2, .L486
	mov	r3, #1
	str	r3, [r2, #0]
.L472:
	ldr	r2, .L486+16
	mov	r3, #0
	str	r3, [r2, #0]
.L470:
	ldr	r3, .L486
	ldr	r3, [r3, #0]
	cmp	r3, #1
	bne	.L476
	ldr	r3, .L486+20
	ldr	r3, [r3, #0]
	cmp	r3, #0
	bne	.L476
	sub	r3, fp, #80
	mov	r0, r3
	bl	_Z12gps_readdataP8gps_data
	mov	r3, r0
	str	r3, [fp, #-32]
	ldr	r3, [fp, #-32]
	cmp	r3, #1
	bne	.L477
	bl	_Z8dio_lockv
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #80
	add	r5, r2, r3
	ldr	r0, [fp, #-44]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	r5, {r3-r4}
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #88
	add	r5, r2, r3
	ldr	r0, [fp, #-40]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	r5, {r3-r4}
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #96
	add	r5, r2, r3
	ldr	r0, [fp, #-52]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	r5, {r3-r4}
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #104
	add	r5, r2, r3
	ldr	r0, [fp, #-48]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	stmia	r5, {r3-r4}
	mov	r3, #0
	str	r3, [fp, #-124]
	ldr	r3, [fp, #-60]
	str	r3, [fp, #-120]
	ldr	r3, [fp, #-64]
	str	r3, [fp, #-116]
	ldr	r3, [fp, #-68]
	str	r3, [fp, #-112]
	ldr	r3, [fp, #-72]
	sub	r3, r3, #1
	str	r3, [fp, #-108]
	ldr	r3, [fp, #-76]
	sub	r3, r3, #1888
	sub	r3, r3, #12
	str	r3, [fp, #-104]
	mov	r3, #0
	str	r3, [fp, #-100]
	mov	r3, #0
	str	r3, [fp, #-96]
	mvn	r3, #0
	str	r3, [fp, #-92]
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #112
	add	r7, r2, r3
	sub	r3, fp, #124
	mov	r0, r3
	bl	timegm
	mov	r3, r0
	mov	r0, r3
	bl	__floatsidf
	mov	r6, r1
	mov	r5, r0
	ldr	r0, [fp, #-56]	@ float
	bl	__extendsfdf2
	mov	r4, r1
	mov	r3, r0
	mov	r1, r6
	mov	r0, r5
	mov	r2, r3
	mov	r3, r4
	bl	__adddf3
	mov	r4, r1
	mov	r3, r0
	stmia	r7, {r3-r4}
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	ldr	r3, [fp, #-80]
	str	r3, [r2, #76]
	bl	_Z10dio_unlockv
	ldr	r3, [fp, #-80]
	cmp	r3, #0
	beq	.L479
	ldr	r3, .L486+12
	mov	lr, r3
	sub	ip, fp, #80
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip!, {r0, r1, r2, r3}
	stmia	lr!, {r0, r1, r2, r3}
	ldmia	ip, {r0, r1, r2}
	stmia	lr, {r0, r1, r2}
	bl	_Z7gps_logv
	b	.L479
.L477:
	ldr	r3, [fp, #-32]
	cmp	r3, #0
	bne	.L479
	ldr	r3, .L486+24
	ldr	r2, [r3, #0]
	mov	r3, #0
	str	r3, [r2, #76]
.L479:
	ldr	r3, [fp, #-80]
	cmp	r3, #0
	beq	.L481
	ldr	r3, [fp, #-36]
	cmp	r3, #0
	bne	.L468
	mov	r0, #2
	mov	r1, #300
	mov	r2, #300
	bl	_Z6buzzeriii
	mov	r3, #1
	str	r3, [fp, #-36]
	b	.L468
.L481:
	ldr	r3, [fp, #-36]
	cmp	r3, #1
	bne	.L468
	mov	r0, #3
	mov	r1, #300
	mov	r2, #300
	bl	_Z6buzzeriii
	mov	r3, #0
	str	r3, [fp, #-36]
	b	.L468
.L476:
	ldr	r0, .L486+28
	bl	_Z12serial_closePi
	bl	_Z12gps_logclosev
	mov	r0, #9984
	add	r0, r0, #16
	bl	usleep
	b	.L468
.L469:
	bl	_Z9appfinishv
	mov	r3, #0
	str	r3, [fp, #-128]
.L466:
	ldr	r0, [fp, #-128]
	sub	sp, fp, #28
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.L487:
	.align	2
.L486:
	.word	app_state
	.word	_Z11sig_handleri
	.word	glog_poweroff
	.word	validgpsdata
	.word	sigcap
	.word	gps_port_disable
	.word	p_dio_mmap
	.word	gps_port_handle
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
	ble	.L488
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	cmp	r3, #0
	beq	.L488
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
.L493:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	ldr	r2, [fp, #-20]
	cmp	r3, r2
	beq	.L494
	ldr	r3, [fp, #-20]
	sub	r3, r3, #4
	str	r3, [fp, #-20]
	ldr	r0, [fp, #-20]
	ldr	r3, .L496
	mov	lr, pc
	mov	pc, r3
	b	.L493
.L494:
	ldr	r3, [fp, #-16]
	ldr	r3, [r3, #0]
	sub	r3, r3, #4
	mov	r0, r3
	bl	_ZdaPv
.L488:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L497:
	.align	2
.L496:
	.word	_ZN6stringD1Ev
	.size	_ZN5arrayI6stringED1Ev, .-_ZN5arrayI6stringED1Ev
	.text
	.align	2
	.type	_Z41__static_initialization_and_destruction_0ii, %function
_Z41__static_initialization_and_destruction_0ii:
	@ args = 0, pretend = 0, frame = 16
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #16
	str	r0, [fp, #-16]
	str	r1, [fp, #-20]
	ldr	r2, [fp, #-20]
	mov	r3, #65280
	add	r3, r3, #255
	cmp	r2, r3
	bne	.L499
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bne	.L499
	ldr	r0, .L507
	ldr	r3, .L507+4
	mov	lr, pc
	mov	pc, r3
	ldr	r0, .L507+8
	mov	r1, #0
	ldr	r2, .L507+12
	bl	__cxa_atexit
.L499:
	ldr	r2, [fp, #-20]
	mov	r3, #65280
	add	r3, r3, #255
	cmp	r2, r3
	bne	.L498
	ldr	r3, [fp, #-16]
	cmp	r3, #1
	bne	.L498
	ldr	r3, .L507+16
	str	r3, [fp, #-24]
	mov	r3, #31
	str	r3, [fp, #-28]
.L501:
	ldr	r3, [fp, #-28]
	cmn	r3, #1
	bne	.L504
	b	.L506
.L504:
	ldr	r0, [fp, #-24]
	ldr	r3, .L507+4
	mov	lr, pc
	mov	pc, r3
	ldr	r3, [fp, #-24]
	add	r3, r3, #4
	str	r3, [fp, #-24]
	ldr	r3, [fp, #-28]
	sub	r3, r3, #1
	str	r3, [fp, #-28]
	b	.L501
.L505:
.L506:
	ldr	r0, .L507+20
	mov	r1, #0
	ldr	r2, .L507+12
	bl	__cxa_atexit
.L498:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L508:
	.align	2
.L507:
	.word	dvrcurdisk
	.word	_ZN6stringC1Ev
	.word	__tcf_0
	.word	__dso_handle
	.word	sensorname
	.word	__tcf_1
	.size	_Z41__static_initialization_and_destruction_0ii, .-_Z41__static_initialization_and_destruction_0ii
	.align	2
	.type	__tcf_1, %function
__tcf_1:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #8
	str	r0, [fp, #-16]
	ldr	r2, .L514
	str	r2, [fp, #-20]
.L512:
	ldr	r3, .L514+4
	ldr	r2, [fp, #-20]
	cmp	r2, r3
	beq	.L509
	ldr	r3, [fp, #-20]
	sub	r3, r3, #4
	str	r3, [fp, #-20]
	ldr	r0, [fp, #-20]
	ldr	r3, .L514+8
	mov	lr, pc
	mov	pc, r3
	b	.L512
.L509:
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.L515:
	.align	2
.L514:
	.word	sensorname+128
	.word	sensorname
	.word	_ZN6stringD1Ev
	.size	__tcf_1, .-__tcf_1
	.align	2
	.type	__tcf_0, %function
__tcf_0:
	@ args = 0, pretend = 0, frame = 4
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	sub	sp, sp, #4
	str	r0, [fp, #-16]
	ldr	r0, .L517
	ldr	r3, .L517+4
	mov	lr, pc
	mov	pc, r3
	ldmib	sp, {fp, sp, pc}
.L518:
	.align	2
.L517:
	.word	dvrcurdisk
	.word	_ZN6stringD1Ev
	.size	__tcf_0, .-__tcf_0
	.align	2
	.type	_GLOBAL__I_baud_table, %function
_GLOBAL__I_baud_table:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	mov	r0, #1
	mov	r1, #65280
	add	r1, r1, #255
	bl	_Z41__static_initialization_and_destruction_0ii
	ldmfd	sp, {fp, sp, pc}
	.size	_GLOBAL__I_baud_table, .-_GLOBAL__I_baud_table
	.section	.ctors,"aw",%progbits
	.align	2
	.word	_GLOBAL__I_baud_table
	.ident	"GCC: (GNU) 3.4.6"
