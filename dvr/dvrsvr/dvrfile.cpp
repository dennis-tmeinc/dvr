

#include "dvr.h"

// #define FILESYNCSIZE (0x200000)
#define FILETRUNCATINGSIZE (2*1024*1024)

static int file_bufsize;								// file buffer size
//static int file_syncreq;
static int file_encrypt ;
static int file_nodecrypt;                              // do not decrypt file when read
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table
static int file_repaircut=FILETRUNCATINGSIZE ;

unsigned char g_filekey[256] ;

// convert timestamp value to milliseconds
inline int tstamp2ms(int tstamp )
{
    return tstamp*15+tstamp*5/8;	// 	tstamp * 1000 / 64;  ( to preview overflow)
}

dvrfile::dvrfile()
{
    m_handle = NULL;
    m_filebuf = NULL;
    m_initialsize=0;
    m_filestamp = 0;
    m_timestamp = 0 ;
    m_syncsize=0 ;
    m_framepos=0 ;
    m_autodecrypt=0 ;
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
    m_filebuf=NULL ;
    m_filename=filename ;
    f264time(filename, &m_filetime);
    m_filelen=f264length(filename);
    m_filestamp=0;
    m_timestamp=0;
    m_filestart=(int)sizeof(struct hd_file);	// first data frame
    m_openmode=0;
    if (strchr(mode, 'w')) {
        if( file_bufsize>4096 ) {
            setvbuf(m_handle, NULL, _IOFBF, file_bufsize );
//            m_filebuf=(char *)mem_alloc(file_bufsize) ;
//            if( m_filebuf!=NULL ) {
//                setvbuf(m_handle, m_filebuf, _IOFBF, file_bufsize );
//            }
        }
//        file_syncreq = 1;
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
            write( &h264hd, sizeof(struct hd_file));				// write header
        }
        else {
            write(Dvr264Header, sizeof(struct hd_file));		// write header
        }
        m_filestart = tell();
        m_openmode=1;
        m_syncsize=0 ;
    }
    if( strchr(mode, 'r')) {
        if( fstat( fileno(m_handle), &dvrfilestat)==0 ) {
//            setvbuf(m_handle, NULL, _IOFBF, dvrfilestat.st_blksize );
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
        
        if( h264hd.flag == 0x484b4834 ) {
            m_fileencrypt=0 ;								// no encrypted
        }
        else {
            m_fileencrypt = 1 ;
            RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
            if( h264hd.flag == 0x484b4834 ) {
                if( file_nodecrypt ) {
                    m_autodecrypt=0 ;
                }
                else {
                    m_autodecrypt=1 ;
                    m_hdflag = h264hd.flag ;            // auto decrypted header
                }
            }
        }

        readkey();											// read key frame index
        if( m_keyarray.size()>0 ) {
            seek( m_keyarray[0].koffset );
        }
        framesize();									// update frame time stamp
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
            if( m_keyarray.size()>0 ) {
                writekey();
            }
//            file_syncreq = 1;
            if( file_close(m_handle)!=0 ) {
                dvr_log("Close file failed : %s",m_filename.getstring());
                rec_basedir="" ;
            }
//            sync();
        }
        else {
            file_close(m_handle);
        }
        m_handle = NULL;
        if( m_filebuf!=NULL  ) {
            mem_free( m_filebuf );
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
        return file_read(buffer, buffersize, m_handle);
    }
    else {
        return 0 ;
    }
}

int dvrfile::write(void *buffer, size_t buffersize)
{
    if( isopen() ) {
//        m_syncsize+=buffersize ;
//        if(m_syncsize>file_bufsize) {
//            fdatasync( fileno(m_handle) );
//            m_syncsize=0 ;
//        }
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
        m_keyarray.add(keytime);
        if( m_keyarray.size()==1 ) {
            writekey();											// record first key frame pos
        }
    }
    if( m_fileencrypt ) {
        struct hd_frame * pframe = (struct hd_frame *)buffer ;
        int esize ;
        char encbuf[1024] ;
        if( pframe->flag==1 && size > sizeof( hd_frame )) {
            write( wbuf, sizeof(hd_frame) );
            wbuf+=sizeof(hd_frame);
            size-=sizeof(hd_frame);
            if( pframe->framesize>1024 ) {
                esize=1024 ;
            }
            else {
                esize=pframe->framesize ;
            }
            memcpy( encbuf, wbuf, esize );
            RC4_block_crypt( (unsigned char *)encbuf, esize, 0, file_encrypt_RC4_table, 1024);
            if( write( encbuf, esize)!=esize ) {
                return 0 ;
            }
            wbuf+=esize ;
            size-=esize ;
            if( size<0 ) {
                return 0 ;
            }
            else if( size==0 ) {
                return 1 ;
            }
        }
    }
    if( write( wbuf, size )==(int)size ) {
        return 1;
    }
    return 0;
}

