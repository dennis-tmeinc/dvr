
#include "../cfg.h"

#include "genclass.h"
#include "config.h"

config::config()
{
    m_merged = 0 ;
    m_dirty = 0;
}

config::~config()
{
    close();
}

#include <time.h>

void config::open(const char *configfilename, int mergedef )
{
    close();
    m_configfile=configfilename;
    readtxtfile ( configfilename, m_strlist );

    // don't merge DEFCONF itself
    if( m_strlist.size()>0 && strncmp( m_strlist[0], "#DEFCONF", 8 )==0 ) {
        return ;
    }

    // merge default config
    if( mergedef )
    {
        mergedefconf( CFG_DEFFILE );
    }
}

config::config(const char *configfilename, int mergedef )
{
    open( configfilename, mergedef );
}

void config::close()
{
    m_strlist.empty();
    m_dirty = 0;
    m_merged = 0 ;
}

// merge default configure
void config::mergedefconf( const char * defconffile )
{
    char * mgvalue ;
    char * line ;
    char * p ;
    FILE *rfile;
    string section ;
    char lbuffer[1024];

    rfile = fopen(defconffile, "r");
    if (rfile == NULL) {
        return ;
    }
    while (fgets(lbuffer, sizeof(lbuffer), rfile)) {
        line = str_skipspace( lbuffer ) ;
        if (*line == '[')
        {
            line++ ;
            p = strchr( line, ']' );
            if( p ) {
                *p='\0' ;
                section=str_trim(line);
            }
        }
        else {
            p=strchr( line, '#' );
            if( p ) {
                *p=0 ;
            }
            p=strchr( line, ';' );
            if( p ) {
                *p=0 ;
            }
            if( (mgvalue= strchr( line, '=' ))!=NULL ) {
                *mgvalue='\0' ;
                str_trimtail(line);
                if( findkey(section, line)<0 ) {
                    setvalue(section, line, str_trim(mgvalue+1) );
                }
            }
        }
    }
    fclose( rfile );
    m_merged = 1 ;
}

// search for section
int config::nextsection(int idx)
{
    if( idx>=0 ) {
        for (; idx < m_strlist.size(); idx++) {
            if (*str_skipspace(m_strlist[idx]) == '[') {
                return idx ;
            }
        }
    }
    return -1;					// not found ;
}

// search for section
int config::findsection(const char *section)
{
    int index ;
    char *line;
    int l;

    l=strlen(section);
    if( l<=0 ) {
        return 0 ;
    }

    for (index = 0; index < m_strlist.size(); index++) {
        line = str_skipspace(m_strlist[index]);
        if (*line == '[') {
            line = str_skipspace(line + 1);
            if( strncmp(section, line, l)==0 ) {
                line=str_skipspace (line+l) ;
                if( *line==']' ) {
                    return index + 1;	// return first line after the section
                }
            }
        }
    }
    return -1;					// not found ;
}

int config::findkey(int sectionline, const char *key)
{
    char * line;
    int l ;

    l=(int)strlen(key);
    if( l<=0 ) {
        return -1 ;
    }

    for (; sectionline < m_strlist.size(); sectionline++) {
        line = str_skipspace( m_strlist[sectionline] );
        if (*line == '[') {		// another section, quit
            break;
        }
        if( strncmp(line, key, l )==0 ) {
            line = str_skipspace(line+l) ;
            if( *line=='=' ) {
                return sectionline ;
            }
        }
    }
    return -1;
}

int config::findkey(const char *section, const char *key)
{
    int i;
    i = findsection( section );
    if( i<0 ) {
        return -1 ;
    }
    return findkey( i, key );
}

char * config::getvalue(const char *section, const char *key, string & value)
{
    int keyindex;
    char * p ;

    value="" ;
    keyindex = findkey(section, key);
    if (keyindex < 0) {
        return (char *)value;
    }

    p = strchr( m_strlist[keyindex], '=' ) ;
    if ( p != NULL ) {
        value = str_skipspace( p+1 );
        p = strchr(value, '#');	// comments
        if (p){
            *p = '\0';
        }
        p = strchr(value, ';');	// comments
        if (p) {
            *p = '\0';
        }
        str_trimtail(value);
    }
    return (char *)value;
}

char * config::getvalue(const char *section, const char *key)
{
    return getvalue( section, key, m_tempstr);
}

// search for section
char * config::enumsection(struct config_enum * enumkey)
{
    int i ;
    char *line;
    i = enumkey->line ;
    if( i<0 ) {
        i=0 ;
    }
    for ( ; i < m_strlist.size(); i++) {
        line = m_strlist[i];
        line = str_skipspace(line);
        if (*line == '[') {
            line = str_skipspace(line + 1);
            enumkey->key = line ;
            line = strchr( enumkey->key, ']' );
            if (line) {
                *line = '\0';
            }
            str_trimtail(enumkey->key);
            enumkey->line = i+1;
            return (char *)(enumkey->key);
        }
    }
    return NULL;					// not found ;
}

