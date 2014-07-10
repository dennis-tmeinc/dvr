#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void closeall(int fd)
{
  int fdlimit = sysconf(_SC_OPEN_MAX);

  while (fd < fdlimit)
    close(fd++);
}
int daemon(int nochdir, int noclose)
{
  switch (fork()) {
  case 0: break;
  case -1: return -1;
  default: _exit(0);
  }

  if (setsid() < 0)
    return -1;

  switch (fork()) {
	case 0: break;
	case -1: return -1;
	default: _exit(0);
  }

  if (!nochdir)
    chdir("/");

  if (!noclose) {
    closeall(0);
    open("/dev/null", O_RDWR);
    dup(0); dup(0);
  }

  return 0;
}

#if 0
int main()
{
   struct timeval time1, time2 ;   
   int t_diff;
   int dosfsckid;
   int format_id;
   char devname[60];
   FILE* fp;
   FILE* fw;
   FILE* ft;
   int num=0;
   if (daemon(0, 0) < 0) {
	perror("daemon");
	exit(1);
   }  
   gettimeofday(&time1, NULL);  
   while(1){
       gettimeofday(&time2, NULL);
       t_diff = (int)(int)(time2.tv_sec - time1.tv_sec) ;
       if(t_diff>10||t_diff<0){
	  gettimeofday(&time1, NULL); 
       } else if(num>60) {
	  break; 
       }
       sleep(1);
       num++;
   }
 #if 0   
   ft=fopen("/var/dvr/starttime.txt","w+t");
   if(ft){
      fprintf(ft,"time1:%d\n",time1.tv_sec);
   }
#endif   
   while(1){
       gettimeofday(&time2, NULL);
       t_diff = (int)(int)(time2.tv_sec - time1.tv_sec) ;     
       if(t_diff>180){	 
	  if(t_diff<200)
	    break;
	  else {
	      gettimeofday(&time1, NULL); 
	  }
       }
       sleep(1);
   }
#if 0   
   if(ft){
      fprintf(ft, "t_diff:%d\n",t_diff);
      fclose(ft);
   }
#endif   
   fw=fopen("/var/dvr/startfsckcheck","w");
   format_id=1;
   fprintf(fw,"%d",format_id);
   fclose(fw);
   fp=fopen("/var/dvr/sddev","r");
   if(fp){
       if (fscanf(fp, "%s", devname) != 1){
	  fclose(fp);    
	  return 0;           
       } 
       fclose(fp);
       fp=fopen("/var/dvr/sdcheckdone","r");
       if(fp){
	 fclose(fp);
	 return 0;
       }
       fp=fopen("/var/dvr/dosfsckid","r");
       if(fp){
	  fscanf(fp,"%d",&dosfsckid) ;
	  fclose(fp);
	  fw=fopen("/var/dvr/dosfsckkill","w");
	  if(fw){
	      format_id=1;
	      fprintf(fw,"%d",format_id);
	      fclose(fw);
	  }
	  kill(dosfsckid, SIGTERM);
	  sleep(1);
	  sleep(1);   
       }
   }
   return 0;
}
#endif
int main()
{
   int dosfsckid;
   int format_id;
   char devname[60];
   FILE* fp;
   FILE* fw;
   struct tms tms;
   clock_t t, ot, diff;   
   long ticks_per_sec = sysconf(_SC_CLK_TCK);  
   /* sizeof(clock_t):4, max_clock_t:0x7fffffff */
   long max_clock_t = (1 << (sizeof(clock_t) * 8 - 1)) - 1;
   unsigned long long elapsed; 
   
   if (daemon(0, 0) < 0) {
	perror("daemon");
	exit(1);
   }  

   ot = times(&tms);   
   while(1){
      t = times(&tms);
      diff = t - ot;
      if (diff < 0) { /* overflow */
	diff = diff + max_clock_t + 1;
      }
      elapsed = diff/ ticks_per_sec;
      if(elapsed>200)
	break;
      sleep(1);
   }
   fw=fopen("/var/dvr/startfsckcheck","w");
   format_id=1;
   fprintf(fw,"%d",format_id);
   fclose(fw);
   fp=fopen("/var/dvr/sddev","r");
   if(fp){
       if (fscanf(fp, "%s", devname) != 1){
	  fclose(fp);    
	  return 0;           
       } 
       fclose(fp);
       fp=fopen("/var/dvr/sdcheckdone","r");
       if(fp){
	 fclose(fp);
	 return 0;
       }
       fp=fopen("/var/dvr/dosfsckid","r");
       if(fp){
	  fscanf(fp,"%d",&dosfsckid) ;
	  fclose(fp);
	  fw=fopen("/var/dvr/dosfsckkill","w");
	  if(fw){
	      format_id=1;
	      fprintf(fw,"%d",format_id);
	      fclose(fw);
	  }
	  kill(dosfsckid, SIGTERM);
	  sleep(1);
	  sleep(1);   
       }
   }
   return 0;
}

