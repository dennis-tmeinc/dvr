
#include "dvr.h"

static char *skipspace(char *line)
{
	while ( *line > 0 && *line <= ' ' )
		line++;
	return line;
}

config::config(char *configfilename)
{
	m_configfile=configfilename;
	load();
}

// search for section
char * config::enumsection(int * index)
{
	char *line;
	int i = *index ;
	if( i<0 ) {
		return NULL;
	}
	for ( ; i < m_strlist.size(); i++) {
		line = skipspace(m_strlist[i]);
		if(*line == '[') {
			m_tempstr = skipspace(line + 1) ;
			line = strchr( (char *)m_tempstr, ']' );
			if (line) {
				*line = '\0';
			}
			m_tempstr.trimtail();
			*index = i+1 ;
			return (char *)m_tempstr;
		}
	}
	*index = -1;
	return NULL;					// not found ;
}

// search for section
char * config::enumsection(struct config_enum * e)
{
	char * r = enumsection( &(e->line) );
	if( r!=NULL ) {
		e->key = r ;
		return (char *)(e->key);
	}
	return NULL ;
}

char * config::enumkey( int * index )
{
	char *line;
	int i = *index;
	if( i<0 ) {
		return NULL;
	}
	for(; i < m_strlist.size(); i++) {
		line = skipspace(m_strlist[i]);
		if (*line == '[') {			// another section, quit
			break;
		}
		if (*line == '#' || *line == ';' || *line == '\0' ) {	//  comments or nul line
			continue;
		}
		m_tempstr = line ;
		line = strchr( (char *)m_tempstr, '=');
		if(line){
			*line = '\0';
			m_tempstr.trimtail();
			*index = i+1 ;
			return (char *)m_tempstr ;
		}
	}
	*index = -1;
	return NULL;
}

char * config::enumkey(char *section, struct config_enum * e )
{
	if( e->line<=0 ) {
		e->line = findsection( section ) ;
	}
	char * k = enumkey(&(e->line));
	if( k!=NULL ) {
		e->key = k ;
		return (char *)(e->key);
	}
	return NULL ;
}

// search for section
int config::findsection(char *section)
{
	int index ;
	char * key ;
	int l;
	if (section == NULL || *section == 0 ) {
		return 0;
	}
	string tsection (section) ;
	index = 0 ;
	while( (key = enumsection( &index ))!= NULL ) {
		if( strcmp( key, (char *)tsection )==0 ) {
			return index ;
		}
	}
	return -1 ;
}

/*		
int config::findsection(char *section)
{
    char * line ;
	int index ;
	int l;
	for (index = 0; index < m_strlist.size(); index++) {
		line = skipspace((char *)m_strlist[index]) ;
		if (*line == '[') {
			line = skipspace(line + 1);
			l=(int)strlen(section) ;
			if( strncmp(section, line, l)==0 ) {
				line=skipspace (line+l) ;
				if( *line==']' ) {
					return index + 1;	// return first line after the section
				}
			}
		}
	}
	return -1;					// not found ;
}


int config::findkey(int section, char *key, string * value )
{
	string fstr;
	char *line;
	int l ;
	if( section>=0 ) {
		for (; section < m_strlist.size(); section++) {
			line = skipspace((char *)m_strlist[section]);
			if (*line == '[') {						// another section, quit
				break;
			}
			if (*line == '#' || *line == ';' || *line==0 ) {	//  comments
				continue;
			}
			l=(int)strlen(key);
			if( strncmp(line, key, l )==0 ) {
				line = skipspace(line+l) ;
				if( *line=='=' ) {
					// found the key
					if( value != NULL ) {
						*value = skipspace(line+1) ;
						line = strchr( *value, '#' );
						if( line ) *line=0;
						line = strchr( *value, ';' );
						if( line ) *line=0;
						value->trimtail();
					}
					return section ;
				}
			}
		}
	}
	return -1;
}

*/

