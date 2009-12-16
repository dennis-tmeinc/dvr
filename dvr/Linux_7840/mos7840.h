/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



/*************************************************************************
 ***
 ***
 *** Module Name: Mos7840
 ***
 *** File: mos7840.h 
 ***		
 ***
 *** File Revision: 1.0
 ***
 *** Revision Date:  10/05/06 
 ***
 ***
 *** Purpose	  : It contains defines that will be used by mosc7840.c 
 ***
 *** Change History:
 ***
 ***
 *** LEGEND	:
 ***
 *** 
 ***
 *************************************************************************/

#if !defined(_MOS_CIP_H_)
#define	_MOS_CIP_H_


#define MAX_RS232_PORTS		2	/* Max # of RS-232 ports per device */

#include <linux/version.h>

/* 
 *  All typedef goes here
 */


/* typedefs that the insideout headers need */

#ifndef TRUE
	#define TRUE		(1)
#endif

#ifndef FALSE
	#define FALSE		(0)
#endif

#ifndef LOW8
	#define LOW8(val)	((unsigned char)(val & 0xff))
#endif

#ifndef HIGH8
	#define HIGH8(val)	((unsigned char)((val & 0xff00) >> 8))
#endif

#ifndef NUM_ENTRIES
	#define NUM_ENTRIES(x)	(sizeof(x)/sizeof((x)[0]))
#endif

#ifndef __KERNEL__
	#define __KERNEL__
#endif

#define MAX_SERIALNUMBER_LEN 	12

/* The following table is used to map the USBx port number to 
 * the device serial number (or physical USB path), */

#define MAX_MOSPORTS	2
#define MAX_NAME_LEN	64

#define RAID_REG1 	0x30
#define RAID_REG2 	0x31

#define ZLP_REG1  	0x3A      //Zero_Flag_Reg1    58
#define ZLP_REG2  	0x3B      //Zero_Flag_Reg2    59
#define ZLP_REG3  	0x3C      //Zero_Flag_Reg3    60
#define ZLP_REG4  	0x3D      //Zero_Flag_Reg4    61
#define ZLP_REG5  	0x3E      //Zero_Flag_Reg5    62

#define THRESHOLD_VAL_SP1_1     0x3F
#define THRESHOLD_VAL_SP1_2     0x40
#define THRESHOLD_VAL_SP2_1     0x41
#define THRESHOLD_VAL_SP2_2     0x42

#define THRESHOLD_VAL_SP3_1     0x43
#define THRESHOLD_VAL_SP3_2     0x44
#define THRESHOLD_VAL_SP4_1     0x45
#define THRESHOLD_VAL_SP4_2     0x46


/* For higher baud Rates use TIOCEXBAUD */
#define TIOCEXBAUD	0x5462  

#define BAUD_1152	0	/* 115200bps  * 1	*/
#define BAUD_2304	1	/* 230400bps  * 2	*/
#define BAUD_4032	2	/* 403200bps  * 3.5	*/
#define BAUD_4608	3	/* 460800bps  * 4	*/
#define BAUD_8064	4	/* 806400bps  * 7	*/
#define BAUD_9216	5	/* 921600bps  * 8	*/

#define CHASE_TIMEOUT		(5*HZ)		/* 5 seconds */
#define OPEN_TIMEOUT		(5*HZ)		/* 5 seconds */
#define COMMAND_TIMEOUT		(5*HZ)		/* 5 seconds */

#ifndef SERIAL_MAGIC
	#define SERIAL_MAGIC	0x6702
#endif

#define PORT_MAGIC		0x7301



/* vendor id and device id defines */

#define USB_VENDOR_ID_MOSCHIP		0x9710
#define MOSCHIP_DEVICE_ID_7840		0x7840
#define MOSCHIP_DEVICE_ID_7820		0x7820

/* Product information read from the Moschip. Provided for later upgrade */

/* Interrupt Rotinue Defines	*/
	
#define SERIAL_IIR_RLS      0x06
#define SERIAL_IIR_RDA      0x04
#define SERIAL_IIR_CTI      0x0c
#define SERIAL_IIR_THR      0x02
#define SERIAL_IIR_MS       0x00

