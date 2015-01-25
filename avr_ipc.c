#include "avr.h"
 
long 
  shmid
;
int 
  msqid
;
IPC_DICT 
  *ipc_dict
;
void
  shfree()
, ipcSlotClear(int)
, initNotify()
;

// Signal handlers for all ipc processes
void
usrExit(int err, int sig)
{
	ipcLog("Process %d shutdown Exit code=%d Signal=%d\n"
	, getpid()
	, err
	, sig
	);
	ipcLog("Goodbye!\n");
	exit(err); 
}

char *
getProcName(int ptype)
{
	char *name;
	switch(ptype)
	{
	case	P_ROOT		: name="P_ROOT";	break;
	case	P_AVR		: name="P_AVR";		break;
	case	P_SQLITE	: name="P_SQLITE";	break;
	default				: name="???";		break;
	}
	return name;
}

int
getSharedMemory(int ptype, const char *token)
{
int slot, sz;
pid_t pid=getpid();

	ipcLog("getshmem(%d,%d,%s)\n",pid,ptype,token);

	if(allocateSharedMemory(ptype,token)!=0)
	{
		ipcLog("Shalloc Failed ERROR: %s\n",strerror(errno));
		return errno;
	}

	// scan the dictionary for a spare slot
	// loop will break on pid=0
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		char *s=getProcName(ipc_dict[slot].type);
		ipcLog("slot %d TYPE:%s PID:%d\n",slot,s,ipc_dict[slot].pid);
	}

	if(slot==MAX_IPC)
	{
		ipcLog("No Application Dictionary Slots available %d Max\n",MAX_IPC);
		exit(0);
	}

	// set initial values
	ipc_dict[slot].pid=getpid();
	ipc_dict[slot].type=ptype;
	ipc_dict[slot].online=time(0);

	return slot;
}

int
getPidByType(int type)
{
int slot;
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		if(type==ipc_dict[slot].type) return ipc_dict[slot].pid;
	}
}

int
getSlotByType(int type)
{
int slot;
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		if(type==ipc_dict[slot].type) return slot;
	}
}

int
allocateSharedMemory(int ptype, const char *token)
{
char *mem,*shmat();
char name[255];
key_t key;
int sz, flags=0;

	if(ptype==P_ROOT)
	{
		flags=IPC_CREAT|0666;
	}

	// if there is no file create it
	if(access(token,0)) fclose(fopen(token,"w"));

	if((key=ftok(token,1))<0) 
	{
		ipcLog("ftok failed ERROR: %s\n",strerror(errno));
		return errno;
	}

	/* get enough shared memory for 10 IPC Links */
	sz=sizeof(IPC_DICT)*MAX_IPC;

	// ipcLog("shmget(key=%x,bytes=%x,flags=%x) token=%s\n",key,sz,flags,token);

	if((shmid=shmget(key,sz,flags))<0)
	{
		ipcLog("Can't get shared segment size %d for key %d errno:%d\n",sz,key,errno);
		return errno;
	}

	if((mem=shmat(shmid,(char*)0,0))==(char *)-1L)
	{
		if(ptype==P_ROOT)
		{
			shmctl(shmid,IPC_RMID,0);
		}
		ipcLog("Can't attatch to shared segment errno:%d\n",errno);
		exit errno;
	}

	// only avr_init process should do this 
	if(ptype==P_ROOT)
	{
		//ipcLog("Clearing Shared Memory\n");
		memset(mem,0,sz);
	}

	// cast memory as an array of IPC_DICT structures
	ipc_dict=(IPC_DICT *)mem;

	return 0;
}

void
shfree()
{
	if(shmctl(shmid,IPC_RMID,0)!=0)
		ipcLog("shmctl IPC_RMID Failed ERROR: %s\n",strerror(errno));
	else
		ipcLog("PID:%d Removed shmid %d\n",getpid(),shmid);
}

void
vacateSlot(int n)
{
	ipcSlotClear(n);
}

