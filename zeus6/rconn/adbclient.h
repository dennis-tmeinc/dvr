
#include "list.h"
#include "rconn.h"

#ifndef __ADB_CONN__
#define __ADB_CONN__

class adb_conn
	:public rconn {
public:
	adb_conn() : rconn() {}
		
protected:
	// connect to remote server, make it extendable
	virtual int connect_server();
};

#endif	// __ADB_CONN__
