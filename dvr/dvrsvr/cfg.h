
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
    string key ;
    int line ;                                          // initialize to 0 before call enumkey/enumsection
} ;

class config {
  protected:
	array <string> m_strlist ;
	string  m_configfile;
	string  m_tempstr;
	int 	m_dirty;
    void mergedefconf( char * defconffile );
    int nextsection(int idx);
    int findsection(char *section);
    int findkey(int sectionline, char *key);
    int findkey(char *section, char *key);
  public:
	config();
	config(char *configfilename);
    ~config();
	void open(char *configfilename);
    void close();
    char * getvalue(char *section, char *key);
    char * getvalue(char *section, char *key, string & value);
	int getvalueint(char *section, char *key);
	void setvalue(char *section, char *key, char *value);
  	void setvalueint(char *section, char *key, int value);
	char * enumsection(struct config_enum * enumkey);
	char * enumkey(char *section, struct config_enum * enumkey);
	void removekey(char *section, char *key);
	void save();
};

#endif                                                  // __cfg_h__
