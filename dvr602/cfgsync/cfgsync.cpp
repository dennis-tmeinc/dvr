
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>

#include "../dvrsvr/genclass.h"
#include "../dvrsvr/cfg.h"

char dvrconfigfile[] = "/davinci/dvr/dvrsvr.conf" ;
char defaultconfigfile[] = "/davinci/dvr/default.conf" ;

int main( int argc, char * argv[] )
{
    string filename, defaultfilename ;
    string value ;
    string str ;
    struct config_enum ce_section, ce_key;
    char *s, *k;
    
    filename=dvrconfigfile ;
    defaultfilename = defaultconfigfile;
   
    config dvrconfig(filename.getstring());
    config defaultconfig(defaultfilename.getstring());

    ce_section.line = 0;
    do {
      s = defaultconfig.enumsection(&ce_section);
      if (s) {
	fprintf(stderr, "section:%s\n", s);
	
	ce_key.line = ce_section.line;
	do {
	  k = defaultconfig.enumkey(s, &ce_key);
	  if (k) {
	    fprintf(stderr, "  key:%s\n", k);
	    str = dvrconfig.getvalue(s, k);
	    if (str.length() > 0) {
	      fprintf(stderr, "  --value:%s\n", str.getstring());
	    } else {
	      fprintf(stderr, "  no match found\n");
	      value = defaultconfig.getvalue(s, k);
	      fprintf(stderr, "  seting to:%s\n", value.getstring());
	      dvrconfig.setvalue(s, k, value.getstring());
	      dvrconfig.save();
	    }
	  }
	} while (k);
      }
    } while (s);

    return 0;
}
