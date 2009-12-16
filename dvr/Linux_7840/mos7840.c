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
 *** --------------------------------------------------------------------
 ***
 *** Project Name: MosChip
 ***
 *** Module Name: Mos7840
 ***
 *** File: mos7840.c 
 ***		
 ***
 *** File Revision: 1.2
 ***
 *** Revision Date:  30/07/06 
 ***
 ***
 *** Purpose	  : It gives an interface between USB to  4 Serial 
 ***                and serves as a Serial Driver for the high 
 ***		    level layers /applications.
 ***
 *** Change History:
 ***
 ***
 *** LEGEND	  :
 ***
 *** 
 *** DBG - Code inserted due to as part of debugging
 *** DPRINTK - Debug Print statement
 ***
 *************************************************************************/

/* all file inclusion goes here */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/serial.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/usb.h>


#define KERNEL_2_6		1

#include "mos7840.h"            /* MCS7840 Defines    */
#include "mos7840_16C50.h"	/* 16C50 UART defines */

/* all defines goes here */

/*
 * Debug related defines 
 */

/* 1: Enables the debugging -- 0: Disable the debugging */

//#define printk //

// #define MOS_DEBUG	1

#ifdef MOS_DEBUG
	static int debug = 0;
	#define DPRINTK(fmt, args...) printk( "%s: " fmt, __FUNCTION__ , ## args)

#else
	static int debug = 0;
	#define DPRINTK(fmt, args...)

#endif

#include "usb-serial.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "1.3.1"
#define DRIVER_DESC "Moschip 7840/7820 USB Serial Driver"

/*
 * Defines used for sending commands to port 
 */

#define WAIT_FOR_EVER   (HZ * 0 ) /* timeout urb is wait for ever*/
#define MOS_WDR_TIMEOUT (HZ * 5 ) /* default urb timeout */
                                                                                                                             
#define MOS_PORT1       0x0200
#define MOS_PORT2       0x0300
#define MOS_VENREG      0x0000
#define MOS_MAX_PORT	0x02                                                                                                                             
#define MOS_WRITE       0x0E
#define MOS_READ        0x0D

/* Requests */
#define MCS_RD_RTYPE 0xC0
#define MCS_WR_RTYPE 0x40
#define MCS_RDREQ    0x0D
#define MCS_WRREQ    0x0E
#define MCS_CTRL_TIMEOUT        500
#define VENDOR_READ_LENGTH                      (0x01)


int mos7840_Thr_cnt;
//int mos7840_spectrum_2or4ports; //this says the number of ports in the device
//int NoOfOpenPorts;

int RS485mode=1; //set to 1 for RS485 mode and 0 for RS232 mode
	
static struct usb_serial* mos7840_get_usb_serial (struct usb_serial_port *port, const
char *function);
static int mos7840_serial_paranoia_check (struct usb_serial *serial, const char
*function);
static int mos7840_port_paranoia_check (struct usb_serial_port *port, const char
*function);


/* setting and get register values */
static int mos7840_set_reg_sync(struct usb_serial_port *port, __u16 reg, __u16 val);
static int mos7840_get_reg_sync(struct usb_serial_port *port, __u16 reg, __u16 * val);
static int mos7840_set_Uart_Reg(struct usb_serial_port *port, __u16 reg, __u16 val);
static int mos7840_get_Uart_Reg(struct usb_serial_port *port, __u16 reg, __u16 * val);

void mos7840_Dump_serial_port(struct moschip_port *mos7840_port);

/************************************************************************/
/************************************************************************/
/*             I N T E R F A C E   F U N C T I O N S			*/
/*             I N T E R F A C E   F U N C T I O N S			*/
/************************************************************************/
/************************************************************************/

static inline void mos7840_set_serial_private(struct usb_serial *serial, struct moschip_serial *data)
{
		usb_set_serial_data(serial, (void *)data );
}

static inline struct moschip_serial * mos7840_get_serial_private(struct usb_serial *serial)
{
		return (struct moschip_serial*) usb_get_serial_data(serial);
}

static inline void mos7840_set_port_private(struct usb_serial_port *port, struct moschip_port *data)
{
		usb_set_serial_port_data(port, (void*)data );
}

static inline struct moschip_port * mos7840_get_port_private(struct usb_serial_port *port)
{
	return (struct moschip_port*) usb_get_serial_port_data(port);
}

/*
Description:- To set the Control register by calling usb_fill_control_urb function by passing usb_sndctrlpipe function as parameter.

Input Parameters:
usb_serial_port:  Data Structure usb_serialport correponding to that seril port.
Reg: Register Address
Val:  Value to set in the Register.
 */

static int mos7840_set_reg_sync(struct usb_serial_port *port, __u16 reg, __u16  val)
{
        struct usb_device *dev = port->serial->dev;
	val = val & 0x00ff;
	DPRINTK("mos7840_set_reg_sync offset is %x, value %x\n",reg,val);


        return usb_control_msg(dev, usb_sndctrlpipe(dev, 0), MCS_WRREQ,
                        MCS_WR_RTYPE, val, reg, NULL, 0,MOS_WDR_TIMEOUT);
}



/*
Description:- To set the Uart register by calling usb_fill_control_urb function by passing usb_rcvctrlpipe function as parameter.

Input Parameters:
usb_serial_port:  Data Structure usb_serialport correponding to that seril port.
Reg: Register Address
Val:  Value to receive from the Register.
 */

static int mos7840_get_reg_sync(struct usb_serial_port *port, __u16 reg, __u16 * val)
{
        struct usb_device *dev = port->serial->dev;
        int ret=0;

        ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0), MCS_RDREQ,
                        MCS_RD_RTYPE, 0, reg, val, VENDOR_READ_LENGTH,MOS_WDR_TIMEOUT);
	DPRINTK("mos7840_get_reg_sync offset is %x, return val %x\n",reg,*val);
	*val = (*val) & 0x00ff;
        return ret;
}



/*
Description:- To set the Uart register by calling usb_fill_control_urb function by passing usb_sndctrlpipe function as parameter.

Input Parameters:
usb_serial_port:  Data Structure usb_serialport correponding to that seril port.
Reg: Register Address
Val:  Value to set in the Register.
 */

static int mos7840_set_Uart_Reg(struct usb_serial_port *port, __u16 reg, __u16  val)
{


        struct usb_device *dev = port->serial->dev;
	struct moschip_serial *mos7840_serial;
	mos7840_serial = mos7840_get_serial_private(port->serial);
	val = val & 0x00ff;
        // For the UART control registers, the application number need to be Or'ed
	
	if(mos7840_serial->mos7840_spectrum_2or4ports == 4)
	{
	        val |= (((__u16)port->number - (__u16)(port->serial->minor))+1)<<8;
		DPRINTK("mos7840_set_Uart_Reg application number is %x\n",val);
	}
	else
	{
		if( ((__u16)port->number - (__u16)(port->serial->minor)) == 0)
		{
		//	val= 0x100;
	        val |= (((__u16)port->number - (__u16)(port->serial->minor))+1)<<8;
		DPRINTK("mos7840_set_Uart_Reg application number is %x\n",val);
		}
		else
		{
		//	val=0x300;
	        val |= (((__u16)port->number - (__u16)(port->serial->minor))+2)<<8;
			DPRINTK("mos7840_set_Uart_Reg application number is %x\n",val);
		}
	}
        return usb_control_msg(dev, usb_sndctrlpipe(dev, 0), MCS_WRREQ,
                        MCS_WR_RTYPE, val, reg, NULL, 0,MOS_WDR_TIMEOUT);

}


/*
Description:- To set the Control register by calling usb_fill_control_urb function by passing usb_rcvctrlpipe function as parameter.

Input Parameters:
usb_serial_port:  Data Structure usb_serialport correponding to that seril port.
Reg: Register Address
Val:  Value to receive from the Register.
 */
static int mos7840_get_Uart_Reg(struct usb_serial_port *port, __u16 reg, __u16 * val)
{
        struct usb_device *dev = port->serial->dev;
        int ret=0;
        __u16 Wval;
	 struct moschip_serial *mos7840_serial;
        mos7840_serial = mos7840_get_serial_private(port->serial);

	//DPRINTK("application number is %4x \n",(((__u16)port->number - (__u16)(port->serial->minor))+1)<<8);
	/*Wval  is same as application number*/
	if(mos7840_serial->mos7840_spectrum_2or4ports ==4)
	{
        	Wval=(((__u16)port->number - (__u16)(port->serial->minor))+1)<<8;
			DPRINTK("mos7840_get_Uart_Reg application number is %x\n",Wval);
	}
	else 
	{
		if( ((__u16)port->number - (__u16)(port->serial->minor)) == 0)
		{
	 	//	Wval= 0x100;
        	Wval=(((__u16)port->number - (__u16)(port->serial->minor))+1)<<8;
			DPRINTK("mos7840_get_Uart_Reg application number is %x\n",Wval);
		}
		else
		{
		//	Wval=0x300;
        	Wval=(((__u16)port->number - (__u16)(port->serial->minor))+2)<<8;
			DPRINTK("mos7840_get_Uart_Reg application number is %x\n",Wval);
		}
	}
	ret = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0), MCS_RDREQ,
                        MCS_RD_RTYPE, Wval, reg, val,VENDOR_READ_LENGTH,MOS_WDR_TIMEOUT);
	*val = (*val) & 0x00ff;
        return ret;
}



void mos7840_Dump_serial_port(struct moschip_port *mos7840_port)
{

	DPRINTK("***************************************\n");
	DPRINTK("Application number is %4x\n",mos7840_port->AppNum);
	DPRINTK("SpRegOffset is %2x\n",mos7840_port->SpRegOffset);
	DPRINTK("ControlRegOffset is %2x \n",mos7840_port->ControlRegOffset);	
	DPRINTK("DCRRegOffset is %2x \n",mos7840_port->DcrRegOffset);	
	//DPRINTK("ClkSelectRegOffset is %2x \n",mos7840_port->ClkSelectRegOffset);
	DPRINTK("***************************************\n");

}

/* all structre defination goes here */
/****************************************************************************
 * moschip7840_4port_device
 *              Structure defining MCS7840, usb serial device
 ****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static struct usb_serial_driver moschip7840_4port_device = {
	.driver                 = {
                                        .owner  = THIS_MODULE,
                                        .name   = "mos7840",
                                },
        .description            = DRIVER_DESC,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
static struct usb_serial_device_type moschip7840_4port_device = {
	.owner                  = THIS_MODULE,
        .name                   = "MoschipUSBSerialadapter",
        .short_name             = "Moschip78407820",
#endif
        .id_table               = moschip_port_id_table,
        .num_interrupt_in       = 1,
        #ifdef check
        .num_bulk_in            = 4,
        .num_bulk_out           = 4,
        .num_ports              = 4,
        #endif
	.open			= mos7840_open,
	.close			= mos7840_close,
	.write			= mos7840_write,
	.write_room		= mos7840_write_room,
	.chars_in_buffer	= mos7840_chars_in_buffer,
	.throttle		= mos7840_throttle,
	.unthrottle		= mos7840_unthrottle,
	.calc_num_ports		= mos7840_calc_num_ports,
	//.probe		= mos7840_serial_probe,
	.ioctl			= mos7840_ioctl,
	.set_termios		= mos7840_set_termios,
	.break_ctl		= mos7840_break,
//	.break_ctl  		= mos7840_break_ctl,
	.tiocmget               = mos7840_tiocmget,
        .tiocmset               = mos7840_tiocmset,
	.attach			= mos7840_startup,
	.shutdown		= mos7840_shutdown,
	.read_bulk_callback	= mos7840_bulk_in_callback, 
	.read_int_callback      = mos7840_interrupt_callback, 
};

static struct usb_driver io_driver = {
	.name =		"mos7840",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table_combined,
};



/************************************************************************/
/************************************************************************/
/*            U S B  C A L L B A C K   F U N C T I O N S                */
/*            U S B  C A L L B A C K   F U N C T I O N S                */
/************************************************************************/
/************************************************************************/

/*****************************************************************************
 * mos7840_interrupt_callback
 *	this is the callback function for when we have received data on the 
 *	interrupt endpoint.
 * Input : 1 Input
 *			pointer to the URB packet,
 *
 *****************************************************************************/
