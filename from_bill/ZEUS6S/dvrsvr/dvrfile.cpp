/* --- Changes ---
 *
 * 10/20/2009 by Harrison
 *   1. Fixed file encryption bug 
 *
 * 12/02/2009 by Harrison
 *   1. Fixed memory leak bug 
 *   2. No decryption on remote playback
 *
 */

#include "dvr.h"
#include <errno.h>
#include "mpheader.h"

// #define FILESYNCSIZE (0x200000)
#define FILETRUNCATINGSIZE (2*1024*1024)

static int file_bufsize;								// file buffer size
static int file_syncreq;
static int file_encrypt ;
//static int file_nodecrypt;                              // do not decrypt file when read
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table
static int file_repaircut=FILETRUNCATINGSIZE ;

// convert timestamp value to milliseconds
/*
inline int tstamp2ms(int tstamp )
{
    return tstamp/90;
}
*/
dvrfile::dvrfile()
{
    m_handle = NULL;
    m_khandle=NULL;
    m_filebuf = NULL;
    m_initialsize=0;
    m_filestamp = 0;
    m_timestamp = 0 ;
    m_syncsize=0 ;
    m_framepos=0 ;
}

dvrfile::~dvrfile()
{
    close();
}


int dvrfile::writeopen(const char *filename, char *mode,void* pheader)
{
    struct hd_file h264hd ;
    struct stat dvrfilestat ;
    close();
    m_initialsize = 0;
    m_handle = fopen(filename, mode);
    if (m_handle == NULL) {
        dvr_log("open file:%s failed",filename);
        return 0;
    }
    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_filelen=f264length(filename);
    m_filestamp=0;
    m_timestamp=0;
    m_filestart=(int)sizeof(struct hd_file);	// first data frame
    m_openmode=0;
    if (strchr(mode, 'w')) {
       // setvbuf(m_handle, (char *)NULL, _IOFBF, file_bufsize );
        file_syncreq = 1;
 
        seek(0,SEEK_SET);
        m_fileencrypt=file_encrypt ;

	struct File_format_info *pFileHead=(struct File_format_info*)pheader;
	
	//dvr_log("in dvrfile:width:%d height:%d framerate:%d",pFileHead->video_width,pFileHead->video_height,pFileHead->video_framerate);
        if( m_fileencrypt ) {
            memcpy( &h264hd, pheader, sizeof(h264hd));
            RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
            write( &h264hd, sizeof(struct hd_file));	// write header
        }
        else {
            write(pheader, sizeof(struct hd_file));// write header
        }
        m_filestart = tell();
        m_openmode=1;
        m_syncsize=0 ;
        //open the k file
        string keyfilename(m_filename) ;
        char * pk = keyfilename.getstring() ;
        int l=strlen(pk);
        if( strcmp( &pk[l-4], ".266")==0 ) {
            strcpy(&pk[l-4],".k");
            m_khandle=fopen( pk, "w" );
        }
    }
    return 1;  
  
}

