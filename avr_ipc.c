#include "avr.h"
#include <sys/ipc.h>
#include <sys/msg.h>
 
long shmid;

int 
  msqid
, slot
;

IPC_HEAD *ipc_head;
IPC_DICT *ipc_dict;

// Common Exit Point for all processes
void
ipcExit(pid_t pid, int err, int sig)
{
	ipcLog("Process %d shutdown Exit code=%d [%s] Signal=%d\n"
	, pid
	, err
	, strerror(err)
	, sig
	);

	// Vacate our ipc slot
	ipcClearSlot(pid);

	ipcLog("Goodbye!\n");
	exit(err); 
}

int
ipcGetSharedMemory(pid_t pid, int ptype, const char *token)
{
int ix, sz;

	//ipcLog("getshmem(pid:%d,token:%s)\n",pid,token);

	if(ipcAllocSharedMemory(pid,ptype,token)!=0)
	{
		ipcLog("allocateSharedMemory Failed ERROR: %s\n",strerror(errno));
		ipcExit(pid,errno,0);
	}

	// scan the dictionary for a spare slot, loop will break on vacant slot (pid=0)
	for(ix=0; ix<MAX_IPC && ipc_dict[ix].pid; ix++)
	{
		;;
		/*
		ipcLog("Busy IPC Slot %d Type:%s Pid:%d\n"
		, ix
		, ipc_dict[ix].stype
		, ipc_dict[ix].pid);
		*/
	}

	if(ix==MAX_IPC)
	{
		ipcLog("No Application Dictionary Slots available %d Max\n",MAX_IPC);
		ipcExit(pid,errno,0);
	}

	// slot is global
	slot=ix;

	// assign initial values
	ipc_dict[slot].pid=pid;
	ipc_dict[slot].type=ptype;
	ipc_dict[slot].online=time(0);
	strcpy(ipc_dict[slot].stype,ipcTypeName(ptype));

	ipcLog("Found Vacant IPC Slot %d Pid:%d Type:%s\n"
	, slot
	, ipc_dict[slot].pid
	, ipc_dict[slot].stype
	);

	return slot;
}

int
ipcGetPidByType(int type)
{
int slot;
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		if(type==ipc_dict[slot].type) return ipc_dict[slot].pid;
	}
}

int
ipcGetSlotByType(int type)
{
int slot;
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		if(type==ipc_dict[slot].type) return slot;
	}
	return -1;
}

int
ipcGetSlotByPid(pid_t pid)
{
int slot;
	for(slot=0; slot<MAX_IPC && ipc_dict[slot].pid; slot++)
	{
		if(pid==ipc_dict[slot].pid) return slot;
	}
	return -1;
}

int
ipcAllocSharedMemory(pid_t pid, int ptype, const char *token)
{
char *mem, *shmat();
key_t key;
int sz, flags=0;

	// if there is no token file create it
	if(access(token,0)) fclose(fopen(token,"w"));

	// use ftok on a file to generate the key
	if((key=ftok(token,1))<0) 
	{
		ipcLog("ftok(%s) failed ERROR: %s\n",token,strerror(errno));
		ipcExit(pid,errno,0);
	}
	// only lead process creates a segment
	if(ptype==P_ROOT) flags=IPC_CREAT|0666;

	/* get enough shared memory for MAX_IPC Links (devined in avr.h)*/
	sz=sizeof(IPC_DICT)*MAX_IPC+sizeof(IPC_HEAD);

	//ipcLog("shmget(key=%d,bytes=%d,flags=%x) token=%s\n",key,sz,flags,token);

	if((shmid=shmget(key,sz,flags))<0)
	{
		ipcLog("Can't get shared segment size %d for key %d errno:%d\n",sz,key,errno);
		ipcExit(pid,errno,0);
	}

	if((mem=shmat(shmid,(char*)0,0))==(char *)-1L)
	{
		if(ptype==P_ROOT)
		{
			shmctl(shmid,IPC_RMID,0);
		}
		ipcLog("Can't attatch to shared segment errno:%d\n",errno);
		ipcExit(pid,errno,0);
	}

	// only avr_init process should do this 
	if(ptype==P_ROOT)
	{
		//ipcLog("Clearing Shared Memory\n");
		memset(mem,0,sz);
	}

	// cast memory as an array of IPC_DICT structures
	sz-=sizeof(IPC_DICT);
	ipc_dict=(IPC_DICT *)mem;
	ipc_head=(IPC_HEAD *)&mem[sz];
	if(ptype==P_ROOT)
	{
		ipc_head->proot=pid;
	}

	ipcLog("head-root : %d\n",ipc_head->proot);
	ipcLog("head-txmsg: %d\n",ipc_head->txmsg);
	ipcLog("head-rxmsg: %d\n",ipc_head->rxmsg);

	return 0;
}

