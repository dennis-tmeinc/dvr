
#include "dvr.h"

static char *skipspace(char *line)
{
	while (*line == ' ' || *line == '\t')
		line++;
	return line;
}

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

config::config(char *configfilename)
{
	m_configfile=configfilename;
	readtxtfile ( configfilename, m_strlist );
	m_dirty = 0;
}

// search for section
int config::findsection(char *section)
{
	int index ;
	char *line;
	int l;
	if (section == NULL)
		return 0;
	if (strlen(section) == 0)
		return 0;
	for (index = 0; index < m_strlist.size(); index++) {
		line = m_strlist[index].getstring();
		line = skipspace(line);
		if (*line == '[') {
			line = skipspace(line + 1);
			l=(int)strlen(section) ;
			if( strncmp(section, line, l)==0 ) {
				line=skipspace (line+l) ;
				if( *line==']' ) {
					return index + 1;	// return first line under the section
				}
			}
		}
	}
	return -1;					// not found ;
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
		line = skipspace(line);
		if (*line == '[') {
			line = skipspace(line + 1);
			enumkey->key = line ;
			line = strchr( enumkey->key.getstring(), ']' );
			if (line) {
				*line = '\0';
			}
			trimtail(enumkey->key.getstring());
			enumkey->line = i+1;
			return enumkey->key.getstring();
		}
	}
	return NULL;					// not found ;
}

int config::findkey(int section, char *key)
{
	string fstr;
	char *line;
	int l ;
	int index;
	index = section;
	for (index = section; index < m_strlist.size(); index++) {
		line = m_strlist[index].getstring();
		line = skipspace(line);
		if (*line == '[') {		// another section, quit
			break;
		}
		if (*line == '#' || *line == ';') {	//  comments
			continue;
		}
		l=(int)strlen(key);
		if( strncmp(line, key, l )==0 ) {
			line = skipspace(line+l) ;
			if( *line=='=' ) {
				return index ;
			}
		}
	}
	return -1;
}

string & config::getvalue(char *section, char *key)
{
	int sectionindex;
	int keyindex;
	char *value;
	m_tempstr = "" ;
	sectionindex = findsection(section);
	if (sectionindex < 0) {
		return m_tempstr;
	}
	// section found
	keyindex = findkey(sectionindex, key);
	if (keyindex < 0) {
		return m_tempstr;
	}

	value = m_strlist[keyindex].getstring();
	value = strchr(value, '=');
	if (value == NULL) {
		return m_tempstr;
	}
	value = skipspace(value + 1);
	m_tempstr = value ;
	value = strchr(m_tempstr.getstring(), '#');	// comments
	if (value){
		*value = '\0';
	}
	value = strchr(m_tempstr.getstring(), ';');	// comments
	if (value) {
		*value = '\0';
	}
	trimtail(m_tempstr.getstring());
	return m_tempstr ;
}

char * config::enumkey(char *section, struct config_enum * enumkey )
{
	char *line;
	int i;
	i = findsection(section);	//	section line index
	if (i < 0) {
		return NULL;
	}
	// section found

	if( i<enumkey->line ) {
		i=enumkey->line ;
	}
	for (; i < m_strlist.size(); i++) {
		line = m_strlist[i].getstring();
		line = skipspace(line);
		if (*line == '[') {		// another section, quit
			break;
		}
		if (*line == '#' || *line == ';' || *line == '\0' ) {	//  comments or nul line
			continue;
		}
		enumkey->key = line ;
		line = strchr(enumkey->key.getstring(), '=');	// values 
		if (line){
			*line = '\0';
		}
		line = strchr(enumkey->key.getstring(), '#');	// comments
		if (line) {
			*line = '\0';
		}
		line = strchr(enumkey->key.getstring(), ';');	// comments
		if (line) {
			*line = '\0';
		}
		trimtail(enumkey->key.getstring());
		enumkey->line = i+1 ;
		return enumkey->key.getstring() ;
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
	int sectionindex;
	int keyindex;
	string sectstr;
	string keystr;

	keystr.setbufsize(strlen(key) + strlen(value) + 4) ;
	sprintf(keystr.getstring(), "%s=%s", key, value);

	sectionindex = findsection(section);
	if (sectionindex < 0) {
		// make a new section
		sectstr.setbufsize(strlen(section) + 4);
		sprintf(sectstr.getstring(), "[%s]", section);
		m_strlist.add(sectstr);							// add new section
		m_strlist.add(keystr);							// add new key
	}
	else {
		// section found
		keyindex = findkey(sectionindex, key);
		if (keyindex < 0) {								// no key found
			m_strlist.insert(keystr,sectionindex);
		}
		else {											// key found
			m_strlist[keyindex] = keystr ;
		}
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
	int sectionindex;
	int keyindex;
	string strvalue;
	sectionindex = findsection(section);
	if (sectionindex < 0) {
		return;
	}
	// section found
	keyindex = findkey(sectionindex, key);
	if (keyindex < 0) {
		return;
	}
	m_strlist.remove(keyindex);
	m_dirty=1;
}

void config::save()
{
	if (m_dirty) {
		savetxtfile(m_configfile.getstring(), m_strlist);
	}
	m_dirty = 0;
}