int dvrfile::open(const char *filename, char *mode, int initialsize, int encrypt)
{
    struct hd_file h264hd ;
    struct stat dvrfilestat ;
    close();
    m_initialsize = 0;
    m_handle = fopen(filename, mode);
    if (m_handle == NULL) {
        dvr_log("open file:%s failed",filename);
        return 0;
    }
    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_filelen=f264length(filename);
    m_filestamp=0;
    m_timestamp=0;
    m_filestart=(int)sizeof(struct hd_file);	// first data frame
    m_openmode=0;
    if (strchr(mode, 'w')) {
        setvbuf(m_handle, (char *)NULL, _IOFBF, file_bufsize );
        file_syncreq = 1;
        if (initialsize > 0) {
            seek(initialsize - 1);
            if (fputc(0, m_handle) == EOF) {
                close();
                return 0;
            };
            m_initialsize = initialsize;
        }
        seek(0,SEEK_SET);
        m_fileencrypt=file_encrypt ;
        if( m_fileencrypt ) {
            memcpy( &h264hd, Dvr264Header, sizeof(h264hd));
            RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
            write( &h264hd, sizeof(struct hd_file));	// write header
        }
        else {
            write(Dvr264Header, sizeof(struct hd_file));// write header
        }
        m_filestart = tell();
        m_openmode=1;
        m_syncsize=0 ;
        //open the k file
        string keyfilename(m_filename) ;
        char * pk = keyfilename.getstring() ;
        int l=strlen(pk);
        if( strcmp( &pk[l-4], ".266")==0 ) {
            strcpy(&pk[l-4],".k");
            m_khandle=fopen( pk, "w" );
        }
    }
    if( strchr(mode, 'r')) {
     //   dvr_log("open file:%s to read",filename);
        if( fstat( fileno(m_handle), &dvrfilestat)==0 ) {
	    setvbuf(m_handle, (char *)NULL, _IOFBF, dvrfilestat.st_blksize );
            m_filesize=dvrfilestat.st_size ;
            if( m_filesize<64000 ) {   //128000
                dvr_log("opened file:%s is too small,size:%d",filename,m_filesize);
                close();	
                return 0 ;
            }
        }
        else {
            dvr_log("get file:%s status failed",filename);
            close();	
            return 0 ;
        }
        m_fileencrypt=-1 ;
        read(&h264hd, sizeof(h264hd) );
        m_fileheader = h264hd.flag;
     //   dvr_log("flag=%x",h264hd.flag);
        if( h264hd.flag == 0x544d4546 ) {
            m_fileencrypt=0 ;	// no encrypted
        }
        else {
            if( file_encrypt ) {
                RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
                if( h264hd.flag == 0x544d4546) {
                    m_fileencrypt=1 ;
                }
            }
        }
#if 0        
        if( m_fileencrypt<0 ) {
            dvr_log("file:%s header file error",filename);
            close();		
            return 0;
        }
#else
        if( m_fileencrypt<0 )
           m_fileencrypt=1;
#endif
        readkey();// read key frame index
       // printf("key array size:%d\n",m_keyarray.size());
        if( m_keyarray.size()>0 ) {
            seek( m_keyarray[0].koffset,SEEK_SET);
        }
        int type,frsize;
        type=frametypeandsize(&frsize);

      //  m_timestamp= m_keyarray[0].ktime;
        //framesize();	// update frame time stamp
        m_filestart=tell();
        m_filestamp=m_timestamp ;
    }

    return 1;
}

void dvrfile::close()
{
    int size;
    if (isopen()) {
        if( m_openmode ) {  // open for write
            size = tell();
            if (m_initialsize > size) {
                truncate(size);
            }
            if(m_khandle){
	       //fdatasync( fileno(m_handle) );
               fclose(m_khandle);
               m_khandle=NULL;
            }else{
		if( m_keyarray.size()>0 ) {
			writekey();
		}
            }
            file_syncreq = 1;
         //   sync();
	    fdatasync( fileno(m_handle) );
            if( fclose(m_handle)!=0 ) {
                dvr_log("Close file failed : %s %d",m_filename.getstring(),errno);
                //rec_basedir="" ;
            }
        }
        else {
            fclose(m_handle);
        }
        m_handle = NULL;
        if( m_filebuf!=NULL  ) {
            free( m_filebuf );
        }
    }
    m_keyarray.empty();
    m_initialsize=0;
    m_filestamp = 0;
    m_timestamp = 0 ;
    m_filebuf=NULL ;
}

int dvrfile::read(void *buffer, size_t buffersize)
{
    if( isopen() ) {
        return fread(buffer, 1, buffersize, m_handle);
    }
    else {
        return 0 ;
    }
}

int dvrfile::write(void *buffer, size_t buffersize)
{
    if( isopen() ) {
#if 0   
        m_syncsize+=buffersize ;
        if(m_syncsize>=file_bufsize) {
            fdatasync( fileno(m_handle) );
            m_syncsize=0 ;
        }
        //return fwrite(buffer, 1, buffersize, m_handle);
      return safe_fwrite(buffer, 1, buffersize, m_handle);
#else
      return safe_fwrite(buffer, 1, buffersize, m_handle);
#endif	
    }
    else {
        return 0;
    }
}

int dvrfile::tell() 
{
    if( isopen() ) {
        return ftell(m_handle);
    }
    else {
        return 0;
    }
}

int dvrfile::seek(int pos, int from) 
{
    if( isopen() ) {
        return fseek(m_handle, pos, from);
    }
    else {
        return 0 ;
    }
}

int dvrfile::truncate( int tsize )
{
    int res ;
    fflush(m_handle);
    res=ftruncate(fileno(m_handle), tsize);
    fflush(m_handle);
    return res ;
}

int dvrfile::readheader(void * buffer, size_t headersize) 
{
    int framepos ;
    
    if( headersize<sizeof(struct hd_file) ) {
        return 0 ;
    }
    
      // save old file pointer
    framepos = tell();
    
    seek(0);
    read( buffer, sizeof(struct hd_file));
    seek( framepos );
    return sizeof( struct hd_file );
}

