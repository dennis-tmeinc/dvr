
#include "list.h"
#include "rconn.h"

#ifndef __ADB_CONN__
#define __ADB_CONN__

class adb_conn
	:public rconn {
protected:
	// connect to remote server, make it extendable
	virtual int connect_server();

public:
	char * m_serialno ;

	adb_conn( char * serialno );
	virtual ~adb_conn();
};

int adb_setfd( struct pollfd * pfd, int max );
void adb_process();
void adb_reset();

#endif	// __ADB_CONN__
