
#ifndef __genclass_h__
#define __genclass_h__

#include <string.h>

// General purpose classes

#define ARRAYSTEPSIZE (50)

// dynamic growable array
template <class T> 
class array {
    T * m_array ;
    int m_size ;
    int m_arraysize ;
    
    // swap items of index a and b
    void swap( int a, int b) {
        m_array[m_size] = m_array[a] ;
        m_array[a] = m_array[b] ;
        m_array[b] = m_array[m_size] ;
    }
    
    void quicksort(int lo, int hi) {
        int i=lo, j=hi-1;
        while(i<=j) {
            while( i<=j && m_array[i] < m_array[hi] ) {
                i++ ;
            }
            while( j>i  && (!( m_array[j]< m_array[hi]))  ) {
                j-- ;
            }
            if( i<j ) {
                swap(i, j);
                i++;
            }
            j-- ;
        }
        if( i<hi ) {
            swap(i,hi);
        }
        if( lo<i-1 ) {
            quicksort( lo, i-1 );
        }
        if( hi>i+1 ) {
            quicksort( i+1, hi );
        }		
    }
    public:
        array(int initialsize=0){
            m_size=0;
            if( initialsize>0 ) {
                m_arraysize=initialsize ;
                m_array=new T [m_arraysize] ;
            }
            else {
                m_arraysize=0;
                m_array=NULL;
            }
        }
        ~array(){
            if(m_arraysize>0) {
                delete [] m_array ;
            }
        }
        int size() {
            return m_size;
        }
        void expand(int newsize){
            if( newsize>m_arraysize ) {
                if( newsize<50) {
                    newsize = ARRAYSTEPSIZE ;
                }
                else if( newsize<m_arraysize*2 ) {
                    newsize=m_arraysize*2 ;
                }
                T * newarray = new T [newsize] ;
                if( m_arraysize > 0 ) {
                    for( int i=0; i<m_size; i++ ) {
                        newarray[i] = m_array[i] ;
                    }
                    delete [] m_array ;
                }
                m_arraysize = newsize ;
                m_array = newarray ;
            }
        }
        void setsize(int newsize) {
            expand(newsize);
            m_size=newsize ;
        }
        T * at(int index) {
            if( index>=m_size ) {
                expand( index+1 );
                m_size=index+1 ;
            }
            return & m_array[index] ;
        }
        T & operator [] (int index) {
            if( index>=m_size ) {
                expand( index+1 );
                m_size=index+1 ;
            }
            return m_array[index] ;
        }
        void add( T & t ) {
            expand(m_size+1);
            m_array[m_size++]=t ;
        }
        void insert( T & t, int pos ) {
            if( pos>= m_size ) {
                expand(pos+1);
                m_size=pos+1;
                m_array[pos]=t ;
            }
            else {
                expand(m_size+1);
                for( int i=m_size; i>pos; i-- ) {
                    m_array[i] = m_array[i-1] ;
                }
                m_array[pos]=t ;
                m_size++;
            }
        }
        void remove(int pos) {
            if( pos<m_size ) {
                m_size--;
                for( int i=pos; i<m_size; i++ ) {
                    m_array[i]=m_array[i+1] ;
                }
            }
        }
        void remove(T * pos) {
            int i;
            for( i=0; i<m_size; i++ ) {
                if( ( &m_array[i] )==pos ) {
                    remove( i );
                    break;
                }
            }
        }
        void empty(){
            if( m_arraysize>0 ) {
                delete [] m_array ;
                m_array=NULL;
            }
            m_arraysize=0;
            m_size=0;
        }
        void compact(){
            if( m_size<m_arraysize ) {
                if( m_size==0 ) {
                    delete [] m_array;
                    m_array=NULL;
                    m_arraysize = 0 ;
                }
                else {
                    T * newarray = new T [m_size];
                    for( int i=0; i<m_size; i++) {
                        newarray[i]=m_array[i] ;
                    }
                    delete [] m_array;
                    m_array = newarray ;
                    m_arraysize = m_size ;
                }
            }
        }
        void sort(){
            expand(m_size+1);
            if( m_size>1 ) {
                quicksort(0,m_size-1);
            }
        }
        array <T> & operator = ( array <T> & a ) {
            int i;
            setsize( a.size() );
            for( i=0; i<m_size; i++) {
                m_array[i] = a[i] ;
            }
            return *this;
        }
};

