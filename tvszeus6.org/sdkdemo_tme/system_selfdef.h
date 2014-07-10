#ifndef	__SYSTEM_SELFDEF_H__
#define	__SYSTEM_SELFDEF_H__

extern void *taskWrap ( void *arg  );
extern int taskPriority ( int pri );
extern int startUpTask(int pri, int stackSize, int(* entryPoint)(int, int, int, int, int),
	                                                  int arg1, int arg2, int arg3, int arg4, int arg5);
extern int semMCreate(pthread_mutex_t * mSem);
extern int semMTake(pthread_mutex_t * mSem);
extern int semMGive(pthread_mutex_t *mSem);
extern int semMDestroy(pthread_mutex_t * mSem);
extern int SysInit();
extern int ConnectAdd(int chnl, CONNECTCFG * cnetcfg);
extern int ConnectDelete(int chnl, CONNECTCFG * cnetcfg);
#endif
