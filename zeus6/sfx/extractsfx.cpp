#include <stdio.h>

int extract( const char * filename , const char * xfile=NULL);

int main(int argc, char * argv[])
{
    if( argc<3 ) {
        printf("Usage extractsfx <sfxfile> <pattern>"  );
        return 1;
    }

    if( extract( argv[1], argv[2] )==0 ) {
        return 1 ;
    }
    return 0;
}
