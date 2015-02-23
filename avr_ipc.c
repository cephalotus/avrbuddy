#include "avr.h"
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
 
long 
  shmid
, msqid
, semid
;

int ipc_slot;

pid_t  // global for simplicity
  pid
, ppid
;

static int debug=0;

/* defined in sem.h
struct sembuf 
{
	short sem_num;        // semaphore number
	short sem_op;         // semaphore operation 
	short sem_flg;        // operation flags
};
*/

IPC_HEAD *ipc_head;
IPC_DICT *ipc_dict;
IPC_TEXT *ipc_text;

void // signal handler
sigAlarm(int sig)
{
	// do nothing
}

// Common Exit Point for all processes
void
ipcExit(int err, int sig)
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
ipcGetIpcResources(int ptype, char *token)
{
	pid=getpid();
	ppid=getppid();

	ipcGetSharedMemory(ptype, token);
	ipcGetMessageQueue(ptype, token);
	ipcGetSemaphore(pid, ptype, token);

	// make sure semaphore is free on creation
	if(ptype==P_ROOT) ipcSemRelease();
}

int
ipcGetSharedMemory(int ptype, char *token)
{
int ix, sz;

	//ipcLog("getshmem(pid:%d,token:%s)\n",pid,token);

	if(ipcAllocSharedMemory(ptype,token)!=0)
	{
		ipcLog("allocateSharedMemory Failed ERROR: %s\n",strerror(errno));
		ipcExit(errno,0);
	}

	// scan the dictionary for a spare slot, loop will break on vacant slot (pid=0)
	for(ix=0; ix<MAX_IPC && ipc_dict[ix].pid; ix++)
	{
		ipcLog("Busy IPC Slot %d Type:%s Pid:%d\n"
		, ix
		, ipc_dict[ix].stype
		, ipc_dict[ix].pid
		);
	}

	if(ix==MAX_IPC)
	{
		ipcLog("No Application Dictionary Slots available %d Max\n",MAX_IPC);
		ipcExit(errno,0);
	}

	// ipc_slot is global
	ipc_slot=ix;

	// assign initial values
	ipc_dict[ipc_slot].pid=pid;
	ipc_dict[ipc_slot].type=ptype;
	ipc_dict[ipc_slot].online=time(0);
	strcpy(ipc_dict[ipc_slot].stype,ipcTypeName(ptype));

	debug&&ipcLog("Acquired Vacant IPC Slot %d Pid:%d Type:%s\n"
	, ipc_slot
	, ipc_dict[ipc_slot].pid
	, ipc_dict[ipc_slot].stype
	);

	return ipc_slot;
}