/*
 *  Emulation of the bit mask on the LINE STATUS REGISTER.
 */
#define SERIAL_LSR_DR       0x0001
#define SERIAL_LSR_OE       0x0002
#define SERIAL_LSR_PE       0x0004
#define SERIAL_LSR_FE       0x0008
#define SERIAL_LSR_BI       0x0010
#define SERIAL_LSR_THRE     0x0020
#define SERIAL_LSR_TEMT     0x0040
#define SERIAL_LSR_FIFOERR  0x0080

//MSR bit defines(place holders)
#define MOS_MSR_CTS         0x01
#define MOS_MSR_DSR         0x02 
#define MOS_MSR_RI          0x04
#define MOS_MSR_CD          0x08
#define MOS_MSR_DELTA_CTS   0x10
#define MOS_MSR_DELTA_DSR   0x20 
#define MOS_MSR_DELTA_RI    0x40
#define MOS_MSR_DELTA_CD    0x80

// Serial Port register Address
#define RECEIVE_BUFFER_REGISTER    ((__u16)(0x00))
#define TRANSMIT_HOLDING_REGISTER  ((__u16)(0x00))
#define INTERRUPT_ENABLE_REGISTER  ((__u16)(0x01))
#define INTERRUPT_IDENT_REGISTER   ((__u16)(0x02))
#define FIFO_CONTROL_REGISTER      ((__u16)(0x02))
#define LINE_CONTROL_REGISTER      ((__u16)(0x03))
#define MODEM_CONTROL_REGISTER     ((__u16)(0x04))
#define LINE_STATUS_REGISTER       ((__u16)(0x05))
#define MODEM_STATUS_REGISTER      ((__u16)(0x06))
#define SCRATCH_PAD_REGISTER       ((__u16)(0x07))
#define DIVISOR_LATCH_LSB          ((__u16)(0x00))
#define DIVISOR_LATCH_MSB          ((__u16)(0x01))

#define SP_REGISTER_BASE           ((__u16)(0x08))
#define CONTROL_REGISTER_BASE      ((__u16)(0x09))
#define DCR_REGISTER_BASE          ((__u16)(0x16))

#define SP1_REGISTER               ((__u16)(0x00))
#define CONTROL1_REGISTER          ((__u16)(0x01))
#define CLK_MULTI_REGISTER         ((__u16)(0x02))
#define CLK_START_VALUE_REGISTER   ((__u16)(0x03))
#define DCR1_REGISTER              ((__u16)(0x04))
#define GPIO_REGISTER              ((__u16)(0x07))

#define CLOCK_SELECT_REG1          ((__u16)(0x13))
#define CLOCK_SELECT_REG2          ((__u16)(0x14))

#define SERIAL_LCR_DLAB       	   ((__u16)(0x0080))

/*
 * URB POOL related defines
 */
#define NUM_URBS                        16     /* URB Count */
#define URB_TRANSFER_BUFFER_SIZE        32     /* URB Size  */

struct moschip_product_info 
{
	__u16	ProductId;		/* Product Identifier */
	__u8	NumPorts;		/* Number of ports on moschip */
	__u8	ProdInfoVer;		/* What version of structure is this? */

	__u32	IsServer        :1;	/* Set if Server */
	__u32	IsRS232         :1;	/* Set if RS-232 ports exist */
	__u32	IsRS422         :1;	/* Set if RS-422 ports exist */
	__u32	IsRS485         :1;	/* Set if RS-485 ports exist */
	__u32	IsReserved      :28;	/* Reserved for later expansion */

	__u8	CpuRev;			/* CPU revision level (chg only if s/w visible) */
	__u8	BoardRev;		/* PCB revision level (chg only if s/w visible) */

	__u8	ManufactureDescDate[3];	/* MM/DD/YY when descriptor template was compiled */
	__u8	Unused1[1];		/* Available */
};

// different USB-serial Adapter's ID's table
static struct usb_device_id moschip_port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_MOSCHIP,MOSCHIP_DEVICE_ID_7840) },
	{ USB_DEVICE(USB_VENDOR_ID_MOSCHIP,MOSCHIP_DEVICE_ID_7820) },
	{ } /* terminating entry */
};