void
ipcSlotClear(int x)
{
	ipcLog("clearing slot %d\n",x);
	ipc_dict[x].pid=0;
	ipc_dict[x].type=0;
}

int 
getMessageQueue(int ptype, char *token)
{
	int flags=0;
	int msqid;
	key_t key;
	pid_t pid=getpid();

	ipcLog("getMessageQueueue(%d,%d,%s)\n",pid,ptype,token);

	// if there is no file create it
	if(access(token,0)) fclose(fopen(token,"w"));

	if((key=ftok(token,1))<0) 
	{
		ipcLog("ftok failed ERROR: %s\n",strerror(errno));
		return errno;
	}

	if(ptype==P_ROOT)
	{
		flags=IPC_CREAT|0666;
	}

	ipcLog("mssget(key=%d,flags=%x) token=%s\n"
	, key
	, flags
	, token
	);

	if((msqid=msgget(key,flags))<0) 
	{
		ipcLog("ftok failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	ipcLog("getMessageQueueue() got msqid=%d\n",msqid);
	return msqid;
}

int
sendMessage(int msqid, long mtype, int cmd, char *txt)
{
	MSG_BUF msg;
	int msgflg;
	
	// mtype should be the pid of the receiver
	msg.rsvp = getpid();
	msg.type = mtype; 
	msg.cmd  = cmd; 
	msg.len  = sizeof(msg)-sizeof(long);
	msgflg   = 0;

	strcpy(msg.text,txt);

	// man: int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);k
	ipcLog("msgsnd(msqid=%d,msg=%x,len=%d,msgflg=%d)\n"
	, msqid
	, msg
	, msg.len
	, msgflg
	);

	if(msgsnd(msqid,&msg,msg.len,msgflg)<0) 
	{
		ipcLog("msgsnd failed ERROR: %s\n",strerror(errno));
		ipcLog("msgsnd errno: %d\n",errno);
		ipcLog("killing pid : %d\n",ipc_dict[0].pid);
		// fatal, shut down the whole process group
		kill(ipc_dict[0].pid,SIGTERM);
	}

	return 0;
}

int
rm_queue(msqid)
{
	if(msgctl(msqid,IPC_RMID,NULL)<0) 
	{
		ipcLog("msgctl IPC_RMID failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	return 0;
}

MSG_BUF *
recvMessage(int msqid, long mtype)
{
	static MSG_BUF msg;
	size_t size=sizeof(msg)-sizeof(long);
	int msgflg=0;  // may add later if need for selective IPC_NOWAIT arises

	/*
	ipcLog("msgrcv(msqid=%d,msg=%x,size=%d,mtype=%d,msgflg=%d)\n"
	, msqid
	, msg
	, size
	, mtype
	, msgflg
	);
	*/

 	// man: ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);

	// mtype should be the pid of the caller
	if(msgrcv(msqid,&msg,size,mtype,msgflg)<0) 
	{
		ipcLog("msgrcv failed Fatal ERROR: %s\n",strerror(errno));
		// fatal, shut down the whole process group
		kill(ipc_dict[0].pid,SIGTERM);
	}
	return &msg;
}

void
initNotify(int qid)
{
static char msg[81];
pid_t root_pid;

	ipcLog("init_notyfy() qid=%d\n",qid);
	// root init process is always in slot[0]
	root_pid=ipc_dict[0].pid;
	sprintf(msg,"Process %d Loggin in",getpid());
	sendMessage(qid, root_pid, C_LOGIN, msg);
}

char *
ipcTypeString(int type)
{
	switch(type)
	{
	case P_ROOT		: return "ROOT";
	case P_AVR		: return "AVR";
	case P_SQLITE	: return "SQLITE";
	}
	return "UNKNOWN";
}

char *
ipcCmdName(int cmd)
{
	switch(cmd)
	{
	case C_LOGIN:  return "LOGIN";
	case C_LOGOUT: return "LOGOUT";
	}
	return "UNKNOWN";
}
