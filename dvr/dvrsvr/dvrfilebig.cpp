
#include "dvr.h"
#include "dir.h"

#define FILETRUNCATINGSIZE (2*1024*1024)

static int file_bufsize;								// file buffer size
static int file_encrypt ;
static int file_nodecrypt;                              // do not decrypt file when read
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table
static int file_repaircut=FILETRUNCATINGSIZE ;

unsigned char g_filekey[256] ;

#define F264FILEFLAG    (0x484b4834)
#define F264FRAMESYNC   (0x01000000)
#define F264EXT         ".264"

#define F265FILEFLAG    (0x484b4d49)
#define F265FRAMESYNC   (0x000001BA)
#define F265EXT         ".265"

#define F266FILEFLAG    (0x544d4546)
#define F266FRAMESYNC   (0x000001BA)
#define F266EXT         ".266"

#ifdef EAGLE32
const char g_264ext[]=F264EXT;
#define H264FILEFLAG  F264FILEFLAG
#define FRAMESYNC     F264FRAMESYNC

// convert timestamp value to milliseconds
inline int tstamp2ms(int tstamp )
{
    return tstamp*15+tstamp*5/8;	// 	tstamp * 1000 / 64;  ( to preview overflow)
}

#endif

#ifdef EAGLE34
const char g_264ext[]=F265EXT;
#define H264FILEFLAG  F265FILEFLAG
#define FRAMESYNC     F265FRAMESYNC
#endif

#ifdef EAGLE368
const char g_264ext[]=F266EXT ;
#define H264FILEFLAG  F266FILEFLAG
#define FRAMESYNC     F266FRAMESYNC
#endif

const char g_kext[]=".k" ;

dvrfile::dvrfile()
{
    m_handle = NULL;
    m_keyfile = NULL ;
    m_filebuffer = NULL ;
    m_filebufferpos = 0 ;
    m_fileencrypt = 0 ;
    m_openmode = 0 ;
}

dvrfile::~dvrfile()
{
    close();
}

