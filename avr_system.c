#include "avr.h"
#include <libgen.h>

static pid_t
  pid
, ppid
, cpid
;

int slot;

extern IPC_DICT *ipc_dict;
static IPC_DICT *dict;

static struct termio tty;

void // signal handler
sysTerminate(int sig)
{
int x,id;
	ipcLog("Proc %d P_SYSTEM Terminiate Requested SIG: %s!\n",getpid(),ipcSigName(sig));
	ipcLog("Proc %d P_SYSTEM cpid=%d\n",cpid);
	if(cpid)
	{
		ipcLog("Killing Child P_SYSTEM Process %d\n",cpid);
		kill(cpid,SIGTERM);

		// don't leave a zombie
		id=wait(&x);

		ipcLog("Child Process %d died With exit code=%d\n",id,x);
	}
	ipcExit(getpid(),0,0);
}

int 
main(int argc, char* argv[])
{
    int pp[2], cc, c, status, cslot; 
	char *part, **aa, buf[1024];
	IPC_DICT *d;
	MSG_BUF *msg;

	signal(SIGINT, sysTerminate);
	signal(SIGTERM,sysTerminate);

	ppid=getppid(); 
	pid=getpid();

	/* set up application logging */
	logOpen("system");

	ipcLog("Starting P_SYSTEM process!\n");

	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_SYSTEM,"/tmp/ipc_application");
	//ipcLog("Got memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=ipcGetMessageQueue(pid,P_SYSTEM,"/tmp/ipc_application");

	// notify root process we are here
	ipcNotify(pid,msqid);

	ipcLog("P_SYSTEM Process %d On-Line in slot %d\n"
	, pid
	, slot
	);

	for(;;)
	{
		ipcLog("Waiting for Message\n");
		// wait for messages or signals
		msg=ipcRecvMessage(msqid, pid);

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

		// make an array for execvp
		aa=ipcSplit(msg->text,' ');

		// open a system pipe
		if((cc=pipe(pp))<0)
		{
			fprintf(stderr,"Line %d Error:%s",__LINE__,strerror(errno));
			exit(1);
		}

		// standard fork with pipe arrangement
		switch(cpid=fork())
		{
		case -1:			// error
			exit(1);

		case 0:				// child
			close(1);		// close stdin
			dup(pp[1]);		// replace stdin with pipe write 
			close(pp[0]);

			
			strcpy(buf,aa[0]);
			part=dirname(buf);
			ipcLog("dirname: %s\n",part);

			strcpy(buf,aa[0]);
			part=basename(buf);
			ipcLog("basename: %s\n",part);

			// show what we are about to do
			ipcLog("execvp(%s,[",aa[0]); for(cc=1; aa[cc]; cc++) ipcRawLog("%s ",aa[cc]); ipcRawLog("])\n",aa[cc]);

			// have linux run the request
			execvp(aa[0],aa);
			exit(0);

		default:			// parent
			close(0);       // close stdout
			dup(pp[0]);     // replace stdout with pipe write 
			close(pp[1]);
			break;
		}

		// read ouput lines and message them to requestor
		while(fgets(buf, sizeof(buf), stdin)!=NULL)
		{
			// insulate from interrupt system calls
			if(errno && errno==EINTR) continue;

			ipcLog("<< %s\n",buf);

			// strip the newline
			buf[strlen(buf)-1]='\0';

			// return message to requestor
			ipcSendMessage(pid,msqid,msg->rsvp,C_ACK,buf);
		}

		// that's all folks
		ipcSendMessage(pid,msqid,msg->rsvp,C_EOF,"C_EOF");

		sleep(1);

		// kill and wait to prevent zombie process
		kill(cpid,SIGTERM); 
		waitpid(cpid,&status,WNOHANG);
	}
}
