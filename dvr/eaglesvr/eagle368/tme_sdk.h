#ifndef TME_368_SDK_
#define TME_368_SDK_
#ifdef __cplusplus
extern "C"{
#endif

//#include <time.h>

#define MODE_CBR 0 
#define MODE_VBR 1

#define MAIN_CHANNEL 1 
#define SUB_CHANNEL  0 

#define STREAM_TYPE_VIDEO 	1	
#define STREAM_TYPE_AUDIO 	2	
#define STREAM_TYPE_AVSYN 	3	

#define FRMAE_TYPE_VIDEO_I      1
#define FRAME_TYPE_VIDEO_P      2
#define FRAME_TYPE_AUDIO        3

enum picture_format {
	ENC_QCIF =2,
	ENC_CIF = 1,
	ENC_2CIF =4,
	ENC_DCIF =0,
	ENC_4CIF =3,
	ENC_D1 = 0xf
};

typedef enum {
	BOARD_TYPE_DM365_DVR,
	BOARD_TYPE_DM368_DVR,
	BOARD_TYPE_DVEVM
} board_type_enum;

typedef struct {
	unsigned int  board_type;
	unsigned int  enc_chan; 		// number of encoding channels
	unsigned int  dec_chan; 		// number of decoding channels
} board_info;

enum video_standard
{
   STANDARD_NTSC=0,
   STANDARD_PAL,
};  

struct RECT 
{
	int left;
	int top;
	int right;
	int bottom;
};

struct RECT_EX
{
       int x,y,w,h;
};

struct SYSTEMTIME
{
	unsigned short year;
	unsigned short month;
	unsigned short dayofweek;
	unsigned short day;
	unsigned short hour;
	unsigned short minute;
	unsigned short second;
	unsigned short milliseconds;
};

typedef struct
{
    int channel;  //channel handle
    int size;     //buffer size
    int frameType;   //frame type, it can be one of FRMAE_TYPE_VIDEO_I,FRMAE_TYPE_VIDEO_P,FRMAE_TYPE_AUDIO
    void * pBuf;     //the buffer pointer
    int t_sec;       //seconds part of time stamp. 
    int t_msec;      //micro seconds part of time stamp
}CALLBACK_DATA;

typedef void (*STREAM_DATA_CALLBACK)(CALLBACK_DATA CallBackData,void* context);

int RegisterStreamDataCallback(STREAM_DATA_CALLBACK func,void* context);

/* Notes for GetBoardInfo:
   Get board information, the return value is always true;
*/
int GetBoardInfo( board_info *info );
/* Notes for SetVideoParam:
   Set the video parameters in the applicable TVP5158 video decoder
   Inputs:
   int channel:   channel handle , minimum value is 0
   int brightness: brightness value is among 0 to 255, the deafult is 128
   int contrast: constrast value is among 0 to 255, the default is 128
   int saturation: saturation value is among 0 to 255, the default is 128
   int hue: hue value is among -128 to 128, the default is 0
*/
int SetVideoParam(int channel, int brightness,int contrast, int saturation, int hue);

/* Notes for GetVideoParam
   Get the video parameters in the TVP5158 video decoder
   Outputs:
   int channel:   channel handle , minimum value is 0
   int brightness: brightness value is among 0 to 255
   int contrast: constrast value is among 0 to 255
   int saturation: saturation value is among 0 to 255
   int hue: hue value is among -128 to 128
*/
int GetVideoParam(int channel, int* brightness, int*contrast, int*saturation,int* hue);

/* Notes for SetBitrateControl
    Sets up maximum bit rate. 
	Bitrate can also be controlled as VBR or CBR.
   INPUTS:
    int channel :   channel handle, minumum value is 0
    unsigned int max_bps : maximum bit rate
    bitrate_control_enum brc  // either MODE_VBR or MODE_CBR   

*/
int SetBitrateControl(int channel,int chan_type, unsigned int max_bps,int brc );

/* Notes for SetStreamType
    Sets up stream typede 
    INUPUTS:
    int channel:   channel handle, minimum value is 0
    int chan_type: channel type
    int type:   Stream type , its value can be STREAM_TYPE_VIDEO, STREAM_TYPE_AUDIO,and STREAM_TYPE_AVSYN
*/
int SetStreamType(int channel,int chan_type, int type);

/* Notes for Set SetIBPMode 
   Set up parameter for encoder
   INPUTS:
   int channel:   channel handle
   int chan_type:  channel type
   int key_frame_inter:  the interval of between two key frames
   int b_frames:   b frame numbers
   int frame_rate: frame rate

*/
int SetIBPMode(int channel,  int chan_type,int key_frame_inter, int b_frames,int frame_rate);

/* Notes for SetEncoderPictureFormat
   Set up the picture format 
   INPUTS:
   int channel:   channel handle
   int chan_type: channel type 
   int format:   the format of picture

*/
int SetEncoderPictureFormat(int channel, int chan_type,int format);
/* Notes for SetVideoInputStandard
   Set up video input standard
   INPUTS:
   int standard:   the standard of video input, STANDARD_NTSC for NTSC, STANDARD_PAL for PAL

*/
int SetVideoInputStandard(int standard);

/* Notes for SetDisplayParams
   Set Display parameters
   INPUTS:
   int screen:  display layout configuration, 1 for 1-screen layout,4 for 4-screenlayout
*/
int SetDisplayParams(int screen);

/* Notes for SetPreviewScreen
    Set Preview channel
    INPUTS:
    int channel: channel handle
    int mode: 1-open, 0-close
*/
int SetPreviewScreen(int channel,int mode);

/*Notes for  SetPreviewAudio
    Set up Preview Audio channel
    INPUTS:
    int channel:  channel handle
    int mode: 1-open,0-close;
*/
int SetPreviewAudio(int channel,int mode);

/*Notes for SetOutputVolume
   Set up output volume
   INPUTS:
   int channel: preview channel handle
   int volume:  the volume of audio is among 0 to 16

*/
int SetOutputVolume(int channel,int volume);

/*Notes for InitSystem
  Initialize system global resource, it must be called before other API
  Return value: true is 0 ,otherwise -is -1;

*/
int InitSystem(void);
/*Notes for FiniSystem
  Release system global resource. It must be called when exit
  Return value: true is 0 , otherwise is -1;
*/
int FiniSystem(void);

/* Notes for GetVideoSignal
   Get Video Signal 
   Return value:  1 has signal, 0 no signal
*/
int GetVideoSignal(int channel);

int SystemStart(void);

int SystemStop(void);

/* Notes for  StartCodec
   Start compression on channel
   INPUTS:
   int channel:  channel handle
   int channel_type:  the type of channel
*/
int StartCodec(int channel, int chan_type);
/* Notes for StopCodec
   Start compression on channel
   INPUTS:
   int channel:  channel handle
   int channel_type:  the type of channel   
*/
int StopCodec(int channel, int chan_type);

/*Notes for SetDefaultQuant
    Set Default image quality on channel
    INPUTS:
    int channel:  channel handle
    int channel_type:  the type of channel 
    int quality: I frame quality coefficient
*/
int SetDefaultQuant(int channel,int chan_type,int quality);

/*Notes for SetMotionDetection
   Set sensitity for motion detection
   INPUTS:
   int channel: channel handle
   int sensitity: the value of sensitivity level, the value is among 0 -9. 0 is highest sensitive and 9 is the lowest sensitive.
   
*/
int SetMotionDetection(int channel, int sensitity);

/*Notes for EnalbeMotionDetection
    Enable the Motion Detection for channel
    INPUTS:
    int channel: channel handle
    int enable: 1 enable,0 disable
  
*/
int EnalbeMotionDetection(int channel, int enable);
/*Notes for GetChannelMotionStatus
   Get the motion status for channel
   INPUTS:
   int channel: channel handle
*/
int GetChannelMotionStatus(int channel);

int SetOSDDisplayMode(int channel, int brightness, int translucent, int param, int line_count,unsigned short**format);
int EnableOSD(int channel, int enable);
int GetVideoWidth(int channel);
int GetVideoHight(int channel);

int SetProgressiveCamera(int channel,int progressive);
/*Notes for GetJPEGImage
   Get JPEG format picture
   INPUTS:
   int channel:  channel handle
   int quality: JPEG picture quality (0-best,1-better,2-average)
   
*/
int GetJPEGImage(int channel,int quality,int mode,unsigned char* image,unsigned int* size);

/*Notes for SetDecodeScreen
   Set screen for decode
   INPUTS:
   int index: 0--main output 
   int channel: channel handle
   int mode: 1--open 0--close
*/
int SetDecodeScreen(int index, int channel, int mode);

/*Notes for SetDecodeAudio
   Set audio channel for decode
   INPUTS:
   int index: 0--main output 
   int channel: channel handle
   int mode: 1--open 0--close
*/
int SetDecodeAudio(int index, int channel, int mode);

/* Notes for StartDecode
   Start decode
   INPUTS:
   int index: 0--main output 
   int channel: channel handle
   int mode: 1--File mode  2--live stream mode  

*/
int StartDecode(int index, int channel, int mode, void * file_header);

/*Notes for StopDecode
  Stop decode
    INPUTS:
    int handle:  playback handle
*/
int StopDecode(int handle);
/*Notes for SetDecodeSpeed
  Set speed for decoder
  not support yet
   
*/

int SetDecodeSpeed(int handle, int speed);

/* Notes for SetDecodeVolume
   Adjust playback audio volume
   INPUTS:
   int handle:  playback channel
   int volume: 0-16, 0--no audio and 16--maximum

*/
int SetDecodeVolume(int handle, int volume);

/* Notes for DecodeNextFrame
   start to decode next video frame
   INPUTS:
   int handle: channel handle
*/
int DecodeNextFrame(int handle);

/*Notes for InputAvData
   Write data to decoder
   INPUTS:
   int handle:  channel handle
   void* buf:   data buffer
   int size:   data buffer size;
*/
int InputAvData(int handle, void* buf, int size);


#ifdef __cplusplus
}
#endif

#endif