//#ifdef mos7840
static void mos7840_interrupt_callback (struct urb *urb,struct pt_regs *regs)
{
	int result;
	int length ;
	struct moschip_port   *mos7840_port;
	struct moschip_serial *mos7840_serial;
	struct usb_serial *serial;
	__u16 Data;
	unsigned char *data;
	__u8 sp[5],st;
	int i;
	__u16 wval;
	//printk("in the function mos7840_interrupt_callback Length %d, Data %x \n",urb->actual_length,(unsigned int)urb->transfer_buffer);
	DPRINTK("%s"," : Entering\n");
	
	mos7840_serial= (struct moschip_serial *)urb->context;
	if(!urb)// || mos7840_serial->status_polling_started == FALSE )
	{
		DPRINTK("%s","Invalid Pointer !!!!:\n");
		return;
	}

	switch (urb->status) 
	{
		case 0:
			/* success */
			break;
		case -ECONNRESET:
		case -ENOENT:
		case -ESHUTDOWN:
			/* this urb is terminated, clean up */
			dbg("%s - urb shutting down with status: %d", __FUNCTION__, urb->status);
			return;
		default:
			dbg("%s - nonzero urb status received: %d", __FUNCTION__, urb->status);
			goto exit;
	}
	length = urb->actual_length;
	data = urb->transfer_buffer;

	//mos7840_serial= (struct moschip_serial *)urb->context;
	//serial  = mos7840_get_usb_serial(port,__FUNCTION__);
	serial = mos7840_serial->serial;

	/* Moschip get 5 bytes 
	 * Byte 1 IIR Port 1 (port.number is 0)
	 * Byte 2 IIR Port 2 (port.number is 1)
	 * Byte 3 IIR Port 3 (port.number is 2)
	 * Byte 4 IIR Port 4 (port.number is 3)
	 * Byte 5 FIFO status for both */

	if(length && length>5)
	{
		DPRINTK("%s \n","Wrong data !!!");
		return;
	}

	/* MATRIX */
	if(mos7840_serial->mos7840_spectrum_2or4ports == 4)
	{
	sp[0]=(__u8)data[0];	
	sp[1]=(__u8)data[1];	
	sp[2]=(__u8)data[2];	
	sp[3]=(__u8)data[3];	
	st=(__u8)data[4];
	}
	else
	{
	sp[0]=(__u8)data[0];	
	sp[1]=(__u8)data[2];	
	//sp[2]=(__u8)data[2];	
	//sp[3]=(__u8)data[3];	
	st=(__u8)data[4];

	}
	//	printk("%s data is sp1:%x sp2:%x sp3:%x sp4:%x status:%x\n",__FUNCTION__,sp1,sp2,sp3,sp4,st);
	for(i=0;i<serial->num_ports;i++)
	{	
	mos7840_port = mos7840_get_port_private(serial->port[i]);
	if((mos7840_serial->mos7840_spectrum_2or4ports == 2) && (i != 0))
		wval = (((__u16)serial->port[i]->number - (__u16)(serial->minor))+2)<<8;
	else
		wval = (((__u16)serial->port[i]->number - (__u16)(serial->minor))+1)<<8;
	if(mos7840_port->open != FALSE)
	{	
		//printk("%s wval is:(for 7840) %x\n",__FUNCTION__,wval);
		
		if(sp[i] & 0x01)
		{
			DPRINTK("SP%d No Interrupt !!!\n",i);
		}
		else
		{
			switch(sp[i] & 0x0f)
			{
			case SERIAL_IIR_RLS: 
			    DPRINTK("Serial Port %d: Receiver status error or ",i);
			    DPRINTK("address bit detected in 9-bit mode\n");
			     mos7840_port->MsrLsr=1;
			     mos7840_get_reg(mos7840_port,wval,LINE_STATUS_REGISTER,&Data);
			     break;
            		case SERIAL_IIR_MS:  
			     DPRINTK("Serial Port %d: Modem status change\n",i);
			     mos7840_port->MsrLsr=0;	
			     mos7840_get_reg(mos7840_port,wval, MODEM_STATUS_REGISTER, &Data);	
			     break;
			}
		}
	}
	
	}
exit:
	if( mos7840_serial->status_polling_started == FALSE )
		return;

	result = usb_submit_urb (urb, GFP_ATOMIC);
	if (result) 
	{
		dev_err(&urb->dev->dev, "%s - Error %d submitting interrupt urb\n", __FUNCTION__, result);
	}

	return;

}
//#endif

static void mos7840_control_callback(struct urb *urb, struct pt_regs *regs)
{
	unsigned char *data;
	struct moschip_port *mos7840_port;
	__u8 regval=0x0;

	if(!urb)
        {
                DPRINTK("%s","Invalid Pointer !!!!:\n");
                return;
        }

        switch (urb->status)
        {
                case 0:
                        /* success */
                        break;
                case -ECONNRESET:
                case -ENOENT:
                case -ESHUTDOWN:
                        /* this urb is terminated, clean up */
                        dbg("%s - urb shutting down with status: %d", __FUNCTION__, urb->status);                        return;
                default:
                        dbg("%s - nonzero urb status received: %d", __FUNCTION__, urb->status);
                        goto exit;
        }

	
	mos7840_port = (struct moschip_port *)urb->context;
	
	DPRINTK("%s urb buffer size is %d\n",__FUNCTION__,urb->actual_length);
	DPRINTK("%s mos7840_port->MsrLsr is %d port %d\n",__FUNCTION__,mos7840_port->MsrLsr,mos7840_port->port_num);
	data=urb->transfer_buffer;
	regval=(__u8)data[0];
	DPRINTK("%s data is %x\n",__FUNCTION__,regval);
	if(mos7840_port->MsrLsr==0)
		handle_newMsr(mos7840_port,regval);
	else if(mos7840_port->MsrLsr==1)
		handle_newLsr(mos7840_port,regval);

exit:
	return;
}
int handle_newMsr(struct moschip_port *port,__u8 newMsr)
{
	struct moschip_port *mos7840_port;
	struct  async_icount *icount;
	mos7840_port=port;
	icount = &mos7840_port->icount;
	if (newMsr & (MOS_MSR_DELTA_CTS | MOS_MSR_DELTA_DSR | MOS_MSR_DELTA_RI | MOS_MSR_DELTA_CD)) {
	        icount = &mos7840_port->icount;

                /* update input line counters */
                if (newMsr & MOS_MSR_DELTA_CTS) {
                        icount->cts++;
                }
                if (newMsr & MOS_MSR_DELTA_DSR) {
                        icount->dsr++;
                }
                if (newMsr & MOS_MSR_DELTA_CD) {
                        icount->dcd++;
                }
                if (newMsr & MOS_MSR_DELTA_RI) {
                        icount->rng++;
                }
        }

	
	return 0;
}
int handle_newLsr(struct moschip_port *port,__u8 newLsr)
{
        struct  async_icount *icount;

        dbg("%s - %02x", __FUNCTION__, newLsr);


        if (newLsr & SERIAL_LSR_BI) {
                //
                // Parity and Framing errors only count if they
                // occur exclusive of a break being
                // received.
                //
                newLsr &= (__u8)(SERIAL_LSR_OE | SERIAL_LSR_BI);
        }


        /* update input line counters */
        icount = &port->icount;
        if (newLsr & SERIAL_LSR_BI) {
                icount->brk++;
        }
	if (newLsr & SERIAL_LSR_OE) {
                icount->overrun++;
        }
        if (newLsr & SERIAL_LSR_PE) {
                icount->parity++;
        }
        if (newLsr & SERIAL_LSR_FE) {
                icount->frame++;
        }


	return 0;
}
static int mos7840_get_reg(struct moschip_port *mcs,__u16 Wval, __u16 reg, __u16 * val)
{
        struct usb_device *dev = mcs->port->serial->dev;
        struct usb_ctrlrequest *dr=NULL;
        unsigned char  *buffer=NULL;
        int ret=0;
        buffer= (__u8 *)mcs->ctrl_buf;

//      dr=(struct usb_ctrlrequest *)(buffer);
        dr=(void *)(buffer + 2);
        dr->bRequestType = MCS_RD_RTYPE;
        dr->bRequest = MCS_RDREQ;
        dr->wValue = cpu_to_le16(Wval);//0;
        dr->wIndex = cpu_to_le16(reg);
        dr->wLength = cpu_to_le16(2);

        usb_fill_control_urb(mcs->control_urb,dev,usb_rcvctrlpipe(dev,0),(unsigned char *)dr,buffer,2,mos7840_control_callback,mcs);
        mcs->control_urb->transfer_buffer_length = 2;
        ret=usb_submit_urb(mcs->control_urb,GFP_ATOMIC);
        return ret;
}

/*****************************************************************************
 * mos7840_bulk_in_callback
 *	this is the callback function for when we have received data on the 
 *	bulk in endpoint.
 * Input : 1 Input
 *			pointer to the URB packet,
 *
 *****************************************************************************/
 
static void mos7840_bulk_in_callback (struct urb *urb, struct pt_regs *regs)
{
	int			status;
	unsigned char		*data ;
	struct usb_serial	*serial;
	struct usb_serial_port	*port;
	struct moschip_serial	*mos7840_serial;
	struct moschip_port	*mos7840_port;
	struct tty_struct *tty;
	int i;
	if(!urb)
	{
		DPRINTK("%s","Invalid Pointer !!!!:\n");
		return;
	}

	if (urb->status) 
	{
		DPRINTK("nonzero read bulk status received: %d",urb->status);
//		if(urb->status==84)
		//ThreadState=1;
		return;
	}

	mos7840_port= (struct moschip_port*)urb->context;
	if(!mos7840_port)
	{
		DPRINTK("%s","NULL mos7840_port pointer \n");
		return ;
	}

	port = (struct usb_serial_port *)mos7840_port->port;
	if (mos7840_port_paranoia_check (port, __FUNCTION__)) 
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return;
	}

	serial  = mos7840_get_usb_serial(port,__FUNCTION__);	
	if(!serial)
	{
		DPRINTK("%s\n","Bad serial pointer ");
		return;
	}

	DPRINTK("%s\n","Entering... \n");

	data = urb->transfer_buffer;
	mos7840_serial = mos7840_get_serial_private(serial);

	DPRINTK("%s","Entering ........... \n");

	if (urb->actual_length) 
	{
//MATRIX
	#if LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,15)
		// 2.6.17 Block
		tty = mos7840_port->port->tty;
		if (tty) 
		{
			tty_buffer_request_room(tty, urb->actual_length);
			tty_insert_flip_string(tty, data, urb->actual_length);
				DPRINTK(" %s \n",data);
			tty_flip_buffer_push(tty);
		}
		
	#endif

	#if LINUX_VERSION_CODE <KERNEL_VERSION(2,6,15)
		tty = mos7840_port->port->tty;
                if (tty)
                {
                        for (i = 0; i < urb->actual_length; ++i)
                        {
                        /* if we insert more than TTY_FLIPBUF_SIZE characters, we drop them. */
                                if(tty->flip.count >= TTY_FLIPBUF_SIZE)
                                {
                                                tty_flip_buffer_push(tty);
                                }
                        /* this doesn't actually push the data through unless tty->low_latency is set */
                                tty_insert_flip_char(tty, data[i], 0);
                                        DPRINTK(" %c \n",data[i]);
                        }
                                tty_flip_buffer_push(tty);
                }

	#endif
		mos7840_port->icount.rx += urb->actual_length;
		DPRINTK("mos7840_port->icount.rx is %d:\n",mos7840_port->icount.rx);
//MATRIX
	}

	if(!mos7840_port->read_urb)
	{
		DPRINTK("%s","URB KILLED !!!\n");
		return;
	}

	if(mos7840_port->read_urb->status!=-EINPROGRESS)
	{
		mos7840_port->read_urb->dev = serial->dev;

		status = usb_submit_urb(mos7840_port->read_urb, GFP_ATOMIC);

		if (status) 
		{
			DPRINTK(" usb_submit_urb(read bulk) failed, status = %d", status);
		}
	}
}

/*****************************************************************************
 * mos7840_bulk_out_data_callback
 *	this is the callback function for when we have finished sending serial data
 *	on the bulk out endpoint.
 * Input : 1 Input
 *			pointer to the URB packet,
 *
 *****************************************************************************/

static void mos7840_bulk_out_data_callback (struct urb *urb, struct pt_regs *regs)
{
	struct moschip_port *mos7840_port ;
	struct tty_struct *tty;
	if(!urb)
	{
		DPRINTK("%s","Invalid Pointer !!!!:\n");
		return;
	}

	if (urb->status) 
	{
		DPRINTK("nonzero write bulk status received:%d\n", urb->status);
		return;
	}

	mos7840_port = (struct moschip_port *)urb->context;
	if(!mos7840_port)
	{
		DPRINTK("%s","NULL mos7840_port pointer \n");
		return ;
	}

	if (mos7840_port_paranoia_check (mos7840_port->port, __FUNCTION__)) 
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return;
	}

	DPRINTK("%s \n","Entering .........");

	tty = mos7840_port->port->tty;

	if (tty && mos7840_port->open) 
	{
		/* let the tty driver wakeup if it has a special *
		 * write_wakeup function 			 */

		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
			(tty->ldisc.write_wakeup)(tty);
		}

		/* tell the tty driver that something has changed */
		wake_up_interruptible(&tty->write_wait);
	}

	/* Release the Write URB */
	mos7840_port->write_in_progress = FALSE;

//schedule_work(&mos7840_port->port->work);
	
}






/************************************************************************/
/*       D R I V E R  T T Y  I N T E R F A C E  F U N C T I O N S       */
/************************************************************************/
#ifdef MCSSerialProbe
static int mos7840_serial_probe(struct usb_serial *serial, const struct usb_device_id *id)
{

	/*need to implement the mode_reg reading and updating\
			 structures usb_serial_ device_type\
			(i.e num_ports, num_bulkin,bulkout etc)*/
	/* Also we can update the changes  attach */
	return 1;
}
#endif

/*****************************************************************************
 * SerialOpen
 *	this function is called by the tty driver when a port is opened
 *	If successful, we return 0
 *	Otherwise we return a negative error number.
 *****************************************************************************/

