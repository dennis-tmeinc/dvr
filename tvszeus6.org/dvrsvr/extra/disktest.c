#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>


void list_dir(char * dir)
{
	struct dirent * pdirentry;
	DIR * hdir = opendir(dir);
	if( hdir==NULL )
		return ;
	while( (pdirentry = readdir(hdir))!=NULL ) {
		printf("\n%02x - %s", pdirentry->d_type, pdirentry->d_name);
	}
	
}

int main(int argc, char * argv[])
{
	list_dir(argv[1]);
	return 0;
}
