
#include "dvr.h"

#define FILETRUNCATINGSIZE (2*1024*1024)

static int file_bufsize;								// file buffer size
static int file_encrypt ;
static int file_nodecrypt;                              // do not decrypt file when read
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table
static int file_repaircut=FILETRUNCATINGSIZE ;

unsigned char g_filekey[256] ;

#ifdef EAGLE32
const char g_264ext[]=".264" ;
#define H264FILEFLAG  (0x484b4834)
#define FRAMESYNC   (0x00000001)
#endif
#ifdef EAGLE34
const char g_264ext[]=".265" ;
#define H264FILEFLAG  (0x484b4d49)
#define FRAMESYNC   (0xBA010000)
#endif

// convert timestamp value to milliseconds
inline int tstamp2ms(int tstamp )
{
    return tstamp*15+tstamp*5/8;	// 	tstamp * 1000 / 64;  ( to preview overflow)
}

dvrfile::dvrfile()
{
    m_handle = NULL;
    m_framepos=0;
}

dvrfile::~dvrfile()
{
    close();
}

int dvrfile::open(const char *filename, char *mode, int initialsize, int encrypt)
{
    struct hd_file h264hd ;
    struct stat dvrfilestat ;
    close();
    m_initialsize = 0;
    m_handle = file_open(filename, mode);
    if (m_handle == NULL) {
        return 0;
    }
    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_filelen=f264length(filename);
    m_filestart=(int)sizeof(struct hd_file);	// first data frame
    m_openmode=0;
    if (strchr(mode, 'w')) {
        if( file_bufsize>4096 ) {
            setvbuf(m_handle, NULL, _IOFBF, file_bufsize );
        }
        if (initialsize > 0) {
            seek(initialsize - 1, SEEK_SET );
            if (fputc(0, m_handle) == EOF) {
                close();
                return 0;
            };
            m_initialsize = initialsize;
        }
        seek(0,SEEK_SET);
        memcpy( &h264hd, Dvr264Header, sizeof(h264hd));
        h264hd.flag =  H264FILEFLAG;
        m_fileencrypt=file_encrypt ;
        if( m_fileencrypt ) {
            RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
        }
        write( &h264hd, sizeof(struct hd_file));				// write header
        m_filestart = tell();
        m_openmode=1;
    }
    else if( strchr(mode, 'r')) {
        if( fstat( fileno(m_handle), &dvrfilestat)==0 ) {
            m_filesize=dvrfilestat.st_size ;
            if( m_filesize<128000 ) {
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
            RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
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
        }
        m_filestart=tell();
        m_framepos=0;
        m_reftstamp = 0 ;
        m_reftime = 0 ;
        m_frame_kindex=0 ;
        getframe();                                     // advance to first frame
    }
    else {
        file_close(m_handle);
        m_handle = NULL;
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
            if( file_close(m_handle)!=0 ) {
                dvr_log("Close file failed : %s",m_filename.getstring());
                rec_basedir="" ;
            }
            if( m_keyarray.size()>0 ) {
                writekey();
            }
        }
        else {
            file_close(m_handle);
        }
        m_handle = NULL;
    }
    m_keyarray.empty();
    m_framepos=0;
}

int dvrfile::read(void *buffer, size_t buffersize)
{
    if( isopen() ) {
        return file_read(buffer, buffersize, m_handle);
    }
    else {
        return 0 ;
    }
}

