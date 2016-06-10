#ifndef __IDX_H__
#define __IDX_H__

// recording support
class rec_index {
protected:
	struct rec_index_item {
		int onoff ;
		int onofftime ;
	} * idx_array ;
	array <struct rec_index_item> m_array ;

public:
	rec_index(){};
	~rec_index(){};
	void empty(){
		m_array.empty();
	}
	void addopen(int opentime);
	void addclose(int closetime);
	void savefile( char * filename );
	void readfile( char * filename );
	int getsize() { 
		return m_array.size(); 
	}
	int getidx( int n, int * ponoff, int * ptime ) {
		if( n<0 || n>=m_array.size() )
			return 0;
		*ponoff=m_array[n].onoff ;
		*ptime=m_array[n].onofftime ;
		return 1 ;
	}
};

#endif		// __IDX_H__

