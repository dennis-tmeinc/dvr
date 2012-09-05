
#ifndef __config_h__
#define __config_h__

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "genclass.h"

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
    int     m_merged;
    int     m_dirty;
    void mergedefconf( const char * defconffile );
    int nextsection(int idx);
    int findsection(const char *section);
    int findkey(int sectionline, const char *key);
    int findkey(const char *section, const char *key);
public:
    config();
    config(const char *configfilename, int mergedef=1 );
    ~config();
    void open(const char *configfilename, int mergedef=1 );
    void close();
    char * getvalue(const char *section, const char *key);
    char * getvalue(const char *section, const char *key, string & value);
    int getvalueint(const char *section, const char *key);
    void setvalue(const char *section, const char *key, const char *value);
    void setvalueint(const char *section, const char *key, int value);
    char * enumsection(struct config_enum * enumkey);
    char * enumkey(const char *section, struct config_enum * enumkey);
    void removekey(const char *section, const char *key);
    void save();
};

#endif                                                  // __config_h__