int dvrfile::write(void *buffer, size_t buffersize)
{
    if( isopen() ) {
        return file_write(buffer, buffersize, m_handle);
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
    file_flush(m_handle);
    res=ftruncate(fileno(m_handle), tsize);
    file_flush(m_handle);
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
    seek(0, SEEK_SET);
    read( buffer, sizeof(struct hd_file));
    seek( framepos, SEEK_SET );
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
    seek(0, SEEK_SET);
    write( buffer, sizeof(struct hd_file));
    seek( framepos, SEEK_SET );
    return sizeof( struct hd_file );
}

// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe(void *buffer, size_t size, int keyframe, dvrtime * frametime)
{
    unsigned char * wbuf = (unsigned char *)buffer ;
    char encbuf[1024] ;
    unsigned int esize ;
    if( keyframe ) {
        dvr_key_t keytime ;
        keytime.koffset = tell();
        keytime.ktime = time_dvrtime_diffms(frametime, &m_filetime);
        m_keyarray.add(keytime);
        if( m_keyarray.size()==1 ) {
            writekey();											// record first key frame pos
        }
    }
    if( m_fileencrypt ) {
#ifdef EAGLE32
        struct hd_frame * pframe = (struct hd_frame *)buffer ;
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
            mem_cpy32( encbuf, wbuf, esize );
            RC4_block_crypt( (unsigned char *)encbuf, esize, 0, file_encrypt_RC4_table, 1024);
            write( encbuf, esize);
            wbuf+=esize ;
            size-=esize ;
        }
#endif

#ifdef EAGLE34
        while( size>0 ) {
            unsigned int si ;
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
                            write( wbuf, 17 );      //  17 ? a strange number? ok, it must follow a byte value of 0x41 or 0x65
                            wbuf+=17 ;
                            size-=17 ;
                            esize=size ;
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
                        mem_cpy32( encbuf, wbuf, esize );
                        RC4_block_crypt( (unsigned char *)encbuf, esize, 0, file_encrypt_RC4_table, 1024);
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
#endif          // EAGLE34
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
                RC4_block_crypt( ((unsigned char *)framebuf)+sizeof(struct hd_frame), encsize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
            }
#endif            
#ifdef EAGLE34
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
                        wbuf+=17 ;                  // 17?, see whats on writeframe
                        size-=17 ;
                        esize=size ;
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
                    dvr_log( "Un-recogized frame detected!" );
                    return 0 ;
                }
                if( esize ) {
                    if( esize>1024 ) {
                        esize=1024 ;
                    }
                    RC4_block_crypt(wbuf, esize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
                    break ;
                }
                wbuf+=si ;
                size-=si ;
            }
#endif            
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
    int framepos = tell();
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

#ifdef EAGLE34
        unsigned char ps_sync[4] ;
        int fpos = tell();
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
                            if( m_frametype != FRAMETYPE_UNKNOWN ) {
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

#define SEARCHBUFSIZE	(0x2000)
#define MAXSEARCH       (SEARCHBUFSIZE*64)

int dvrfile::nextframe()
{
    int pos;
    int readbytes;
    int j, i ;
    if (framesize()>0 ) {
        seek(m_framesize, SEEK_CUR );
        return tell();
    } 
    else {					// no correct frame structure
        unsigned int searchbuf[SEARCHBUFSIZE];
        pos = tell();
        pos &= (~3);
        seek(pos, SEEK_SET);
        for (j = 0; j < (MAXSEARCH/SEARCHBUFSIZE); j++) {	
            pos = tell();
            readbytes = read(searchbuf, SEARCHBUFSIZE);
            if (readbytes < 16 ) {
                return 0;		// file ends
            }
            for (i = 0; i < (readbytes / (int)(sizeof(int))); i++) {
                if (searchbuf[i] == FRAMESYNC) {
                    // find it
                    seek(pos+ i * (sizeof(int)), SEEK_SET );
                    if( frametype()!=FRAMETYPE_UNKNOWN ) {	// find a correct frame
                        return tell();
                    }
                    seek(pos+SEARCHBUFSIZE, SEEK_SET);
                }
            }
        }
    }
    return 0;                   // could not find, file error!
}

int dvrfile::prevframe()
{
    int pos ;
    int j, i, readbytes;
    unsigned int searchbuf[SEARCHBUFSIZE];
    pos = tell();
    pos &= (~3);
    seek(pos, SEEK_SET);
    for (j = 0; j < (MAXSEARCH/SEARCHBUFSIZE); j++) {	
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
            if (searchbuf[i] == FRAMESYNC) {
                // find it
                seek(pos+ i * (sizeof(int)), SEEK_SET );
                if( frametype()!=FRAMETYPE_UNKNOWN ) {	// find a correct frame
                    return tell();
                }
            }
        }
    }
    seek(0, SEEK_SET);
    return 0;					// could not find, file error!
}

int dvrfile::nextkeyframe()
{
    int i ;
    int pos ;
    if( m_keyarray.size()>1 ) {                         // key frame index available
        pos = tell();
        for( i=0; i<m_keyarray.size(); i++ ) {
            if( m_keyarray[i].koffset>pos ) {
                seek(m_keyarray[i].koffset, SEEK_SET);
                return (m_keyarray[i].koffset);
            }
        }
        return 0 ;
    }
    
    while (nextframe() != 0) {
        if (frametype() == FRAMETYPE_KEYVIDEO)
            return tell();
    }
    return 0;					// could not found
}

int dvrfile::prevkeyframe()
{
    int i ;
    int pos ;
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
    
    while (prevframe() != 0) {
        if (frametype() == FRAMETYPE_KEYVIDEO)
            return tell();
    }
    return 0;
}

void dvrfile::readkey()
{
    string keyfilename(m_filename) ;
    char * pk = keyfilename.getstring() ;
    int l=strlen(pk);
    FILE * keyfile = NULL;
    dvr_lock();
    m_keyarray.empty();
    if( strcmp( &pk[l-4], g_264ext)==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=file_open( pk, "r" );
        if( keyfile ) {
            struct dvr_key_t key;
            struct dvr_key_t skey;
            skey.ktime=0;
            skey.koffset=0;
            while(fscanf(keyfile,"%d,%d\n", &(key.ktime), &(key.koffset))==2){
                if( key.ktime > skey.ktime && key.koffset > skey.koffset ) {
                    skey = key ;
                    m_keyarray.add(key);
                }
            }
            file_close(keyfile);
        }
    }
    dvr_unlock();
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
    dvr_lock();
    if( strcmp( &pk[l-4], g_264ext)==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=file_open( pk, "w" );
        if( keyfile ) {
            for( i=0;i<m_keyarray.size();i++) {
                fprintf(keyfile,"%d,%d\n", m_keyarray[i].ktime,  m_keyarray[i].koffset);
            }
            file_close(keyfile);
        }
    }
    dvr_unlock();
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

    // no key file available
    
    // seek to end of file
    seek(0, SEEK_END);
    int filesize = tell();
    
    int pos = (filesize/m_filelen) * seeks ;
    pos &= ~3; 
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
/*
int dvrfile::repair1()
{
    struct hd_frame frame;
    int filepos, filelength;
    int framecount;
    DWORD starttimestamp=0 ;
    DWORD endtimestamp=0 ;
    char newfilename[512] ;
    char * rn ;
    char * tail ;
    dvr_key_t keyt ;
    int frsize ;
    
    if( m_filesize < (file_repaircut+512000) ) {        // file too small, not worth to repair
        return 0 ;
    }
    
    // pre-truncate bad files, to repair file system error ?
    m_filesize-=file_repaircut ;
    
    truncate( m_filesize );
    
    int ms = 0;
    
    if( m_keyarray.size()>0 ) {
        ms+=m_keyarray[0].ktime ;
        m_keyarray.setsize(0);
    }
    
    framecount = 0;
    filepos=m_filestart;
    starttimestamp = 0 ;                                        // initial start time stamp
    seek(filepos, SEEK_SET);
    while ((frsize=framesize()) > 0) {

        // if rec_busy, delay for 10 s
        int busywait ;
        for( busywait=0; busywait<100; busywait++) {
            if( rec_busy || disk_busy || g_cpu_usage>0.5 ) {
                usleep(100000);
            }
            else {
                break ;
            }
        }
        
        if( filepos+frsize > m_filesize ) {
            break ;
        }
        read(&frame, sizeof(frame));                            // read frame header
        if( starttimestamp==0 ) {
            starttimestamp = frame.timestamp ;
        }
        endtimestamp = frame.timestamp;
        seek(filepos+frsize, SEEK_SET );	        // seek to next frame
        // key frame ?
        if( HD264_FRAMETYPE(frame) == 3 ) 
        {
            keyt.koffset = filepos;
            keyt.ktime = tstamp2ms (endtimestamp - m_filestamp) + ms ;
            m_keyarray.add(keyt);
        }
        framecount++;
        filepos = tell();
    }
    
    if (framecount < 5 || filepos < 0x10000
        || endtimestamp < starttimestamp+100) 
    {
        return 0 ;                                          // file too small, failed
    }
    
    m_initialsize = filepos + 1 ;       // set a fake init size, so close() will do file truncate
    m_openmode = 1 ;                    // set open mode to write, so close() would save key file
    close();
    
    // rename file to include new length
    filelength = (endtimestamp - starttimestamp) / 64;          // repaired file length
    
    strcpy( newfilename, m_filename.getstring() );
    rn = strstr(newfilename, "_0_");
    if( rn ) {
        tail = strstr(m_filename.getstring(), "_0_")+3 ;
        sprintf( rn, "_%d_%s", filelength, tail );
        if( rename( m_filename.getstring(), newfilename )>=0 ) {         // success
            return 1;
        }
    }
    return 0;
}
*/

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
        nextframe();
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

    strcpy( newfilename, m_filename.getstring() );
    rn = strstr(newfilename, "_0_");
    if( rn ) {
        tail = strstr(m_filename.getstring(), "_0_")+3 ;
        sprintf( rn, "_%d_%s", m_frametime/1000, tail );
        if( rename( m_filename.getstring(), newfilename )>=0 ) {         // success
            return 1;
        }
    }
    return 0;
}

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
    
    locklength = f264locklength( m_filename.getstring() );
    
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
    
    strcpy( lockfilename, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_L_%s%s", 
        breaktime.year,
        breaktime.month,
        breaktime.day,
        breaktime.hour,
        breaktime.minute,
        breaktime.second,
        g_hostname,
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
            if( rec_busy || disk_busy || g_cpu_usage>0.5 ) {
                usleep(100000);
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
        
        framebuf = (char *) mem_alloc ( framesize ) ;
        seek( framepos );
        read( framebuf, framesize );
        lockfile.writeframe( framebuf, framesize, 1, &frametime );
        mem_free( framebuf );
        
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
        g_hostname,
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
    strcpy( lockfilename1, m_filename.getstring() );
    lockfilenamebase = basefilename( lockfilename1 );
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_N_%s%s", 
            m_filetime.year,
            m_filetime.month,
            m_filetime.day,
            m_filetime.hour,
            m_filetime.minute,
            m_filetime.second,
            locklength,
            g_hostname, g_264ext );
    dvrfile::rename( m_filename.getstring(), lockfilename1 );
    return 1 ;
}
   
// rename .264 file as well .k file
int dvrfile::rename(const char * oldfilename, const char * newfilename)
{
    int res ;
    char oldkfile[512], newkfile[512] ;
    int  lo, ln ;
    dvr_lock();
    res = ::rename( oldfilename, newfilename );
    strcpy( oldkfile, oldfilename );
    strcpy( newkfile, newfilename );
    lo = strlen( oldkfile );
    ln = strlen( newkfile );
    if( strcmp( &oldkfile[lo-4], g_264ext )== 0 &&
       strcmp( &newkfile[ln-4], g_264ext )== 0 ) {
           strcpy( &oldkfile[lo-4], ".k" );
           strcpy( &newkfile[ln-4], ".k" );
           ::rename( oldkfile, newkfile );
       }
    dvr_unlock();
    return res;
}

// remove .264 file as well .idx file
int dvrfile::remove(const char * filename)
{
    int res ;
    char kfile[256] ;
    int l ;
    dvr_lock();
    res = ::remove( filename );
    strcpy( kfile, filename );
    l = strlen( kfile );
    if( strcmp( &kfile[l-4], g_264ext ) == 0 ) {
        strcpy( &kfile[l-4], ".k" ) ;
        ::remove( kfile );
    }
    dvr_unlock();
    return res ;
}

int dvrfile::chrecmod(string & filename, char oldmode, char newmode)
{
    static char rmod[] = "_M_" ;
    char oldname[512];
    char * p ;
    strcpy( oldname, filename.getstring() );
    rmod[1]=oldmode ;
    p = strstr(basefilename(filename.getstring()), rmod);
    if( p ) {
        p[1]=newmode ;
        return dvrfile::rename( oldname, filename.getstring());
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

FILE * file_open(const char *path, const char *mode)
{
    FILE * f ;
    dvr_lock();
    f = fopen(path, mode);
    dvr_unlock();
    return f ;
}

int file_read(void *ptr, int size, FILE *stream)
{
    int r ;
    dvr_lock();
    r = (int)fread( ptr, 1, (size_t)size, stream );
    dvr_unlock();
    return r ;
}

int file_write(const void *ptr, int size, FILE *stream)
{
    int r ;
    dvr_lock();
    r = (int)fwrite( ptr, 1, (size_t)size, stream );
    dvr_unlock();
    return r ;
}

int file_close(FILE *fp)
{
    int r ;
    dvr_lock();
    r = fclose( fp );
    dvr_unlock();
    return r ;
}

int file_flush(FILE *stream)
{
    int r ;
    dvr_lock();
    r = fflush( stream );
    dvr_unlock();
    return r ;
}

void file_sync()
{
    dvr_lock();
    disk_sync();
    dvr_unlock();
}

void file_init()
{
    int iv ;
    string v;
    config dvrconfig(dvrconfigfile);
    
    file_encrypt=dvrconfig.getvalueint("system", "fileencrypt");
    v = dvrconfig.getvalue("system", "filepassword");
    if( v.length()>=342 ) {
        c642bin(v.getstring(), g_filekey, 256);
        RC4_crypt_table( file_encrypt_RC4_table, 1024, g_filekey);
    }
    else {
        file_encrypt=0;
    }
    
    v=dvrconfig.getvalue("system", "filebuffersize");
    char unit='b' ;
    int n=sscanf(v.getstring(), "%d%c", &iv, &unit);
    if( n==2 && (unit=='k' || unit=='K') ) {
         iv*=1024 ;
    }
    else if( n==2 && (unit=='M' || unit=='m' ) ) {
        iv*=1024*1024 ;
    }
    // to make sure buffer size is power of 2
    file_bufsize = 1024;
    while( iv > file_bufsize && file_bufsize<(4*1024*1024) ) {
        file_bufsize*=2 ;
    }

    file_nodecrypt=dvrconfig.getvalueint("system", "file_nodecrypt");

    iv=dvrconfig.getvalueint("system", "file_repaircut");
    if( iv>=8192 && iv<=(8*1024*1024) ) {
        file_repaircut=iv;
    }
}

void file_uninit()
{
    file_sync();
}