static int mos7840_open (struct usb_serial_port *port, struct file * filp)
{
    int response;
    int j;
    struct usb_serial *serial;
//    struct usb_serial_port *port0;
    struct urb *urb;
    __u16 Data;
    int status;		
    struct moschip_serial *mos7840_serial;
    struct moschip_port *mos7840_port;
	struct termios tmp_termios;                                                                                                                         
	if (mos7840_port_paranoia_check (port, __FUNCTION__))
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return -ENODEV;
	}

	//mos7840_serial->NoOfOpenPorts++;
	serial = port->serial;

	if (mos7840_serial_paranoia_check (serial, __FUNCTION__))
	{
		DPRINTK("%s","Serial Paranoia failed \n");
		return -ENODEV;
	}

	mos7840_port = mos7840_get_port_private(port); 

	if (mos7840_port == NULL)
		return -ENODEV;
/*
	if (mos7840_port->ctrl_buf==NULL)
        {
                mos7840_port->ctrl_buf = kmalloc(16,GFP_KERNEL);
                if (mos7840_port->ctrl_buf == NULL) {
                        printk(", Can't allocate ctrl buff\n");
                        return -ENOMEM;
                }

        }
	
	if(!mos7840_port->control_urb)
	{
	mos7840_port->control_urb=kmalloc(sizeof(struct urb),GFP_KERNEL);
	}
*/
//	port0 = serial->port[0];

	mos7840_serial = mos7840_get_serial_private(serial);

	if (mos7840_serial == NULL )//|| port0 == NULL) 
	{
		return -ENODEV;
	}
	// increment the number of openports counter here
	mos7840_serial->NoOfOpenPorts++;
	//printk("the num of ports opend is:%d\n",mos7840_serial->NoOfOpenPorts);


     	usb_clear_halt(serial->dev, port->write_urb->pipe);
     	usb_clear_halt(serial->dev, port->read_urb->pipe);
	
		 /* Initialising the write urb pool */
                 for (j = 0; j < NUM_URBS; ++j)
                 {
                        urb = usb_alloc_urb(0,SLAB_ATOMIC);
                        mos7840_port->write_urb_pool[j] = urb;
                                                                                                                             
                        if (urb == NULL)
                        {
                                err("No more urbs???");
                                continue;
                        }
                                                                                                                                                                                                                                                           
                        urb->transfer_buffer = NULL;
                        urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);
                        if (!urb->transfer_buffer)
                        {
                                err("%s-out of memory for urb buffers.", __FUNCTION__);
                                continue;
                        }
                }


/*****************************************************************************
 * Initialize MCS7840 -- Write Init values to corresponding Registers
 *
 * Register Index
 * 1 : IER
 * 2 : FCR
 * 3 : LCR
 * 4 : MCR
 *
 * 0x08 : SP1/2 Control Reg
 *****************************************************************************/

//NEED to check the fallowing Block

	status=0;
	Data=0x0;
	status=mos7840_get_reg_sync(port,mos7840_port->SpRegOffset,&Data);
	if(status<0){
		DPRINTK("Reading Spreg failed\n");
		return -1;
	}
	Data |= 0x80;
	status = mos7840_set_reg_sync(port,mos7840_port->SpRegOffset,Data);
	if(status<0){
		DPRINTK("writing Spreg failed\n");
		return -1;
	}
	
	Data &= ~0x80;
	status = mos7840_set_reg_sync(port,mos7840_port->SpRegOffset,Data);
	if(status<0){
		DPRINTK("writing Spreg failed\n");
		return -1;
	}

	
//End of block to be checked	
//**************************CHECK***************************//

		if(RS485mode==1)
                       Data = 0xC0;
	       else
                        Data = 0x00;
                status=0;
                status=mos7840_set_Uart_Reg(port,SCRATCH_PAD_REGISTER,Data);
                if(status<0) {
                        DPRINTK("Writing SCRATCH_PAD_REGISTER failed status-0x%x\n", status);
                        return -1;
                }
                else DPRINTK("SCRATCH_PAD_REGISTER Writing success status%d\n",status);


//**************************CHECK***************************//
		
	status=0;
	Data=0x0;
	status=mos7840_get_reg_sync(port,mos7840_port->ControlRegOffset,&Data);
	if(status<0){
		DPRINTK("Reading Controlreg failed\n");
		return -1;
	}
	Data |= 0x08;//Driver done bit
	/*
	status = mos7840_set_reg_sync(port,mos7840_port->ControlRegOffset,Data);
	if(status<0){
		DPRINTK("writing Controlreg failed\n");
		return -1;
	}
	*/
	Data |= 0x20;//rx_disable
	status=0;
	status = mos7840_set_reg_sync(port,mos7840_port->ControlRegOffset,Data);
	if(status<0){
		DPRINTK("writing Controlreg failed\n");
		return -1;
	}

	//do register settings here
	// Set all regs to the device default values.
	////////////////////////////////////
	// First Disable all interrupts.
	////////////////////////////////////
	
	Data = 0x00;
	status=0;
	status = mos7840_set_Uart_Reg(port,INTERRUPT_ENABLE_REGISTER,Data);
	if(status<0){
		DPRINTK("disableing interrupts failed\n");
		return -1;
	}
	 // Set FIFO_CONTROL_REGISTER to the default value 
	Data = 0x00;
	status=0;
	status = mos7840_set_Uart_Reg(port,FIFO_CONTROL_REGISTER,Data);
	if(status<0){
		DPRINTK("Writing FIFO_CONTROL_REGISTER  failed\n");
		return -1;
	}

	Data = 0xcf;  //chk
	status=0;
	status = mos7840_set_Uart_Reg(port,FIFO_CONTROL_REGISTER,Data);
	if(status<0){
		DPRINTK("Writing FIFO_CONTROL_REGISTER  failed\n");
		return -1;
	}

	Data = 0x03; //LCR_BITS_8
	status=0;
	status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	mos7840_port->shadowLCR=Data;

	Data = 0x0b; // MCR_DTR|MCR_RTS|MCR_MASTER_IE
	status=0;
	status = mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
	mos7840_port->shadowMCR=Data;

#ifdef Check
	Data = 0x00;
	status=0;
	status = mos7840_get_Uart_Reg(port,LINE_CONTROL_REGISTER,&Data);
	mos7840_port->shadowLCR=Data;

	Data |= SERIAL_LCR_DLAB; //data latch enable in LCR 0x80
	status = 0;
	status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	
	Data = 0x0c;
	status=0;
	status = mos7840_set_Uart_Reg(port,DIVISOR_LATCH_LSB,Data);
	
	Data = 0x0;
	status=0;
	status = mos7840_set_Uart_Reg(port,DIVISOR_LATCH_MSB,Data);

	Data = 0x00;
	status=0;
	status = mos7840_get_Uart_Reg(port,LINE_CONTROL_REGISTER,&Data);

//	Data = mos7840_port->shadowLCR; //data latch disable
	Data = Data & ~SERIAL_LCR_DLAB;
	status = 0;
	status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	mos7840_port->shadowLCR=Data;
#endif
	//clearing Bulkin and Bulkout Fifo
	Data = 0x0;
	status = 0;
	status = mos7840_get_reg_sync(port,mos7840_port->SpRegOffset,&Data);
	
	Data = Data | 0x0c;
	status = 0;
        status = mos7840_set_reg_sync(port,mos7840_port->SpRegOffset,Data);
	  
	Data = Data & ~0x0c;
	status = 0;
        status = mos7840_set_reg_sync(port,mos7840_port->SpRegOffset,Data);
	//Finally enable all interrupts
	Data = 0x0;
	Data = 0x0c;
	status = 0;
        status = mos7840_set_Uart_Reg(port,INTERRUPT_ENABLE_REGISTER,Data);

	//clearing rx_disable
	Data = 0x0;
	status = 0;
        status = mos7840_get_reg_sync(port,mos7840_port->ControlRegOffset,&Data);
	Data = Data & ~0x20;
	status = 0;
        status = mos7840_set_reg_sync(port,mos7840_port->ControlRegOffset,Data);

	// rx_negate
	Data = 0x0;
	status = 0;
        status = mos7840_get_reg_sync(port,mos7840_port->ControlRegOffset,&Data);
	Data = Data |0x10;
	status = 0;
        status = mos7840_set_reg_sync(port,mos7840_port->ControlRegOffset,Data);


	/* force low_latency on so that our tty_push actually forces *
	 * the data through,otherwise it is scheduled, and with      *
	 * high data rates (like with OHCI) data can get lost.       */
        
	if (port->tty)
		port->tty->low_latency = 1;                                                                                                                    
/* 
	printk("port number is %d \n",port->number);
	printk("serial number is %d \n",port->serial->minor);
	printk("Bulkin endpoint is %d \n",port->bulk_in_endpointAddress);
	printk("BulkOut endpoint is %d \n",port->bulk_out_endpointAddress);
	printk("Interrupt endpoint is %d \n",port->interrupt_in_endpointAddress);
	printk("port's number in the device is %d\n",mos7840_port->port_num);
*/	
////////////////////////
//#ifdef CheckStatusPipe
/* Check to see if we've set up our endpoint info yet    *
     * (can't set it up in mos7840_startup as the structures *
     * were not set up at that time.)                        */
if(mos7840_serial->NoOfOpenPorts==1)
{
    	// start the status polling here
    	mos7840_serial->status_polling_started = TRUE;
	//if (mos7840_serial->interrupt_in_buffer == NULL)
       // {
		/* If not yet set, Set here */
                mos7840_serial->interrupt_in_buffer = serial->port[0]->interrupt_in_buffer;
         mos7840_serial->interrupt_in_endpoint = serial->port[0]->interrupt_in_endpointAddress;
		//printk(" interrupt endpoint:%d \n",mos7840_serial->interrupt_in_endpoint);
                mos7840_serial->interrupt_read_urb = serial->port[0]->interrupt_in_urb;

                /* set up interrupt urb */
	        usb_fill_int_urb(                                   \
                        mos7840_serial->interrupt_read_urb,     \
                        serial->dev,                            \
                        usb_rcvintpipe(serial->dev,mos7840_serial->interrupt_in_endpoint),    \
                        mos7840_serial->interrupt_in_buffer,             \
                        mos7840_serial->interrupt_read_urb->transfer_buffer_length,\
		        mos7840_interrupt_callback, mos7840_serial,     \
                        mos7840_serial->interrupt_read_urb->interval  );

	/* start interrupt read for mos7840               *
                 * will continue as long as mos7840 is connected  */

               response = usb_submit_urb (mos7840_serial->interrupt_read_urb,GFP_KERNEL);
                if (response)
                {
                        DPRINTK("%s - Error %d submitting interrupt urb", __FUNCTION__, response);
                }
	//	else
			// printk(" interrupt URB submitted\n"); 

        //}

}
//#endif


///////////////////////
    	/* see if we've set up our endpoint info yet   *
	 * (can't set it up in mos7840_startup as the  *
	 * structures were not set up at that time.)   */

	DPRINTK("port number is %d \n",port->number);
	DPRINTK("serial number is %d \n",port->serial->minor);
	DPRINTK("Bulkin endpoint is %d \n",port->bulk_in_endpointAddress);
	DPRINTK("BulkOut endpoint is %d \n",port->bulk_out_endpointAddress);
	DPRINTK("Interrupt endpoint is %d \n",port->interrupt_in_endpointAddress);
	DPRINTK("port's number in the device is %d\n",mos7840_port->port_num);
	mos7840_port->bulk_in_buffer    = port->bulk_in_buffer;
    	mos7840_port->bulk_in_endpoint  = port->bulk_in_endpointAddress;
	mos7840_port->read_urb          = port->read_urb;
	mos7840_port->bulk_out_endpoint = port->bulk_out_endpointAddress;


	/* set up our bulk in urb */
	if((mos7840_serial->mos7840_spectrum_2or4ports==2)&&(((__u16)port->number - (__u16)(port->serial->minor)) != 0))
        {
        usb_fill_bulk_urb(
                mos7840_port->read_urb,serial->dev,\
	        usb_rcvbulkpipe(serial->dev, (port->bulk_in_endpointAddress+2)),\
  		 port->bulk_in_buffer,\
                mos7840_port->read_urb->transfer_buffer_length,         \
                mos7840_bulk_in_callback,mos7840_port);
        }
        else
	usb_fill_bulk_urb(  
		mos7840_port->read_urb, 				\
		serial->dev,						\
		usb_rcvbulkpipe(serial->dev, port->bulk_in_endpointAddress),\
		port->bulk_in_buffer,					\
		mos7840_port->read_urb->transfer_buffer_length,		\
		mos7840_bulk_in_callback,mos7840_port);

	DPRINTK("mos7840_open: bulkin endpoint is %d\n",port->bulk_in_endpointAddress);	
	response = usb_submit_urb (mos7840_port->read_urb,GFP_KERNEL);
	if (response)
        {
                err("%s - Error %d submitting control urb", __FUNCTION__, response);
        }

        /* initialize our wait queues */
        init_waitqueue_head(&mos7840_port->wait_open);
        init_waitqueue_head(&mos7840_port->wait_chase);
        init_waitqueue_head(&mos7840_port->delta_msr_wait);
        init_waitqueue_head(&mos7840_port->wait_command);

        /* initialize our icount structure */
        memset (&(mos7840_port->icount), 0x00, sizeof(mos7840_port->icount));

        /* initialize our port settings */
        mos7840_port->shadowMCR  = MCR_MASTER_IE; /* Must set to enable ints! */
        mos7840_port->chaseResponsePending = FALSE; 
        /* send a open port command */
        mos7840_port->openPending = FALSE;
        mos7840_port->open        = TRUE; 
	//mos7840_change_port_settings(mos7840_port,old_termios);
	/* Setup termios */
        if (port->tty) {
                mos7840_set_termios (port, &tmp_termios);
        }
        mos7840_port->rxBytesAvail = 0x0;
	mos7840_port->icount.tx=0;
	mos7840_port->icount.rx=0;

	 DPRINTK("\n\nusb_serial serial:%x       mos7840_port:%x\nmos7840_serial:%x      usb_serial_port port:%x\n\n",(unsigned int)serial,(unsigned int)mos7840_port,(unsigned int)mos7840_serial,(unsigned int)port);




        return 0;

}


