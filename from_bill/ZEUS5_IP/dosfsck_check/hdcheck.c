#include <stdio.h>
int main()
{
  FILE* fr;
  int size=0;
  fr=fopen("/var/dvr/disksize","r");
  if(fr){
    fscanf(fr,"%d",&size);
    fclose(fr); 
  }
  printf("size=%d\n",size);
  if(size>587202560){
     fr=fopen("/var/dvr/hdfound","w"); 
     if(fr){
        fprintf(fr,"1"); 
	fclose(fr);
     }
  }
  return 0;
}
