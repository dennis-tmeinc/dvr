
// patch for eagle34 compiler error, which do not initilize static objects

#include <stdlib.h>

class cppinit {
public:
    int init ;
    cppinit() {
        init=0x5566 ;
    }
};

static cppinit initpatch ;

extern void (*__init_array_start []) (void) ;
extern void (*__init_array_end []) (void) ;

void cppinitpatch()
{
    // if initpatch is been initialized, the compiler is good and we don't do init again.
    if( initpatch.init != 0x5566 ) {
        int i ;
        for( i=0 ; i<(__init_array_end - __init_array_start); i++ ) {
            __init_array_start[i]();
        }
    }
}
