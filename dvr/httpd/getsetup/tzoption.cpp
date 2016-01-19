

#include <stdio.h>

#include "../../cfg.h"
#include "../../dvrsvr/crypt.h"
#include "../../dvrsvr/genclass.h"
#include "../../dvrsvr/config.h"

int main()
{
    char * zoneinfobuf ;
    string s ;
    char * p ;
    config_enum enumkey ;
    config dvrconfig(CFG_FILE);
    string tzi ;
    string value ;

	// initialize enumkey
	enumkey.line=0 ;
	while( (p=dvrconfig.enumkey("timezones", &enumkey))!=NULL ) {
		tzi=dvrconfig.getvalue("timezones", p );
		printf("<option value=\"%s\">%s ", p, p );
		if( tzi.length()>0 ) {
			zoneinfobuf=tzi;
			while( *zoneinfobuf != ' ' &&
				  *zoneinfobuf != '\t' &&
				  *zoneinfobuf != 0 ) {
					  zoneinfobuf++ ;
				  }
			if( strlen(zoneinfobuf) > 1 ) {
				printf("-%s", zoneinfobuf );
			}
		}
		printf("</option>\n");
	}

    return 0;
}
