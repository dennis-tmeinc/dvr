#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>   
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/cfg.h"

#define MAX_DEVICE_NUM 10
#define MAX_PARTITIONS_PERDISK 4
#define MAX_PARTITION  32


char dvrconfigfile[]="/etc/dvr/dvr.conf" ;

typedef struct tdevice {
    char devicename[256];
    int  size;
    int  partitionNum;
    int initialed;    
} deviceinfo;

struct mounteddevice {
    deviceinfo tdevice[MAX_DEVICE_NUM]; 
    int total;
} m_deviceinfo;

class dir_find {
    protected:
        DIR * m_pdir ;
        struct dirent * m_pent ;
        struct dirent m_entry ;
        char m_dirname[PATH_MAX] ;
        char m_pathname[PATH_MAX] ;
        struct stat m_statbuf ;
    public:
        dir_find() {
            m_pdir=NULL ;
        }
        // close dir handle
        void close() {
            if( m_pdir ) {
                closedir( m_pdir );
                m_pdir=NULL ;
            }
        }
        // open an dir for reading
        void open( const char * path ) {
            int l ;
            close();
            m_pent=NULL ;
            strncpy( m_dirname, path, sizeof(m_dirname) );
            l=strlen( m_dirname ) ;
            if( l>0 ) {
                if( m_dirname[l-1]!='/' ) {
                    m_dirname[l]='/' ;
                    m_dirname[l+1]='\0';
                }
            }
            m_pdir = opendir(m_dirname);
        }
        ~dir_find() {
            close();
        }
        dir_find( const char * path ) {
            m_pdir=NULL;
            open( path );
        }
        int isopen(){
            return m_pdir!=NULL ;
        }
        // find directory.
        // return 1: success
        //        0: end of file. (or error)
        int find() {
            struct stat findstat ;
            if( m_pdir ) {
                while( readdir_r( m_pdir, &m_entry, &m_pent)==0  ) {
                    if( m_pent==NULL ) {
                        break;
                    }
                    if( (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='\0') || 
                       (m_pent->d_name[0]=='.' && m_pent->d_name[1]=='.' && m_pent->d_name[2]=='\0') ) {
                           continue ;
                       }
                    if( m_pent->d_type == DT_UNKNOWN ) { // d_type not available
                        strcpy( m_pathname, m_dirname );
                        strcat( m_pathname, m_pent->d_name );
                        if( stat( m_pathname, &findstat )==0 ) {
                            if( S_ISDIR(findstat.st_mode) ) {
                                m_pent->d_type = DT_DIR ;
                            }
                            else if( S_ISREG(findstat.st_mode) ) {
                                m_pent->d_type = DT_REG ;
                            }
                        }
                    }
                    return 1 ;
                }
            }
            m_pent=NULL ;
            return 0 ;
        }
        char * pathname()  {
            if(m_pent) {
                strcpy( m_pathname, m_dirname );
                strcat( m_pathname, m_pent->d_name );
                return (m_pathname) ;
            }
            return NULL ;
        }
        char * filename() {
            if(m_pent) {
                return (m_pent->d_name) ;
            }
            return NULL ;
        }
        
        // check if found a dir
        int    isdir() {
            if( m_pent ) {
                return (m_pent->d_type == DT_DIR) ;
            }
            else {
                return 0;
            }
        }
        
        // check if found a regular file
        int    isfile(){
            if( m_pent ) {
                return (m_pent->d_type == DT_REG) ;
            }
            else {
                return 0;
            }
        }
        
        // return file stat
        struct stat * filestat(){
            char * path = pathname();
            if( path ) {
                if( stat( path, &m_statbuf )==0 ) {
                    return &m_statbuf ;
                }
            }
            return NULL ;
        }
};

void killdvrprocess()
{
    FILE* fr;
    int mpid;
    int ret;
    fr=fopen("/var/dvr/dvrsvr.pid","r");
    if(!fr)
      return;
    ret=fscanf(fr,"%d",&mpid);
    if(ret==1){
      kill(mpid,SIGTERM);
    }
    fclose(fr);
    while(1){
       sleep(2);
       fr=fopen("/var/dvr/dvrsvr.pid","r");
       if(!fr)
	 break;
       fclose(fr);
    }

    return;
}