static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(USB_VENDOR_ID_MOSCHIP,MOSCHIP_DEVICE_ID_7840) },
	{ USB_DEVICE(USB_VENDOR_ID_MOSCHIP,MOSCHIP_DEVICE_ID_7820) },
	{ } /* terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table_combined);

/* This structure holds all of the local port information */
struct moschip_port 
{
	int		port_num;          /*Actual port number in the device(1,2,etc)*/
	__u8		bulk_out_endpoint;   	/* the bulk out endpoint handle */
	unsigned char 	*bulk_out_buffer;     	/* buffer used for the bulk out endpoint */	
	struct urb    	*write_urb;	     	/* write URB for this port */
	__u8	      	bulk_in_endpoint;	/* the bulk in endpoint handle */
	unsigned char 	*bulk_in_buffer;	/* the buffer we use for the bulk in endpoint */
	struct urb   	*read_urb;	     	/* read URB for this port */
	__s16	     rxBytesAvail;/*the number of bytes that we need to read from this device */
	__s16		rxBytesRemaining;	/* the number of port bytes left to read */
	char		write_in_progress;	/* TRUE while a write URB is outstanding */
	__u8		shadowLCR;		/* last LCR value received */
	__u8		shadowMCR;		/* last MCR value received */
	__u8		shadowMSR;		/* last MSR value received */
	__u8		shadowLSR;		/* last LSR value received */
	__u8		shadowXonChar;		/* last value set as XON char in moschip */
	__u8		shadowXoffChar;		/* last value set as XOFF char in moschip */
	__u8		validDataMask;
	__u32		baudRate;
	char		open;
	char		openPending;
	char		commandPending;
	char		closePending;
	char		chaseResponsePending;
	wait_queue_head_t	wait_chase;		/* for handling sleeping while waiting for chase to finish */
	wait_queue_head_t	wait_open;		/* for handling sleeping while waiting for open to finish */
	wait_queue_head_t	wait_command;		/* for handling sleeping while waiting for command to finish */
	wait_queue_head_t	delta_msr_wait;		/* for handling sleeping while waiting for msr change to happen */
	 int                     delta_msr_cond;
	struct async_icount	icount;
	struct usb_serial_port	*port;			/* loop back to the owner of this object */
	/*Offsets*/
	__u16 		AppNum;
	__u8 		SpRegOffset;
	__u8 		ControlRegOffset;
	__u8 		DcrRegOffset;
	__u8 		ClkSelectRegOffset;
	//for processing control URBS in interrupt context
	struct urb 	*control_urb;
       // __le16 rx_creg;
        char 		*ctrl_buf;
	int  		MsrLsr;
	
	struct urb      *write_urb_pool[NUM_URBS];
	/* we pass a pointer to this as the arguement sent to cypress_set_termios old_termios */
        struct termios tmp_termios;        /* stores the old termios settings */
	spinlock_t 	lock;                   /* private lock */
};


/* This structure holds all of the individual serial device information */
struct moschip_serial 
{
	char		name[MAX_NAME_LEN+1];		/* string name of this device */
	struct	moschip_product_info	product_info;	/* Product Info */
	__u8		interrupt_in_endpoint;		/* the interrupt endpoint handle */
	unsigned char  *interrupt_in_buffer;		/* the buffer we use for the interrupt endpoint */
	struct urb *	interrupt_read_urb;	/* our interrupt urb */
	__u8		bulk_in_endpoint;	/* the bulk in endpoint handle */
	unsigned char  *bulk_in_buffer;		/* the buffer we use for the bulk in endpoint */
	struct urb 	*read_urb;		/* our bulk read urb */
	__u8		bulk_out_endpoint;	/* the bulk out endpoint handle */
	__s16	 rxBytesAvail;	/* the number of bytes that we need to read from this device */
	__u8		rxPort;		/* the port that we are currently receiving data for */
	__u8		rxStatusCode;		/* the receive status code */
	__u8		rxStatusParam;		/* the receive status paramater */
	__s16		rxBytesRemaining;	/* the number of port bytes left to read */
	struct usb_serial	*serial;	/* loop back to the owner of this object */
	int	mos7840_spectrum_2or4ports; 	//this says the number of ports in the device
	// indicates the abt the no.of open ports of an individual USB-serial adapater.
	unsigned int	NoOfOpenPorts;
	// a flag for Status endpoint polling
	unsigned char	status_polling_started;
};

