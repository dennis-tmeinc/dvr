#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main()
{
  FILE* fp=NULL;
  fp=fopen("/var/dvr/tab102check","w");
  if(!fp){
     return -1; 
  }
  fprintf(fp,"1");
  fclose(fp);
  while(1){
     fp=fopen("/var/dvr/tab102check","r");
     if(!fp){        
        break; 
     }
     fclose(fp);
     fp=fopen("/var/dvr/tab102.pid","r");
     if(!fp)
       break;
     fclose(fp);
     sleep(1);
  }
  fp=fopen("/var/dvr/dvrcurdisk","r");
  if(!fp)
    return -1;
  fclose(fp);
  fp=fopen("/var/dvr/dvrsvr.pid","r");
  if(!fp)
    return 0;
  fclose(fp);
  fp=fopen("/var/dvr/logcopy","w");
  if(!fp)
    return -1;
  fprintf(fp,"1");
  fclose(fp);
  sleep(1);
  while(1){
     fp=fopen("/var/dvr/logcopy","r"); 
     if(!fp)
       break;
     fclose(fp);
     sleep(1);
  }
  while(1){
     fp=fopen("/var/dvr/copyack","r");
     if(!fp)
       break;
     fclose(fp);
     sleep(1);
  }
  return 0;
}
