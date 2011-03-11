
#include "genclass.h"
#include "cfg.h"

static char defconf[]="/davinci/dvr/defconf" ;

config::config()
{
	m_dirty = 0;
}

config::~config()
{
    close();
}

#include <time.h>

void config::open(char *configfilename)
{
    close();
    m_configfile=configfilename;
	readtxtfile ( configfilename, m_strlist );
    // merge default config
    if( m_strlist.size()<=0 ||
        strncmp( m_strlist[0].getstring(), "#DEFCONF", 8 )!=0 ) 
    {
        mergedefconf( defconf );
    }
	m_dirty = 0;
}

config::config(char *configfilename)
{
    open( configfilename );
}

void config::close()
{
    m_strlist.empty();
}

// merge default configure
void config::mergedefconf( char * defconffile )
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
                if( findkey(section.getstring(), line)<0 ) {
                    setvalue(section.getstring(), line, str_trim(mgvalue+1) );
                }
            }
        }
    }
    fclose( rfile );
}
            
// search for section
int config::nextsection(int idx)
{
    if( idx>=0 ) {
        for (; idx < m_strlist.size(); idx++) {
            if (*str_skipspace(m_strlist[idx].getstring()) == '[') {
				return idx ;
            }
        }
    }
	return -1;					// not found ;
}

// search for section
int config::findsection(char *section)
{
	int index ;
	char *line;
	int l;

	l=strlen(section);
	if( l<=0 ) {
		return 0 ;
	}

	for (index = 0; index < m_strlist.size(); index++) {
		line = str_skipspace(m_strlist[index].getstring());
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

int config::findkey(int sectionline, char *key)
{
	char * line;
	int l ;
       
	l=(int)strlen(key);
    if( l<=0 ) {
        return -1 ;
    }

	for (; sectionline < m_strlist.size(); sectionline++) {
		line = str_skipspace( m_strlist[sectionline].getstring() );
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

int config::findkey(char *section, char *key)
{
    int i;
    i = findsection( section );
    if( i<0 ) {
        return -1 ;
    }
    return findkey( i, key );
}

char * config::getvalue(char *section, char *key, string & value)
{
	int keyindex;
    char * p ;

    value="" ;
	keyindex = findkey(section, key);
	if (keyindex < 0) {
		return value.getstring();
	}

    p = strchr( m_strlist[keyindex].getstring(), '=' ) ;
	if ( p != NULL ) {
        value = str_skipspace( p+1 ); 
        p = strchr(value.getstring(), '#');	// comments
        if (p){
            *p = '\0';
        }
        p = strchr(value.getstring(), ';');	// comments
        if (p) {
            *p = '\0';
        }
        str_trimtail(value.getstring());
    }
	return value.getstring();
}

char * config::getvalue(char *section, char *key)
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
		line = m_strlist[i].getstring();
		line = str_skipspace(line);
		if (*line == '[') {
			line = str_skipspace(line + 1);
			enumkey->key = line ;
			line = strchr( enumkey->key.getstring(), ']' );
			if (line) {
				*line = '\0';
			}
			str_trimtail(enumkey->key.getstring());
			enumkey->line = i+1;
			return enumkey->key.getstring();
		}
	}
	return NULL;					// not found ;
}

char * config::enumkey(char *section, struct config_enum * enumkey )
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
		line = m_strlist[i].getstring();
		line = str_skipspace(line);
		if (*line == '[') {		// another section, quit
			break;
		}
        enumkey->key = line ;
		line = strchr(enumkey->key.getstring(), '#');	// comments
		if (line) {
			*line = '\0';
		}
		line = strchr(enumkey->key.getstring(), ';');	// comments
		if (line) {
			*line = '\0';
		}
        line = strchr(enumkey->key.getstring(), '=');	// key=value
		if (line){
			*line = '\0';
            enumkey->line = i+1 ;
            return str_trim(enumkey->key.getstring()) ;
        }
	}
	return NULL;
}


int config::getvalueint(char *section, char *key)
{
	string pv ;
	int v = 0;
	pv = getvalue(section, key);
	if( pv.length()==0 ) {
		return 0 ;
	}
	if (sscanf(pv.getstring(), "%d", &v)) {
		return v;
	} else {
		if( strcmp(pv.getstring(), "yes")==0 ||
			strcmp(pv.getstring(), "YES")==0 ||
			strcmp(pv.getstring(), "on")==0 ||
			strcmp(pv.getstring(), "ON")==0 ) {
			return 1 ;
		}
		else {
			return 0 ;
		}
	}
}

void config::setvalue(char *section, char *key, char *value)
{
    int sectionindex ;
	int keyindex;
	string sectstr;
	string keystr;

    keystr.setbufsize(strlen(key) + strlen(value) + 4) ;
    sprintf(keystr.getstring(), "%s=%s", key, value);

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
        sprintf(sectstr.getstring(), "[%s]", section);
        m_strlist.add(sectstr);							// add new section
        m_strlist.add(keystr);							// add new key
    }
	m_dirty = 1;
}

void config::setvalueint(char *section, char *key, int value)
{
	char cvalue[40] ;
	sprintf(cvalue, "%d", value);
	setvalue(section, key, cvalue);
}

void config::removekey(char *section, char *key)
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
        if( m_strlist.size()>0 &&
            strncmp( m_strlist[0].getstring(), "#DEFCONF", 8 )==0 ) 
        {
            // the default configure file
            savetxtfile(m_configfile.getstring(),m_strlist);
        }
        else {
            FILE *sfile ;
            config def(defconf);
            sfile = fopen(m_configfile.getstring(), "w");
            if( sfile ) {
                int i ;
                string section ;
                int sectionline=-1 ;
                string line ;
                char * key ;
                char * value ;
                char * p ;
                for( i=0; i<m_strlist.size(); i++) {
                    line = str_skipspace( m_strlist[i].getstring() ) ;
                    key = line.getstring();
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
                        if( strcmp( def.getvalue( section.getstring(), key ), value )==0 ) {
                            // skip the value as same on default setttings
                            continue ;
                        }
                    }
                    if( sectionline>=0 ) {
                        fputs(m_strlist[sectionline].getstring(), sfile);
                        fputs("\n", sfile);
                        sectionline=-1;
                    }
                    fputs(m_strlist[i].getstring(), sfile);
                    fputs("\n", sfile);
                }
                fclose(sfile);
            }
        }
        m_dirty = 0;
    }
}
