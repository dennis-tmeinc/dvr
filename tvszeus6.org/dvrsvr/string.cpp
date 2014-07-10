
#include "dvr.h"

static void trimtail(char *line)
{
	char *tail;
	int len;
	len = strlen(line);
	if (len == 0)
		return;
	tail = line + len - 1;
	while (*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == '\r') {
		*tail-- = '\0';
	}
}

int savetxtfile(char *filename, array <string> & strlist )
{
	FILE *sfile ;
	int i;
	sfile = fopen(filename, "w");
	if (sfile == NULL) {		//      can't open file
		return 0;
	}
	for (i = 0; i < strlist.size(); i++) {
		fputs(strlist[i].getstring(), sfile);
		fputs("\n", sfile);
	}
	fclose(sfile);
	return strlist.size();
}

int readtxtfile(char *filename, array <string> & strlist)
{
	FILE *rfile;
	char buffer[1024];
	string str ;
	rfile = fopen(filename, "r");
	if (rfile == NULL) {
		return 0;
	}
	while (fgets(buffer, 1020, rfile)) {
		trimtail(buffer);
		str=buffer ;
		strlist.add(str);
	}
	fclose(rfile);
	return strlist.size();
}