char * config::enumkey(const char *section, struct config_enum * enumkey )
{
    char *line;
    int i;
    if( enumkey->line <=0 ) {
        enumkey->line=findsection(section);	//	section line index
    }
    i=enumkey->line ;
    if (i < 0) {
        return NULL;
    }
    // section found
    for (; i < m_strlist.size(); i++) {
        line = m_strlist[i];
        line = str_skipspace(line);
        if (*line == '[') {		// another section, quit
            break;
        }
        enumkey->key = line ;
        line = strchr(enumkey->key, '#');	// comments
        if (line) {
            *line = '\0';
        }
        line = strchr(enumkey->key, ';');	// comments
        if (line) {
            *line = '\0';
        }
        line = strchr(enumkey->key, '=');	// key=value
        if (line){
            *line = '\0';
            enumkey->line = i+1 ;
            return str_trim(enumkey->key) ;
        }
    }
    return NULL;
}


int config::getvalueint(const char *section, const char *key)
{
    string pv ;
    int v = 0;
    pv = getvalue(section, key);
    if( pv.length()==0 ) {
        return 0 ;
    }
    if (sscanf(pv, "%d", &v)) {
        return v;
    } else {
        if( strcmp(pv, "yes")==0 ||
            strcmp(pv, "YES")==0 ||
            strcmp(pv, "on")==0 ||
            strcmp(pv, "ON")==0 ) {
            return 1 ;
        }
        else {
            return 0 ;
        }
    }
}

void config::setvalue(const char *section, const char *key, const char *value)
{
    int sectionindex ;
    int keyindex;
    string sectstr;
    string keystr;

    keystr.setbufsize(strlen(key) + strlen(value) + 4) ;
    sprintf(keystr, "%s=%s", key, value);

    sectionindex = findsection( section );
    if( sectionindex >= 0 ) {
        keyindex = findkey( sectionindex, key ) ;
        if( keyindex>=0 ) {
            // key found, replace it
            m_strlist[keyindex] = keystr ;
        }
        else {
            // no key
            keyindex = nextsection( sectionindex ) ;
            if( keyindex>=0 ) {
                m_strlist.insert(keystr,keyindex);
            }
            else {
                // last section
                m_strlist.add(keystr);
            }
        }
    }
    else {
        // section not exist
        // add a new section
        sectstr.setbufsize(strlen(section) + 4);
        sprintf(sectstr, "[%s]", section);
        m_strlist.add(sectstr);							// add new section
        m_strlist.add(keystr);							// add new key
    }
    m_dirty = 1;
}

void config::setvalueint(const char *section, const char *key, int value)
{
    char cvalue[40] ;
    sprintf(cvalue, "%d", value);
    setvalue(section, key, cvalue);
}

void config::removekey(const char *section, const char *key)
{
    int keyindex;
    keyindex = findkey(section, key);
    // key found
    if (keyindex >= 0) {
        m_strlist.remove(keyindex);
        m_dirty=1;
    }
}

void config::save()
{
    if (m_dirty) {
        if( m_merged ) {
            FILE *sfile ;
            config def(CFG_DEFFILE);
            sfile = fopen(m_configfile, "w");
            if( sfile ) {
                int i ;
                string section ;
                int sectionline=-1 ;
                string line ;
                char * key ;
                char * value ;
                char * p ;
                for( i=0; i<m_strlist.size(); i++) {
                    line = str_skipspace( m_strlist[i] ) ;
                    key = line;
                    p=strchr( key, '#' );
                    if( p ) {
                        *p=0 ;
                    }
                    p=strchr( key, ';' );
                    if( p ) {
                        *p=0 ;
                    }
                    if( *key=='[' )
                    {
                        // section name
                        key++ ;
                        p = strchr( key, ']' );
                        if( p ) {
                            *p='\0' ;
                        }
                        section=str_trim(key) ;
                        sectionline=i;
                        continue ;
                    }
                    else if( (value= strchr( key, '=' ))!=NULL ) {
                        *value='\0' ;
                        key=str_trim(key);
                        value=str_trim(value+1);
                        if( strcmp( def.getvalue( section, key ), value )==0 ) {
                            // skip the value as same on default setttings
                            continue ;
                        }
                    }
                    if( sectionline>=0 ) {
                        fputs(m_strlist[sectionline], sfile);
                        fputs("\n", sfile);
                        sectionline=-1;
                    }
                    fputs(m_strlist[i], sfile);
                    fputs("\n", sfile);
                }
                fclose(sfile);
            }
        }
        else {
            // the default configure file
            savetxtfile(m_configfile,m_strlist);
        }
        m_dirty = 0;
    }
}
