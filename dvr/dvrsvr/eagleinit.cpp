#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "eagle32/davinci_sdk.h"

int main()
{
    struct board_info binfo ;
    int res ;
    res = InitSystem();
    printf("InitSystem:%d", res );
    res = GetBoardInfo(&binfo);
    if( res>=0 ) {
        printf("Board type : %d\n", binfo.board_type );
        printf("Dsp count  : %d\n", binfo.dsp_count );
        printf("Encoder channel : %d\n", binfo.enc_chan );
        printf("Decoder channel : %d\n", binfo.dec_chan );
    }
    while(1) sleep(5);
    return 0;
}