int dvrfile::writeheader(void * buffer, size_t headersize) 
{
    int framepos ;
    
    if( headersize<sizeof(struct hd_file) ) {
        return 0 ;
    }
    
    // save old file pointer
    framepos = tell();
    seek(0);
    write( buffer, sizeof(struct hd_file));
    seek( framepos );
    return sizeof( struct hd_file );
}

// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe(void *buffer, size_t size, int keyframe, dvrtime * frametime)
{
    char * wbuf = (char *)buffer ;
    if( keyframe ) {
        dvr_key_t keytime ;
        keytime.koffset = tell();
        keytime.ktime = time_dvrtime_diffms(frametime, &m_filetime);
        if(m_khandle){
          	fprintf(m_khandle,"%d,%d\n",  keytime.ktime, keytime.koffset);
          	fflush(m_khandle);
        }
        else{
       	    m_keyarray.add(keytime);
	    if( m_keyarray.size()==1 ) {
		writekey();	// record first key frame pos
	    }
        }
    }
    if( m_fileencrypt ) {
        int i,mIndex;
        unsigned char nextbyte;
        unsigned int mValue;
        unsigned int audiofound;
        unsigned int mLength;
        int mEnsize=0;
        unsigned char * pbuf = (unsigned char *)buffer ;
        mValue=(unsigned int)pbuf[0]<<16| \
               (unsigned int)pbuf[1]<<8| \
               (unsigned int)pbuf[2];
        mIndex=3;
        audiofound=0;
        while(1){
           nextbyte=pbuf[mIndex];
           mIndex++;
           if(mIndex>=size)
              break;
           if(mValue==START_CODE){

              if(nextbyte==0x25){
                 break;
              } else if(nextbyte==0x21){
                 break;
              } else if(nextbyte==0xc0){
                audiofound=1;
                break;
              }
           }
           mValue=(mValue<<8)|(unsigned int)nextbyte;
        }
        if(audiofound){
           mIndex+=4;
           mLength=pbuf[mIndex];
           mIndex++;
           mIndex+=mLength;
           mEnsize=size-mIndex;
	  // dvr_log("audio enc size:%d size:%d\n",mEnsize,size);
        } else{
		mEnsize=size-mIndex;
		if(mEnsize>1024)
		mEnsize=1024;
	//	dvr_log("video enc size:%d size:%d index:%d\n",mEnsize,size,mIndex);
        }
        
        if(mEnsize>0)
            RC4_block_crypt(pbuf+mIndex, mEnsize, 0, file_encrypt_RC4_table, 1024);
    }

    if (size > 0) {
      if( write( wbuf, size )==(int)size ) {
        return 1;
      }
    }
    return 0;
}

// read one frame
int dvrfile::readframe(void * framebuf, size_t bufsize)
{
    //int encsize ;
    int rsize=0 ;
    if( bufsize>0 ){
       rsize = read(framebuf, bufsize);
       return rsize ;
    } else
       return 0;
}

#if 0
// read one frame, return 0 for error, 
int dvrfile::readframe(struct cap_frame *pframe)
{
    int frsize ;
    int type;
    type=frametypeandsize(&frsize);
    if(type==FRAMETYPE_UNKNOWN){
        return 0;
    }
    pframe->frametype=type;
    pframe->framesize=frsize;
    if( pframe->framesize<=0 ) {
        pframe->framesize = 4096 ;
    }
    pframe->framedata = (char *) mem_alloc(pframe->framesize);
    if (pframe->framedata == NULL)
        return 0;
    return read(pframe->framedata, pframe->framesize);
    //return readframe (pframe->framedata, pframe->framesize);
}
#endif
int dvrfile::getnextbyte(unsigned char* mbyte)
{
     int readbytes;
     if(mBuf_Index>=1024){
    	readbytes = read(mBuff,1024);
        if(readbytes<=0)
           return -1;
    	mBuf_Index=0;
     }
     *mbyte=mBuff[mBuf_Index];
     ++mBuf_Index;
     return 0;
}
int dvrfile::skipnextnbytes(int n)
{
   int readbytes;
   mBuf_Index+=n;
   if(mBuf_Index>=1024){
    	readbytes = read(mBuff,1024);
        if(readbytes<=0)
           return -1; 
    	mBuf_Index-=1024;
   }
   return 0;
}