// read one frame
int dvrfile::readframe(void * framebuf, size_t bufsize)
{
    int frsize ;
    int encsize ;
    int rsize=0 ;
    struct hd_frame * pframe;
    frsize = framesize() ;
    if( frsize<=0 ) {
        return 0 ;
    }
    else if( (int)bufsize>=frsize ) {
        rsize=read(framebuf, frsize);
        if( m_autodecrypt ) {
            pframe = (struct hd_frame *)framebuf ;
            if( pframe->flag==1 && m_fileencrypt ) {
                encsize = pframe->framesize ;
                if( encsize>1024 ) {
                    encsize=1024 ;
                }
                RC4_block_crypt( ((unsigned char *)framebuf)+sizeof(struct hd_frame), encsize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
            }
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

// read one frame, return 0 for error, 
int dvrfile::readframe(struct cap_frame *pframe)
{
    pframe->frametype = frametype() ;
    pframe->framesize = framesize();
    if( pframe->framesize<=0 ) {
        pframe->framesize = 4096 ;
    }
    pframe->framedata = (char *) mem_alloc(pframe->framesize);
    if (pframe->framedata == NULL)
        return 0;
    return readframe (pframe->framedata, pframe->framesize);
}

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

// return current frame time
dvrtime dvrfile::frametime()
{
    dvrtime t=m_filetime ;
    if( isopen() ) {
        if( m_timestamp>=m_filestamp ) {
            time_dvrtime_addms(&t, tstamp2ms(m_timestamp-m_filestamp) );
        }
    }
    return t;
}	

#define SEARCHBUFSIZE	(0x10000)

int dvrfile::nextframe()
{
    int frsize;
    int pos;
    int readbytes;
    int j, i ;
    int *searchbuf;
    frsize=framesize() ;
    if (frsize>0) {
        seek(frsize, SEEK_CUR );
        return tell();
    } 
    else {					// not a frame structure
        pos = tell();
        pos &= (~3);
        seek(pos, SEEK_SET);
        searchbuf = (int *) mem_alloc(SEARCHBUFSIZE);	// 64Kbytes
        for (j = 0; j < 16; j++) {	// search 16 blocks, 1Mbytes
            pos = tell();
            readbytes = read(searchbuf, SEARCHBUFSIZE);
            if (readbytes < 16 ) {
                mem_free(searchbuf);
                return 0;		// file ends
            }
            for (i = 0; i < (readbytes / (int)(sizeof(int))); i++) {
                if (searchbuf[i] == 1) {
                    // find it
                    seek(pos+ i * (sizeof(int)), SEEK_SET );
                    if( frametype() ) {		// find a correct frame
                        mem_free(searchbuf);
                        return tell();
                    }
                    seek(pos+SEARCHBUFSIZE, SEEK_SET );
                }
            }
        }
        mem_free(searchbuf);
        return 0;				// could not find, file error!
    }
    return 0;
}

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
    if( m_keyarray.size()>0 ) {                         // key frame index available
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
    if( strcmp( &pk[l-4], ".264")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=file_open( pk, "r" );
        if( keyfile ) {
            struct dvr_key_t key;
            while(fscanf(keyfile,"%d,%d\n", &(key.ktime), &(key.koffset))==2){
                m_keyarray.add(key);
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
    if( strcmp( &pk[l-4], ".264")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=file_open( pk, "w" );
        if( keyfile ) {
            for( i=0;i<m_keyarray.size();i++) {
                struct dvr_key_t * pkey ;
                pkey = m_keyarray.at(i);
                fprintf(keyfile,"%d,%d\n", pkey->ktime, pkey->koffset);
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
    int seekms ;					// seek in milliseconds frome time of file
    if( !isopen() ) {
        return 0;
    }
    if( *seekto < m_filetime ) {
        seek(m_filestart, SEEK_SET );
        return 0;
    }
    seekms = time_dvrtime_diffms( seekto, &m_filetime );
    if( seekms/1000 >= m_filelen ) {
        seek(0, SEEK_END );
        return 0 ;
    }
    if( m_keyarray.size()>0 ) {                         // key frame index available
        for( i=m_keyarray.size()-1; i>=0; i-- ) {
            if( m_keyarray[i].ktime<= seekms ) {
                break;
            }
        }
        if( i< 0) i=0;
        seek(m_keyarray[i].koffset, SEEK_SET );
        return 1 ;
    }
    
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
    
    framesize();                                        // this will get current frame timestamp
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
}

// repair a .264 file, return 1 for success, 0 for failed
int dvrfile::repair()
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

        // if rec_busy, delay for 5 s
        int busywait ;
        for( busywait=0; busywait<500; busywait++) {
            if( disk_busy || cpu_usage()>0.6 ) {
                usleep(10000);
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
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_L_%s.264", 
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
    
    char lockfilehead[40] ;
    
    readheader( (void *)lockfilehead, 40 );
    lockfile.writeheader( lockfilehead, 40 );
    lockfile.m_fileencrypt=0 ;
    
    for( i=breakindex ; i<m_keyarray.size(); i++ ) {
        // if rec_busy, delay for 5 s
        int busywait ;
        for( busywait=0; busywait<500; busywait++) {
            if( disk_busy || cpu_usage()>0.6 ) {
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
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_L_%s.264", 
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
    sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_%d_N_%s.264", 
            m_filetime.year,
            m_filetime.month,
            m_filetime.day,
            m_filetime.hour,
            m_filetime.minute,
            m_filetime.second,
            locklength,
            g_hostname);
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
    if( strcmp( &oldkfile[lo-4], ".264" )== 0 &&
       strcmp( &newkfile[ln-4], ".264" )== 0 ) {
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
    if( strcmp( &kfile[l-4], ".264" ) == 0 ) {
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
//    if (file_syncreq) {
//        file_syncreq = 0;
//        sync();
//    }
//	  sync();
    disk_sync();
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
