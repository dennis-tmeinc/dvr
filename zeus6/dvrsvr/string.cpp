
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "genclass.h"

int savetxtfile(char *filename, array <string> & strlist )
{
	FILE *sfile ;
	int i;
	sfile = fopen(filename, "w");
	if( sfile ) {
		for (i = 0; i < strlist.size(); i++) {
			fputs(strlist[i].getstring(), sfile);
			fputs("\n", sfile);
		}
		fclose(sfile);
	}
	return strlist.size();
}

int readtxtfile(char *filename, array <string> & strlist)
{
	FILE *rfile;
	string str ;
	char * buffer = new char [4096] ;
	
	rfile = fopen(filename, "r");
	if( rfile ) {
		while (fgets(buffer, 4096, rfile)) {
			str=buffer ;
			str.trim();
			strlist.add(str);
		}
		fclose(rfile);
	}

	delete buffer ;
	return strlist.size();
}

