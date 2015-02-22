#include "avr.h"

int 
  debug_level
;

static int 
  prog_cnt
, event_id
, ipc_slot
;

static pid_t pid;

IPC_DICT *ipc_dict;

char *libdir="/home/yun/lib";

pid_t startChildProcess(char **a);

void // signal handler
sigDeadChild(int sig)
{
int status; pid_t cpid;
	if((cpid=waitpid(cpid,&status,WNOHANG))>0)
	{
		ipcLog("Child %d is Dead\n",cpid);

	}

	// clean up slot in case child abandoned it
	ipcClearSlot(cpid);

	// reset the signal
	signal(SIGCLD,sigDeadChild);
}

void // signal handler
sigTerminate(int sig)
{
int status, slot, kflag;
pid_t cpid;

	ipcLog("Init Proc %d Terminiate Requested! SIG: %s\n",pid,ipcSigName(sig));

	// kill all processes in the application group, 
	// 0 is me, so start at slot number 1
	for(kflag=slot=1; slot<MAX_IPC; slot++)
	{
		// skip empty slots
		if(!ipc_dict[slot].pid) continue;
		kflag++;

		cpid=ipc_dict[slot].pid;

		ipcLog("Killing Child Process %d in Ipc Slot %d\n",cpid,slot);

		// kill and wait to prevent zombie process
		kill(cpid,SIGTERM); 

		// trap death and cleanup after child 
		sigDeadChild(SIGTERM);
	}
	if(!kflag)
	{
		ipcLog("No Slots Occupied by Sub-Processes\n");
	}

	ipcLog("Freeing Shared Memory segment pid:%d\n",pid);
	ipcFreeSharedMem(pid);

	ipcLog("Getting Queue Status, id=%d\n",msqid);
	ipcMessageStatus(msqid);

	ipcLog("Cleaning out Message Queue, id=%d\n",msqid);
	ipcCleanMessageQueue(msqid);

	ipcLog("Removing Message Queue, id=%d\n",msqid);
	ipcRemoveMsgQueue(msqid);

	ipcLog("Removing Semaphore, id=%d\n",semid);
	ipcRemoveSemaphore(semid);

	// we should be the last one standing.
	ipcExit(pid,0,0);
}

main(char **ac, int av)
{
char **aa;
char ib[128], fn[41];

FILE *fp;
MSG_BUF *msg;

	/* divorce ourselves from the process
	** group that we started in so that
	** we are immune to death if the
	** process group leader dies
	** see daemon.c
	*/
	daemonize();

	// have to do this after daemonize because it forked!
	pid=getpid();

	logOpen("init");

	ipcLog("PID %d Starting Avr Application Group!\n",pid);


	// ipc_slot=ipcGetSharedMemory(pid,P_ROOT,"/tmp/ipc_application");
	ipc_slot=ipcGetIpcResources(pid,P_ROOT,"/tmp/ipc_application");

	ipcLog("Shared Memory at %x slot=%d\n",ipc_dict,ipc_slot);

	//msqid=ipcGetMessageQueue(pid,P_ROOT,"/tmp/ipc_application");
	//semid=ipcGetSemaphore(pid,P_ROOT,"/tmp/ipc_application");

	sprintf(fn,"%s/avr.config",libdir);

	if((fp=fopen(fn,"r"))==NULL)
	{
		ipcLog("Config file %s not found -- swawning no processes\n",fn);
	}
	else
	{
		// read config file and startup processes
		while(fgets(ib,80,fp))
		{
			// skip comments and blank lines
			if(*ib == '#'
			|| *ib == '\n'
			|| *ib == '\r'
			) continue;

			// we have a process to start
			ib[strlen(ib)-1]='\0';
			aa=ipcSplit(ib,' ');
			startChildProcess(aa);
		}
		fclose(fp);
	}

	// set signal handlers
	signal(SIGTERM,sigTerminate);
	signal(SIGCLD ,sigDeadChild);

	for(;;) // until we get SIGTERM
	{
	IPC_DICT *d;
	int status, cslot;
	pid_t cpid;

		//ipcLog("Waiting for Message\n");
		// wait for messages or signals
		msg=ipcRecvMessage(msqid, pid);

		// point to sender's dictionary entry
		d=&ipc_dict[msg->slot];

		// compute slot number
		cslot=d-ipc_dict;

		ipcLog("Message From: %d  Slot: %d type: %s cmd: %s msg: %s\n"
		, msg->rsvp
		, cslot
		, d->stype
		, msg->scmd
		, msg->text
		);
		switch(msg->cmd)
		{
		case C_FAIL :
			kill(msg->rsvp,SIGTERM);
			sigDeadChild(SIGTERM);
		}
	}
}

pid_t
startChildProcess(char **a)
{
int i ,l ,qsize;
pid_t fk;
static char 
  wb[128]
, prog[41]
, arg1[41]
,*bin_dir
;

	for(i=0; a[i]; i++)
	{
		if(!memcmp(a[i],"-v",2)) break;
	}

	l=0;

	bin_dir="/home/yun/bin";

	sprintf(prog,"%s/%s",bin_dir,a[0]);
	l+=sprintf(wb,"%s ", prog);

	for(i=0; a[i]; i++) 
	{
		l+=sprintf(wb+l,"%s ",a[i]);
	}

	wb[l-1]='\0';

	ipcLog("PROG: %s\n",prog);

	for(i=0; a[i]; i++)
	{
		ipcLog("ARG: %s\n",a[i]);
	}

	switch(fk=fork())
	{
		case -1:
			ipcLog("Starting up [%s]\n",wb);
			ipcLog("Fork Failed !! THE END !\n"); 
			break;

		case 0:
			execv(prog,a);

			// if we get here, execv failed
			ipcLog("Can't execute [%s] Error %d\n",prog,errno);

			// kill child just in case
			kill(fk,SIGTERM);

		default:
			ipcLog("Started up [%s] as pid: %d\n",wb,fk);
			break;
	}

	/* we may be called back, so
	** let things stabilize a bit
	*/
	sleep(1);
	return fk;
}

