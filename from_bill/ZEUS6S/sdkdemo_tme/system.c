#include "include_all.h"

typedef struct
{
	int(*entryPoint)( int, int , int, int, int );
	int arg[5];
}taskStruct;

//static taskStruct taskTcb;
static int g_wdTimer = 5;
pthread_mutex_t globalMSem = PTHREAD_MUTEX_INITIALIZER;

void *taskWrap ( void *arg  )
{//printf("in taskwrap**********************\n");
	taskStruct 	taskTcb;
	taskTcb = *(( taskStruct * )arg);
	free ( arg );
	semMGive ( &globalMSem );
	printf ( "######Give sem.\n" );
//	(*(taskTcb.entryPoint))(taskTcb.arg[0],taskTcb.arg[1],taskTcb.arg[2],taskTcb.arg[3],taskTcb.arg[4]);
	(*(taskTcb.entryPoint))(taskTcb.arg[0],taskTcb.arg[1],taskTcb.arg[2],taskTcb.arg[3],taskTcb.arg[4]);

	return ((void *)NULL);
}

int taskPriority ( int pri )
{//printf("in taskpriority*************\n");
	int min_pri, max_pri;
	if ( pri > MAX_TASK_PRIORITY || pri < 0 ) pri = DEFAULT_TASK_PRI;
	min_pri = sched_get_priority_min ( SCHED_FIFO );
	max_pri = sched_get_priority_max ( SCHED_FIFO );
	pri = MAX_TASK_PRIORITY - pri;
	pri %= ( max_pri - 1 );
	if ( pri < min_pri ) pri = min_pri;
	//printf("exit taskpriority*********************%d\n",pri);
	return pri;
}

int startUpTask ( /*pthread_t *pid, */int pri, int stackSize, int ( *entryPoint )( int, int, int, int, int ),
						int arg1, int arg2, int arg3, int arg4, int arg5 )
{
	pthread_t pid;
	int ret;
	pthread_attr_t		thread_attr;
	struct sched_param thread_pri;
	taskStruct *taskTcb;

	semMTake ( &globalMSem );
	printf ( "#####Take sem.\n" );
	pthread_attr_init ( &thread_attr );

	if ( stackSize < MIN_STACK_SIZE ) stackSize = MIN_STACK_SIZE;
	pthread_attr_setstacksize ( &thread_attr, stackSize );

	pthread_attr_getschedparam ( &thread_attr, &thread_pri );

	pthread_attr_setschedpolicy ( &thread_attr, SCHED_RR );

	thread_pri.sched_priority = taskPriority ( pri );
	pthread_attr_setschedparam ( &thread_attr, &thread_pri );

	taskTcb = malloc ( sizeof ( taskStruct ) );
	if ( taskTcb == NULL ) return ERROR;
	//else printf("malloc tasktcb*********************\n");
	bzero ( taskTcb, sizeof ( taskStruct ) );
	taskTcb->entryPoint = entryPoint;
	taskTcb->arg[0] = arg1;
	taskTcb->arg[1] = arg2;
	taskTcb->arg[2] = arg3;
	taskTcb->arg[3] = arg4;
	taskTcb->arg[4] = arg5;


	ret = pthread_create( &pid,& thread_attr, taskWrap, (void *)taskTcb );
	if ( ret != 0 )return ERROR;
	//else printf("pthread_create successful*********************\n");
	return OK;
}

int semMCreate ( pthread_mutex_t *mSem )
{
	return pthread_mutex_init( mSem, (pthread_mutexattr_t *)NULL );
}

int semMTake ( pthread_mutex_t *mSem )
{
	return pthread_mutex_lock( mSem );
}
int semMGive ( pthread_mutex_t *mSem )
{
	return pthread_mutex_unlock( mSem );
}

int semMDestroy ( pthread_mutex_t *mSem )
{
	return pthread_mutex_destroy ( mSem );
}

int SysInit ()
{
//	init_system ();	//api:initial system,include gpio,dsp,voice channel.

	//initial serial parameter.

	//initial mutex sem.
	semMCreate ( &globalMSem );
	printf ( "globalMSem created.\n" );
	return OK;
}