// return line index of found key
int config::findkey(int index, char *key )
{
	char * k ;
	string tkey( key );
	while( (k=enumkey(&index))!=NULL ) {
		if( strcmp( k, (char *)tkey )==0 ) {				// found key
			return index-1 ;
		}
	}
	return -1 ;
}

string & config::getvalue(char *section, char *key)
{
	int index;
	index = findkey(findsection(section), key );
	if( index>=0 ) {
		char * v = strchr( (char *)(m_strlist[index]), '=');
		if( v != NULL ) {
			m_tempstr = skipspace(v+1) ;
			// remove comments
			v = strchr( (char *)m_tempstr, '#' );
			if( v ) *v=0;
			v = strchr( (char *)m_tempstr, ';' );
			if( v ) *v=0;
			m_tempstr.trimtail();
			return m_tempstr ;
		}
	}
	m_tempstr="";
	return m_tempstr;
}

int config::getvalueint(char *section, char *key)
{
	int v=0 ;
	char * pv ;
	pv = (char *)getvalue(section, key);
	if(sscanf(pv, "%d", &v)) {
		return v;
	} 
	else {
		if( strcasecmp(pv, "yes")==0 ||
			strcasecmp(pv, "on")==0 )
		{
			return 1 ;
		}
	}
	return 0 ;
}

void config::setvalue(char *section, char *key, char *value)
{
	int sectionindex;
	int keyindex;
	string kv ;		// key value string
	string tsection (section) ;
	string tkey( key ) ;
	string tvalue(value) ;

	sectionindex = findsection(tsection);
	if (sectionindex < 0) {		// no such section
		// add a new section
		sprintf( kv.setbufsize(strlen(tsection) + 4), "[%s]", section);
		m_strlist.add(kv);							// add new section
		// add key=value
		sprintf( kv.setbufsize(strlen(tkey) + strlen(tvalue) + 4), "%s=%s", key, value);
		m_strlist.add(kv);							// add new section
		m_dirty = 1;
	}
	else {
		// section found
		keyindex = findkey(sectionindex, tkey);
		if (keyindex < 0) {								// no key found
			sprintf( kv.setbufsize(strlen(tkey) + strlen(tvalue) + 4), "%s=%s", key, value);
			m_strlist.insert( kv, sectionindex );
			m_dirty = 1;
		}
		else {
			char * ov = (char *)getvalue(tsection, tkey);
			if( strcmp( ov, tvalue )!=0 ) {
				sprintf( kv.setbufsize(strlen(tkey) + strlen(tvalue) + 4), "%s=%s", key, value);
				m_strlist[keyindex] = kv ;
				m_dirty = 1 ;
			}
		}
	}
}

void config::setvalueint(char *section, char *key, int value)
{
	char cvalue[40] ;
	sprintf(cvalue, "%d", value);
	setvalue(section, key, cvalue);
}

void config::removekey(char *section, char *key)
{
	int index = findkey(findsection(section), key);
	if( index>=0 ) {
		m_strlist.remove(index);
		m_dirty = 1 ;
	}
}

void config::save()
{
	int i;
	FILE *sfile ;
	if( m_dirty ) {
		sfile = fopen((char *)m_configfile, "w");
		if (sfile) {
			for (i = 0; i < m_strlist.size(); i++) {
				fputs((char *)(m_strlist[i]), sfile);
				fputs("\n", sfile);
			}
			fclose(sfile);
		}
		m_dirty = 0;
	}
}

void config::load()
{
	FILE *rfile;
	rfile = fopen(m_configfile, "r");
	if( rfile ) {
		char * line = new char [8192] ;
		while (fgets(line, 8192, rfile)) {
			string str ;
			str=line ;
			str.trimtail();
			m_strlist.add(str);
		}
		delete line ;
		fclose( rfile );
	}
	m_dirty = 0 ;
}
