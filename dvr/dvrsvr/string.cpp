
#include <stdio.h>

#include "genclass.h"

char * str_trimtail(char *line)
{
	int len;
	len = strlen(line);
    while(len>0) {
		if( line[len-1] <= ' ' && line[len-1]>0  ) {
			len-- ;
		}
		else {
			break;
		}
	}
	if( len>=0 ) line[len]='\0' ;
    return line ;
}

char * str_skipspace(char *line)
{
	while (*line >0 && *line<=' ' )
		line++;
	return line;
}

char * str_trim(char *line)
{
    return  str_trimtail(str_skipspace(line)) ;
}

int savetxtfile(const char *filename, array <string> & strlist )
{
	FILE *sfile ;
	int i;
	sfile = fopen(filename, "w");
	if (sfile == NULL) {		//      can't open file
		return 0;
	}
	for (i = 0; i < strlist.size(); i++) {
		fputs(strlist[i], sfile);
		fputs("\n", sfile);
	}
	fclose(sfile);
	return strlist.size();
}

int readtxtfile(const char *filename, array <string> & strlist)
{
	FILE *rfile;
	char buffer[1024];
	string str ;
	rfile = fopen(filename, "r");
	if (rfile == NULL) {
		return 0;
	}
	while (fgets(buffer, sizeof(buffer), rfile)) {
		str_trimtail(buffer);
		str=buffer ;
		strlist.add(str);
	}
	fclose(rfile);
	return strlist.size();
}