void scandiskinfo()
{
    char * diskname;
    char* str;
    int total=0;
    int i;
    int size;
    int classId=0;
    char filename[256];
    FILE* fr;
    struct stat diskstat;
    dir_find disks("/sys/block"); 
    m_deviceinfo.total=0;
    while( disks.find() ) {
	if( disks.isdir() ) {
	    diskname = disks.pathname();
	    if( stat(diskname, &diskstat)==0 ) {
	        str=strstr(diskname,"mmcblk");
		if(str){
		   str[7]='\0';
		   classId=0;
		} else {
		   str=strstr(diskname,"sd");
		   if(str){ 
		      str[3]='\0';
		      classId=1;
		   } else {
		      continue;
		   }
		}		
                snprintf(filename,sizeof(filename),"%s/size",diskname);
		fr=fopen(filename,"r");
		if(fr){
		    fscanf(fr,"%d",&size);
		    fclose(fr);
		    total=m_deviceinfo.total;	
		    	
		    sprintf(m_deviceinfo.tdevice[total].devicename,"%s",str);
		    m_deviceinfo.tdevice[total].size=size/2;
		    
		    dir_find dfind1(diskname);
		    int m_Num=0;
		    m_deviceinfo.tdevice[total].partitionNum=0;
		    while(dfind1.find()){
		        if(dfind1.isdir()){
			    if(classId){
			        if(strncmp(dfind1.filename(), "sd", 2)==0){
				    snprintf(filename,sizeof(filename),"%s/size",dfind1.pathname());
				    fr=fopen(filename,"r");
				    if(fr){
				        fscanf(fr,"%d",&size);
					fclose(fr);
					if(size>1024*1024){
					  m_Num++;
					}
				    }
				}
			    } else {
				if(strncmp(dfind1.filename(), "mmcblk", 6)==0 ){
				    snprintf(filename,sizeof(filename),"%s/size",dfind1.pathname());
				    fr=fopen(filename,"r");
				    if(fr){
				        fscanf(fr,"%d",&size);
					fclose(fr);
					if(size>1024*1024){
					  m_Num++;
					}
				    }
				}			      
			    }
			}
		    }
		    
		    m_deviceinfo.tdevice[total].partitionNum=m_Num;
		    if(m_Num>1){
			m_deviceinfo.tdevice[total].initialed=1;
		    } else {
			m_deviceinfo.tdevice[total].initialed=0;
		    }
		    
		    m_deviceinfo.total++;
		}
	    }
	}
    }//while
    return;  
 
};

int FormatFlashToFAT32(char* diskname,int major,int minor)
{  
    char devname[60];
    char cmd[156];
    int ret=0;
    pid_t format_id;
    struct stat diskstat;
    sprintf(devname,"/dev/%s",diskname);  
    printf("%s major:%d minor:%d\n",diskname,major,minor);
    if( stat(diskname, &diskstat)!=0 ) {
        snprintf(cmd,sizeof(cmd),"mknod /dev/%s b %d %d",diskname,major,minor);
	system(cmd);
	sleep(1);
    }
    format_id=fork();
    if(format_id==0){     
       execlp("/mnt/nand/mkdosfs", "mkdosfs", "-F","32", devname, NULL );  
       exit(0);
    } else if(format_id>0){    
        int status=-1;
        waitpid( format_id, &status, 0 ) ;   
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            ret=1 ;
        }	
    }    
    if(ret>=0){
       printf("%s is formatted to FAT32\n",devname);	        
    } else {
       printf("format %s failed",devname); 
    }
    return ret;
}

int doformat(char* devname)
{
    char devicename[256];
    char filename[256];
    int size;
    int major,minor;
    FILE* fr;
    snprintf(devicename,sizeof(devicename),"/sys/block/%s",devname);
    dir_find dfind(devicename);
    while(dfind.find()){
	if(dfind.isdir()){
	    if(strncmp(dfind.filename(), "sd", 2)==0){
		  snprintf(filename,sizeof(filename),"%s/size",dfind.pathname());
		  fr=fopen(filename,"r");
		  if(fr){
		      fscanf(fr,"%d",&size);
		      fclose(fr);
		      if(size>1024*1024){
			  snprintf(filename,sizeof(filename),"%s/dev",dfind.pathname());
			  fr=fopen(filename,"r");
			  if(fr){
			      fscanf(fr,"%d:%d",&major,&minor);			      
			      fclose(fr);
			      
			      FormatFlashToFAT32(dfind.filename(),major,minor);
			  }
		      }
		  }	         	      
	    } else if(strncmp(dfind.filename(), "mmcblk", 6)==0 ){
		  snprintf(filename,sizeof(filename),"%s/size",dfind.pathname());
		  fr=fopen(filename,"r");
		  if(fr){
		      fscanf(fr,"%d",&size);
		      fclose(fr);
		      if(size>1024*1024){
   			  snprintf(filename,sizeof(filename),"%s/dev",dfind.pathname());
			  fr=fopen(filename,"r");
			  if(fr){
			      fscanf(fr,"%d:%d",&major,&minor);			      
			      fclose(fr);
			      
			      FormatFlashToFAT32(dfind.filename(),major,minor);
			  }
		      }
		  }	        	      
	    }	  
	}      
    }
}
int  dopartition(char* devname,int start,int end)
{
    char start_offset[256];
    char end_offset[256];
    pid_t format_id; 
    int ret=0;
    snprintf(start_offset,sizeof(start_offset),"%d",start);
    snprintf(end_offset,sizeof(end_offset),"%d",end);
    format_id=fork();
    if(format_id==0){        
       execlp("/mnt/nand/parted","parted","-s",devname,"mkpart","primary","fat32",start_offset,end_offset, NULL );  
       exit(0);
    } else if(format_id>0){    
        int status=-1;
        waitpid( format_id, &status, 0 ) ;   
        if( WIFEXITED(status) && WEXITSTATUS(status)==0 ) {
            ret=1 ;
        }	      
    }
    if(ret>=0){
       return 0;
    }
    return -1;
}