int dvrfile::frametypeandsize(int* size)
{
    int readbytes;
    int framepos ;
    int curpos;
    unsigned int mValue;
    int type=FRAMETYPE_UNKNOWN;
    unsigned char nextbyte;
    unsigned int mPesLength;
    unsigned int mHigh;
    unsigned int mLow;
    int framesize;
    framepos = tell();
    if( framepos >= m_filesize ) {
        framepos = m_filesize;
    }        
    
    if( framepos < m_filestart ) {
        framepos=m_filestart ;
    }
    readbytes = read(mBuff,1024);
    if(readbytes<1024){
      *size=0;
      return type;
    }
    mBuf_Index=3;
    mValue=(unsigned int)mBuff[0]<<16 | \
           (unsigned int)mBuff[1]<<8 | \
           (unsigned int)mBuff[2];
    while(1){
         if(getnextbyte(&nextbyte)<0)
            break;
         if(mValue==START_CODE){
	    if(nextbyte==0xe0){
               if(getnextbyte(&nextbyte)<0)
                   break;
               mHigh=(unsigned int)nextbyte;
               if(getnextbyte(&nextbyte)<0)
                   break;
               mLow=(unsigned int)nextbyte;
               mPesLength=mHigh<<8|mLow;
               if(mPesLength>1024){
		   type=FRAMETYPE_VIDEO;
		   break;
               }
            } else if(nextbyte==0xc0){
               if(getnextbyte(&nextbyte)<0)
                   break;
               mHigh=(unsigned int)nextbyte;
               if(getnextbyte(&nextbyte)<0)
                   break;
               mLow=(unsigned int)nextbyte;
               mPesLength=mHigh<<8|mLow;
	       type=FRAMETYPE_AUDIO;
		   break;
            }
         }
         mValue=((mValue<<16)>>8)|(unsigned int) nextbyte;
    }
    if(type==FRAMETYPE_UNKNOWN){
        seek(framepos);
        *size=0;
        return type;
    }
    curpos=tell()-1024+mBuf_Index+mPesLength;
    if(curpos> m_filesize)
       curpos=m_filesize;
    *size=curpos-framepos;
    seek(framepos);
    return type;
}

#if 0
// return frame type
//      0 - no frame
//      1 - keyframe
//     >1 - other frame
int dvrfile::frametype()
{
    struct hd_frame frame;
    int readbytes;
    int framepos ;
    
    framepos = tell();
    
    if( framepos >= m_filesize ) {
        framepos = tell();
    }        
    
    if( framepos < m_filestart ) {
        framepos=m_filestart ;
    }
    if ((framepos & 3) != 0) {
        return FRAMETYPE_UNKNOWN ;
    }
    
    readbytes = read((char *) &frame, sizeof(frame));
    
    seek(framepos);				// set back old position
    if (readbytes == sizeof(frame) && 
        frame.flag == 1 && 
        HD264_FRAMESUBFRAME(frame) < 10 &&
        frame.framesize < 1000000) 
    {
        switch ( HD264_FRAMETYPE(frame) ) {
            case 3 :
                return FRAMETYPE_KEYVIDEO;
            case 4:
                return FRAMETYPE_VIDEO ;
            case 1:
                return FRAMETYPE_AUDIO ;
            default:
                return FRAMETYPE_UNKNOWN;
        }
    }
    return FRAMETYPE_UNKNOWN;
}

// current frame size
int dvrfile::framesize()
{
    struct hd_frame frame;
    struct hd_subframe subframe;
    int framepos ;
    int subframes;
    int frsize;
    
    frsize=0;
    if (frametype() > 0) {
        framepos = tell();
        if( read(&frame, sizeof(frame)) != sizeof(frame)){
            seek(framepos);	
            return 0;
        }
        m_timestamp=frame.timestamp ;								// assign time stamp
        subframes =  HD264_FRAMESUBFRAME(frame) ;
        seek(frame.framesize, SEEK_CUR);
        while (--subframes > 0) {
            subframe.d_flag = 0;
            if( read(&subframe, sizeof(subframe))!= sizeof(subframe) ) {
                seek( framepos );
                return 0;
            }
            if (HD264_SUBFLAG(subframe)!= 0x1001 && HD264_SUBFLAG(subframe)!= 0x1005) {
                break;
            }
            seek(subframe.framesize, SEEK_CUR);
        }
        if (subframes == 0)  {
            frsize=tell()-framepos ;
        }
        seek(framepos);
    }
    return frsize;	
}
#endif
// return current frame time
dvrtime dvrfile::frametime()
{
    int i=0 ;
    int pos ;
    unsigned int timeoffset=0;
    dvrtime t=m_filetime ;
    if( m_keyarray.size()>0 ) {                         // key frame index available
        pos = tell();
#if 0	
	for(i=m_keyarray.size()-1;i>=0;--i)
	{
	   if( m_keyarray[i].koffset<=pos ){
	     timeoffset=m_keyarray[i].ktime;
	   }	  
	}
#endif	
        for( i=0; i<m_keyarray.size(); i++ ) {
            if( m_keyarray[i].koffset>=pos ) {
	        if(i>0){
		   timeoffset=m_keyarray[i-1].ktime;
		} else {
                  timeoffset=m_keyarray[i].ktime;
		}
                break;
            }
        }
    }
    if(timeoffset==0&&i==m_keyarray.size()){
      timeoffset=m_keyarray[i-1].ktime;
    }
    time_dvrtime_addms(&t, timeoffset );
    return t;
}	

