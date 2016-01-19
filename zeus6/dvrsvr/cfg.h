
#ifndef __cfg_h__
#define __cfg_h__

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

// struct used to enumerate config file
struct config_enum {
    int line ;                                          // initialize to 0 before call enumkey/enumsection
    string key ;
} ;

class config {
  protected:
	array <string> m_strlist ;
	string  m_configfile ;
	string  m_tempstr ;
	int 	m_dirty ;
	int findsection(char *section);
	int findkey(int index, char *key );
  public:
	config(char *configfilename);
	char * enumsection(int * index);
	char * enumsection(struct config_enum * e);
	char * enumkey( int * index );
	char * enumkey(char *section, struct config_enum * e );

	string & getvalue(char *section, char *key);
	int 	getvalueint(char *section, char *key);
	void 	setvalue(char *section, char *key, char *value);
	void 	setvalueint(char *section, char *key, int value);
	void 	removekey(char *section, char *key);
	void 	save();
	void 	load();
};

#endif                                                  // __cfg_h__