/*****************************************************************************
 * mos7840_close
 *	this function is called by the tty driver when a port is closed
 *****************************************************************************/

static void mos7840_close (struct usb_serial_port *port, struct file * filp)
{
	struct usb_serial *serial;
	struct moschip_serial *mos7840_serial;
	struct moschip_port *mos7840_port;
	int	no_urbs;
	__u16	Data;
	//__u16   Data1= 20;
	
	DPRINTK("%s\n","mos7840_close:entering...");
	/* MATRIX  */
	//ThreadState = 1;
	/* MATRIX  */
	//printk("Entering... :mos7840_close\n");
	if (mos7840_port_paranoia_check (port, __FUNCTION__))
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return;
	}
	serial = mos7840_get_usb_serial (port, __FUNCTION__);
	if (!serial)
	{
		DPRINTK("%s","Serial Paranoia failed \n");
		return;
	}
	// take the Adpater and port's private data
	mos7840_serial = mos7840_get_serial_private(serial);
	mos7840_port = mos7840_get_port_private(port);
	if ((mos7840_serial == NULL) || (mos7840_port == NULL))
	{
		return;
	}
	if (serial->dev) 
	{
		/* flush and block(wait) until tx is empty*/
		mos7840_block_until_tx_empty(mos7840_port);
	}
	// kill the ports URB's
	for (no_urbs = 0; no_urbs < NUM_URBS;no_urbs++)
	#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
 		usb_kill_urb (mos7840_port->write_urb_pool[no_urbs]);
	#endif
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
		usb_unlink_urb (mos7840_port->write_urb_pool[no_urbs]);
	#endif
	/* Freeing Write URBs*/
	for (no_urbs = 0; no_urbs< NUM_URBS; ++no_urbs)
	{
        	if (mos7840_port->write_urb_pool[no_urbs])
                {
                	if (mos7840_port->write_urb_pool[no_urbs]->transfer_buffer)
                        	kfree(mos7840_port->write_urb_pool[no_urbs]->transfer_buffer);
                	usb_free_urb (mos7840_port->write_urb_pool[no_urbs]);
                }
        }
	/* While closing port, shutdown all bulk read, write  *
	 * and interrupt read if they exists                  */
	if (serial->dev)
	{
		if (mos7840_port->write_urb) 
		{
			DPRINTK("%s","Shutdown bulk write\n");
		#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
			usb_kill_urb (mos7840_port->write_urb);
		#endif
		#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
			usb_unlink_urb (mos7840_port->write_urb);
		#endif
		}
		if (mos7840_port->read_urb) 
		{
			DPRINTK("%s","Shutdown bulk read\n");
		#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
			usb_kill_urb (mos7840_port->read_urb);
		#endif
		#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
			usb_unlink_urb (mos7840_port->read_urb);
		#endif
		}
		if((&mos7840_port->control_urb))
		{
			DPRINTK("%s","Shutdown control read\n");
		//	usb_kill_urb (mos7840_port->control_urb);

		}
	}
	//if(mos7840_port->ctrl_buf != NULL)
  	//kfree(mos7840_port->ctrl_buf);
	// decrement the no.of open ports counter of an individual USB-serial adapter.
	mos7840_serial->NoOfOpenPorts--;
        DPRINTK("NoOfOpenPorts in close%d:in port%d\n", mos7840_serial->NoOfOpenPorts, port->number );	
        //printk("the num of ports opend is:%d\n",mos7840_serial->NoOfOpenPorts); 
	if(mos7840_serial->NoOfOpenPorts==0)
	{	
		//stop the stus polling here
		//printk("disabling the status polling flag to FALSE :\n");
		mos7840_serial->status_polling_started = FALSE;
		if(mos7840_serial->interrupt_read_urb) 
		{
			DPRINTK("%s","Shutdown interrupt_read_urb\n");
			//mos7840_serial->interrupt_in_buffer=NULL;
			//usb_kill_urb (mos7840_serial->interrupt_read_urb); 
		}
	}
	if (mos7840_port->write_urb) 
	{
		/* if this urb had a transfer buffer already (old tx) free it */
		if (mos7840_port->write_urb->transfer_buffer != NULL) 
		{
			kfree(mos7840_port->write_urb->transfer_buffer);
		}
		usb_free_urb(mos7840_port->write_urb);
	}
	// clear the MCR & IER
	Data = 0x00;
	mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
	Data = 0x00;
	mos7840_set_Uart_Reg(port,INTERRUPT_ENABLE_REGISTER,Data);
	
	//mos7840_get_Uart_Reg(port,MODEM_CONTROL_REGISTER,&Data1);
	//printk("value of MCR after closing the port is : 0x%x\n",Data1);
 
	mos7840_port->open         = FALSE;
	mos7840_port->closePending = FALSE;
	mos7840_port->openPending  = FALSE;
	DPRINTK("%s \n","Leaving ............");
	//printk("Leaving... :mos7840_close\n");

}   


/*****************************************************************************
 * SerialBreak
 *	this function sends a break to the port
 *****************************************************************************/
static void mos7840_break (struct usb_serial_port *port, int break_state)
{
        unsigned char data;
	struct usb_serial *serial;
	struct moschip_serial *mos7840_serial;
	struct moschip_port *mos7840_port;
	
	DPRINTK("%s \n","Entering ...........");
	DPRINTK("mos7840_break: Start\n");

	if (mos7840_port_paranoia_check (port, __FUNCTION__))
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return;
	}
		 
	serial = mos7840_get_usb_serial (port, __FUNCTION__);
	if (!serial)
	{
		DPRINTK("%s","Serial Paranoia failed \n");
		return;
	}

	mos7840_serial = mos7840_get_serial_private(serial);
	mos7840_port = mos7840_get_port_private(port);
	
	if ((mos7840_serial == NULL) || (mos7840_port == NULL))
	{
		return;
	}
	
	/* flush and chase */
	mos7840_port->chaseResponsePending = TRUE;

	if (serial->dev) 
	{

		/* flush and block until tx is empty*/
		mos7840_block_until_chase_response(mos7840_port);
	}

        if(break_state == -1)
        {
                data = mos7840_port->shadowLCR | LCR_SET_BREAK;
        }
        else
        {
                data = mos7840_port->shadowLCR & ~LCR_SET_BREAK;
        }

        mos7840_port->shadowLCR  = data;
	DPRINTK("mcs7840_break mos7840_port->shadowLCR is %x\n",mos7840_port->shadowLCR);
	mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,mos7840_port->shadowLCR);

	return;
}


/************************************************************************
 *
 * mos7840_block_until_chase_response
 *
 *	This function will block the close until one of the following:
 *		1. Response to our Chase comes from mos7840
 *		2. A timout of 10 seconds without activity has expired
 *		   (1K of mos7840 data @ 2400 baud ==> 4 sec to empty)
 *
 ************************************************************************/

static void mos7840_block_until_chase_response(struct moschip_port *mos7840_port)
{
	int timeout = 1*HZ;
	int wait = 10;
	int count ;


	while (1) 
	{
		count = mos7840_chars_in_buffer(mos7840_port->port);
		
		/* Check for Buffer status */
		if(count<=0)
		{
			mos7840_port->chaseResponsePending = FALSE;
			return;
		}

		/* Block the thread for a while */
		interruptible_sleep_on_timeout (&mos7840_port->wait_chase, timeout);
                /* No activity.. count down section */
		wait--;
		if (wait == 0) 
		{
			dbg("%s - TIMEOUT", __FUNCTION__);
			return;
		}
		else 
		{
	                /* Reset timout value back to seconds */	
			wait = 10;
		}
	}

}


/************************************************************************
 *
 * mos7840_block_until_tx_empty
 *
 *	This function will block the close until one of the following:
 *		1. TX count are 0
 *		2. The mos7840 has stopped
 *		3. A timout of 3 seconds without activity has expired
 *
 ************************************************************************/
static void mos7840_block_until_tx_empty (struct moschip_port *mos7840_port)
{
	int timeout = HZ/10;
	int wait = 30;
	int count;

	while (1) 
	{

		count = mos7840_chars_in_buffer(mos7840_port->port);

                /* Check for Buffer status */
		if(count<=0)
		{
			return;
		}

                /* Block the thread for a while */
		interruptible_sleep_on_timeout (&mos7840_port->wait_chase, timeout);

                /* No activity.. count down section */
		wait--;
		if (wait == 0) 
		{
			dbg("%s - TIMEOUT", __FUNCTION__);
			return;
		}
		else 
		{
	                /* Reset timout value back to seconds */
			wait = 30;
		}
	}
}

/*****************************************************************************
 * mos7840_write_room
 *	this function is called by the tty driver when it wants to know how many
 *	bytes of data we can accept for a specific port.
 *	If successful, we return the amount of room that we have for this port
 *	Otherwise we return a negative error number.
 *****************************************************************************/

static int mos7840_write_room (struct usb_serial_port *port)
{
    int i;
    int room = 0;
    struct moschip_port *mos7840_port;
	

//	DPRINTK("%s \n"," mos7840_write_room:entering ...........");

        if(mos7840_port_paranoia_check(port,__FUNCTION__) )
        {
                DPRINTK("%s","Invalid port \n");
                DPRINTK("%s \n"," mos7840_write_room:leaving ...........");
                return -1;
        }

        mos7840_port = mos7840_get_port_private(port);
        if (mos7840_port == NULL)
        {
                DPRINTK("%s \n","mos7840_break:leaving ...........");
                return -1;
        }
                                                                                                                             
        for (i = 0; i < NUM_URBS; ++i) 
	{
                if (mos7840_port->write_urb_pool[i]->status != -EINPROGRESS) 
		{
                        room += URB_TRANSFER_BUFFER_SIZE;
                }
        }
       
        dbg("%s - returns %d", __FUNCTION__, room);
        return (room);

}


/*****************************************************************************
 * mos7840_chars_in_buffer
 *	this function is called by the tty driver when it wants to know how many
 *	bytes of data we currently have outstanding in the port (data that has
 *	been written, but hasn't made it out the port yet)
 *	If successful, we return the number of bytes left to be written in the 
 *	system, 
 *	Otherwise we return a negative error number.
 *****************************************************************************/

static int mos7840_chars_in_buffer (struct usb_serial_port *port)
{
	int i;
    	int chars = 0;
 	struct moschip_port *mos7840_port;

	//DPRINTK("%s \n"," mos7840_chars_in_buffer:entering ...........");

	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return -1;
	}
                                                                                                                             
        mos7840_port = mos7840_get_port_private(port);
        if (mos7840_port == NULL)
        {
                DPRINTK("%s \n","mos7840_break:leaving ...........");
                return -1;
        }
                                                                                                                       
    	for (i = 0; i < NUM_URBS; ++i) 
    	{
        	if (mos7840_port->write_urb_pool[i]->status == -EINPROGRESS) 
	    	{
                	chars += URB_TRANSFER_BUFFER_SIZE;
            	}
    	}                                                                                                                         
    	dbg("%s - returns %d", __FUNCTION__, chars);
    	return (chars);

}