#define SEARCHBUFSIZE	(0x10000)

int dvrfile::nextframe()
{
    int frsize;
    int type;
    type=frametypeandsize(&frsize);
    if (type!=FRAMETYPE_UNKNOWN) {
        seek(frsize, SEEK_CUR );
        return tell();
    } 
    return 0;
}

#if 0
int dvrfile::prevframe()
{
    int pos ;
    int j, i, readbytes;
    int *searchbuf;
    pos = tell();
    pos &= (~3);
    seek(pos, SEEK_SET);
    searchbuf = (int *) mem_alloc(SEARCHBUFSIZE);	// 64Kbytes
    for (j = 0; j < 32; j++) {	// search 32 blocks, 2Mbytes
        pos -= SEARCHBUFSIZE;
        if (pos < m_filestart) {
            break;
        }
        seek(pos,SEEK_SET);
        readbytes = read(searchbuf, SEARCHBUFSIZE);
        if (readbytes < 16) {	// probably file error
            break;
        }
        for (i = readbytes / (sizeof(int)) - 1; i >= 0; i--) {
            if (searchbuf[i] == 1) {
                // find it
                seek(pos+ i * (sizeof(int)), SEEK_SET );
                if( framesize() ) {		// find a correct frame
                    mem_free(searchbuf);
                    return tell();
                }
            }
        }
    }
    mem_free(searchbuf);
    seek(0, SEEK_SET);
    return 0;					// could not find, file error!
}
#endif
int dvrfile::nextkeyframe()
{
    int i ;
    int pos ;
    if( m_keyarray.size()>0 ) {                         // key frame index available
        pos = tell();
        for( i=0; i<m_keyarray.size(); i++ ) {
            if( m_keyarray[i].koffset>pos ) {
                seek(m_keyarray[i].koffset, SEEK_SET);
                return (m_keyarray[i].koffset);
            }
        }
    }
    return 0;					// could not found
}

int dvrfile::prevkeyframe()
{
    int i ;
    int pos ;
    if( m_keyarray.size()>0 ) {                         // key frame index available
        pos = tell();
        for( i=m_keyarray.size()-1; i>=0; i--) {
            if( m_keyarray[i].koffset<pos ) {
                seek(m_keyarray[i].koffset, SEEK_SET);
                return (m_keyarray[i].koffset);
            }
        }
        return 0 ;

    }
    return 0;
}

void dvrfile::readkey()
{
    string keyfilename(m_filename) ;
    char * pk = keyfilename.getstring() ;
    int l=strlen(pk);
    FILE * keyfile = NULL;
    m_keyarray.empty();
    if( strcmp( &pk[l-4], ".266")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=fopen( pk, "r" );
	if(!keyfile){
	      string kname=keyfilename;
	      char *strp=strstr(kname.getstring(),"_L_");
	      if(strp){
		*(strp+1)='N'; 
	      } else {
		strp=strstr(kname.getstring(),"_N_");
		if(strp){
		  *(strp+1)='L'; 
		}
	      }
	      if(strp){
		keyfile=fopen(kname.getstring(),"r");
	      }	      	      	  	  
	}
        if( keyfile ) {
            struct dvr_key_t key;
            while(fscanf(keyfile,"%d,%d\n", &(key.ktime), &(key.koffset))==2){
                m_keyarray.add(key);
            }
            fclose(keyfile);
        }
    }
}