// duel link list
template <class T>
class list {
    struct item {
        item * prev ;
        item * next ;
        T data ;
    } ;
    item * m_head ;
    item * m_tail ;
    item * m_iter ;			// iterator
    T * getdata() {
        if( m_iter ) {
            return &(m_iter->data) ;
        }
        else {
            return NULL;
        }
    }
    public:
        list() {
            m_head=NULL;
            m_tail=NULL;
            m_iter=NULL;	
        }
        ~list() {
            while(m_head) {
                m_iter=m_head;
                m_head=m_head->next ;
                delete m_iter ;
            }
        }
        T * head() {
            m_iter=m_head ;
            return getdata();
        }
        T * tail() {
            m_iter=m_tail ;
            return getdata();
        }
        T * next() {
            if( m_iter ) {
                m_iter=m_iter->next ;
            }
            return getdata();
        }
        T * prev() {
            if( m_iter ) {
                m_iter=m_iter->prev ;
            }
            return getdata();
        }
        int size() {
            int count=0;
            item * pit = m_head ;
            while( pit ) {
                count++;
                pit=pit->next ;
            }
            return count ;
        }
        T * find(T & t) {				// find a item equal to t, return NULL failed
            T * f ;
            f=head();
            while( f != NULL ) {
                if( t == *f ) {
                    return f;
                }
                f=next();
            }
            return NULL;
        }
        T * inserthead(T & t){
            item * nitem = new item ;
            nitem->data = t ;
            nitem->next = m_head ;
            nitem->prev = NULL ;
            if( m_head ) {
                m_head->prev = nitem ;
                m_head = nitem ;
            }
            else {
                m_head=m_tail=nitem;
                
            }
            return head();
        }
        T * inserttail(T & t){
            item * nitem = new item ;
            nitem->data = t;
            nitem->next = NULL;
            nitem->prev = m_tail ;
            if( m_tail ) {
                m_tail->next = nitem;
                m_tail=nitem ;
            }
            else {
                m_head=m_tail=nitem;
            }
            return tail();	
        }
        int removehead() {
            item * t ;
            if( m_head ) {
                t=m_head->next ;
                delete m_head ;
                m_head=t ;
                if( m_head ) {
                    m_head->prev=NULL ;
                }
                else {
                    m_tail=NULL ;
                }
                return 1;
            }
            return 0;
        }
        int removetail() {
            item * t ;
            if( m_tail ) {
                t=m_tail->prev ;
                delete m_tail;
                m_tail=t ;
                if( m_tail ) {
                    m_tail->next=NULL ;
                }
                else {
                    m_head=NULL;
                }
                return 1;
            }
            return 0;
        }
        int removecurrent() {		// remove current item pointer by m_iter
            if( m_iter==NULL ) {
                return 0 ;
            }
            else if( m_iter == m_head ) {
                m_iter=NULL ;
                return removehead();
            }
            else if( m_iter==m_tail ) {
                m_iter=NULL ;
                return removetail();
            }
            else {
                m_iter->prev->next = m_iter->next ;
                m_iter->next->prev = m_iter->prev ;
                delete m_iter ;
                m_iter=NULL;
                return 1;
            }
        }
        void clean() {          // clean all items
            while(m_head) {
                m_iter=m_head;
                m_head=m_head->next ;
                delete m_iter ;
            }
            m_tail=NULL;
            m_iter=NULL;
        }
};


// string and string list
// string functions