int dvrfile::open(const char *filename, char *mode, int initialsize)
{
    struct hd_file h264hd ;
    struct stat dvrfilestat ;
    int channel ;
    close();
    m_initialsize = 0;
    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_filelen=f264length(filename);
    m_filestart=(int)sizeof(struct hd_file);	// first data frame
    m_openmode=0;
	if (strchr(mode, 'w')) {
		struct stat fstat ;
		if( stat(filename, &fstat)==0 ) {		// file existed!
			m_handle = fopen(filename, "r+");	// open as read+write
		}
		else {
			m_handle = fopen(filename, mode);
		}
		if (m_handle == NULL) {
			return 0;
		}
		
		m_fileencrypt=file_encrypt ;
		seek(0, SEEK_END) ;
		
		if( tell()<100) {
			// new file
	        seek(0,SEEK_SET);
			channel = f264channel(filename) ;
			memcpy( &h264hd, cap_fileheader(channel), sizeof(h264hd) );
			m_hdflag = h264hd.flag ;

			if( m_fileencrypt ) {
				RC4_block_crypt( (unsigned char*)&h264hd, (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
			}
			write( &h264hd, sizeof(struct hd_file));				// write header
		}

		m_frametime = 0 ;
        m_openmode=1;
        return 1 ;
    }
    else if( strchr(mode, 'r')) {
		m_handle = fopen(filename, mode);
		if (m_handle == NULL) {
			return 0;
		}		
        if( fstat( fileno(m_handle), &dvrfilestat)==0 ) {
            m_filesize=dvrfilestat.st_size ;
            if( m_filesize<=(int)sizeof(h264hd) ) {
                close();
                return 0 ;
            }
        }
        else {
            close();
            return 0 ;
        }

        read(&h264hd, sizeof(h264hd) );
        m_hdflag = h264hd.flag ;

        if( h264hd.flag == H264FILEFLAG ) {
            m_fileencrypt=0 ;								// no encrypted
        }
        else {
            m_fileencrypt = 1 ;
            RC4_block_crypt( (unsigned char*)&h264hd, (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
            if( h264hd.flag ==  H264FILEFLAG ) {
                if( file_nodecrypt ) {
                    m_autodecrypt=0 ;
                }
                else {
                    m_autodecrypt=1 ;
                    m_hdflag = h264hd.flag ;            // auto decrypted header
                }
            }
        }
        readkey();				                        // read key frame index
        if( m_keyarray.size()>0 ) {
            seek( m_keyarray[0].koffset, SEEK_SET );
            int flen = m_keyarray[m_keyarray.size()-1].ktime / 1000 ;
            if( flen > m_filelen ) m_filelen = flen ;
        }
        m_framepos=0;
        m_reftstamp = 0 ;
        m_reftime = 0 ;
        m_frame_kindex=0 ;
        // getframe();                                     // advance to first frame
        return 1 ;
    }
    return 0;
}

void dvrfile::close()
{
    OFF_T size;
    flushbuffer() ;
    if (isopen()) {
		if( m_keyfile ) {
			fclose( m_keyfile );
			m_keyfile = NULL ;
		}           
        if( m_openmode ) {  // open for write
            size = tell();
            if (m_initialsize > size) {
                truncate(size);
            }
            if( fclose(m_handle)==0 ) {
				// try rename to new file length.
				if( m_frametime>0 ) {
					string nname ;
					char * nfname = nname.setbufsize(500) ;
					strcpy( nfname, (char *)m_filename );
					char * base = strrchr( nfname, '/' );
					if( base ) {
						sprintf( base+21, "%d_L_%s%s",
							m_frametime/1000,
							(char *)g_servername, g_264ext);	
						rename( m_filename, nfname );
					}
				}				
			}
			else {
                dvr_log("Close file failed : %s", (char *)m_filename);
                rec_basedir="" ;
                disk_error=1 ;
            }
            m_handle = NULL ;
        }
        else {
            fclose(m_handle);
			m_handle = NULL;
        }
    }
    m_keyarray.clear();
    m_framepos=0;
}

string * dvrfile::makedvrfilename( struct dvrtime * filetime, int channel )
{
	if (rec_basedir.length() < 5) {	// no base dir, no recording disk available
		// disk not ready !!!
		return NULL;
	}

	struct stat dstat ;
	int l;
	
	string * fname = new string ;
	char * newfilename = fname->setbufsize(500) ;
            
	// make host directory
	sprintf(newfilename, "%s/_%s_",
		(char *)rec_basedir, (char *)g_servername );
	if( stat(newfilename, &dstat)!=0 ) {
		mkdir( newfilename, 0777 );
	}

	// make date directory
	l = strlen(newfilename);
	sprintf(newfilename+l, "/%04d%02d%02d",
		filetime->year,
		filetime->month,
		filetime->day );
	if( stat(newfilename, &dstat)!=0 ) {
		mkdir( newfilename, 0777 );
	}

	// make channel directory
	l = strlen(newfilename);
	sprintf(newfilename + l, "/CH%02d", channel);
	if( stat(newfilename, &dstat)!=0 ) {
		mkdir( newfilename, 0777 );
	}

	// newfilename is directory of video files
	dir_find dfind(newfilename);			
	l = strlen(newfilename);
	
	int lastfiletime = -1 ;
	char filepattern[40] ;
	sprintf(filepattern, "CH%02d_%04d%02d%02d*%s",
			channel,
			filetime->year,
			filetime->month,
			filetime->day,
			g_264ext);		
			
	while( dfind.find(filepattern) ) {
		if( dfind.isfile() ) {
			int fti = -1 ;
			if( sscanf( dfind.filename()+13, "%d", &fti ) == 1 ) {
				if( fti>lastfiletime ) {
					// found a newer file
					lastfiletime = fti ;
					strcpy( newfilename + l + 1, dfind.filename() );
				}
			}
		}
	}

	extern OFF_T rec_maxfilesize;
	
	if( lastfiletime>=0 ) {
		newfilename[l] = '/' ;
		if( stat(newfilename, &dstat)==0 ) {
			if( dstat.st_size < rec_maxfilesize-1000000 ) { 
				return fname ;
			}
		}			
	}
	
	// make new file name		
	sprintf(newfilename + l,
			"/CH%02d_%04d%02d%02d%02d%02d%02d_0_L_%s%s",
			channel,
			filetime->year,
			filetime->month,
			filetime->day,
			filetime->hour,
			filetime->minute,
			filetime->second,
			(char *)g_servername, g_264ext);		
	return fname ;
}

int dvrfile::read(void *buffer, size_t buffersize)
{
    flushbuffer();
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
        // auto use buffer
        if( m_filebuffer == NULL ) {
            m_filebuffersize = file_bufsize ;
            m_filebuffer = (char *)malloc(m_filebuffersize);
            m_filebufferpos = 0 ;
        }
        if( m_filebuffer ) {
            if( m_filebufferpos+buffersize > m_filebuffersize ) {
				if( m_filebufferpos>0 ) {
					fwrite(m_filebuffer, 1, m_filebufferpos, m_handle);				
					m_filebufferpos=0 ;
				}
				if( buffersize > m_filebuffersize ) {
					fwrite(buffer, 1, buffersize, m_handle);
				}
				else {
					mem_cpy32( m_filebuffer, buffer, buffersize );
					m_filebufferpos=buffersize ;
				}
			}
			else {
                mem_cpy32( m_filebuffer+m_filebufferpos, buffer, buffersize );
                m_filebufferpos+=buffersize ;
            }
			return (int)buffersize ;
		}
        else {
            return (int)fwrite(buffer, 1, buffersize, m_handle);
        }
    }
    else {
        flushbuffer();      // remove buffer
        return 0;
    }
}

OFF_T dvrfile::tell()
{
    if( isopen() ) {
        if( m_filebuffer ) {
            return file_tell(m_handle)+m_filebufferpos ;
        }
        else {
            return file_tell(m_handle);
        }
    }
    else {
        return 0;
    }
}

int dvrfile::seek(OFF_T pos, int from)
{
    if( isopen() ) {
		flushbuffer();
        return file_seek(m_handle, pos, from);
    }
    else {
        return 0 ;
    }
}

int dvrfile::truncate( OFF_T tsize )
{
    int res ;
    flushbuffer();
    fflush(m_handle);
    res=file_truncate(fileno(m_handle), tsize);
    fflush(m_handle);
    return res ;
}

void dvrfile::flushbuffer()
{
    if( m_filebuffer ) {
        if( isopen() && m_filebufferpos>0 ) {
            // write to file
            fwrite(m_filebuffer, 1, m_filebufferpos, m_handle);
			fflush(m_handle);
        }
        free(m_filebuffer);
        m_filebuffer = NULL ;
        m_filebufferpos = 0;
    }
    if( m_keyfile ) {
		fflush( m_keyfile );
	}
}

int dvrfile::readheader(void * buffer, size_t hsize)
{
    int framepos ;

    if( hsize < headersize() ) {
        return 0 ;
    }

    // save old file pointer
    framepos = tell();
    seek(0, SEEK_SET);
    read( buffer, hsize);

    if( m_fileencrypt ) {
        RC4_block_crypt( (unsigned char*)buffer, (unsigned char*)buffer, hsize, 0, file_encrypt_RC4_table, 1024);
    }

    seek( framepos, SEEK_SET );
    return hsize;
}

int dvrfile::writeheader(void * buffer, size_t headersize)
{
    int framepos ;

    // save old file pointer
    framepos = tell();
    seek(0, SEEK_SET);

    if( m_fileencrypt ) {
        unsigned char header[headersize] ;
        RC4_block_crypt( header, (unsigned char*)buffer, headersize, 0, file_encrypt_RC4_table, 1024);
        write( header, headersize);                 // write header
    }
    else {
        write( buffer, headersize);
    }
    seek( framepos, SEEK_SET );
    return headersize;
}

// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe( rec_fifo * data )
{
    int size = 0  ;
    unsigned char * wbuf = NULL ;
    unsigned int esize = 0 ;        // encryption size
	REC_STATE recstat = REC_STOP ;		
	int frametype = FRAMETYPE_UNKNOWN ;
    
    if( data ) {
		m_frametime = time_dvrtime_diffms(&(data->time), &m_filetime); 
		wbuf = (unsigned char *)data->buf ;
		size = data->bufsize ;
		frametype = data->frametype ;
		recstat = data->rectype ;
	}

	// current frame time in ms from file time
    if( frametype == FRAMETYPE_KEYVIDEO || recstat == REC_STOP ) {
		// open key file if not yet.
		if( m_keyfile == NULL ) {
			string keyfilename(m_filename) ;
			char * pk = keyfilename ;
			int l=strlen(pk);
			if( strcmp( &pk[l-4], g_264ext)==0 ) {
				strcpy(&pk[l-4], g_kext);
				m_keyfile=fopen( pk, "a" );
			}
		}
		if( m_keyfile ) {
			char t ;
			if( recstat == REC_LOCK ) {
				t = DVR_KEY_TYPE_LOCK ;
			}
			else if( recstat == REC_RECORD ) {
				t = DVR_KEY_TYPE_NORM ;
			}
			else if( recstat == REC_PRERECORD ) {
				t = DVR_KEY_TYPE_PRE ;
			}
			else if( recstat == REC_STOP ) {
				t = DVR_KEY_TYPE_BREAK ;
			}
			else {
				t = DVR_KEY_TYPE_END ;
			}
			fprintf(m_keyfile,"%d,%lld,%c\n", 
				m_frametime,
				(long long)tell(),
				t ) ;
		}
    }
    
    if( size>0 && m_fileencrypt ) {
#ifdef EAGLE32
        struct hd_frame * pframe = (struct hd_frame *)wbuf ;
        if( pframe->flag==1 && size > sizeof( hd_frame )) {
            write( wbuf, sizeof(hd_frame) );
            wbuf+=sizeof(hd_frame);
            size-=sizeof(hd_frame);
            esize=pframe->framesize ;
            if( esize>1024 ) {
                esize=1024 ;
            }
            if( esize>size ) {
                esize=size ;
            }
            char encbuf[esize] ;
            RC4_block_crypt( (unsigned char *)encbuf, wbuf, esize, 0, file_encrypt_RC4_table, 1024);
            write( encbuf, esize);
            wbuf+=esize ;
            size-=esize ;
        }
#endif

#if defined(EAGLE34) || defined(EAGLE368)

        // .266 file format is similor to .265, different on encryption part in video frame.
        // Refer to Bill's code, video frame encryption should follow byte sequence "00 00 01 65" and "00 00 01 61"

        while( size>0 ) {
            unsigned int si=0 ;
            if( wbuf[0] == 0 &&
                wbuf[1] == 0 &&
                wbuf[2] == 1 )
            {
                if( wbuf[3]==0xba ) {               // PS header
                    si=20 ;                         // always 20 bytes from eagle34
                }
                else {
                    si = (((unsigned int)wbuf[4])<<8) +
                        +(((unsigned int)wbuf[5])) +
                        6 ;                         // sync (4b) + size (2b)
                    esize=0;
                    if( wbuf[3]==0xe0 ) {           // video frame
                        if( si==size ) {            // last pack
#ifdef EAGLE34
                            write( wbuf, 17 );      //  17 ? a strange number? ok, it always follow a byte value of 0x41 or 0x65 (not the case for eagle368)
                            wbuf+=17 ;
                            size-=17 ;
                            esize=size ;
#endif

#ifdef EAGLE368
                            // .266 file, encrypted portion is following the byte sequence "00 00 01 65" or "00 00 01 61"

                            int ei ;    // encryption offset
                            for( ei=10; ei<size-4 ; ei++ ) {
                                if( wbuf[ei] == 0 &&
                                    wbuf[ei+1] == 0 &&
                                    wbuf[ei+2] == 1 )
                                {
                                    if( wbuf[ei+3] == 0x61 || wbuf[ei+3] == 0x65 ) {        // found encryption part
                                        ei+=4 ;
                                        write( wbuf, ei ) ;
                                        wbuf+=ei ;
                                        size-=ei ;
                                        esize = size ;
                                        break ;
                                    }
                                }
                            }
#endif

                        }
                    }
                    else if( wbuf[3]==0xc0 ) {      // audio frame
                        if( si==size ) {            // last pack
                            write( wbuf, 16 );      // by pass audio frame header,
                            wbuf+=16 ;
                            size-=16 ;
                            esize=size ;
                        }
                    }
                    if( esize ) {                           // do encrypt
                        if( esize>1024 ) {
                            esize=1024 ;
                        }
                        if( esize>size ) {
                            esize=size ;
                        }
                        unsigned char encbuf[esize] ;
                        RC4_block_crypt( encbuf, wbuf, esize, 0, file_encrypt_RC4_table, 1024);
                        write( encbuf, esize);
                        wbuf+=esize ;
                        size-=esize ;
                        break ;
                    }
                }
            }
            else {
                dvr_log( "Un-recognized frame detected!" );
                break;
            }
            write( wbuf, si );
            wbuf+=si ;
            size-=si ;
        }
#endif          // EAGLE34 or EAGLE368

/*
#ifdef EAGLE_ZEUS3
        // following are examples modified from original Bill's code
        unsigned int si = 0 ;
        esize = 0 ;
        while( si<size ) {
            if( wbuf[si] == 0 &&
                wbuf[si+1] == 0 &&
                wbuf[si+2] == 1 )                      // valid frame?
            {
                unsigned char frameflag = wbuf[si+3] ;
                if( frameflag == (unsigned char)0xc0 ) {                                            // audio frame
                    si+=9+wbuf[si+8] ;
                    esize = size-si ;
                }
                else if( frameflag == (unsigned char)0x65 || frameflag == (unsigned char)0x61 ) {    // video frame
                    si+=4 ;
                    esize = size-si ;
                }
            }

            if( esize>0 ) {
                // no encrypt frame header
                write( wbuf, si ) ;
                wbuf+=si ;
                size-=si ;
                // do encrypt
                if( esize>1024 ) esize = 1024 ;
                unsigned char encbuf[esize] ;
                RC4_block_crypt( encbuf, wbuf, esize, 0, file_encrypt_RC4_table, 1024);
                write( encbuf, esize);
                wbuf+=esize ;
                size-=esize ;
                break;
            }
            si++ ;
        }
#endif      // EAGLE_ZEUS3
*/

    }
    if( size>0 ) {
        return ( write( wbuf, size )==(int)size ) ;
    }
    return 1;
}

// read one frame
int dvrfile::readframe(void * framebuf, size_t bufsize)
{
    int frsize ;
    int rsize=0 ;
    frsize = framesize() ;
    if( frsize<=0 ) {
        return 0 ;
    }
    else if( (int)bufsize>=frsize ) {
        rsize=read(framebuf, frsize);
        if( m_autodecrypt ) {

#ifdef EAGLE32
            struct hd_frame * pframe;
            int encsize ;
            pframe = (struct hd_frame *)framebuf ;
            if( pframe->flag==1 && m_fileencrypt ) {
                encsize = pframe->framesize ;
                if( encsize>1024 ) {
                    encsize=1024 ;
                }
                unsigned char * ebuf =  ((unsigned char *)pframe)+sizeof(struct hd_frame) ;
                RC4_block_crypt( ebuf, ebuf, encsize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
            }
#endif

#if defined(EAGLE34) || defined(EAGLE368)

            unsigned char * wbuf = (unsigned char *)framebuf ;
            unsigned int size=rsize ;
            while( size>0 ) {
                unsigned int sync ;
                unsigned int si ;
                unsigned int esize ;
                sync = (((unsigned int)wbuf[0])<<24)
                    +(((unsigned int)wbuf[1])<<16)
                    +(((unsigned int)wbuf[2])<<8)
                    +(((unsigned int)wbuf[3])) ;
                si = (((unsigned int)wbuf[4])<<8) +
                    +(((unsigned int)wbuf[5])) +
                    6 ;
                esize = 0 ;
                if( sync==0x000001ba ) {            // PS header
                    si=20 ;                         // always 20 bytes from eagle34
                }
                else if( sync==0x000001e0 ) {       // video frame
                    if( si==size ) {                // last pack

#ifdef EAGLE34
                        wbuf+=17 ;                  // 17?, see whats on writeframe
                        size-=17 ;
                        esize=size ;
#endif

#ifdef EAGLE368
                        // .266 file, encrypted portion is following the byte sequence "00 00 01 65" or "00 00 01 61"

                        int ei ;    // encryption offset
                        for( ei=10; ei<size-4 ; ei++ ) {
                            if( wbuf[ei] == 0 &&
                                    wbuf[ei+1] == 0 &&
                                    wbuf[ei+2] == 1 )
                            {
                                if( wbuf[ei+3] == 0x61 || wbuf[ei+3] == 0x65 ) {        // found encryption part
                                    ei+=4 ;
                                    wbuf+=ei ;
                                    size-=ei ;
                                    esize = size ;
                                    break ;
                                }
                            }
                        }
#endif

                    }
                }
                else if( sync==0x000001c0 ) {           // audio frame
                    if( si==size ) {                    // last pack
                        wbuf+=16 ;
                        size-=16 ;
                        esize=size ;
                    }
                }
                else if( (sync&0xffffff00)!=0x00000100 ) {      // other pack could be 0x000001bc, a key frame tag
                    dvr_log( "Readframe : Un-recogized frame detected!" );
                    return 0 ;
                }
                if( esize ) {
                    if( esize>1024 ) {
                        esize=1024 ;
                    }
                    RC4_block_crypt(wbuf, wbuf, esize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
                    break ;
                }
                wbuf+=si ;
                size-=si ;
            }
#endif      // EAGLE34 or EAGLE368

        }
    }
    else if( bufsize>0 ) {
        rsize = read(framebuf, bufsize);
    }
    else {
        rsize = frsize ;
    }
    return rsize ;
}

// current frame size
// return 0 : end of file
int dvrfile::framesize()
{
    if( isopen() ) {
        getframe();
        return m_framesize;
    }
    return 0;
}

// return frame type
//      0 - no frame
//      1 - keyframe
//     >1 - other frame
int dvrfile::frametype()
{
    if( isopen() ) {
        getframe();
        return m_frametype;
    }
    return FRAMETYPE_UNKNOWN;
}

dvrtime dvrfile::frametime()
{
    dvrtime t=m_filetime ;
    if( isopen() ) {
        getframe();
        time_dvrtime_addms(&t, m_frametime);
    }
    return t;
}

// get frame informations
int dvrfile::getframe()
{
    if( !isopen() ) {
        m_framepos=0;
        m_frametype=FRAMETYPE_UNKNOWN;
        m_framesize=0;
        m_frametime=0;
        return 0 ;
    }
    OFF_T framepos = tell();
    if( framepos >= m_filesize ) {
        m_framepos = framepos ;
        m_frametype=FRAMETYPE_UNKNOWN;
        m_framesize=0;
        return 0 ;
    }
    if( framepos < m_filestart ) {
        framepos=m_filestart ;
        seek(framepos);
        m_frame_kindex=0 ;
    }

//    if( framepos&3 ) {          // frame position is not dword aligned
//        ;;;
//    }

    if( m_framepos != framepos ) {
        m_framesize = 0 ;
        m_frametype = FRAMETYPE_UNKNOWN ;

        // adjust key frame index
        int ksize = m_keyarray.size() ;
        if( ksize > 0 ) {
            if( m_frame_kindex<0 || m_frame_kindex>=ksize ) {
                m_frame_kindex=0 ;
            }
            while( 1 ) {
                int kpos = m_keyarray[m_frame_kindex].koffset ;
                if( framepos == kpos ) {
                    m_frametime =  m_keyarray[m_frame_kindex].ktime ;       // may be not necessory here
                    break;
                }
                else if( framepos>kpos ) {
                    if( m_frame_kindex>=ksize-1 ||
                        framepos < m_keyarray[m_frame_kindex+1].koffset )
                    {
                        break;
                    }
                    m_frame_kindex++ ;
                }
                else { // framepos<kpos
                    if( m_frame_kindex == 0 ) {
                        break;
                    }
                    m_frame_kindex--;
                }
            }
        }

#ifdef EAGLE32
        struct hd_frame frame;
        struct hd_subframe subframe;
        int subframes;
        int rbytes ;

        rbytes = read((char *) &frame, sizeof(frame));
        if (rbytes == sizeof(frame) &&
            frame.flag == 1 &&
            HD264_FRAMESUBFRAME(frame) < 10 &&
            frame.framesize < 1000000)
        {
            switch ( HD264_FRAMETYPE(frame) ) {
                case 3 :
                    m_frametype = FRAMETYPE_KEYVIDEO;
                    break ;
                case 4:
                    m_frametype = FRAMETYPE_VIDEO;
                    break ;
                case 1:
                    m_frametype = FRAMETYPE_AUDIO;
                    break ;
                default:
                    break ;
            }


            if (m_frametype != FRAMETYPE_UNKNOWN ) {
                // update ref time
                if( ksize > 0 ) {
                    if( m_keyarray[m_frame_kindex].koffset == framepos ) {
                        // match key frame recorded, reload referrent time stamp
                        m_reftstamp = frame.timestamp ;
                        m_reftime = m_keyarray[m_frame_kindex].ktime ;
                    }
                }
                else if( framepos == m_filestart ) {    // reading first frame
                    m_reftstamp = frame.timestamp ;
                    m_reftime = 0 ;
                }
                // update frame time
                if( frame.timestamp > m_reftstamp ) {
                    m_frametime = m_reftime+tstamp2ms(frame.timestamp-m_reftstamp) ;
                }
                else {
                    m_frametime = m_reftime ;
                }
                subframes =  HD264_FRAMESUBFRAME(frame) ;
                seek(frame.framesize, SEEK_CUR);
                while (--subframes > 0) {
                    subframe.d_flag = 0;
                    if( read(&subframe, sizeof(subframe))!= sizeof(subframe) ) {
                        break ;
                    }
                    if (HD264_SUBFLAG(subframe)!= 0x1001 && HD264_SUBFLAG(subframe)!= 0x1005) {
                        break ;
                    }
                    seek(subframe.framesize, SEEK_CUR);
                }
                if (subframes == 0)  {
                    m_framesize=tell()-framepos ;
                }
            }
        }
#endif

#if defined(EAGLE34) || defined(EAGLE368)
        // .265 and .266 file structure are the same

        unsigned char ps_sync[4] ;
        OFF_T fpos = tell();
        while (fpos<=m_filesize && read(ps_sync, 4)==4)
        {
            if( ps_sync[0]==0 &&
                ps_sync[1]==0 &&
                ps_sync[2]==1 )
            {
                if( ps_sync[3]==0xba ) {            // PS stream header
                    unsigned char ps_header[16] ;
                    if( m_frametype!=FRAMETYPE_UNKNOWN ) {  // hit the next header
                        break;
                    }
                    if( read(ps_header,16)==16 ) {             // read rest of PS header, (total 20 bytes )
                        // calculate time stamp ;
                        unsigned int systemclock ;
                        int marker ;
                        marker = (ps_header[0]>>6) & 3 ;
                        if( marker != 1 ) {
                            NET_DPRINT( "First marker bits error!\n");
                            break;
                        }
                        systemclock = ((((unsigned int)ps_header[0])&0x38)>>3);
                        marker = (ps_header[0]>>2) & 1 ;
                        if( marker != 1 ) {
                            NET_DPRINT( "Second marker bits error!\n");
                            break;
                        }
                        systemclock<<=15 ;
                        systemclock|=
                            ((((unsigned int)ps_header[0])&0x3)<<13) |
                            ((((unsigned int)ps_header[1])&0xff)<<5) |
                            ((((unsigned int)ps_header[2])&0xf8)>>3) ;
                        marker = ((((unsigned int)ps_header[2])&0x4)>>2) ;
                        if( marker != 1 ) {
                            NET_DPRINT( "Third marker bits error!\n");
                            break;
                        }
                        //                systemclock<<=15 ;
                        systemclock<<=14 ;
                        systemclock|=
                            ((((unsigned int)ps_header[2])&0x3)<<12) |
                            ((((unsigned int)ps_header[3])&0xff)<<4) |
                            ((((unsigned int)ps_header[4])&0xf8)>>4) ;
                        marker = ((((unsigned int)ps_header[4])&0x4)>>2) ;
                        if( marker != 1 ) {
                            NET_DPRINT( "Third marker bits error!\n");
                            break;
                        }
                        marker = ((((unsigned int)ps_header[5])&1)) ;
                        if( marker != 1 ) {
                            NET_DPRINT( "4th marker bits error!\n");
                            break;
                        }
                        marker = ((((unsigned int)ps_header[8])&3)) ;
                        if( marker != 3 ) {
                            NET_DPRINT( "5th marker bits error!\n");
                            break;
                        }
                        // update ref time stamp
                        if( ksize > 0 ) {
                            if( m_keyarray[m_frame_kindex].koffset == framepos ) {
                                // match key frame recorded, reload referrent time stamp
                                m_reftstamp = systemclock ;
                                m_reftime = m_keyarray[m_frame_kindex].ktime ;
                            }
                        }
                        else if( framepos == m_filestart ) {    // reading first frame
                            m_reftstamp = systemclock ;
                            m_reftime = 0 ;
                        }
                        // update frame time
                        if( systemclock > m_reftstamp ) {
                            m_frametime = m_reftime+(systemclock-m_reftstamp)/45 ;  // system clock has been shifted, it is 45000Hz
                        }
                        else {
                            m_frametime = m_reftime ;
                        }
#ifdef NETDBG
                        NET_DPRINT( "getframe: m_reftstamp=%d m_reftime=%d systemclock=%d m_frametime=%d\n",
                            m_reftstamp,
                            m_reftime,
                            systemclock,
                            m_frametime );
#endif
                    }
                    else {
                        break;
                    }
                }
                else {                              // PS pack header
                    unsigned char ps_si[2] ;
                    if( read(ps_si, 2)==2 ) {
                        unsigned int si ;
                        if( ps_sync[3]==0xc0 ) {                // audio frame
                            if( m_frametype != FRAMETYPE_UNKNOWN ) {        // hit next audio frame
                                break ;
                            }
                            m_frametype = FRAMETYPE_AUDIO ;
                        }
                        else if( ps_sync[3]==0xe0 ) {
                            if( m_frametype == FRAMETYPE_UNKNOWN ) {
                                m_frametype = FRAMETYPE_VIDEO ;
                            }
                        }
                        else if( ps_sync[3]==0xbc ) {           // pack 0x000001bc belong to key frames
                            m_frametype = FRAMETYPE_KEYVIDEO ;
                        }
                        si = (((unsigned int)ps_si[0])<<8) +
                            +(((unsigned int)ps_si[1])) ;
                        seek( si, SEEK_CUR );
                    }
                    else {
                        break;
                    }
                }
            }
            else {
                break ;
            }
            fpos = tell();
        }
        m_framesize = fpos-framepos ;
#endif          // EAGLE34

        if( m_framesize<=0 || m_frametype ==  FRAMETYPE_UNKNOWN ){
            m_framesize = 0 ;
            m_frametype =  FRAMETYPE_UNKNOWN ;
        }
        m_framepos = framepos ;
        seek(m_framepos);
    }
    return m_framepos ;
}

#define SEARCHBUFSIZE	(1024)
#define MAXSEARCH       (800000)        // max frame size for searching

int dvrfile::nextkeyframe()
{
    int i ;
    OFF_T pos ;
    if( m_keyarray.size()>1 ) {                         // key frame index available
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
    OFF_T pos ;
    if( m_keyarray.size()>1 ) {                         // key frame index available
        pos = tell();
        for( i=m_keyarray.size()-1; i>0; i--) {
            if( m_keyarray[i].koffset<=pos ) {
                seek(m_keyarray[i-1].koffset, SEEK_SET);
                return (m_keyarray[i-1].koffset);
            }
        }
        return 0 ;
    }

    return 0;
}

// read complete key index
void dvrfile::readkey()
{
    string keyfilename(m_filename) ;
    char * pk = keyfilename ;
    int l=strlen(pk);
    FILE * keyfile = NULL;
    char deftype ;
    if( strstr( m_filename, "_L_" )!=NULL ) {
		deftype = DVR_KEY_TYPE_LOCK ;
	}
	else {
		deftype = DVR_KEY_TYPE_NORM ;
	}
    m_keyarray.empty();
    if( strcmp( &pk[l-4], g_264ext)==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=fopen( pk, "r" );
        if( keyfile ) {
			
            struct dvr_key_t key;
            memset( &key, 0, sizeof(key) );

            char linebuf[256] ;
            while( fgets(linebuf, 255, keyfile) ) {
				char type ;
				type = deftype ;
				if( sscanf( linebuf,"%d,%lld,%c", &(key.ktime), &(key.koffset), &type)>=2){
					if( type == DVR_KEY_TYPE_LOCK ) {
						key.ktype = REC_LOCK ;
					}
					else if( type == DVR_KEY_TYPE_NORM ) {
						key.ktype = REC_RECORD ;
					}
					else {
						key.ktype = REC_STOP ;
					}
					m_keyarray.add(key);
				}
			}
            
            fclose(keyfile);

            int i;
            for( i=0; i<m_keyarray.size()-1 ; i++ ) {
				m_keyarray[i].ksize = m_keyarray[i+1].koffset - m_keyarray[i].koffset ;
				m_keyarray[i].klength = m_keyarray[i+1].ktime - m_keyarray[i].ktime ;
			}
        }
    }
}

int dvrfile::getclipinfo( array <struct dayinfoitem> &dayinfo, int lock )
{
	REC_STATE recstate ;	
	struct dayinfoitem di ;
	int filestarttime = m_filetime.hour * 3600 + m_filetime.minute * 60 + m_filetime.second ; 

	if( m_keyarray.size()>0 ) {
		filestarttime *= 1000 ;		// milliseconds
		int xontime = -100000 ;		// invalid ontime 
		int xofftime = -100000 ;	// invalid offtime 
		int i ;
		recstate = REC_STOP ;
		for(i=0; i<m_keyarray.size(); i++ ) {
			int ontime = filestarttime + m_keyarray[i].ktime ;
			if( recstate != m_keyarray[i].ktype ||  (ontime-xofftime) > 5000 ) {
				// to start a new clip
				if( xofftime>xontime ) {
					if( ( lock && recstate==REC_LOCK ) || 
						( (!lock) && recstate==REC_RECORD ) ) {
						di.ontime = xontime/1000 ;
						di.offtime = xofftime/1000 ;
						dayinfo.add( di );
					}
				}
				recstate = m_keyarray[i].ktype ;
				xontime = ontime ;
				xofftime = ontime + m_keyarray[i].klength ;
			}
			else {
				// merge block
				xofftime = ontime + m_keyarray[i].klength ;
			}
		}
		if( xofftime>xontime ) {
			if( ( lock && recstate==REC_LOCK ) || 
				( (!lock) && recstate==REC_RECORD ) ) {
				di.ontime = xontime/1000 ;
				di.offtime = xofftime/1000 ;
				dayinfo.add( di );
				return 1 ;
			}
		}		
	}
	else {
		if( m_filelen>0 ) {
			if( strstr( (char *)m_filename, "_L_" )==NULL ) {
				recstate = REC_LOCK ;
			}
			else {
				recstate = REC_RECORD ;
			}
			if( ( lock && recstate==REC_LOCK ) || 
				( (!lock) && recstate==REC_RECORD ) ) {
				di.ontime = filestarttime ;
				di.offtime = filestarttime + m_filelen ;
				dayinfo.add( di );
				return 1 ;
			}
		}
	}
	return 0;
}

// return 1: seek success, 0: out of range
int dvrfile::seek( dvrtime * seekto )
{
    int i;
    int seeks ;					// seek in seconds from begin of file
    int seekms ;
    if( !isopen() ) {
        return 0;
    }

    if( *seekto < m_filetime ) {
        seek(m_filestart, SEEK_SET );
        return 0;
    }

    seeks = time_dvrtime_diff( seekto, &m_filetime );
    if( seeks >= m_filelen ) {
        seek(0, SEEK_END );
        return 0 ;
    }
    else if( seeks<3 ) {
        seek(m_filestart, SEEK_SET );
        return 1;
    }

    seekms = seeks*1000 ;

#ifdef EAGLE368

    // EAGLE368 player require seek to time next to requested time. (Aug10,2012)

    if( m_keyarray.size()>1 ) {                         // key frame index available
        for( i=0 ; i<m_keyarray.size(); i++ ) {
            if( m_keyarray[i].ktime >= seekms ) {
                break;
            }
        }
        if( i>=m_keyarray.size() ) {
            seek( 0, SEEK_END ) ;
        }
        else {
            seek(m_keyarray[i].koffset, SEEK_SET );
        }
        return 1 ;
    }

#else
    if( m_keyarray.size()>1 ) {                         // key frame index available
        for( i=m_keyarray.size()-1; i>=0; i-- ) {
            if( m_keyarray[i].ktime <= seekms ) {
                break;
            }
        }
        if( i< 0) i=0;
        seek(m_keyarray[i].koffset, SEEK_SET );
        return 1 ;
    }
#endif


    // no key file available

    // seek to end of file
    seek(0, SEEK_END);
    OFF_T filesize = tell();

    OFF_T pos = (filesize/m_filelen) * seeks ;
    pos &= ~(OFF_T)3;
    seek(pos, SEEK_SET );

    if (nextkeyframe() == 0){
        seek(0, SEEK_END );                  // seek to end of file
        return 1 ;
    }

    getframe();                              // update current frame time
    if( m_frametime > seekms ) {
        // go backward
        while( prevkeyframe() ) {
            getframe() ;    // update frame time
            if( m_frametime <= seekms ) {
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
            getframe() ;    // update frame time
            if( m_frametime > seekms ) {
                // got it
                break;
            }
            pos=tell();
        }
        seek(pos, SEEK_SET );
    }
    getframe();                          // update current frame time
    return 1 ;
}

// repair a .264 file, return 1 for success, 0 for failed
int dvrfile::repair()
{
    int framecount;
    dvr_key_t keyt ;
        array <struct dvr_key_t> keyarray ;

    if( m_filesize < (file_repaircut+512000) ) {        // file too small, not worth to repair
        return 0 ;
    }

    // pre-truncate bad files, to repair file system error ?
    m_initialsize=m_filesize ;
    m_filesize-=file_repaircut ;

    if( m_keyarray.size()>0 ) {
        m_keyarray.setsize(1);
        if( m_keyarray[0].ktime<0 ||
            m_keyarray[0].ktime>1000 ||
            m_keyarray[0].koffset<0 ||
            m_keyarray[0].koffset>(int)sizeof( hd_file ) )
        {
            m_keyarray[0].ktime = m_keyarray[0].ktime % 1000 ;
            m_keyarray[0].koffset = sizeof( hd_file ) ;
            m_filestart = sizeof( hd_file ) ;
        }
    }
    m_framepos = 0 ;            // to refresh reference time stamp
    framecount = 0;

    while( framesize()>0 ) {
        if( m_frametype == FRAMETYPE_KEYVIDEO ) {
            keyt.koffset = m_framepos;
            keyt.ktime = m_frametime;
            keyarray.add( keyt ) ;
        }
        framecount++;
    }

    if (framecount < 5 ||
        m_framepos < 0x10000 ||
        m_frametime < 1000 )
    {
        return 0 ;                                          // file too small, failed
    }

    m_openmode = 1 ;                    // set open mode to write, so close() would save key file and set correct file size
    m_keyarray = keyarray ;
    close();                            // close file, so we can rename it.

    // rename file to include new length
    char newfilename[512] ;
    char * rn ;
    char * tail ;

    strcpy( newfilename, m_filename );
    rn = strstr(newfilename, "_0_");
    if( rn ) {
        tail = strstr(m_filename, "_0_")+3 ;
        sprintf( rn, "_%d_%s", m_frametime/1000, tail );
        if( rename( m_filename, newfilename )>=0 ) {         // success
            return 1;
        }
    }
    return 0;
}

int dvrfile::repairpartiallock()
{
	// not more partial lock file in big recording mode?
	return 0;
}

/*
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

    locklength = f264locklength( m_filename );

    if( locklength >= m_filelen || locklength<2 ) {        // not a partial locked file
        return 0 ;
    }

    if( m_keyarray.size()<=1 ) {            // no key index ?
        return 0 ;
    }

    for( breakindex=m_keyarray.size()-1; breakindex>=0; breakindex-- ) {
        if( m_keyarray[breakindex].ktime < (m_filelen-locklength)*1000 ) {
            break;
        }
    }

    breaktime = m_filetime ;
    time_dvrtime_addms( &breaktime, m_keyarray[breakindex].ktime );

    strcpy( lockfilename, m_filename );
    lockfilenamebase = basefilename( lockfilename );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_L_%s%s",
        breaktime.year,
        breaktime.month,
        breaktime.day,
        breaktime.hour,
        breaktime.minute,
        breaktime.second,
        (char *)g_servername,
        g_264ext);
    lockfile.open(lockfilename, "wb");
    if( !lockfile.isopen() ) {
        return 0 ;
    }

    char lockfilehead[40] ;

    readheader( (void *)lockfilehead, 40 );
    lockfile.writeheader( lockfilehead, 40 );
    lockfile.m_fileencrypt=0 ;                      // do not enc again

    for( i=breakindex ; i<m_keyarray.size(); i++ ) {
        // if rec_busy, delay for 10 s
        int busywait ;
        for( busywait=0; busywait<100; busywait++) {
            if( rec_busy || disk_busy || g_cpu_usage>0.7 ) {
                usleep(10000);
            }
            else {
                break ;
            }
        }

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
        if( framesize<=0 )
            break;

        frametime = m_filetime ;
        time_dvrtime_addms( &frametime, m_keyarray[i].ktime );

        seek( framepos );

        framebuf = new char [framesize] ;
        read( framebuf, framesize );
        lockfile.writeframe( framebuf, framesize, FRAMETYPE_KEYVIDEO, &frametime );
        delete framebuf ;

    }

    lockfile.close();
    // rename new locked file
    strcpy( lockfilename1, lockfilename );
    lockfilenamebase = basefilename( lockfilename1 );

    locklength = m_keyarray[breakindex].ktime / 1000 ;
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_L_%s%s",
        breaktime.year,
        breaktime.month,
        breaktime.day,
        breaktime.hour,
        breaktime.minute,
        breaktime.second,
        m_filelen-locklength,
        (char *)g_servername,
        g_264ext );

    dvrfile::rename( lockfilename, lockfilename1 );

    // close original file
    seek( m_keyarray[breakindex].koffset ) ;
    m_initialsize = tell() + 1 ;       // set a fake init size, so close() will do file truncate
    m_keyarray.setsize(breakindex);
    m_openmode = 1 ;                    // set open mode to write, so close() would save key file
    close();

    dvr_log( "Event locked file breakdown success. (%s)", lockfilename1 );

    // rename old locked filename
    strcpy( lockfilename1, m_filename );
    lockfilenamebase = basefilename( lockfilename1 );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_N_%s%s",
            m_filetime.year,
            m_filetime.month,
            m_filetime.day,
            m_filetime.hour,
            m_filetime.minute,
            m_filetime.second,
            locklength,
            (char *)g_servername, g_264ext );
    dvrfile::rename( m_filename, lockfilename1 );
    return 1 ;
}
*/

// rename .264 file as well .k file
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
    if( strcmp( &oldkfile[lo-4], g_264ext )== 0 &&
       strcmp( &newkfile[ln-4], g_264ext )== 0 )
    {
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
    int l ;
    string kfile ;
    char * extension ;
    res = ::remove( filename );
    kfile = filename ;
    l = kfile.length() ;
    extension = (char *)kfile+l-4 ;
    if( strcmp( extension, g_264ext ) == 0 ) {
        strcpy( extension, ".k" ) ;
        ::remove( kfile );
    }
    return res ;
}

int dvrfile::chrecmod(string & filename, char oldmode, char newmode)
{
    static char rmod[] = "_M_" ;
    char oldname[512];
    char * p ;
    strcpy( oldname, filename );
    rmod[1]=oldmode ;
    p = strstr(basefilename(filename), rmod);
    if( p ) {
        p[1]=newmode ;
        return dvrfile::rename( oldname, filename);
    }
    return 0 ;
}

int dvrfile::unlock(const char * filename)
{
    string f(filename);
    return chrecmod( f, 'L', 'N' );
}

int dvrfile::lock(const char * filename)
{
    string f(filename);
    return chrecmod( f, 'N', 'L' );
}

void file_init(config &dvrconfig)
{
    int iv ;
    string v;

    file_encrypt=dvrconfig.getvalueint("system", "fileencrypt");
    v = dvrconfig.getvalue("system", "filepassword");
    if( v.length()>=342 ) {
        c642bin(v, g_filekey, 256);
        RC4_crypt_table( file_encrypt_RC4_table, 1024, g_filekey);
    }
    else {
        file_encrypt=0;
    }

    file_bufsize = 1024*1024;
    v=dvrconfig.getvalue("system", "filebuffersize");
    char unit='b' ;
    int n=sscanf(v, "%d%c", &iv, &unit);
    if( n==2 && (unit=='k' || unit=='K') ) {
        file_bufsize = iv*1024 ;
    }
    else if( n==2 && (unit=='M' || unit=='m' ) ) {
        file_bufsize = iv*1024*1024 ;
    }
    else if( n==1 ) {
		file_bufsize = iv ; 
	}
    if( file_bufsize<16*1024 ) {
        file_bufsize=16*1024 ;
    }
    else if( file_bufsize>4*1024*1024 ) {
        file_bufsize=4*1024*1024 ;
    }

    file_nodecrypt=dvrconfig.getvalueint("system", "file_nodecrypt");

    iv=dvrconfig.getvalueint("system", "file_repaircut");
    if( iv>=8192 && iv<=(8*1024*1024) ) {
        file_repaircut=iv;
    }
}

void file_uninit()
{
}