void dvrfile::writekey()
{
    string keyfilename(m_filename) ;
    char * pk = keyfilename.getstring() ;
    int l=strlen(pk);
    int i;
    FILE * keyfile=NULL ;
    if( m_keyarray.size()==0 ) {
        return;
    }
    if( strcmp(&pk[l-4], ".266")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=fopen( pk, "w" );
        if( keyfile ) {
            for( i=0;i<m_keyarray.size();i++) {
                struct dvr_key_t * pkey ;
                pkey = m_keyarray.at(i);
                fprintf(keyfile,"%d,%d\n", pkey->ktime, pkey->koffset);
            }
            fclose(keyfile);
        }
    }
}

// return 1: seek success, 0: out of range
int dvrfile::seek( dvrtime * seekto )
{
    int i;
    int seekms ; // seek in milliseconds frome time of file
    int seeks;
    if( !isopen() ) {
        return 0;
    }
    if( *seekto < m_filetime ) {
        seek(m_filestart, SEEK_SET );
        return 0;
    }
#if 0
    dvr_log("filetime:day:%d hour:%d minute:%d second:%d",\
	    m_filetime.day,m_filetime.hour,m_filetime.minute,m_filetime.second);
    dvr_log("seekto:day:%d hour:%d minute:%d second:%d millisec:%d",\
            seekto->day, seekto->hour,seekto->minute,seekto->second,seekto->milliseconds);
    dvr_log("file length:%d",m_filelen);    
#endif   
    seeks = time_dvrtime_diff( seekto, &m_filetime );
    if( seeks >= m_filelen ) {
        seek(0, SEEK_END );
        return 0 ;
    }

   // seekms=1000*seeks;
    seekms=time_dvrtime_diffms( seekto, &m_filetime );
 //   dvr_log("seekms:%d",seekms);
    if( m_keyarray.size()>0 ) {         
      // key frame index available
#if 0      
        for(i=m_keyarray.size()-1;i>=0;i--){
	   if(m_keyarray[i].ktime<=seekms){
	      break; 
	   }	  
	}
	if(i<0) i=0;
	if(m_keyarray[i].ktime==seekms){
          seek(m_keyarray[i].koffset, SEEK_SET );
	}
	else {
	  if(i==(m_keyarray.size()-1)){
	    
	  } else
	  seek(m_keyarray[i+1].koffset,SEEK_SET);
	}
	dvr_log("i:%d ktime:%d",i,m_keyarray[i].ktime);
        return 1 ;
#else
       for(i=0;i<m_keyarray.size();++i){
	  if(m_keyarray[i].ktime>=seekms)
	    break;	 
       }
       if(i<m_keyarray.size()){
	   seek(m_keyarray[i].koffset, SEEK_SET );
         //  dvr_log("i:%d ktime:%d arraysize:%d return 1",i,m_keyarray[i].ktime,m_keyarray.size());	   
	   return 1;
       } else {
	  seek(0, SEEK_END );
	//  dvr_log("i:%d ktime:%d arraysize:%d return 0",i,m_keyarray[i].ktime,m_keyarray.size());	
	 return 1; 
       }
#endif
    }

   // dvr_log("return to file start");
    seek(m_filestart, SEEK_SET );
    return 0;

   #if 0
    // seek to end of file
    seek(0, SEEK_END);
    int filesize = tell();
    
    int pos = (filesize/m_filelen) * (seekms/1000) ;
    pos &= ~3; 
    seek(pos, SEEK_SET );
    
    if (nextkeyframe() == 0){
        seek(0, SEEK_END );                  // seek to end of file
        return 1 ;
    }
    int type,frsize;
    type=frametypeandsize(&frsize);
    //framesize();    // this will get current frame timestamp
    if( tstamp2ms(m_timestamp - m_filestamp) > seekms ) {
        // go backward
        while( prevkeyframe() ) {
            framesize();	// update timestamp
            if( tstamp2ms(m_timestamp - m_filestamp) <= seekms ) {
                // got it
                return 1 ;
            }
        }
        seek(0, SEEK_SET );                  // seek to begin of file
    }
    else {
        // go forward
        pos = tell();
        while( nextkeyframe() ) {
            framesize();
            if( tstamp2ms(m_timestamp - m_filestamp) > seekms ) {
                // got it
                break;
            }
            pos=tell();
        }
        seek(pos, SEEK_SET );
    } 
    return 1 ;
#endif
}