/*
class string {
    protected:
        char *m_str;
        int   m_s ;
		        
        void setstring(const char *str){
			char * stmp = m_str ;
			if( str ) {
				m_s = strlen( str ) + 1 ;
				m_str = new char [m_s] ;
				strcpy( m_str, str );
			}
			else {
				m_s = 0 ;
				m_str = NULL ;
			}
			if( stmp ) 
				delete stmp ;
        }		
    public:
    
        string(const char *str = NULL) {
			m_str = NULL ;
			m_s = 0 ;
			setstring( str );
        }
        
        ~string() {
			if( m_str )
				delete m_str ;
        }
        
        char * getstring(){
			if( m_str ) {
				return m_str;
			}
			else {
				m_s = 2 ;
				m_str = new char [m_s] ;
				*m_str = 0 ;
				return m_str ;
			}
        }

		operator char * (){
			return getstring();
		}
		
        string & operator = (const char *str) {
			setstring( str );
            return *this;
        }
        
        int operator < ( string & s2 ) {
            return ( strcmp( getstring(), s2.getstring())<0 );
        }
        
        int length(){
            return strlen(m_str);
        }
        
        int size() {		// buffer size
			return m_s ;
		}
		
		// expand buffer size. (no shrinking)
		char * expand(int nsize){
			if( m_s < nsize ) {
				char * nbuf = new char [nsize] ;
				if( m_str ) {
					memcpy( nbuf, m_str, m_s ) ;
					delete m_str ;
				}
				m_str = nbuf ;
				m_s = nsize ;
			}
            return m_str ;
        }
        
        char * setbufsize(int nsize){
			return expand( nsize+1 );
        }
        
        string & trim()
        {
			int l = strlen( m_str );
			while( l>0 && m_str[l-1]<=' ' && m_str[l-1]>0 ) {
				m_str[l-1] = 0 ;
				l-- ;
			}
			l = 0 ;
			while( l<m_s-1 && m_str[l]<=' ' && m_str[l]>0 ) {
				l++;
			}
			setstring( &m_str[l] );
			return *this ;
		}
        
        int isempty() {
            return (m_str[0] == 0);
        }
        char & operator[] (int idx) {
            return m_str[idx] ;
        }
};
*/

// string and string list
// string functions
class string {
    protected:
        char *m_str ;
        int   m_s ;
        void setstring(const char *str){
			char * t_str = m_str ;
            m_str=NULL ;			
			m_s = 0 ;
            if( str && *str != 0 ) {
                m_s = strlen(str)+1;
                m_str=new char [m_s];
                strcpy(m_str, str);
            }
            if( t_str!=NULL ) {
                delete t_str ;
            }
        }
    public:
        string( const char * str = NULL ) {
			m_s = 0 ;
            m_str=NULL;
            if( str ) 
				setstring( str );
        } 

        string(string & str) {
			m_s = 0 ;
            m_str=NULL;
            if( str.length() )
				setstring((char *)str);
        }

        ~string() {
            if( m_str ) {
                delete m_str ;
            }
        }

		char *getstring(){
			if (m_str == NULL) {
				m_s = 4;
				m_str=new char [m_s] ;
				m_str[0]='\0' ;
			}
			return m_str;
		}

		operator char * (){
			return getstring() ;
		}

        string & operator =(const char *str) {
            setstring(str);
            return *this;
        }
        
        string & operator =(string & str) {
			setstring( (char *)str );
            return *this;
        }
        
        int operator < ( string & s2 ) {
            return ( strcmp(getstring(), s2.getstring())<0 );
        }
        
        int length(){
            if( m_str ) {
                return strlen(m_str);
            }
            else {
                return 0;
            }
        }
        int size() {
			return m_s ;
		}
        char * expand(int nsize){
			if( nsize>m_s ) {
				char * nbuf = new char [nsize] ;
				*nbuf = 0 ;
				if( m_str ) {
					memcpy( nbuf, m_str, m_s );
					delete m_str ;
				}
				m_str = nbuf ;
				m_s = nsize ;
			}
			return getstring();
		}
		char * setbufsize( int nsize ) {
			return expand(nsize+1);
		}
		string & trim()
        {
			if( m_str ) {
				int l = strlen( m_str );
				while( l>0 && m_str[l-1]<=' ' && m_str[l-1]>0 ) {
					m_str[l-1] = 0 ;
					l-- ;
				}
				l = 0 ;
				while( l<m_s-1 && m_str[l]<=' ' && m_str[l]>0 ) {
					l++;
				}
				setstring( &m_str[l] );
			}
			return *this ;
		}
        int isempty() {
            return (length() == 0);
        }
        char & operator[] (int idx) {
            return m_str[idx] ;
        }
};

int savetxtfile(char *filename, array <string> & strlist );
int readtxtfile(char *filename, array <string> & strlist);

#endif		// __genclass_h__