int
ipcAllocSharedMemory(int ptype, char *token)
{
char *mem, *shmat();
key_t key;
int flags=0;
unsigned long sz;

	if((key=ipcGetIpcKey(pid,token))<0)
		return key;

	// only lead process creates a segment
	if(ptype==P_ROOT) flags=IPC_CREAT|0666;

	/* get enough shared memory for MAX_IPC Links MAX_LINE, & IPC_HEAD (devined in avr.h)*/
	sz=(sizeof(IPC_HEAD)+sizeof(IPC_DICT)*MAX_IPC)+(sizeof(IPC_TEXT)*MAX_TEXT);

	debug&&ipcLog("shmget(key=%d,bytes=%d,flags=%x) token=%s\n",key,sz,flags,token);

	if((shmid=shmget(key,sz,flags))<0)
	{
		ipcLog("Can't get shared segment size %d for key %d errno:%d\n",sz,key,errno);
		ipcExit(errno,0);
	}

	if((mem=shmat(shmid,(char*)0,0))==(char *)-1L)
	{
		if(ptype==P_ROOT)
		{
			shmctl(shmid,IPC_RMID,0);
		}
		ipcLog("Can't attatch to shared segment errno:%d\n",errno);
		ipcExit(errno,0);
	}

	// only avr_init process should do this 
	if(ptype==P_ROOT)
	{
		//ipcLog("Clearing Shared Memory\n");
		memset(mem,0,sz);
	}

	// cast memory as an array of IPC_DICT, etc structures
	ipc_head=(IPC_HEAD *)mem;
	mem+=sizeof(IPC_HEAD);
	ipc_dict=(IPC_DICT *)mem;
	mem+=sizeof(IPC_DICT)*MAX_IPC;
	ipc_text=(IPC_TEXT *)mem;

	debug&&ipcLog("head-root : %d\n",ipc_head->proot);
	debug&&ipcLog("head-txmsg: %d\n",ipc_head->txmsg);
	debug&&ipcLog("head-rxmsg: %d\n",ipc_head->rxmsg);

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


long
ipcGetSemaphore(pid_t pid, int ptype, char *token)
{
key_t key;
int flags=0;

	if((key=ipcGetIpcKey(pid,token))<0)
		return key;

	if(ptype==P_ROOT)
	{
		flags=IPC_CREAT|0666;
	}

	if((semid=semget(key,1,flags))<0)
	{
		ipcLog("Can't get semaphore for key %d errno:%d\n",key,errno);
		ipcExit(errno,0);
	}
	return semid;
}


static struct sembuf sem_release = { 0, 1, IPC_NOWAIT };

int 
ipcSemRelease()
{

	debug&&ipcLog("ipcSemRelease() -- releasing\n");
	if(semop(semid,&sem_release,1)<0)
	{ 
		ipcLog("SemRelease semop failed ERROR: %s\n",strerror(errno));
		return errno;
	} 
	debug&&ipcLog("ipcSemRelease() success\n");
}

static struct sembuf sem_lock = { 0, -1, SEM_UNDO };

int 
ipcSemLock()
{

	debug&&ipcLog("ipcSemLock() -- Locking\n");
	if(semop(semid,&sem_lock,1)<0)
	{ 
		ipcLog("SemLock semop failed ERROR: %s\n",strerror(errno));
		return errno;
	} 
	debug&&ipcLog("ipcSemLock() success\n");
}

key_t
ipcGetIpcKey(pid_t pid, char *token)
{
key_t key;

	if(access(token,0)) fclose(fopen(token,"w"));

	if((key=ftok(token,1))<0) 
	{
		ipcLog("ftok failed ERROR: %s\n",strerror(errno));
		ipcExit(errno,0);
	}
	return key;
}

int 
ipcGetMessageQueue(int ptype, char *token)
{
	int flags=0;
	key_t key;

	//ipcLog("getMessageQueueue(%d,%d,%s)\n",pid,ptype,token);

	// if there is no file create it

	if((key=ipcGetIpcKey(pid,token))<0)
		return key;

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
		ipcLog("msgget failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	//ipcLog("getMessageQueueue() got msqid=%d\n",msqid);
	return msqid;
}

int
ipcSendMessage(long addr, int cmd, char *txt)
{
	MSG_BUF msg;
	int msgflg;
	
	// addr should be the pid of the receiver
	msg.rsvp = pid;
	msg.slot = ipc_slot; // global per process
	msg.type = addr; 
	msg.cmd  = cmd; 
	msg.len  = sizeof(msg)-sizeof(long);
	msgflg   = 0;
	strcpy(msg.scmd,ipcCmdName(cmd));
	strcpy(msg.text,txt);

	debug&&ipcLog("Sending (%s) -> TO: %d (%s) [%s]\n"
	, ipcCmdName(cmd)
	, addr
	, ipcTypeNameByPid(addr)
	, msg.text
	);

	for(;;) 
	{
		// man: int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
		if(msgsnd(msqid,&msg,msg.len,msgflg)<0) 
		{
			ipcLog("msgsnd failed ERROR: %s\n",strerror(errno));
			// don't let intrrupted system call screw us
			if(errno!=EINTR) return errno;
		}
			debug&&ipcLog("Sent OK\n");
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
	ipcLog("msgctl IPC_INFO ok. Add more logging to see msqid_ds structure\n");
	return 0;
}

int
ipcRemoveMsgQueue()
{
	if(msgctl(msqid,IPC_RMID,NULL)<0) 
	{
		ipcLog("msgctl IPC_RMID failed ERROR: %s\n",strerror(errno));
		return errno;
	}
	return 0;
}

int
ipcRemoveSemaphore()
{
	if(semctl(semid,1,IPC_RMID)<0) 
	{
		ipcLog("semctl IPC_RMID failed on semid:%d ERROR: %s\n",semid,strerror(errno));
		return errno;
	}
	return 0;
}

int
ipcCleanMessageQueue()
{
	MSG_BUF msg;
	size_t size=sizeof(msg)-sizeof(long);
	int mtype=0;
	int msgflg=IPC_NOWAIT;

	debug&&ipcLog("msgrcv(msqid=%d,size=%d,mtype=%d,msgflg=%02x)\n"
	, msqid
	, size
	, mtype
	, msgflg
	);

	while(msgrcv(msqid,&msg,size,mtype,msgflg)>0) 
	{
		ipcLog("Abandoned Message From: %d  Command: %s\n",msg.rsvp,ipcCmdName(msg.cmd));
	}
}


MSG_BUF *
ipcRecvMessage()
{
	static MSG_BUF msg;
	size_t size=sizeof(msg)-sizeof(long);
	int msgflg=0;  // may add later if need for selective IPC_NOWAIT arises
	int i;

	debug&&ipcLog("%d %s Recieving -- \n"
	, pid
	, ipcTypeNameByPid(pid)
	);

	for(i=0 ;; i++) 
	{
		// man: ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
		if(msgrcv(msqid,&msg,size,pid,msgflg)<0) 
		{
			ipcLog("msgrcv failed ERROR: %s\n",strerror(errno));
			// don't let intrrupted system call screw us
			if(errno!=EINTR) 
			{
				ipcLog("Receiving --  Interrupt count=%d\n",i+1);
				if(i>2)
				{
					ipcLog("Receiveing -- Too Many retries\n");
					return NULL;
				}
			}
		}

		debug&&ipcLog("Message From:[%d] Cmd:[%s] Txt:[%s]\n",msg.rsvp,ipcCmdName(msg.cmd),msg.text);

		// increment system message receive count
		ipc_head->rxmsg++;
		return &msg;
	}
}


int
ipcAddText(pid_t to_pid, const char *text)
{
int i; 
	for(i=0; i<MAX_TEXT; i++)
	{
		// look for vacant text slot
		if(ipc_text[i].from_pid) continue;

		ipcSemLock();

		// fill the structure
		ipc_text[i].from_pid=pid;
		ipc_text[i].to_pid=to_pid;
		strcpy(ipc_text[i].text,text);

		ipcSemRelease();

		debug&&ipcLog("Added text: %d->%d [%s]\n"
		, ipc_text[i].from_pid
		, ipc_text[i].to_pid
		, ipc_text[i].text
		);

		debug&&ipcLog("Saved [%s] in text ipc_slot %d\n",text,i);
		return 0;
	}
	ipcLog("No empty text slots!\n");
	return -1;
}

IPC_TEXT *
ipcGetText(pid_t from_pid)
{
int i;
	debug&&ipcLog("Scanning Text messges %d->%d\n",from_pid,pid);
	for(i=0; i<MAX_TEXT; i++)
	{
		// look for vacant one
		if(ipc_text[i].from_pid==from_pid 
		&& ipc_text[i].to_pid==pid)
		{
			debug&&ipcLog("ix:%d %d->%d > %s\n"
			, i
			, ipc_text[i].from_pid
			, ipc_text[i].to_pid
			, ipc_text[i].text
			);
			return &ipc_text[i];
		}
	}
	debug&&ipcLog("No Text messges found!\n");
	return NULL;
}

void
ipcNotify(pid_t pid)
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
	ipcSendMessage(root_pid, C_LOGIN, msg);
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

char * 
ipcTypeNameByPid(pid_t pid)
{
	int slot, type;
	if((slot=ipcGetSlotByPid(pid))<0)
	{
		return "UNK?";
	}

	// point to the slot
	return(ipcTypeName(ipc_dict[slot].type));
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
	case	P_MON	 : return "P_MON";
	case	P_SYSTEM : return "P_SYSTEM";
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
	case C_PING		: return "C_PING";
	case C_ACK		: return "C_ACK";
	case C_NAK		: return "C_NAK";
	case C_EOF		: return "C_EOF";
	case C_SQL		: return "C_SQL";
	case C_SYS		: return "C_SYS";
	case C_ERR		: return "C_ERR";
	default			: return "C_UNK";
	}
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
