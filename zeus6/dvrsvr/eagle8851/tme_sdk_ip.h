#ifndef TME_SDK_IP_
#define TME_SDK_IP_
#ifdef __cplusplus
extern "C"{
#endif	

#define FRMAE_TYPE_VIDEO_I      1
#define FRAME_TYPE_VIDEO_P      2
#define FRAME_TYPE_AUDIO        3


int RegisterIPStreamDataCallback(STREAM_DATA_CALLBACK func,void* context,int channel);

int InitIPSystem(void);
/*Notes for FiniSystem
  Release system global resource. It must be called when exit
  Return value: true is 0 , otherwise is -1;
*/
int FiniIPSystem(void);
/* Notes for SetIPAddress
 * set IP address for IP channel
 */
int SetIPAddress(char* ip,int channel);
int SetStreamURL(char* ip,int channel);

/* Notes for GetVideoSignal
   Get Video Signal 
   Return value:  1 has signal, 0 no signal
*/
int GetIPVideoSignal(int channel);

int IPSystemStart(int channel);

int IPSystemStop(int channel);
void IPCameraEnable(int channel);

int GetIPVideoWidth(int channel);
int GetIPVideoHight(int channel);
int GetIPVideoFrameRate(int channel);
/*resolution
 * 0-640x480
 * 1-360x240
 * 2-180x120
 * 3-720x480
 * 4-720x240
 * 5-1280x720 (720p)
 * 6-1920x1080 (1080p)
 */
void SetIPVideoResolution(int channel,int resolution);
void SetIPVideoFrameRate(int channel,int rate);
void SetIPVideoBitRate(int channel,int bitrate);
void SetIPVideoKeyframeInterval(int channel,int interval);

#ifdef __cplusplus
}
#endif

#endif

