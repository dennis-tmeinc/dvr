#include <stdio.h>

int extract( const char * filename );

int main(int argc, char * argv[])
{
    if( argc<2 ) {
        printf("Usage extractsfx <sfxfile>" );
        return 1;
    }

    if( extract( argv[1] )==0 ) {
        return 1 ;
    }
    return 0;
}
