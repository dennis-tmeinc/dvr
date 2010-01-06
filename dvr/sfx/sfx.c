#include <stdio.h>

char   defaultrun[] = "sfxrun" ;

int extract( char * filename );

int main(int argc, char * argv[])
{
    if( extract( "/proc/self/exe" )==0 ) {
        return 1 ;
    }
    if( argc>1 ) {
        execl( argv[1], argv[1], NULL );
    }
    else {
        execl(defaultrun, defaultrun, NULL );
    }
    return 0 ;
}
