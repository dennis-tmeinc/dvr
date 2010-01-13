#include <unistd.h>
#include <stdio.h>

char   defaultrun[] = "./sfxrun" ;

int extract( char * filename );

int main(int argc, char * argv[])
{
    int res = extract( "/proc/self/exe" ) ;
    if( argc>1 ) {
        execl( argv[1], argv[1], NULL );
    }
    else {
        execl(defaultrun, defaultrun, NULL );
    }
    return res ;
}