/*****************************************************************************
 * SerialWrite
 *	this function is called by the tty driver when data should be written to
 *	the port.
 *	If successful, we return the number of bytes written, otherwise we 
 *      return a negative error number.
 *****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
static int mos7840_write (struct usb_serial_port *port, const unsigned char *data, int count)
#else
static int mos7840_write (struct usb_serial_port *port, int from_user, const unsigned char *data, int count)
#endif
{
	int status;
	int i;
	int bytes_sent = 0;
	int transfer_size;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
	int from_user=0;
	#endif

	struct moschip_port *mos7840_port;
	struct usb_serial *serial;
	struct moschip_serial *mos7840_serial;
	struct urb    *urb;
	//__u16 Data;
	const unsigned char *current_position = data;
	unsigned char * data1;
	DPRINTK("%s \n","entering ...........");
	//DPRINTK("mos7840_write: mos7840_port->shadowLCR is %x\n",mos7840_port->shadowLCR);

	#ifdef NOTMOS7840
	Data = 0x00;
        status=0;
        status = mos7840_get_Uart_Reg(port,LINE_CONTROL_REGISTER,&Data);
	mos7840_port->shadowLCR = Data;
	DPRINTK("mos7840_write: LINE_CONTROL_REGISTER is %x\n",Data);
	DPRINTK("mos7840_write: mos7840_port->shadowLCR is %x\n",mos7840_port->shadowLCR);
	
	//Data = 0x03;
        //status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
        //mos7840_port->shadowLCR=Data;//Need to add later
	
        Data |= SERIAL_LCR_DLAB; //data latch enable in LCR 0x80
        status = 0;
        status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);

	//Data = 0x0c;
        //status = mos7840_set_Uart_Reg(port,DIVISOR_LATCH_LSB,Data);
        Data = 0x00;
        status=0;
        status = mos7840_get_Uart_Reg(port,DIVISOR_LATCH_LSB,&Data);
	DPRINTK("mos7840_write:DLL value is %x\n",Data);

        Data = 0x0;
        status=0;
        status = mos7840_get_Uart_Reg(port,DIVISOR_LATCH_MSB,&Data);
	DPRINTK("mos7840_write:DLM value is %x\n",Data);

        Data = Data & ~SERIAL_LCR_DLAB;
	DPRINTK("mos7840_write: mos7840_port->shadowLCR is %x\n",mos7840_port->shadowLCR);
        status = 0;
        status = mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	#endif
	
	if (mos7840_port_paranoia_check (port, __FUNCTION__))
	{
		DPRINTK("%s","Port Paranoia failed \n");
		return -1;
	}

	serial = port->serial;
	if (mos7840_serial_paranoia_check (serial, __FUNCTION__))
	{
		DPRINTK("%s","Serial Paranoia failed \n");
		return -1;
	}

	mos7840_port = mos7840_get_port_private(port);
	if(mos7840_port==NULL)
	{
		DPRINTK("%s","mos7840_port is NULL\n");
		return -1;
	}

	mos7840_serial =mos7840_get_serial_private(serial);
	if(mos7840_serial==NULL)
	{
		DPRINTK("%s","mos7840_serial is NULL \n");
		return -1;
	}

	
        	/* try to find a free urb in the list */
        	urb = NULL;

        	for (i = 0; i < NUM_URBS; ++i) 
		{
                	if (mos7840_port->write_urb_pool[i]->status != -EINPROGRESS) 
			{
                        	urb = mos7840_port->write_urb_pool[i];
				DPRINTK("\nURB:%d",i);
                        	break;
                	}
        	}

	        if (urb == NULL) 
		{
                	dbg("%s - no more free urbs", __FUNCTION__);
                	goto exit;
        	}

        	if (urb->transfer_buffer == NULL) 
		{
          	urb->transfer_buffer = kmalloc (URB_TRANSFER_BUFFER_SIZE, GFP_KERNEL);

                	if (urb->transfer_buffer == NULL) 
			{
                        	err("%s no more kernel memory...", __FUNCTION__);
                        	goto exit;
                	}
        	}                                                                                                                   
        	transfer_size = min (count, URB_TRANSFER_BUFFER_SIZE);

        	if (from_user) 
		{
                	if (copy_from_user (urb->transfer_buffer, current_position, transfer_size)) 	{
                        	bytes_sent = -EFAULT;
	                       	goto exit;
            		}
        	} 
		else 
		{
                	memcpy (urb->transfer_buffer, current_position, transfer_size);
        	}                                                                                                                  
        	//usb_serial_debug_data (__FILE__, __FUNCTION__, transfer_size, urb->transfer_buffer);

		/* fill urb with data and submit  */
	if((mos7840_serial->mos7840_spectrum_2or4ports==2)&&(((__u16)port->number - (__u16)(port->serial->minor)) != 0))
        {
                usb_fill_bulk_urb (urb,
                 mos7840_serial->serial->dev,
                 usb_sndbulkpipe(mos7840_serial->serial->dev,
                 (port->bulk_out_endpointAddress)+2),
                 urb->transfer_buffer,
                 transfer_size,
                 mos7840_bulk_out_data_callback,
                 mos7840_port);
        }
        else



           	usb_fill_bulk_urb (urb,
		 mos7840_serial->serial->dev,
		 usb_sndbulkpipe(mos7840_serial->serial->dev,
		 port->bulk_out_endpointAddress),
		 urb->transfer_buffer, 
		 transfer_size,
		 mos7840_bulk_out_data_callback,
		 mos7840_port);

	data1=urb->transfer_buffer;
	DPRINTK("\nbulkout endpoint is %d",port->bulk_out_endpointAddress);
	//for(i=0;i < urb->actual_length;i++)
	//	DPRINTK("Data is %c\n ",data1[i]);

        	/* send it down the pipe */
        	status = usb_submit_urb(urb,GFP_ATOMIC);

        	if (status) 
		{
                	err("%s - usb_submit_urb(write bulk) failed with status = %d", __FUNCTION__, status);
                	bytes_sent = status;
			goto exit;
        	}
        	bytes_sent = transfer_size;
    	 	mos7840_port->icount.tx += transfer_size;
		DPRINTK("mos7840_port->icount.tx is %d:\n",mos7840_port->icount.tx);
exit:
	
	return bytes_sent;

}


/*****************************************************************************
 * SerialThrottle
 *	this function is called by the tty driver when it wants to stop the data
 *	being read from the port.
 *****************************************************************************/

static void mos7840_throttle (struct usb_serial_port *port)
{
	struct moschip_port *mos7840_port;
	struct tty_struct *tty;
	int status;

	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return;
	}

	DPRINTK("- port %d\n", port->number);

	mos7840_port = mos7840_get_port_private(port); 

	if (mos7840_port == NULL)
		return;

	if (!mos7840_port->open) 
	{
		DPRINTK("%s\n","port not opened");
		return;
	}

	DPRINTK("%s","Entering .......... \n");

	tty = port->tty;
	if (!tty) 
	{
		dbg ("%s - no tty available", __FUNCTION__);
		return;
	}

	/* if we are implementing XON/XOFF, send the stop character */
	if (I_IXOFF(tty)) 
	{
		unsigned char stop_char = STOP_CHAR(tty);
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
		status = mos7840_write (port, &stop_char, 1); //FC4
	#endif
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
		status = mos7840_write (port,0, &stop_char, 1);
	#endif
		if (status <= 0) 
		{
			return;
		}
	}

	/* if we are implementing RTS/CTS, toggle that line */
	if (tty->termios->c_cflag & CRTSCTS) 
	{
		mos7840_port->shadowMCR &= ~MCR_RTS;
		status=0;
		status=mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,mos7840_port->shadowMCR);

		if (status < 0) 
		{
			return;
		}
	}

	return;
}


/*****************************************************************************
 * mos7840_unthrottle
 *	this function is called by the tty driver when it wants to resume the data
 *	being read from the port (called after SerialThrottle is called)
 *****************************************************************************/
static void mos7840_unthrottle (struct usb_serial_port *port)
{
	struct tty_struct *tty;
	int status;
	struct moschip_port *mos7840_port = mos7840_get_port_private(port); 

	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return;
	}

	if (mos7840_port == NULL)
		return;

	if (!mos7840_port->open) {
		dbg("%s - port not opened", __FUNCTION__);
		return;
	}

	DPRINTK("%s","Entering .......... \n");

	tty = port->tty;
	if (!tty) 
	{
		dbg ("%s - no tty available", __FUNCTION__);
		return;
	}

	/* if we are implementing XON/XOFF, send the start character */
	if (I_IXOFF(tty)) 
	{
		unsigned char start_char = START_CHAR(tty);
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
		status = mos7840_write (port, &start_char, 1); //FC4
	#endif
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
		status = mos7840_write (port,0, &start_char, 1);
	#endif
		if (status <= 0) 
		{
			return;
		}
	}

	/* if we are implementing RTS/CTS, toggle that line */
	if (tty->termios->c_cflag & CRTSCTS) 
	{
		mos7840_port->shadowMCR |= MCR_RTS;
		status=0;
		status=mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,mos7840_port->shadowMCR);
		if (status < 0) 
		{
			return;
		}
	}

	return;
}


static int mos7840_tiocmget(struct usb_serial_port *port, struct file *file)
{
        //struct ti_port *tport = usb_get_serial_port_data(port);
	struct moschip_port *mos7840_port;
        unsigned int result;
        __u16 msr;
        __u16 mcr;
        //unsigned int mcr;
	int status=0;
	mos7840_port = mos7840_get_port_private(port); 

        DPRINTK("%s - port %d", __FUNCTION__, port->number);

        if (mos7840_port == NULL)
                return -ENODEV;

	status=mos7840_get_Uart_Reg(port,MODEM_STATUS_REGISTER,&msr);
	status=mos7840_get_Uart_Reg(port,MODEM_CONTROL_REGISTER,&mcr);
//        mcr = mos7840_port->shadowMCR;
// COMMENT2: the Fallowing three line are commented for updating only MSR values
        result = ((mcr & MCR_DTR) ? TIOCM_DTR : 0)
                | ((mcr & MCR_RTS) ? TIOCM_RTS : 0)
                | ((mcr & MCR_LOOPBACK) ? TIOCM_LOOP : 0)
         	| ((msr & MOS7840_MSR_CTS) ? TIOCM_CTS : 0)
                | ((msr & MOS7840_MSR_CD) ? TIOCM_CAR : 0)
                | ((msr & MOS7840_MSR_RI) ? TIOCM_RI : 0)
                | ((msr & MOS7840_MSR_DSR) ? TIOCM_DSR : 0);

        DPRINTK("%s - 0x%04X", __FUNCTION__, result);

        return result;
}



static int mos7840_tiocmset(struct usb_serial_port *port, struct file *file,
        unsigned int set, unsigned int clear)
{
	struct moschip_port *mos7840_port;
        //struct ti_port *tport = usb_get_serial_port_data(port);
        unsigned int mcr;
	unsigned int status;

        DPRINTK("%s - port %d", __FUNCTION__, port->number);

	mos7840_port = mos7840_get_port_private(port); 

	if (mos7840_port == NULL)
                return -ENODEV;



         mcr = mos7840_port->shadowMCR;
        if (clear & TIOCM_RTS)
                mcr &= ~MCR_RTS;
        if (clear & TIOCM_DTR)
                mcr &= ~MCR_DTR;
        if (clear & TIOCM_LOOP)
                mcr &= ~MCR_LOOPBACK;
	
        if (set & TIOCM_RTS)
                mcr |= MCR_RTS;
        if (set & TIOCM_DTR)
                mcr |= MCR_DTR;
        if (set & TIOCM_LOOP)
                mcr |= MCR_LOOPBACK;

         mos7840_port->shadowMCR = mcr;

        status=0;
        status = mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,mcr);
        if(status <0)
        {
                DPRINTK("setting MODEM_CONTROL_REGISTER Failed\n");
                return -1;
        }

        return 0;
}







/*****************************************************************************
 * SerialSetTermios
 *	this function is called by the tty driver when it wants to change the termios structure
 *****************************************************************************/

static void mos7840_set_termios (struct usb_serial_port *port, struct termios *old_termios)
{
	int status;
	unsigned int cflag;
	struct usb_serial *serial;
	struct moschip_port *mos7840_port;
	struct tty_struct *tty;
	DPRINTK("mos7840_set_termios: START\n");	
	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return;
	}

	serial = port->serial;

	if(mos7840_serial_paranoia_check(serial,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid Serial \n");
		return;
	}

	mos7840_port = mos7840_get_port_private(port); 

	if (mos7840_port == NULL)
		return;

	tty = port->tty;

	if (!port->tty || !port->tty->termios)
	{
		dbg ("%s - no tty or termios", __FUNCTION__);
		return;
	}

	if (!mos7840_port->open) 
	{
		dbg("%s - port not opened", __FUNCTION__);
		return;
	}

	DPRINTK("%s\n","setting termios - ");

	cflag = tty->termios->c_cflag;

	if (!cflag)
	{
           DPRINTK("%s %s\n",__FUNCTION__,"cflag is NULL");
	   return;
	}            

	/* check that they really want us to change something */
	if (old_termios)
	{
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag))) 
		{
			DPRINTK("%s\n","Nothing to change");
			return;
		}
	}

	dbg("%s - clfag %08x iflag %08x", __FUNCTION__, 
	    tty->termios->c_cflag,
	    RELEVANT_IFLAG(tty->termios->c_iflag));
	
	if (old_termios) 
	{
		dbg("%s - old clfag %08x old iflag %08x", __FUNCTION__,
		    old_termios->c_cflag,
		    RELEVANT_IFLAG(old_termios->c_iflag));
	}

	dbg("%s - port %d", __FUNCTION__, port->number);

	/* change the port settings to the new ones specified */

	mos7840_change_port_settings (mos7840_port, old_termios);

	if(!mos7840_port->read_urb)
	{
		DPRINTK("%s","URB KILLED !!!!!\n");
		return;
	}

	if(mos7840_port->read_urb->status!=-EINPROGRESS)
	{
		mos7840_port->read_urb->dev = serial->dev;
	 	status = usb_submit_urb(mos7840_port->read_urb, GFP_ATOMIC);
		if (status) 
		{
			DPRINTK(" usb_submit_urb(read bulk) failed, status = %d", status);
		}
	}
	return;
}

/*
static void mos7840_break_ctl( struct usb_serial_port *port, int break_state )
{
        //struct usb_serial *serial = port->serial;

//        if (BSA_USB_CMD(BELKIN_SA_SET_BREAK_REQUEST, break_state ? 1 : 0) < 0)
  //              err("Set break_ctl %d", break_state);
}
*/