// repair a .264 file, return 1 for success, 0 for failed
#if 1
int dvrfile::repair()
{
    int filepos, filelength;
    char newfilename[512] ;
    char tBuffer[1024];
    char * rn ;
    char * tail ;
    int keyindex=0;
    int readbytes;
    if(m_keyarray.size()<30||m_filesize<file_repaircut+512000){
      dvr_log("key size is small:%d\n",m_keyarray.size());
      return 0;
    }
    filepos=0;
    filelength=0;
    m_filesize-=file_repaircut;
    for(keyindex=0;keyindex<m_keyarray.size();++keyindex){
       if(m_keyarray[keyindex].koffset>=m_filesize)
	 break;
       seek(m_keyarray[keyindex].koffset, SEEK_SET);
       filepos=tell();
       if(filepos!=m_keyarray[keyindex].koffset){
          break;
       }
       readbytes = read(tBuffer,1024);
       if(readbytes!=1024)
	 break;
    }
    if(keyindex<30){
      return 0;
    }
    truncate(m_keyarray[keyindex-1].koffset);
    filelength=m_keyarray[keyindex-2].ktime/1000;
    m_keyarray.setsize(keyindex-2);
    m_openmode=1;
    close();
    strcpy( newfilename, m_filename.getstring() );
    rn = strstr(newfilename, "_0_");
    if( rn ) {
        tail = strstr(m_filename.getstring(), "_0_")+3 ;
        sprintf( rn, "_%d_%s", filelength, tail );
        if( rename( m_filename.getstring(), newfilename )>=0 ) {         // success
            return 1;
        }
    }
    dvr_log("repair failed\n");
    return 0;
}
#endif
int dvrfile::repairpartiallock()
{
    dvrfile lockfile ;
    struct dvrtime breaktime ;
    int breakindex ;
    char lockfilename[300] ;
    char lockfilename1[300] ;
    char * lockfilenamebase ;
    int i;
    int locklength ;
    int busywait ;
    
    locklength = f264locklength( m_filename.getstring() );
    
    if( locklength >= m_filelen || locklength<2 ) {        // not a partial locked file
        return 0 ;
    }
    
    if( m_keyarray.size()<=2 ) {            // no key index ?
        return 0 ;
    }
    
    for( breakindex=m_keyarray.size()-2; breakindex>=0; breakindex-- ) {
        if( m_keyarray[breakindex].ktime < (m_filelen-locklength)*1000 ) {
            breakindex++;	  
            break;
        }
    }
   // dvr_log("inside repair lock");
    breaktime = m_filetime ;
    time_dvrtime_addms( &breaktime, m_keyarray[breakindex].ktime );
    
    strcpy( lockfilename, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_L_%s.266", 
                breaktime.year,
                breaktime.month,
                breaktime.day,
                breaktime.hour,
                breaktime.minute,
                breaktime.second,
                g_hostname);
    lockfile.open(lockfilename, "wb");
    if( !lockfile.isopen() ) {
        return 0 ;
    }
   // dvr_log("file:%s is opened",lockfilename);
    char lockfilehead[40] ;
    
    readheader( (void *)lockfilehead, 40 );
    lockfile.writeheader( lockfilehead, 40 );
    lockfile.m_fileencrypt=0 ;
    
    for( i=breakindex ; i<m_keyarray.size(); i++ ) {
        // if rec_busy, delay for 0.1 s
	/*
        for( busywait=0; busywait<10; busywait++) {
            if( rec_busy==0 ) break ;
            usleep(1000);
        }*/
        
        char * framebuf ;
        int framesize ;
        int framepos ;
        struct dvrtime frametime ;
        
        framepos = m_keyarray[i].koffset ;
        if( i<m_keyarray.size()-1 ) {
            framesize = m_keyarray[i+1].koffset - m_keyarray[i].koffset ;
        }
        else {
            framesize = m_filesize - m_keyarray[i].koffset ;
        }
        if(framesize<0)
	  break;
	if(framesize>10*1024*1024)
	  break;
      //  dvr_log("i=%d keysize:%d framesize=%d",i,m_keyarray.size(),framesize);
      //dvr_log("%d:%d",m_keyarray[i+1].koffset,m_keyarray[i].koffset);
        frametime = m_filetime ;
        time_dvrtime_addms( &frametime, m_keyarray[i].ktime );
        
        framebuf = (char *) malloc ( framesize ) ;
        seek( framepos );
        read( framebuf, framesize );
        lockfile.writeframe( framebuf, framesize, 1, &frametime );
        free( framebuf );
        
    }
    
    lockfile.close();
 //   dvr_log("lock file is closed");
    // rename new locked file
    strcpy( lockfilename1, lockfilename );
    lockfilenamebase = basefilename( lockfilename1 );
    
    locklength = m_keyarray[breakindex].ktime / 1000 ;
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_L_%s.266", 
            breaktime.year,
            breaktime.month,
            breaktime.day,
            breaktime.hour,
            breaktime.minute,
            breaktime.second,
            m_filelen-locklength,
            g_hostname);
    
    dvrfile::rename( lockfilename, lockfilename1 );
    
    // close original file
    seek( m_keyarray[breakindex].koffset ) ;
    m_initialsize = tell() + 1 ;       // set a fake init size, so close() will do file truncate
    m_keyarray.setsize(breakindex);
    m_openmode = 1 ;                    // set open mode to write, so close() would save key file
    close();
    
    dvr_log( "Event locked file breakdown success. (%s)", lockfilename1 );

    // rename old locked filename
    strcpy( lockfilename1, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename1 );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_N_%s.266", 
            m_filetime.year,
            m_filetime.month,
            m_filetime.day,
            m_filetime.hour,
            m_filetime.minute,
            m_filetime.second,
            locklength,
            g_hostname);
   
    usleep(1000);	    
    dvrfile::rename( m_filename.getstring(), lockfilename1 );
    disk_renew( lockfilename1 );
    return 1 ;
}
   
