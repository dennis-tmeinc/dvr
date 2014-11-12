#ifndef __CDC_H__
#define __CDC_H__

#ifdef PWII_APP

#define ID_CDC  (0x5)

// CDC commands
#define PWII_CMD_BOOTUPREADY    (4)
#define PWII_CMD_VERSION        (0x11)
#define PWII_CMD_C1             (0x12)
#define PWII_CMD_C2             (0x13)
#define PWII_CMD_LEDMIC         (0xf)
#define PWII_CMD_LCD	        (0x16)
#define PWII_CMD_STANDBY        (0x15)
#define PWII_CMD_LEDPOWER       (0x14)
#define PWII_CMD_LEDERROR       (0x10)
#define PWII_CMD_POWER_GPSANT   (0x0e)
#define PWII_CMD_POWER_GPS      (0x0d)
#define PWII_CMD_POWER_RF900    (0x0c)
#define PWII_CMD_POWER_WIFI     (0x19)
#define PWII_CMD_OUTPUTSTATUS   (0x17)
#define PWII_CMD_SPEAKERVOLUME  (0x1a)

// CDC inputs
#define PWII_INPUT_REC ('\x05')
#define PWII_INPUT_C1 (PWII_INPUT_REC)
#define PWII_INPUT_C2 ('\x06')
#define PWII_INPUT_TM ('\x07')
#define PWII_INPUT_LP ('\x08')
#define PWII_INPUT_BO ('\x0b')
#define PWII_INPUT_MEDIA ('\x09')
#define PWII_INPUT_BOOTUP ('\x18')
#define PWII_INPUT_SPEAKERSTATUS ('\x1a')


extern int pwii_tracemark_inverted ;
extern int pwii_rear_ch ;
extern int pwii_front_ch ;

extern unsigned int pwii_keytime ;
extern unsigned int pwii_keyreltime ;
extern unsigned int pwii_outs ;
extern int mcu_pwii_cdcfailed ;

void mcu_pwii_output();
void mcu_pwii_input(char * ibuf);
int mcu_pwii_cmd(char * rsp, int cmd, int datalen=0, ...);
int mcu_pwii_bootupready();
int mcu_pwii_version(char * version);
int mcu_pwii_speakervolume();	// return 0: speaker off, 1: speaker on
unsigned int mcu_pwii_ouputstatus();
void pwii_keyautorelease();
void mcu_pwii_init();

#endif      // PWII_APP

#endif  // __CDC_H__