/*****************************************************************************
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 *****************************************************************************/

static int get_lsr_info(struct moschip_port *mos7840_port, unsigned int *value)
{
	int count;
	unsigned int result = 0;

        count = mos7840_chars_in_buffer(mos7840_port->port);
        if(count == 0)
	{
		dbg("%s -- Empty", __FUNCTION__);
		result = TIOCSER_TEMT;
	}

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

/*****************************************************************************
 * get_number_bytes_avail - get number of bytes available
 *
 * Purpose: Let user call ioctl to get the count of number of bytes available.
 *****************************************************************************/

static int get_number_bytes_avail(struct moschip_port *mos7840_port, unsigned int *value)
{
	unsigned int result = 0;
	struct tty_struct *tty = mos7840_port->port->tty;

	if (!tty)
		return -ENOIOCTLCMD;

	result = tty->read_cnt;

	dbg("%s(%d) = %d", __FUNCTION__,  mos7840_port->port->number, result);
	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;

	return -ENOIOCTLCMD;
}


/*****************************************************************************
 * set_modem_info
 *      function to set modem info
 *****************************************************************************/

static int set_modem_info(struct moschip_port *mos7840_port, unsigned int cmd, unsigned int *value)
{
	unsigned int mcr ;
	unsigned int arg;
	__u16 Data;
	int status;
	struct usb_serial_port *port;

	if (mos7840_port == NULL)
		return -1;


	port = (struct usb_serial_port*)mos7840_port->port;
	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return -1;
	}

	mcr = mos7840_port->shadowMCR;

	if (copy_from_user(&arg, value, sizeof(int)))
		return -EFAULT;

	switch (cmd) {
		case TIOCMBIS:
			if (arg & TIOCM_RTS)
				mcr |= MCR_RTS;
			if (arg & TIOCM_DTR)
				mcr |= MCR_RTS;
			if (arg & TIOCM_LOOP)
				mcr |= MCR_LOOPBACK;
			break;

		case TIOCMBIC:
			if (arg & TIOCM_RTS)
				mcr &= ~MCR_RTS;
			if (arg & TIOCM_DTR)
				mcr &= ~MCR_RTS;
			if (arg & TIOCM_LOOP)
				mcr &= ~MCR_LOOPBACK;
			break;

		case TIOCMSET:
			/* turn off the RTS and DTR and LOOPBACK 
			 * and then only turn on what was asked to */
			mcr &=  ~(MCR_RTS | MCR_DTR | MCR_LOOPBACK);
			mcr |= ((arg & TIOCM_RTS) ? MCR_RTS : 0);
			mcr |= ((arg & TIOCM_DTR) ? MCR_DTR : 0);
			mcr |= ((arg & TIOCM_LOOP) ? MCR_LOOPBACK : 0);
			break;
	}

	mos7840_port->shadowMCR = mcr;

	Data = mos7840_port->shadowMCR;
	status=0;
	status = mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
	if(status <0)
	{
		DPRINTK("setting MODEM_CONTROL_REGISTER Failed\n");
		return -1;
	}

	return 0;
}

/*****************************************************************************
 * get_modem_info
 *      function to get modem info
 *****************************************************************************/

static int get_modem_info(struct moschip_port *mos7840_port, unsigned int *value)
{
	unsigned int result = 0;
	__u16 msr;
	unsigned int mcr = mos7840_port->shadowMCR;
	int status=0;
	status=mos7840_get_Uart_Reg(mos7840_port->port,MODEM_STATUS_REGISTER,&msr);
	result = ((mcr & MCR_DTR)	? TIOCM_DTR: 0)	  /* 0x002 */
		  | ((mcr & MCR_RTS)	? TIOCM_RTS: 0)   /* 0x004 */
		  | ((msr & MOS7840_MSR_CTS)	? TIOCM_CTS: 0)   /* 0x020 */
		  | ((msr & MOS7840_MSR_CD)	? TIOCM_CAR: 0)   /* 0x040 */
		  | ((msr & MOS7840_MSR_RI)	? TIOCM_RI:  0)   /* 0x080 */
		  | ((msr & MOS7840_MSR_DSR)	? TIOCM_DSR: 0);  /* 0x100 */


	dbg("%s -- %x", __FUNCTION__, result);

	if (copy_to_user(value, &result, sizeof(int)))
		return -EFAULT;
	return 0;
}

/*****************************************************************************
 * get_serial_info
 *      function to get information about serial port
 *****************************************************************************/

