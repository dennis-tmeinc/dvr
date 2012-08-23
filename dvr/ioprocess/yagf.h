
#ifndef __YAGF_H__
#define __YAGF_H__

#define ID_YAGF     (4)

#define YAGF_CMD_SETRTC             (0x07)
#define YAGF_CMD_UPLOADCRASHDATA    (0x19)
#define YAGF_CMD_UPLOADCRASHDATAACK (0x1a)
#define YAGF_CMD_UPLOADPEAKDATA     (0x1f)
#define YAGF_CMD_UPLOADPEAKDATAACK  (0x20)
#define YAGF_CMD_SETTRIGGER         (0x12)
#define YAGF_CMD_BOOTUPREADY        (0x08)
#define YAGF_CMD_ENABLEPEAK         (0x0f)
#define YAGF_CMD_ENABLEDI           (0x1b)
#define YAGF_CMD_FIRMWAREVERSION    (0x0e)
#define YAGF_CMD_RESET              (0x09)

#define YAGF_REPORT_PEAK            (0x1E)
#define YAGF_REPORT_DI              (0x1C)

// initialize gforce sensor
void yagf_init( config & dvrconfig );
void yagf_finish();

// input to host
void yagf_input( char * ibuf );

#endif      // __YAGF_H__
