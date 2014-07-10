#include <unistd.h>
#include <stdio.h>

int extract( const char * filename );

int main(int argc, char * argv[])
{
    char * sfxrun = "./sfxrun" ;
    int res = extract( "/proc/self/exe" ) ;
    if( argc>1 ) {
        sfxrun = argv[1] ;
    }
    execl(sfxrun, sfxrun, NULL );
    return res ;
}