static int get_serial_info(struct moschip_port *mos7840_port, struct serial_struct * retinfo)
{
	struct serial_struct tmp;

	if (mos7840_port == NULL)
		return -1;


	if (!retinfo)
		return -EFAULT;

	memset(&tmp, 0, sizeof(tmp));

	tmp.type		= PORT_16550A;
	tmp.line		= mos7840_port->port->serial->minor;
	tmp.port		= mos7840_port->port->number;
	tmp.irq			= 0;
	tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
        tmp.xmit_fifo_size      = NUM_URBS * URB_TRANSFER_BUFFER_SIZE;
	tmp.baud_base		= 9600;
	tmp.close_delay		= 5*HZ;
	tmp.closing_wait	= 30*HZ;


	if (copy_to_user(retinfo, &tmp, sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

/*****************************************************************************
 * SerialIoctl
 *	this function handles any ioctl calls to the driver
 *****************************************************************************/

static int mos7840_ioctl (struct usb_serial_port *port, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct moschip_port *mos7840_port;
	struct tty_struct *tty;

	struct async_icount cnow;
	struct async_icount cprev;
	struct serial_icounter_struct icount;
	int mosret=0;
	//int retval;
	//struct tty_ldisc *ld;

	//printk("%s - port %d, cmd = 0x%x\n", __FUNCTION__, port->number, cmd);
	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return -1;
	}

	mos7840_port = mos7840_get_port_private(port); 
	tty = mos7840_port->port->tty;

	if (mos7840_port == NULL)
		return -1;

	dbg("%s - port %d, cmd = 0x%x", __FUNCTION__, port->number, cmd);

	switch (cmd) 
	{
                /* return number of bytes available */
	
		case TIOCINQ:
			dbg("%s (%d) TIOCINQ", __FUNCTION__,  port->number);
			return get_number_bytes_avail(mos7840_port, (unsigned int *) arg);
			break;

		case TIOCOUTQ:
			dbg("%s (%d) TIOCOUTQ", __FUNCTION__,  port->number);
			return put_user(tty->driver->chars_in_buffer ?
					tty->driver->chars_in_buffer(tty) : 0,
					(int __user *) arg);
			break;

		/*  //2.6.17 block
		case TCFLSH:
			retval = tty_check_change(tty);
			if (retval)
				return retval;

			ld = tty_ldisc_ref(tty);
			switch (arg) {
				case TCIFLUSH:
					if (ld && ld->flush_buffer)
						ld->flush_buffer(tty);
					break;
				case TCIOFLUSH:
					if (ld && ld->flush_buffer)
						ld->flush_buffer(tty);
					// fall through 
				case TCOFLUSH:
					if (tty->driver->flush_buffer)
						tty->driver->flush_buffer(tty);
					break;
				default:
					tty_ldisc_deref(ld);
					return -EINVAL;
			}
			tty_ldisc_deref(ld);
			return 0;
		*/
		case TCGETS:
			if (kernel_termios_to_user_termios((struct termios __user *)arg, tty->termios))
				return -EFAULT;
			return 0;

		case TIOCSERGETLSR:
			dbg("%s (%d) TIOCSERGETLSR", __FUNCTION__,  port->number);
			return get_lsr_info(mos7840_port, (unsigned int *) arg);
			return 0;

		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			dbg("%s (%d) TIOCMSET/TIOCMBIC/TIOCMSET", __FUNCTION__,  port->number);
	//	printk("%s (%d) TIOCMSET/TIOCMBIC/TIOCMSET", __FUNCTION__,  port->number);
			mosret=set_modem_info(mos7840_port, cmd, (unsigned int *) arg);
	//		printk(" %s: ret:%d\n",__FUNCTION__,mosret);
			return mosret;

		case TIOCMGET:  
			dbg("%s (%d) TIOCMGET", __FUNCTION__,  port->number);
			return get_modem_info(mos7840_port, (unsigned int *) arg);

		case TIOCGSERIAL:
			dbg("%s (%d) TIOCGSERIAL", __FUNCTION__,  port->number);
			return get_serial_info(mos7840_port, (struct serial_struct *) arg);

		case TIOCSSERIAL:
			dbg("%s (%d) TIOCSSERIAL", __FUNCTION__,  port->number);
			break;

		case TIOCMIWAIT:
			dbg("%s (%d) TIOCMIWAIT", __FUNCTION__,  port->number);
			cprev = mos7840_port->icount;
			while (1) {
	#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
				//interruptible_sleep_on(&mos7840_port->delta_msr_wait);
				// mos7840_port->delta_msr_cond=0;
		                //wait_event_interruptible(mos7840_port->delta_msr_wait,(mos7840_port->delta_msr_cond==1));
	#endif

				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				cnow = mos7840_port->icount;
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* NOTREACHED */
			break;

		case TIOCGICOUNT:
			cnow = mos7840_port->icount;
			icount.cts = cnow.cts;
			icount.dsr = cnow.dsr;
			icount.rng = cnow.rng;
			icount.dcd = cnow.dcd;
			icount.rx = cnow.rx;
			icount.tx = cnow.tx;
			icount.frame = cnow.frame;
			icount.overrun = cnow.overrun;
			icount.parity = cnow.parity;
			icount.brk = cnow.brk;
			icount.buf_overrun = cnow.buf_overrun;

			dbg("%s (%d) TIOCGICOUNT RX=%d, TX=%d", __FUNCTION__,  port->number, icount.rx, icount.tx );
			if (copy_to_user((void *)arg, &icount, sizeof(icount)))
				return -EFAULT;
			return 0;

		case TIOCEXBAUD:
			return 0;
		default:
				break;
	}

	return -ENOIOCTLCMD;
}


/*****************************************************************************
 * mos7840_send_cmd_write_baud_rate
 *	this function sends the proper command to change the baud rate of the
 *	specified port.
 *****************************************************************************/

static int mos7840_send_cmd_write_baud_rate (struct moschip_port *mos7840_port, int baudRate)
{
	int divisor = 0;
	int status;
	__u16 Data;
	unsigned char number ;
	__u16 clk_sel_val;
	struct usb_serial_port *port;

	if (mos7840_port == NULL)
		return -1;

	port = (struct usb_serial_port*)mos7840_port->port;
	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return -1;
	}

	if(mos7840_serial_paranoia_check(port->serial,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid Serial \n");
		return -1;
	}


	DPRINTK("%s","Entering .......... \n");

	number = mos7840_port->port->number - mos7840_port->port->serial->minor;

	dbg("%s - port = %d, baud = %d", __FUNCTION__, mos7840_port->port->number, baudRate);
	//reset clk_uart_sel in spregOffset
	if(baudRate >115200)
	{
		#ifdef HW_flow_control
		//NOTE: need to see the pther register to modify
		//setting h/w flow control bit to 1;
		status=0;
		//Data = mos7840_port->shadowMCR ;
		Data = 0x2b;
		mos7840_port->shadowMCR=Data;
		status=mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
		if(status<0)
		{		
			DPRINTK("Writing spreg failed in set_serial_baud\n");
			return -1;
		}
		#endif

	}
	else
	{
		#ifdef HW_flow_control
		//setting h/w flow control bit to 0;
		status=0;
		//Data = mos7840_port->shadowMCR ;
		Data = 0xb;
		mos7840_port->shadowMCR=Data;
		status=mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
		if(status<0)
		{		
			DPRINTK("Writing spreg failed in set_serial_baud\n");
			return -1;
		}
	
		#endif

	
	}

	
	if(1)//baudRate <= 115200)
	{
		clk_sel_val=0x0;
		Data=0x0;
		status=0;
		status = mos7840_calc_baud_rate_divisor (baudRate, &divisor,&clk_sel_val);
		status= mos7840_get_reg_sync(port,mos7840_port->SpRegOffset,&Data);
		if(status<0)
		{		
			DPRINTK("reading spreg failed in set_serial_baud\n");
			return -1;
		}
		Data = (Data & 0x8f)|clk_sel_val;
		status=0;
		status= mos7840_set_reg_sync(port,mos7840_port->SpRegOffset,Data);
		if(status<0)
		{		
			DPRINTK("Writing spreg failed in set_serial_baud\n");
			return -1;
		}
       		 /* Calculate the Divisor */
		
	
		if (status) 
		{
			err("%s - bad baud rate", __FUNCTION__);
			DPRINTK("%s\n","bad baud rate");
			return status;
		}
	        /* Enable access to divisor latch */
	        Data = mos7840_port->shadowLCR | SERIAL_LCR_DLAB;
	        mos7840_port->shadowLCR  = Data;
		mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	
		/* Write the divisor */
		Data = LOW8 (divisor);//MOSCHIP:  commented to test
		DPRINTK("set_serial_baud Value to write DLL is %x\n",Data);
		mos7840_set_Uart_Reg(port,DIVISOR_LATCH_LSB,Data);
	
		Data = HIGH8 (divisor); //MOSCHIP:  commented to test
		DPRINTK("set_serial_baud Value to write DLM is %x\n",Data);
		mos7840_set_Uart_Reg(port,DIVISOR_LATCH_MSB,Data);
	
	        /* Disable access to divisor latch */
	        Data = mos7840_port->shadowLCR & ~SERIAL_LCR_DLAB;
	        mos7840_port->shadowLCR = Data;
		mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);
	
	}
		
	return status;
}



/*****************************************************************************
 * mos7840_calc_baud_rate_divisor
 *	this function calculates the proper baud rate divisor for the specified
 *	baud rate.
 *****************************************************************************/
static int mos7840_calc_baud_rate_divisor (int baudRate, int *divisor,__u16 *clk_sel_val)
{
	//int i;
	//__u16 custom,round1, round;

	dbg("%s - %d", __FUNCTION__, baudRate);

		if(baudRate <=115200)
		{
			*divisor = 115200/baudRate;
			*clk_sel_val = 0x0;
		}
	 	if((baudRate > 115200) && (baudRate <= 230400))
		{
			*divisor = 230400/baudRate;	
			*clk_sel_val=0x10;
		}
	 	else if((baudRate > 230400) && (baudRate <= 403200))
		{
			*divisor = 403200/baudRate;	
			*clk_sel_val=0x20;
		}
	 	else if((baudRate > 403200) && (baudRate <= 460800))
		{
			*divisor = 460800/baudRate;	
			*clk_sel_val=0x30;
		}
	 	else if((baudRate > 460800) && (baudRate <= 806400))
		{
			*divisor = 806400/baudRate;	
			*clk_sel_val=0x40;
		}
	 	else if((baudRate >806400) && (baudRate <= 921600))
		{
			*divisor = 921600/baudRate;	
			*clk_sel_val=0x50;
		}
	 	else if((baudRate > 921600) && (baudRate <= 1572864))
		{
			*divisor = 1572864/baudRate;	
			*clk_sel_val=0x60;
		}
	 	else if((baudRate > 1572864) && (baudRate <= 3145728))
		{
			*divisor = 3145728/baudRate;	
			*clk_sel_val=0x70;
		}
	return 0;	

	#ifdef NOTMCS7840

	for (i = 0; i < NUM_ENTRIES(mos7840_divisor_table); i++) 
	{
		if ( mos7840_divisor_table[i].BaudRate == baudrate ) 
		{
			*divisor = mos7840_divisor_table[i].Divisor;
			return 0;
		}
	}

        /* After trying for all the standard baud rates    *
         * Try calculating the divisor for this baud rate  */

	if (baudrate > 75 &&  baudrate < 230400) 
	{
		/* get the divisor */
		custom = (__u16)(230400L  / baudrate);

		/* Check for round off */
		round1 = (__u16)(2304000L / baudrate);
		round = (__u16)(round1 - (custom * 10));
		if (round > 4) {
			custom++;
		}
		*divisor = custom;

		DPRINTK(" Baud %d = %d\n",baudrate, custom);
		return 0;
	}

	DPRINTK("%s\n"," Baud calculation Failed...");
	return -1;
	#endif
}



/*****************************************************************************
 * mos7840_change_port_settings
 *	This routine is called to set the UART on the device to match 
 *      the specified new settings.
 *****************************************************************************/

static void mos7840_change_port_settings (struct moschip_port *mos7840_port, struct termios *old_termios)
{
	struct tty_struct *tty;
	int baud;
	unsigned cflag;
	unsigned iflag;
	__u8 mask = 0xff;
	__u8 lData;
	__u8 lParity;
	__u8 lStop;
	int status;
	__u16 Data;
	struct usb_serial_port *port;
	struct usb_serial *serial;

	if (mos7840_port == NULL)
		return ;

	port = (struct usb_serial_port *)mos7840_port->port;

	if(mos7840_port_paranoia_check(port,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid port \n");
		return ;
	}

	if(mos7840_serial_paranoia_check(port->serial,__FUNCTION__) )
	{
		DPRINTK("%s","Invalid Serial \n");
		return ;
	}

	serial = port->serial;

	dbg("%s - port %d", __FUNCTION__, mos7840_port->port->number);

	if ((!mos7840_port->open) && (!mos7840_port->openPending)) 
	{
		dbg("%s - port not opened", __FUNCTION__);
		return;
	}

	tty = mos7840_port->port->tty;
	
	if ((!tty) || (!tty->termios)) 
	{
		dbg("%s - no tty structures", __FUNCTION__);
		return;
	}

	DPRINTK("%s","Entering .......... \n");

	lData = LCR_BITS_8;
	lStop = LCR_STOP_1;         
	lParity = LCR_PAR_NONE;

	cflag = tty->termios->c_cflag;
	iflag = tty->termios->c_iflag;

	/* Change the number of bits */	

//COMMENT1: the below Line"if(cflag & CSIZE)" is added for the errors we get for serial loop data test i.e serial_loopback.pl -v
	//if(cflag & CSIZE)
	{
	switch (cflag & CSIZE) 
	{
		case CS5:	lData = LCR_BITS_5; 
				mask = 0x1f;    
			    	break;

		case CS6:   	lData = LCR_BITS_6; 
				mask = 0x3f;    
			    	break;

		case CS7:   	lData = LCR_BITS_7; 
				mask = 0x7f;    
			    	break;
		default:
		case CS8:   	lData = LCR_BITS_8;
			    	break;
	}
	}
	/* Change the Parity bit */
	if (cflag & PARENB) 
	{
		if (cflag & PARODD) 
		{
			lParity = LCR_PAR_ODD;
			dbg("%s - parity = odd", __FUNCTION__);
		} 
		else 
		{
			lParity = LCR_PAR_EVEN;
			dbg("%s - parity = even", __FUNCTION__);
		}

	} 
	else 
	{
		dbg("%s - parity = none", __FUNCTION__);
	}
	
	if(cflag & CMSPAR)
	{
		lParity = lParity | 0x20;
	}

	/* Change the Stop bit */
	if (cflag & CSTOPB) 
	{
		lStop = LCR_STOP_2;
		dbg("%s - stop bits = 2", __FUNCTION__);
	} 
	else 
	{
		lStop = LCR_STOP_1;
		dbg("%s - stop bits = 1", __FUNCTION__);
	}


	/* Update the LCR with the correct value */
	mos7840_port->shadowLCR &= ~(LCR_BITS_MASK | LCR_STOP_MASK | LCR_PAR_MASK);
	mos7840_port->shadowLCR |= (lData | lParity | lStop);

	mos7840_port->validDataMask = mask;
	DPRINTK("mos7840_change_port_settings mos7840_port->shadowLCR is %x\n",mos7840_port->shadowLCR);
	/* Disable Interrupts */
	Data = 0x00;
	mos7840_set_Uart_Reg(port,INTERRUPT_ENABLE_REGISTER,Data);
	

	Data = 0x00;
	mos7840_set_Uart_Reg(port,FIFO_CONTROL_REGISTER,Data);

	Data = 0xcf;
	mos7840_set_Uart_Reg(port,FIFO_CONTROL_REGISTER,Data);

	/* Send the updated LCR value to the mos7840 */
	Data = mos7840_port->shadowLCR;

	mos7840_set_Uart_Reg(port,LINE_CONTROL_REGISTER,Data);


        Data = 0x00b;
        mos7840_port->shadowMCR = Data;
	mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
        Data = 0x00b;
	mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);

	/* set up the MCR register and send it to the mos7840 */
	
	mos7840_port->shadowMCR = MCR_MASTER_IE;
	if (cflag & CBAUD) 
	{
		mos7840_port->shadowMCR |= (MCR_DTR | MCR_RTS);
	}


	if (cflag & CRTSCTS) 
	{
		mos7840_port->shadowMCR |= (MCR_XON_ANY);

		
	}
	else
	{
		mos7840_port->shadowMCR &= ~(MCR_XON_ANY); 
	}


	Data = mos7840_port->shadowMCR;
	mos7840_set_Uart_Reg(port,MODEM_CONTROL_REGISTER,Data);
	


	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(tty);

	if (!baud) 
	{
		/* pick a default, any default... */
        	DPRINTK("%s\n","Picked default baud...");
		baud = 9600;
	}


	dbg("%s - baud rate = %d", __FUNCTION__, baud);
	status = mos7840_send_cmd_write_baud_rate (mos7840_port, baud);

	/* Enable Interrupts */
	Data = 0x0c;
	mos7840_set_Uart_Reg(port,INTERRUPT_ENABLE_REGISTER,Data);

	if(mos7840_port->read_urb->status!=-EINPROGRESS)
	{
		mos7840_port->read_urb->dev = serial->dev;
		
		status = usb_submit_urb(mos7840_port->read_urb, GFP_ATOMIC);
	
		if (status) 
		{
			DPRINTK(" usb_submit_urb(read bulk) failed, status = %d", status);
		}
	}
	#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
	//wake_up(&mos7840_port->delta_msr_wait);
    	//mos7840_port->delta_msr_cond=1;
	#endif
	DPRINTK("mos7840_change_port_settings mos7840_port->shadowLCR is End %x\n",mos7840_port->shadowLCR);

	return;
}


static int mos7840_calc_num_ports(struct usb_serial *serial)
{

	__u16 Data=0x00;
        int ret =0;
	int mos7840_2or4ports;
        ret = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),\
	 MCS_RDREQ,MCS_RD_RTYPE,0,GPIO_REGISTER,&Data,VENDOR_READ_LENGTH,MOS_WDR_TIMEOUT);

 	DPRINTK("mos7840_calc_num_ports GPIO is %x\n",Data);


        if((Data&0x01)==0)
        {
                mos7840_2or4ports=2;
                serial->type->num_bulk_in=2;
                serial->type->num_bulk_out=2;
                serial->type->num_ports=2;
        }
        //else if(serial->interface->cur_altsetting->desc.bNumEndpoints == 9)
        else
        {
                mos7840_2or4ports =4;
                serial->type->num_bulk_in=4;
                serial->type->num_bulk_out=4;
                serial->type->num_ports=4;

        }

 	DPRINTK("mos7840_calc_num_ports is %d\n",mos7840_2or4ports);

	return mos7840_2or4ports;

}


/****************************************************************************
 * mos7840_startup
 ****************************************************************************/