/***********************************************************************************************
Fountion:    ConnectAdd()
Input:         
Output:
Description: add a connect to the list(when a client to connect for preview)
***********************************************************************************************/
int ConnectAdd(int chnl,CONNECTCFG *cnetcfg)
{
       CHNLPARA *pChan=&chnlPara[chnl-1];
	CONNECTCFG *pslot,*oldpslot;
	 UINT16 count;
	 if(cnetcfg->bMaster)
	 {
	        if(pChan->linknums==0)
	        {
	               pChan->NetConnect=cnetcfg;
			 cnetcfg->next=NULL;
			 pChan->linknums++;
			 return OK;
	        }
		 else
		 {
		 	  count = pChan->linknums;
			  pslot=pChan->NetConnect;
		 }

	 }
	 else
	 {
	         if(pChan->sublinknums==0)
	         {
	         	  pChan->SubNetConnect=cnetcfg;
			  cnetcfg->next=NULL;
			  pChan->sublinknums++;
			  return OK;
	         }
		  else
		  {
		         count = pChan->sublinknums;
	                pslot = pChan->SubNetConnect;
		  }
	         

	 }
	 oldpslot = pslot;
	 while(pslot)
	 {
	          oldpslot=pslot;
	          pslot = pslot->next;
	  }
	 oldpslot->next=cnetcfg;
	 cnetcfg->next = NULL;
	 if(cnetcfg->bMaster)
	 {
	 	 pChan->linknums++;
	 }
	 else
	 {
	 	pChan->sublinknums++;
	 }
	 return OK;
}


/*******************************************************************************************************
Function:  ConnectDelete()
Input:
Output:   
Description: when a client priview stop ,delete the connect

*******************************************************************************************************/
int ConnectDelete(int chnl,CONNECTCFG *cnetcfg)
{
        CHNLPARA *pChan = &chnlPara[chnl-1];
	 CONNECTCFG *pslot,*oldpslot;
	 int streamfd;
	 printf("linknums=%d\nsublinknums=%d\n",pChan->linknums,pChan->sublinknums);
	 if(cnetcfg->bMaster)
	 {
	        streamfd=cnetcfg->streamfd;
		 if((pChan->linknums==1)&&(pChan->NetConnect->streamfd==streamfd))
		 {
		        pChan->NetConnect->streamfd=-1;
			 pChan->NetConnect->bMaster=-1;
			 pChan->NetConnect->next=NULL;
		        pChan->NetConnect=NULL;
			 pChan->linknums=0;
			 TotalLinks--;
			 return OK;
		 }
		 else
		 {
		 	pslot = pChan->NetConnect;
			pChan->linknums--;
			TotalLinks--;
		 	if(pslot==NULL)
	              {
	 	              printf("no node\n");
		              return ERROR;
	               }
		}
		 
	 }
	 else
	 {
	        streamfd = cnetcfg->streamfd;
	        if((pChan->sublinknums==1)&&(pChan->SubNetConnect->streamfd==streamfd))
	        {
	        	pChan->SubNetConnect->streamfd=-1;
			pChan->SubNetConnect->bMaster=-1;
			pChan->SubNetConnect->next=NULL;
			pChan->SubNetConnect=NULL;
			pChan->sublinknums=0;
			TotalLinks--;
			return OK;
	        }
		else
		{
			pslot = pChan->SubNetConnect;
			pChan->sublinknums--;
			TotalLinks--;
			if(pslot==NULL)
	              {
	 	              printf("no node\n");
		              return ERROR;
	               }
		}
	        
	 }
	 oldpslot = pslot;

	 while((streamfd != pslot->streamfd)&&(pslot->next!=NULL))
	 {
	        oldpslot = pslot;
	        pslot=pslot->next;
	 }
	
	 /*fount and the delete is the last one*/
	 if((pslot->next==NULL)&&(streamfd==pslot->streamfd))
	 {
               oldpslot->next=NULL;
		 pslot->streamfd = -1;
		 pslot->bMaster =-1;
		 pslot->next = NULL;
	 }
	 /*found but not the last one*/
	 else if(streamfd==pslot->streamfd)
	 {
	         oldpslot->next=pslot->next;
		  pslot->streamfd = -1;
		  pslot->bMaster =-1;
		  pslot->next = NULL;
	 }
	 /*didn't find*/
	 else
	 {
	      return ERROR;
	 }

	 return OK;

}