void
ipcFreeSharedMem(pid_t pid)
{
	if(shmctl(shmid,IPC_RMID,0)!=0)
		ipcLog("shmctl IPC_RMID Failed ERROR: %s\n",strerror(errno));
	else
		ipcLog("PID:%d Removed shmid %d\n",pid,shmid);
}

void
ipcClearSlot(pid_t pid)
{
int slot;
	// P_ROOT may have alread vacated it
	if((slot=ipcGetSlotByPid(pid))<0) return;

	pid=ipc_dict[slot].pid;
	memset(&ipc_dict[slot],0,sizeof(IPC_DICT));
	ipc_dict[slot].type=0;
	ipcLog("Vacated ipc slot: %d for pid: %d\n",slot,pid);
}

int 
ipcGetMessageQueue(pid_t pid, int ptype, char *token)
{
	int flags=0;
	int msqid;
	key_t key;

	//ipcLog("getMessageQueueue(%d,%d,%s)\n",pid,ptype,token);

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

	/*
	ipcLog("mssget(key=%d,flags=%x) token=%s\n"
	, key
	, flags
	, token
	);
	*/

	if((msqid=msgget(key,flags))<0) 
	{
		ipcLog("ftok failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	//ipcLog("getMessageQueueue() got msqid=%d\n",msqid);
	return msqid;
}

int
ipcSendMessage(pid_t pid, int msqid, long mtype, int cmd, char *txt)
{
	MSG_BUF msg;
	int msgflg;
	
	// mtype should be the pid of the receiver
	msg.rsvp = pid;
	msg.slot = slot; // global per process
	msg.type = mtype; 
	msg.cmd  = cmd; 
	msg.len  = sizeof(msg)-sizeof(long);
	msgflg   = 0;
	strcpy(msg.scmd,ipcCmdName(cmd));
	strcpy(msg.text,txt);

	/*
	ipcLog("SEND ADDR: %d msgsnd(msqid=%d,msg=%x,len=%d,msgflg=%d, cmd:%d)\n"
	, mtype
	, msqid
	, msg
	, msg.len
	, msgflg
	, cmd
	);
	*/

	for(;;) 
	{
		// man: int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
		if(msgsnd(msqid,&msg,msg.len,msgflg)<0) 
		{
			ipcLog("msgsnd failed ERROR: %s\n",strerror(errno));
			// don't let intrrupted system call screw us
			if(errno!=EINTR) return errno;
		}
		ipc_head->txmsg++;
		return 0;
	}
}
/*
struct msqid_ds 
{
   struct ipc_perm msg_perm;     // Ownership and permissions
   time_t          msg_stime;    // Time of last msgsnd(2)
   time_t          msg_rtime;    // Time of last msgrcv(2)
   time_t          msg_ctime;    // Time of last change
   unsigned long   __msg_cbytes; // Current number of bytes in queue (nonstandard)
   msgqnum_t       msg_qnum;     // Current number of messages in queue
   msglen_t        msg_qbytes;   // Maximum number of bytes allowed in queue
   pid_t           msg_lspid;    // PID of last msgsnd(2)
   pid_t           msg_lrpid;    // PID of last msgrcv(2)
};
*/

int
ipcMessageStatus(int msqid)
{
	struct msqid_ds ds;

	if(msgctl(msqid,IPC_INFO,&ds)<0) 
	{
		ipcLog("msgctl IPC_INFO failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	return 0;
}

int
ipcRemoveMsgQueue(int msqid)
{
	if(msgctl(msqid,IPC_RMID,NULL)<0) 
	{
		ipcLog("msgctl IPC_RMID failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	return 0;
}

MSG_BUF *
ipcRecvMessage(int msqid, pid_t pid)
{
	static MSG_BUF msg;
	size_t size=sizeof(msg)-sizeof(long);
	int msgflg=0;  // may add later if need for selective IPC_NOWAIT arises

	/*
	ipcLog("RECV ADDR: %d msgrcv(msqid=%d,msg=%x,size=%d,mtype=%d,msgflg=%d)\n"
	, mtype
	, msqid
	, msg
	, size
	, mtype
	, msgflg
	);
	*/

	for(;;) 
	{
		// man: ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
		if(msgrcv(msqid,&msg,size,pid,msgflg)<0) 
		{
			ipcLog("msgrcv failed ERROR: %s\n",strerror(errno));
			// don't let intrrupted system call screw us
			if(errno!=EINTR) return NULL;
		}
		ipc_head->rxmsg++;
		return &msg;
	}
}

void
ipcNotify(pid_t pid, int qid)
{
static char msg[81];
pid_t root_pid;
int slot, type;
char *str;

	if((slot=ipcGetSlotByPid(pid))<0) return;
	type=ipc_dict[slot].type;
	str=ipc_dict[slot].stype;

	// root init process is always in slot[0]
	root_pid=ipc_dict[0].pid;
	sprintf(msg,"Process %d Type %s Logged Into IPC Slot %d"
	, pid
	, str
	, slot
	);
	ipcSendMessage(pid, qid, root_pid, C_LOGIN, msg);
}

char *
ipcTypeName(int ptype)
{
	char *name;
	switch(ptype)
	{
	case	P_ROOT   : return "P_ROOT";
	case	P_TTY 	 : return "P_TTY";
	case	P_SQLITE : return "P_SQLITE";
	case	P_SHELL	 : return "P_SHELL";
	case	P_HTTP	 : return "P_HTTP";
	default			 : return "???";
	}
}

char *
ipcCmdName(int cmd)
{
	switch(cmd)
	{
	case C_LOGIN	: return "C_LOGIN";
	case C_LOGOUT	: return "C_LOGOUT";
	case C_FAIL		: return "C_FAIL";
	}
	return "C_UNKN";
}

char *
ipcSigName(int sig)
{
	switch(sig)
	{
	case  1: return "SIGHUP";
	case  2: return "SIGINT";
	case  3: return "SIGQUIT";
	case  4: return "SIGILL";
	case  5: return "SIGTRAP";
	case  6: return "SIGABRT";
	case  7: return "SIGBUS";
	case  8: return "SIGFPE";
	case  9: return "SIGKILL";
	case 10: return "SIGUSR1";
	case 11: return "SIGSEGV";
	case 12: return "SIGUSR2";
	case 13: return "SIGPIPE";
	case 14: return "SIGALRM";
	case 15: return "SIGTERM";
	case 16: return "SIGSTKFLT";
	case 17: return "SIGCHLD";
	case 18: return "SIGCONT";
	case 19: return "SIGSTOP";
	case 20: return "SIGTSTP";
	case 21: return "SIGTTIN";
	case 22: return "SIGTTOU";
	case 23: return "SIGURG";
	case 24: return "SIGXCPU";
	case 25: return "SIGXFSZ";
	case 26: return "SIGVTALRM";
	default: return "???";
	}
}

char ** // split string into an array
ipcSplit(char *s, char k)
{
	/* maximum of 25 list items */
	static char *av[25]; 
	int i=0;

	while(*s && i<25)
	{
		av[i++]=s;
		while(*s && *s!=k)
		{
			if(*s=='"'){
				for(s++; *s && *s!='"'; s++)
				;;
			}
			s++;
		}
		while(*s && *s==k) *(s++)='\0';
	}
	av[i]=NULL;
	return(av);
}