int main()
{
   FILE* fw;
   int i,j;
   char diskmsgfile[128] ;
   char devname[256];
   char cmd[256];
   int numberofdiskused;
   int partitionperdisk;
   int partitionsize;
   int start,end;
   int partitionneeded=0;
   FILE * fmsg ;  
   char * msg ;
   int fd;
   
   config dvrconfig(dvrconfigfile);
   numberofdiskused= dvrconfig.getvalueint("system","numberofdiskused");
   partitionperdisk= dvrconfig.getvalueint("system", "partitionperdisk");
   
   if(numberofdiskused<1){
      numberofdiskused=1; 
   }
   if(partitionperdisk<2){
      partitionperdisk=2; 
   }
   
    // progress file
    sprintf( diskmsgfile, "%s/diskprog", getenv("DOCUMENT_ROOT") ); 
    setenv( "DISKPROG", diskmsgfile, 1 );
    fmsg=fopen(diskmsgfile, "w");
    if( fmsg ) {
	fprintf(fmsg,"0");
	fclose( fmsg );
    }

	// msg file
    sprintf( diskmsgfile, "%s/diskmsg", getenv("DOCUMENT_ROOT") ); 
    setenv( "DISKMSG", diskmsgfile, 1 );
    fmsg=fopen(diskmsgfile, "w");
    if( fmsg ) {
	fprintf(fmsg,"\n");
	fclose( fmsg );
    } 
    fd = open("/dev/null", O_RDWR );
    dup2(fd, 0);                 // set dummy stdin stdout, also close old stdin (socket)
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);    
    
    fd = open(diskmsgfile, O_RDWR ); 
    
   //printf("number of disk used:%d partitions per disk:%d\n",numberofdiskused,partitionperdisk);

   fw=fopen("/var/dvr/partition","w");
   if(!fw)
     return -1;
   fprintf(fw,"1");
   fclose(fw);
   m_deviceinfo.total=0;
   scandiskinfo();   
   
   if(m_deviceinfo.total!=numberofdiskused){
      msg="Actual number of the inserted SD doesn't match the setting\n";
      write(fd, msg, strlen(msg)) ; 
      close(fd);
      return -1; 
   }

   partitionneeded=0;
   for(i=0;i<m_deviceinfo.total;++i){
        if(m_deviceinfo.tdevice[i].initialed)
	  continue;
	partitionneeded=1;
	break;     
   }
   if(!partitionneeded){
	//printf("There is no SD disk to be partitioned\n");
	msg="No SD disk can be partitioned\n";
	write(fd, msg, strlen(msg)) ; 
	close(fd);
	return -1;
   }  
   killdvrprocess();
   system("/mnt/nand/dvr/umountdisks");
   system("killall tdevd");  
   for(i=0;i<m_deviceinfo.total;++i){
        if(m_deviceinfo.tdevice[i].initialed)
	  continue;
	
        snprintf(devname,sizeof(devname),"/dev/%s",m_deviceinfo.tdevice[i].devicename);
        //remove the current partition first;
	for(j=0;j<m_deviceinfo.tdevice[i].partitionNum;++j){
	   snprintf(cmd,sizeof(cmd),"/mnt/nand/parted %s rm 1",devname);
	   system(cmd);
	   sleep(1);
	}
	
	//calculate the size of each partition
	
	partitionsize=m_deviceinfo.tdevice[i].size/partitionperdisk;

	partitionsize=partitionsize/1024;
	//do partitions on SD flash
	for(j=0;j<partitionperdisk;++j){
	    start=j*partitionsize;
	    end=(j+1)*partitionsize;
	    msg="Partition.......................\n";
	    write(fd, msg, strlen(msg)) ; 
	   // printf("partition:%d start:%d end:%d\n",j,start,end);
	    dopartition(devname,start,end);
	} 
	//format each partition to FAT32
	//msg="Formating......................\n";
	//write(fd, msg, strlen(msg)) ; 
	doformat(m_deviceinfo.tdevice[i].devicename);
   }   
   unlink("/var/dvr/partition");
   system("/mnt/nand/dvr/tdevd /mnt/nand/dvr/tdevhotplug");
   system("/mnt/nand/dvr/tdevmount /mnt/nand/dvr/tdevhotplug");
   
   system("/mnt/nand/dvr/dvrsvr");
   msg="All SD disks had been successfuly partitioned and started to do recording\n";
   write(fd, msg, strlen(msg)) ; 
   close(fd);
   return 0;
}