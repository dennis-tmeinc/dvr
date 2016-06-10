
#include "dvr.h"

// #define FILESYNCSIZE (0x200000)
#define FILETRUNCATINGSIZE (2*1024*1024)

static int file_bufsize;								// file buffer size
static int file_syncreq;
static int file_encrypt ;
static unsigned char file_encrypt_RC4_table[1024] ;		// RC4 encryption table

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
} 

dvrfile::~dvrfile()
{
    close();
}

int dvrfile::open(const char *filename, char *mode, int initialsize)
{
    struct hd_file h264hd ;
    struct stat dvrfilestat ;
    close();
    m_initialsize = 0;
    m_handle = fopen(filename, mode);
    if (m_handle == NULL) {
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
        //		setvbuf(m_handle, NULL, _IOFBF, DVRFILEWBUFSIZE );
        m_filebuf=(char *)malloc(file_bufsize) ;
        if( m_filebuf!=NULL ) {
            setvbuf(m_handle, m_filebuf, _IOFBF, file_bufsize );
        }
        file_syncreq = 1;
        if (initialsize > 0) {
            seek(initialsize - 1);
            if (fputc(0, m_handle) == EOF) {
                close();
                return 0;
            };
            m_initialsize = initialsize;
        }
        fseek(m_handle,0,SEEK_SET);
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
            m_filebuf=(char *)malloc(dvrfilestat.st_blksize) ;
            if( m_filebuf!=NULL  ) {
                setvbuf(m_handle, m_filebuf, _IOFBF, dvrfilestat.st_blksize );
            }
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
        m_fileencrypt=-1 ;
        read(&h264hd, sizeof(h264hd) );
        if( h264hd.flag == 0x484b4834 ) {
            m_fileencrypt=0 ;								// no encrypted
        }
        else {
            if( file_encrypt ) {
                RC4_block_crypt( (unsigned char*)&h264hd, sizeof(h264hd), 0, file_encrypt_RC4_table, 1024);
                if( h264hd.flag == 0x484b4834 ) {
                    m_fileencrypt=1 ;
                }
            }
        }
        if( m_fileencrypt<0 ) {
            close();		
            return 0;
        }
        readkey();											// read key frame index
        if( m_keyarray.size()>0 ) {
            time_dvrtime_addms(&m_filetime, m_keyarray[0].ktime);
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
    if (m_handle) {
        if( m_openmode ) {  // open for write
            size = tell();
            if (m_initialsize > size) {
                truncate(size);
            }
            if( m_keyarray.size()>0 ) {
                writekey();
            }
            file_syncreq = 1;
            fclose(m_handle);
            sync();
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
    if( m_handle ) {
        return fread(buffer, 1, buffersize, m_handle);
    }
    else {
        return 0 ;
    }
}

int dvrfile::write(void *buffer, size_t buffersize)
{
    if( m_handle ) {
        m_syncsize+=buffersize ;
        if(m_syncsize>(FILETRUNCATINGSIZE/2)) {
            fdatasync( fileno(m_handle) );
            m_syncsize=0 ;
        }
        return fwrite(buffer, 1, buffersize, m_handle);
    }
    else {
        return 0;
    }
}

int dvrfile::tell() 
{
    if( m_handle ) {
        return ftell(m_handle);
    }
    else {
        return 0;
    }
}

int dvrfile::seek(int pos, int from) 
{
    if( m_handle ) {
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

// write a new frame, return 1 success, 0: failed
int dvrfile::writeframe(void *buffer, size_t size, int keyframe, dvrtime * frametime)
{
    char * wbuf = (char *)buffer ;
    if( keyframe ) {
        key_t keytime ;
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
        write( wbuf, sizeof(hd_frame) );
        wbuf+=sizeof(hd_frame);
        size-=sizeof(hd_frame);
        if( pframe->flag==1 ) {
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
    else if( (int)bufsize==frsize ) {
        rsize=read(framebuf, bufsize);
        pframe = (struct hd_frame *)framebuf ;
        if( pframe->flag==1 && m_fileencrypt ) {
            encsize = pframe->framesize ;
            if( encsize>1024 ) {
                encsize=1024 ;
            }
            RC4_block_crypt( ((unsigned char *)framebuf)+sizeof(struct hd_frame), encsize, 0, file_encrypt_RC4_table, 1024);		// decrypt frame
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
    
    if( framepos> ( m_filesize ) ) {
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
    while (nextframe() != 0) {
        if (frametype() == FRAMETYPE_KEYVIDEO)
            return tell();
    }
    return 0;					// could not found
}

int dvrfile::prevkeyframe()
{
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
    m_keyarray.empty();
    if( strcmp( &pk[l-4], ".264")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=fopen( pk, "r" );
        if( keyfile ) {
            struct key_t key;
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
    if( strcmp( &pk[l-4], ".264")==0 ) {
        strcpy(&pk[l-4],".k");
        keyfile=fopen( pk, "w" );
        if( keyfile ) {
            for( i=0;i<m_keyarray.size();i++) {
                struct key_t * pkey ;
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
    int seeks ;					// seek in seconds frome time of file
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
    if( m_keyarray.size()>0 ) {                         // key frame index available
        for( i=m_keyarray.size()-1; i>=0; i-- ) {
            if( m_keyarray[i].ktime/1000 <= seeks ) {
                break;
            }
        }
        if( i< 0) i=0;
        seek(m_keyarray[i].koffset, SEEK_SET );
        return 1 ;
    }

    // seek to end of file
    seek(0, SEEK_END);
    return 0 ;

}

// repair a .264 file, return 1 for success, 0 for failed
int dvrfile::repair()
{
    struct hd_frame frame;
    int filepos, filelength;
    int framecount;
    DWORD starttimestamp ;
    DWORD endtimestamp ;
    char newfilename[512] ;
    char * rn ;
    char * tail ;
    key_t keyt ;
    int frsize ;
    
    if( m_filesize < (FILETRUNCATINGSIZE+512000) ) {        // file too small, not worth to repair
        return 0 ;
    }
    
    // pre-truncate bad files, to repair file system error ?
    m_filesize-=FILETRUNCATINGSIZE ;
    
    truncate( m_filesize );
    
    m_keyarray.setsize(0);
    
    int ms = m_filetime.milliseconds;
    
    framecount = 0;
    filepos=m_filestart;
    starttimestamp = 0 ;                                        // initial start time stamp
    seek(filepos, SEEK_SET);
    while ((frsize=framesize()) > 0) {
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
        if( HD264_FRAMESUBFRAME(frame) == 1 &&
            HD264_FRAMETYPE(frame) == 3 ) 
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

int dvrfile::reapirpartiallock(int length, int locklength)
{
/*    
    dvrfile lockfile ;
    dvrtime seekto ;
    int lockstartpos ;
     char lockfilename[300] ;
     char * lockfilenamebase ;
    seekto = m_filetime+(length-locklength) ;
    if( this->seek(seekto) ) {              // ok
        lockstartpos = tell();

        strcpy( lockfilename, m_filename.getstring() );
        lockfilenamebase = basefilename( lockfilename );
        sprintf(lockfilenamebase+5, "%04d%02d%02d%02d%02d%02d_0_%c_%s.264", 
                seekto.year,
                seekto.month,
                seekto.day,
                seekto.hour,
                seekto.minute,
                seekto.second,
                'L',
                hostname);
        lockfile.open(lockfilename, "wb");
        if( !lockfile.isopen() ) {
            return 0 ;
        }

        int frsize ;
        int frtype ;
        char * frbuffer ;
        for( ; ; ) {
            // read frames ;
            frtype = frametype();
            frsize = framesize();
            if( frsize<=0 ) {
                break ;
            }
            frbuffer = malloc( frsize );
            if( readframe (frbuffer, frsize)>0 ) {
                // write frames
                lockfile.writeframe( frbuffer, frsize, frtype, frametime );
            }
            else {
                break;
            }
        }

    // rename new locked file
    lockfile.close();
        if( rename( m_filename.getstring(), newfilename )>=0 ) {         // success
            return 1;
        }
    
    

     // resize original file.
     fseek( lockstartpos ) ;
     m_initialsize = lockstartpos + 1 ;       // set a fake init size, so close() will do file truncate
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

    // rename new locked file
    
        
    }
    
    */
    return 0;
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
    if( strcmp( &oldkfile[lo-4], ".264" )== 0 &&
       strcmp( &newkfile[ln-4], ".264" )== 0 ) {
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
    if( strcmp( &kfile[l-4], ".264" ) == 0 ) {
        strcpy( &kfile[l-4], ".k" ) ;
        ::remove( kfile );
    }
    return res ;
}

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
}

void file_uninit()
{
    sync();
}