static int mos7840_startup (struct usb_serial *serial)
{
	struct moschip_serial *mos7840_serial;
	struct moschip_port *mos7840_port;
	struct usb_device *dev;
	int i,status;
	
	__u16 Data;
	DPRINTK("%s \n"," mos7840_startup :entering..........");

	if(!serial)
	{
		DPRINTK("%s\n","Invalid Handler");
		return -1;
	}

	dev = serial->dev;
	
	DPRINTK("%s\n","Entering...");

	/* create our private serial structure */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	mos7840_serial = kzalloc (sizeof(struct moschip_serial), GFP_KERNEL);
	#endif
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
	mos7840_serial = kmalloc (sizeof(struct moschip_serial), GFP_KERNEL);
	#endif
	if (mos7840_serial == NULL) 
	{
		err("%s - Out of memory", __FUNCTION__);
		return -ENOMEM;
	}

	/* resetting the private structure field values to zero */
	memset (mos7840_serial, 0, sizeof(struct moschip_serial));

	mos7840_serial->serial = serial;
	//initilize status polling flag to FALSE
	mos7840_serial->status_polling_started = FALSE;

	mos7840_set_serial_private(serial,mos7840_serial);
	mos7840_serial->mos7840_spectrum_2or4ports = mos7840_calc_num_ports(serial);
	/* we set up the pointers to the endpoints in the mos7840_open *
	 * function, as the structures aren't created yet.             */


	/* set up port private structures */
	for (i = 0; i < serial->num_ports; ++i) 
	{
	        mos7840_port = kmalloc(sizeof(struct moschip_port), GFP_KERNEL);
		if (mos7840_port == NULL) 
		{
			err("%s - Out of memory", __FUNCTION__);
			mos7840_set_serial_private(serial,NULL);
			kfree(mos7840_serial);
			return -ENOMEM;
		}
		memset(mos7840_port, 0, sizeof(struct moschip_port));


	/* Initialize all port interrupt end point to port 0 int endpoint *
	 * Our device has only one interrupt end point comman to all port */



	//	serial->port[i]->interrupt_in_endpointAddress = serial->port[0]->interrupt_in_endpointAddress;

		
		mos7840_port->port = serial->port[i];
//	
			mos7840_set_port_private(serial->port[i],mos7840_port);


		mos7840_port->port_num=((serial->port[i]->number - \
	  			 (serial->port[i] ->serial->minor))+1);
		
	
		mos7840_port->AppNum = (((__u16)serial->port[i]->number - \
				(__u16)(serial->port[i] ->serial->minor))+1)<<8;

		if(mos7840_port->port_num ==1)
		{
			mos7840_port->SpRegOffset =0x0;
			mos7840_port->ControlRegOffset =0x1;
			mos7840_port->DcrRegOffset =0x4 ;
			//mos7840_port->ClkSelectRegOffset =  ;
		}
		else if((mos7840_port->port_num ==2)&&(mos7840_serial->mos7840_spectrum_2or4ports ==4))
		{
			mos7840_port->SpRegOffset =0x8;
			mos7840_port->ControlRegOffset =0x9;
			mos7840_port->DcrRegOffset =0x16;
			//mos7840_port->ClkSelectRegOffset =  ;
		}
		else if((mos7840_port->port_num ==2)&&(mos7840_serial->mos7840_spectrum_2or4ports ==2))
		{
			mos7840_port->SpRegOffset =0xa;
			mos7840_port->ControlRegOffset =0xb;
			mos7840_port->DcrRegOffset =0x19;
			//mos7840_port->ClkSelectRegOffset =  ;
		}
		else if((mos7840_port->port_num ==3)&&(mos7840_serial->mos7840_spectrum_2or4ports ==4))
		{
			mos7840_port->SpRegOffset =0xa;
			mos7840_port->ControlRegOffset =0xb;
			mos7840_port->DcrRegOffset =0x19;
			//mos7840_port->ClkSelectRegOffset =  ;
		}
		else if((mos7840_port->port_num ==4)&&(mos7840_serial->mos7840_spectrum_2or4ports ==4))
		{
			mos7840_port->SpRegOffset =0xc;
			mos7840_port->ControlRegOffset =0xd;
			mos7840_port->DcrRegOffset =0x1c ;
			//mos7840_port->ClkSelectRegOffset =  ;
		}
		mos7840_Dump_serial_port(mos7840_port);
		
		mos7840_set_port_private(serial->port[i],mos7840_port);
	
	
		//enable rx_disable bit in control register

		status=mos7840_get_reg_sync(serial->port[i],mos7840_port->ControlRegOffset,&Data);
		if(status<0) {
	                DPRINTK("Reading ControlReg failed status-0x%x\n", status);
	                break;
	        }
		else DPRINTK("ControlReg Reading success val is %x, status%d\n",Data,status);
		Data |= 0x08;//setting driver done bit
		Data |= 0x04;//sp1_bit to have cts change reflect in modem status reg
				
		//Data |= 0x20; //rx_disable bit
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],mos7840_port->ControlRegOffset,Data);
		if(status<0) {
	                DPRINTK("Writing ControlReg failed(rx_disable) status-0x%x\n", status);
	                break;
	        }
	        else DPRINTK("ControlReg Writing success(rx_disable) status%d\n",status);
	
		//Write default values in DCR (i.e 0x01 in DCR0, 0x05 in DCR2 and 0x24 in DCR3
		Data = 0x01;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],(__u16)(mos7840_port->DcrRegOffset+0),Data);
		if(status<0) {
	                DPRINTK("Writing DCR0 failed status-0x%x\n", status);
        	        break;
	        }
	        else DPRINTK("DCR0 Writing success status%d\n",status);
	
		Data = 0x05;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],(__u16)(mos7840_port->DcrRegOffset+1),Data);
		if(status<0) {
	                DPRINTK("Writing DCR1 failed status-0x%x\n", status);
	                break;
	        }
	        else DPRINTK("DCR1 Writing success status%d\n",status);
	
		Data = 0x24;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],(__u16)(mos7840_port->DcrRegOffset+2),Data);
		if(status<0) {
	                DPRINTK("Writing DCR2 failed status-0x%x\n", status);
	                break;
	        }
	        else DPRINTK("DCR2 Writing success status%d\n",status);
	
		// write values in clkstart0x0 and clkmulti 0x20	
		Data = 0x0;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],CLK_START_VALUE_REGISTER,Data);
		if(status<0) {
	                DPRINTK("Writing CLK_START_VALUE_REGISTER failed status-0x%x\n", status);
	                break;
	        }
	        else DPRINTK("CLK_START_VALUE_REGISTER Writing success status%d\n",status);
	
	
		Data = 0x20;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],CLK_MULTI_REGISTER,Data);
		if(status<0) {
                        DPRINTK("Writing CLK_MULTI_REGISTER failed status-0x%x\n", status);
                        break;
                }
        	else DPRINTK("CLK_MULTI_REGISTER Writing success status%d\n",status);
	
	
		//write value 0x0 to scratchpad register
		/*	
		if(RS485mode==1)
			Data = 0xC0;
		else
			Data = 0x00;
		status=0;
		status=mos7840_set_Uart_Reg(serial->port[i],SCRATCH_PAD_REGISTER,Data);
		if(status<0) {
	                DPRINTK("Writing SCRATCH_PAD_REGISTER failed status-0x%x\n", status);
	                break;
                }
        	else DPRINTK("SCRATCH_PAD_REGISTER Writing success status%d\n",status);
		*/
		
	/*	
		//Threshold Registers 
		if(mos7840_serial->mos7840_spectrum_2or4ports==4)
		{
			Data = 0x00;
			status=0;
			status=mos7840_set_reg_sync(serial->port[i],\
					(__u16)(THRESHOLD_VAL_SP1_1+(__u16)mos7840_Thr_cnt),Data);
			DPRINTK("THRESHOLD_VAL offset is%x\n", (__u16)(THRESHOLD_VAL_SP1_1+(__u16)mos7840_Thr_cnt));
			if(status<0) {
	        		DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
	                	break;
		        }
		        else DPRINTK("THRESHOLD_VAL Writing success status%d\n",status);
			mos7840_Thr_cnt++;
	
			Data = 0x01;
			status=0;
			status=mos7840_set_reg_sync(serial->port[i],\
			(__u16)(THRESHOLD_VAL_SP1_1+(__u16)mos7840_Thr_cnt),Data);
		DPRINTK("THRESHOLD_VAL offsetis%x\n",(__u16)(THRESHOLD_VAL_SP1_1+(__u16)mos7840_Thr_cnt));
			if(status<0) {
                	        DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
                        	break;
	                }
		        else DPRINTK("THRESHOLD_VAL Writing success status%d\n",status);
			mos7840_Thr_cnt++;
		}

		else
		{

			if(mos7840_port->port_num==1)
			{
				Data = 0x00;
		                status=0;
	        	        status=mos7840_set_reg_sync(serial->port[i],\
        	                        0x3f,Data);
                		DPRINTK("THRESHOLD_VAL offset is 0x3f\n");
		                if(status<0) {
        	                DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
                	        break;
				}
				Data = 0x01;
        	                status=0;
                	        status=mos7840_set_reg_sync(serial->port[i],\
                        	        0x40,Data);
	                        DPRINTK("THRESHOLD_VAL offset is 0x40\n");
        	                if(status<0) {
                	        DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
                        	break;

				}
			}
			else	
                        {
	                        Data = 0x00;
	                        status=0;
	                        status=mos7840_set_reg_sync(serial->port[i],\
        	                        0x43,Data);
	                        DPRINTK("THRESHOLD_VAL offset is 0x43\n");
	                        if(status<0) {
        	                DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
                	        break;
                        	}
	                        Data = 0x01;
        	                status=0;
                	        status=mos7840_set_reg_sync(serial->port[i],\
                        	        0x44,Data);
	                        DPRINTK("THRESHOLD_VAL offset is 0x44\n");
        	                if(status<0) {
                	        DPRINTK("Writing THRESHOLD_VAL failed status-0x%x\n",status);
	                        break;

        	                }


			}

		}
		*/	
		//Zero Length flag register 
		if((mos7840_port->port_num != 1)&&(mos7840_serial->mos7840_spectrum_2or4ports==2 ))	
		{
		
		Data = 0xff;
		status=0;
		status=mos7840_set_reg_sync(serial->port[i],\
			(__u16)(ZLP_REG1+((__u16)mos7840_port->port_num)),Data);
		DPRINTK("ZLIP offset%x\n",(__u16)(ZLP_REG1+((__u16)mos7840_port->port_num)));
		if(status<0) {
                        DPRINTK("Writing ZLP_REG%d failed status-0x%x\n",i+2,status);
                        break;
                }
	        else DPRINTK("ZLP_REG%d Writing success status%d\n",i+2,status);
		}
		else
		{
		Data = 0xff;
                status=0;
                status=mos7840_set_reg_sync(serial->port[i],\
                        (__u16)(ZLP_REG1+((__u16)mos7840_port->port_num)-0x1),Data);
                DPRINTK("ZLIP offset%x\n",(__u16)(ZLP_REG1+((__u16)mos7840_port->port_num)-0x1));                if(status<0) {
                        DPRINTK("Writing ZLP_REG%d failed status-0x%x\n",i+1,status);
                        break;
                }
                else DPRINTK("ZLP_REG%d Writing success status%d\n",i+1,status);


		}	
	mos7840_port->control_urb = usb_alloc_urb(0,SLAB_ATOMIC);
	mos7840_port->ctrl_buf = kmalloc(16,GFP_KERNEL);


	}


		mos7840_Thr_cnt=0;
		//Zero Length flag enable
		Data = 0x0f;
		status=0;
		status=mos7840_set_reg_sync(serial->port[0],ZLP_REG5,Data);
		if(status<0) {
	                       DPRINTK("Writing ZLP_REG5 failed status-0x%x\n",status);
	                       return -1;
                }
	        else DPRINTK("ZLP_REG5 Writing success status%d\n",status);

	/* setting configuration feature to one */
	usb_control_msg (serial->dev, usb_sndctrlpipe(serial->dev, 0), (__u8)0x03, 0x00,0x01,0x00, 0x00, 0x00, 5*HZ);
	mos7840_Thr_cnt =0 ;
	return 0;
}



/****************************************************************************
 * mos7840_shutdown
 *	This function is called whenever the device is removed from the usb bus.
 ****************************************************************************/

static void mos7840_shutdown (struct usb_serial *serial)
{
	int i;
	struct moschip_port *mos7840_port;
	DPRINTK("%s \n"," shutdown :entering..........");

/* MATRIX  */
	//ThreadState = 1;
/* MATRIX  */

	if(!serial)
	{
		DPRINTK("%s","Invalid Handler \n");
		return;
	}

	/*	check for the ports to be closed,close the ports and disconnect		*/

	/* free private structure allocated for serial port  * 
	 * stop reads and writes on all ports   	     */

	for (i=0; i < serial->num_ports; ++i) 
	{
		mos7840_port = mos7840_get_port_private(serial->port[i]);
		kfree(mos7840_port->ctrl_buf);
	#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
		usb_kill_urb(mos7840_port->control_urb);
	#endif
	#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)
		usb_unlink_urb(mos7840_port->control_urb);
	#endif
		kfree(mos7840_port);
		mos7840_set_port_private(serial->port[i],NULL);
	}

	/* free private structure allocated for serial device */

	kfree(mos7840_get_serial_private(serial));
	mos7840_set_serial_private(serial,NULL);

        DPRINTK("%s\n","Thank u :: ");

}


/* Inline functions to check the sanity of a pointer that is passed to us */
static int mos7840_serial_paranoia_check (struct usb_serial *serial, const char *function)
{
        if (!serial) {
                dbg("%s - serial == NULL", function);
                return -1;
        }
//      if (serial->magic != USB_SERIAL_MAGIC) {
//              dbg("%s - bad magic number for serial", function);
//              return -1;
//      }
        if (!serial->type) {
                dbg("%s - serial->type == NULL!", function);
                return -1;
        }

        return 0;
}
static int mos7840_port_paranoia_check (struct usb_serial_port *port, const char *function)
{
        if (!port) {
                dbg("%s - port == NULL", function);
                return -1;
        }
//      if (port->magic != USB_SERIAL_PORT_MAGIC) {
//              dbg("%s - bad magic number for port", function);
//              return -1;
//      }
        if (!port->serial) {
                dbg("%s - port->serial == NULL", function);
                return -1;
        }

        return 0;
}
static struct usb_serial* mos7840_get_usb_serial (struct usb_serial_port *port, const char *function) {
        /* if no port was specified, or it fails a paranoia check */
        if (!port ||
                mos7840_port_paranoia_check (port, function) ||
                mos7840_serial_paranoia_check (port->serial, function)) {
     /* then say that we don't have a valid usb_serial thing, which will                  * end up genrating -ENODEV return values */
                return NULL;
        }

        return port->serial;
}



/****************************************************************************
 * moschip7840_init
 *	This is called by the module subsystem, or on startup to initialize us
 ****************************************************************************/
 int __init moschip7840_init(void)
{
	int retval;

	DPRINTK("%s \n"," mos7840_init :entering..........");

        /* Register with the usb serial */
	retval = usb_serial_register (&moschip7840_4port_device);

	if(retval)
		goto failed_port_device_register;

	DPRINTK("%s\n","Entring...");
	info(DRIVER_DESC " " DRIVER_VERSION);


 	/* Register with the usb */
	retval = usb_register(&io_driver);

	if (retval) 
		goto failed_usb_register;

	if(retval == 0) 
	{
		DPRINTK("%s\n","mos7840_init :leaving..........");
		return 0;
	}


failed_usb_register:
	usb_serial_deregister(&moschip7840_4port_device);

failed_port_device_register:

	return retval;
}

/****************************************************************************
 * moschip7840_exit
 *	Called when the driver is about to be unloaded.
 ****************************************************************************/
void __exit moschip7840_exit (void)
{

	DPRINTK("%s \n"," mos7840_exit :entering..........");

	usb_deregister (&io_driver);

	usb_serial_deregister (&moschip7840_4port_device);

	DPRINTK("%s\n","mos7840_exit :leaving...........");
}

module_init(moschip7840_init);
module_exit(moschip7840_exit);

/* Module information */
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

MODULE_PARM_DESC(debug, "Debug enabled or not");