/* baud rate information */
struct mos7840_divisor_table_entry 
{
	__u32  BaudRate;
	__u16  Divisor;
};

/* Define table of divisors for moschip 7840 hardware	   *
 * These assume a 3.6864MHz crystal, the standard /16, and *
 * MCR.7 = 0.						   */
#ifdef NOTMCS7840
static struct mos7840_divisor_table_entry mos7840_divisor_table[] = {
	{   50,		2304},  
	{   110,	1047},	/* 2094.545455 => 230450   => .0217 % over */
	{   134,	857},	/* 1713.011152 => 230398.5 => .00065% under */
	{   150,	768},
	{   300,	384},
	{   600,	192},
	{   1200,	96},
	{   1800,	64},
	{   2400,	48},
	{   4800,	24},
	{   7200,	16},
	{   9600,	12},
	{   19200,	6},
	{   38400,	3},
	{   57600,	2},
	{   115200,	1},
};
#endif

/* local function prototypes */
/* function prototypes for all URB callbacks */

static void mos7840_interrupt_callback(struct urb *urb, struct pt_regs *regs);
static void mos7840_bulk_in_callback(struct urb *urb, struct pt_regs *regs);
static void mos7840_bulk_out_data_callback(struct urb *urb, struct pt_regs *regs);
static void mos7840_control_callback(struct urb *urb, struct pt_regs *regs);
static int mos7840_get_reg(struct moschip_port *mcs,__u16 Wval, __u16 reg, __u16 * val);			
int handle_newMsr(struct moschip_port *port,__u8 newMsr);
int handle_newLsr(struct moschip_port *port,__u8 newLsr);
/* function prototypes for the usbserial callbacks */

static int  mos7840_open(struct usb_serial_port *port, struct file *filp);
static void mos7840_close(struct usb_serial_port *port, struct file *filp);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
static int mos7840_write(struct usb_serial_port *port, const unsigned char *data, int count);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
static int mos7840_write(struct usb_serial_port *port, int from_user, const unsigned char *data, 				int count);
#endif


static int  mos7840_write_room(struct usb_serial_port *port);
static int  mos7840_chars_in_buffer(struct usb_serial_port *port);
static void mos7840_throttle(struct usb_serial_port *port);
static void mos7840_unthrottle(struct usb_serial_port *port);
static void mos7840_set_termios	(struct usb_serial_port *port, struct termios *old_termios);
static int mos7840_tiocmset(struct usb_serial_port *port, struct file *file,
        			unsigned int set, unsigned int clear);
static int mos7840_tiocmget(struct usb_serial_port *port, struct file *file);
//static void mos7840_break_ctl(struct usb_serial_port *port, int break_state );

static int  mos7840_ioctl(struct usb_serial_port *port, struct file *file, unsigned int cmd, unsigned long arg);
static void mos7840_break(struct usb_serial_port *port, int break_state);
static int  mos7840_startup(struct usb_serial *serial);
static void mos7840_shutdown(struct usb_serial *serial);
//static int mos7840_serial_probe(struct usb_serial *serial, const struct usb_device_id *id);
static int mos7840_calc_num_ports(struct usb_serial *serial);

/* function prototypes for all of our local functions */
static int  mos7840_calc_baud_rate_divisor(int baudRate, int *divisor,__u16 *clk_sel_val);
static int  mos7840_send_cmd_write_baud_rate(struct moschip_port *mos7840_port, int baudRate);
static void mos7840_change_port_settings(struct moschip_port *mos7840_port, struct termios *old_termios);
static void mos7840_block_until_chase_response(struct moschip_port *mos7840_port);
static void mos7840_block_until_tx_empty(struct moschip_port *mos7840_port);

int __init moschip7840_init(void);
void __exit moschip7840_exit(void);

#endif