// rename .264 file as well .idx file
int dvrfile::rename(const char * oldfilename, const char * newfilename)
{
    int res ;
    char oldkfile[512], newkfile[512] ;
    int  lo, ln ;
    res = ::rename( oldfilename, newfilename );
    strcpy( oldkfile, oldfilename );
    strcpy( newkfile, newfilename );
    lo = strlen( oldkfile );
    ln = strlen( newkfile );
    if( strcmp( &oldkfile[lo-4], ".266" )== 0 &&
       strcmp( &newkfile[ln-4], ".266" )== 0 ) {
           strcpy( &oldkfile[lo-4], ".k" );
           strcpy( &newkfile[ln-4], ".k" );
           ::rename( oldkfile, newkfile );
    }
    return res;
}

// remove .264 file as well .idx file
int dvrfile::remove(const char * filename)
{
    int res ;
    char kfile[256] ;
    int l ;
    res = ::remove( filename );
    strcpy( kfile, filename );
    l = strlen( kfile );
    if( strcmp( &kfile[l-4], ".266" ) == 0 ) {
        strcpy( &kfile[l-4], ".k" ) ;
        ::remove( kfile );
    }
    return res ;
}
DWORD dvrfile::getfileheader()
{
  	return m_fileheader ;
}
/*
int  dvrfile::getfileheaderdata(char* pBuf)
{
    if(pBuf==NULL)
      return 0;
    memcpy(pBuf,h264hd,sizeof(struct hd_file));
    return sizeof(struct hd_file);
}
*/

void file_sync()
{
    if (file_syncreq) {
        file_syncreq = 0;
        sync();
    }
    //	sync();
}

void file_init()
{
    int iv ;
    string v;
    config dvrconfig(dvrconfigfile);
    unsigned char file_filekey[260] ;
    
    file_encrypt=dvrconfig.getvalueint("system", "fileencrypt");
    v = dvrconfig.getvalue("system", "filepassword");
    if( v.length()>=342 ) {
        c642bin(v.getstring(), file_filekey, 256);
        RC4_crypt_table( file_encrypt_RC4_table, 1024, file_filekey);
    }
    else {
        file_encrypt=0;
    }
    
    v=dvrconfig.getvalue("system", "filebuffersize");
    char unit='b' ;
    file_bufsize=(64*1024) ;
    int n=sscanf(v.getstring(), "%d%c", &file_bufsize, &unit);
    if( n==2 && (unit=='k' || unit=='K') ) {
        file_bufsize*=1024 ;
    }
    else if( n==2 && (unit=='M' || unit=='m' ) ) {
        file_bufsize*=1024*1024 ;
    }
    if( file_bufsize<8192 ) file_bufsize=8192 ;
    if( file_bufsize>(4*1024*1024) ) file_bufsize=(4*1024*1024) ;
    
    //file_nodecrypt=dvrconfig.getvalueint("system", "file_nodecrypt");

    iv=dvrconfig.getvalueint("system", "file_repaircut");
    if( iv>=8192 && iv<=(8*1024*1024) ) {
        file_repaircut=iv;
    }
}

void file_uninit()
{
    sync();
}
