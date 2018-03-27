#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "eaglehttpd.h"

// convert hex to int
static int hextoint(int c)
{
    if( c>='0' && c<='9' )
        return c-'0' ;
    if( c>='a' && c<='f' )
        return 10+c-'a' ;
    if( c>='A' && c<='F' )
        return 10+c-'A' ;
    return 0;
}

// decode url encoded string
static int decode(const char * in, char * out, int osize, char sep )
{
    int i ;
    int o ;
    for(i=0, o=0; o<osize-1; o++ ) {
        if( in[i]=='+' ) {
            out[o] = ' ' ;
            i++ ;
        }
        else if( in[i]=='%' && in[i+1] && in[i+2] ) {
            out[o] = hextoint( in[i+1] ) * 16 + hextoint( in[i+2] );
            i+=3 ;
        }
        else if( in[i]==sep || in[i]=='\0' ) {
            break ;
        }
        else {
            out[o]=in[i++] ;
        }
    }
    out[o] = 0 ;
    return i;
}

static void parseString( char * query, char * prefix, char sep )
{
	char * qbuf ;
	char * qvar ;
	char * eq ;
	qvar = (char *)malloc( 4096 );
	strcpy( qvar, prefix );
	qbuf = qvar+strlen(prefix) ;
	while( *query ) {
		query = trimhead(query) ;
		query += decode( query, qbuf, 4080,  sep ) ;
		eq = strchr( qbuf, '=' );
		if( eq ) {
			*eq=0;
			http_setenv( qvar, trim(eq+1), 1 ); 
		}
		if( *query == sep )
			query++;
	}
	free( qvar );
}

void parseRequest()
{
	char * query ;
    query = http_getenv("QUERY_STRING");
    if( query ) {
		parseString( query, "VAR_G_" , '&' );
	}
    query = http_getenv("POST_STRING");
    if( query ) {
		parseString( query, "VAR_P_" , '&' );
	}
    query = http_getenv("HTTP_COOKIE");
    if( query ) {
		parseString( query, "VAR_C_" , ';' );
	}
}

char * getRequest( const char * req, const char * type )
{
	const char vartype[]="GPC" ;
	char * v ;
	int i ;
	char rname[260] ;
	if( type==NULL ) type=vartype ;
	for( i=0; i<3; i++ ) {
		if( strchr(type, vartype[i]) ) {
			sprintf( rname, "VAR_%c_%s", vartype[i], req );
			v = http_getenv( rname );
			if( v ) return v ;
		}
	}
	return NULL ;
}
