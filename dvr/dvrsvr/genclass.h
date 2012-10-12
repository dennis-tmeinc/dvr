
#ifndef __genclass_h__
#define __genclass_h__

#include <string.h>

// General purpose classes

#define ARRAYSTEPSIZE (50)

// dynamic growable array
/*
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
            while( j>i  && (!( m_array[j] < m_array[hi]))  ) {
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
*/

template <class T>
class array {
    T * * m_array ;
    int m_size ;
    int m_arraysize ;

    // swap items of index a and b
    void swap( int a, int b) {
        T * sw ;
        sw = m_array[a] ;
        m_array[a] = m_array[b] ;
        m_array[b] = sw ;
    }

    void quicksort(int lo, int hi) {
        int i=lo, j=hi-1;
        while(i<=j) {
            while( i<=j && *m_array[i] < *m_array[hi] ) {
                i++ ;
            }
            while( j>i  && (!( *m_array[j] < *m_array[hi]))  ) {
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
    void expand( int newsize ) {
        if( newsize>m_arraysize ) {
            int i;
            m_arraysize=newsize+ARRAYSTEPSIZE ;
            T ** newarray = new T * [m_arraysize] ;
            if( m_array ) {
                for( i=0; i<m_size; i++ ) {
                    newarray[i] = m_array[i] ;
                }
                delete [] m_array ;
            }
            m_array = newarray ;
        }
    }
    public:
        array(){
            m_array=NULL ;
            m_size=0;
            m_arraysize = 0 ;
        }
        array(int initialsize){
            m_array=NULL ;
            m_size=0;
            m_arraysize = 0 ;
            expand( initialsize ) ;
        }
        ~array(){
            empty();
        }
        int size() {
            return m_size;
        }
        void setsize(int newsize) {
            int i;
            expand(newsize);
            if( newsize>m_size ) {
                for( i=m_size; i<newsize; i++ ) {
                    m_array[i]=new T ;
                }
            }
            else {
                for( i=newsize; i<m_size; i++ ) {
                    delete m_array[i] ;
                }
            }
            m_size=newsize ;
        }
//        T * at(int index) {
//            if( index>=m_size ) {
//                setsize(index+1);
//            }
//            return m_array[index] ;
//        }
        T & operator [] (int index) {
            if( index>=m_size ) {
                setsize(index+1);
            }
            return *m_array[index] ;
        }
        void add( T & t ) {
            expand(m_size+1);
            m_array[m_size] = new T ;
            *m_array[m_size]=t ;
            m_size++;
        }
        void insert( T & t, int pos ) {
            if( pos>= m_size ) {
                setsize(pos+1);
            }
            else {
                expand(m_size+1);
                for( int i=m_size; i>pos; i-- ) {
                    m_array[i] = m_array[i-1] ;
                }
                m_array[pos]=new T ;
                m_size++;
            }
            *m_array[pos]=t ;
        }
        void remove(int pos) {
            if( pos<m_size ) {
                delete m_array[pos] ;
                m_size-- ;
                for( int i=pos; i<m_size; i++ ) {
                    m_array[i]=m_array[i+1] ;
                }
            }
        }
        void remove(T * pos) {
            int i;
            for( i=0; i<m_size; i++ ) {
                if( m_array[i]==pos ) {
                    remove( i );
                    break;
                }
            }
        }
        void empty(){
            setsize(0);
            if( m_array ) {
                delete [] m_array ;
                m_array = NULL ;
            }
            m_arraysize = 0 ;
        }
        void compact(){
        }
        void sort(){
            if( m_size>1 ) {
                quicksort(0,m_size-1);
            }
        }
        array <T> & operator = ( array <T> & a ) {
            int i;
            setsize( a.size() );
            for( i=0; i<m_size; i++) {
                *m_array[i] = a[i] ;
            }
            return *this;
        }
};

#define EMPTY_STRING ("")

// string and string list
// string functions
class string {
protected:
    char *m_str;
    void setstring(const char *str){
        if( m_str ) {
            delete [] m_str ;
            m_str = NULL ;
        }
        if( str ) {
            m_str=new char [strlen(str)+1];
            strcpy(m_str, str);
        }
    }
    char *getstring(){
        if( m_str==NULL ) {
            setstring(EMPTY_STRING);
        }
        return m_str;
    }
public:
    string(){
        m_str=NULL;
    }
    string(const char *str) {
        m_str=NULL;
        setstring(str);
    }
    string(string & str) {
        m_str=NULL;
        setstring(str.getstring());
    }
    ~string() {
        if( m_str ) {
            delete [] m_str ;
        }
    }

    operator char * (){
        return getstring() ;
    }

    string & operator =(const char *str) {
        setstring(str);
        return *this;
    }

    string & operator =( string & str ) {
        setstring((char *)str);
        return *this;
    }

    int length(){
        if( m_str ) {
            return strlen(m_str);
        }
        else {
            return 0 ;
        }
    }
    // make buffer not less then given size
    char * setbufsize(int nsize){
        if( length()<nsize ) {
            char * nbuf = new char [nsize+1] ;
            strcpy( nbuf, getstring() );
            if( m_str ) {
                delete [] m_str ;
            }
            m_str = nbuf ;
        }
        return getstring() ;
    }

    char * setsize(int nsize){
        if( length()<nsize ) {
            char * nbuf = new char [nsize+1] ;
            if( m_str ) {
                strcpy( nbuf, m_str ) ;
                delete m_str ;
            }
            else {
                *nbuf=0 ;
            }
            m_str = nbuf ;
        }
        return getstring() ;
    }

    int isempty() {
        if( m_str ) {
            return (m_str[0] == 0);
        }
        else {
            return 1;
        }
    }
};

inline int operator < ( string & s1, string & s2 ) {
    return ( strcmp(s1, s2)<0 );
}

char * str_trimtail(char *line);
char * str_skipspace(char *line);
char * str_trim(char * line );
int savetxtfile(const char *filename, array <string> & strlist );
int readtxtfile(const char *filename, array <string> & strlist);

#endif		// __genclass_h__
