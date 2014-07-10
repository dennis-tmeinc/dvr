#include "dvr.h"

void rec_index::addopen(int opentime)
{
    struct rec_index_item idx;
    idx.onoff = 1 ;
    idx.onofftime = opentime ;
    m_array.add(idx);
}

void rec_index::addclose(int closetime)
{
    struct rec_index_item idx;
    idx.onoff = 0;
    idx.onofftime = closetime;
    m_array.add(idx);
}

void rec_index::savefile(char *filename)
{
    FILE *idxfile=NULL;
    struct dvrtime dvrt ;
    struct dvrtime filetime ;
    struct rec_index_item * pidx;
    
    int onoff;
    int i;
    
    idxfile = fopen(filename, "w");
    if (idxfile == NULL) {
        dvr_log("Can not write index file.");
        return;
    }
    
    f264time(filename, &filetime);
    
    for (i = 0; i < m_array.size(); i++) {
        pidx = m_array.at(i);
        if (pidx->onoff == 0){
            onoff = 'C';
        }
        else if (pidx->onoff == 1){
            onoff = 'O';
        }
        else {
            break;
        }
        dvrt = filetime + pidx->onofftime;
        fprintf(idxfile, "%c%04d%02d%02d%02d%02d%02d\n", onoff,
                dvrt.year,
                dvrt.month,
                dvrt.day,
                dvrt.hour,
                dvrt.minute,
                dvrt.second );
    }
    
    fclose(idxfile);
    empty();
}

void rec_index::readfile(char *filename)
{
    FILE *idxfile=NULL;
    char onoff;
    int state ;
    int i;
    int t ;
    struct dvrtime dvrt ;
    struct dvrtime filetime ;
    
    empty();
    idxfile = fopen(filename, "r");
    if (idxfile == NULL) {
        return;
    }
    
    f264time(filename, &filetime);
    dvrt=filetime;
    
    state = 0 ;
    while (!feof(idxfile)) {
        memset( &dvrt, 0, sizeof(dvrt));
        i = fscanf(idxfile, "%c%04d%02d%02d%02d%02d%02d\n", 
                   &onoff,
                   &(dvrt.year),
                   &(dvrt.month),
                   &(dvrt.day),
                   &(dvrt.hour),
                   &(dvrt.minute),
                   &(dvrt.second) );
        if (i < 7)
            break;
        t=(int)(dvrt-filetime) ;
        if ( onoff == 'O'||onoff =='L' ) {
            if( state==0 ) {
                addopen(t);
                state = 1 ;
            }
        }
        else if ( onoff == 'C' || onoff=='U' ) {
            if( state==1 ) {
                addclose(t);
                state=0;
            }
        }
        else
            break;
    }
    
    fclose(idxfile);
}
