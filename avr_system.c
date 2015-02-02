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
	chdir("/home/yun");
	ipcLog("Chdir: %s\n",strerror(errno));

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
	FILE *pp;
	char cmd[512];

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

		// save the command
		strcpy(cmd,msg->text);

		// make an array for execvp
		aa=ipcSplit(msg->text,' ');
		if(!strcmp(aa[0],"cd"))
		{
			if(chdir(aa[1])<0)
			{
				sprintf(buf,"cd failed: %s",strerror(errno));
				ipcSendMessage(pid,msqid,msg->rsvp,C_ACK,buf);
			}
			ipcSendMessage(pid,msqid,msg->rsvp,C_EOF,"");
			continue;
		}
		if((pp=popen(cmd,"r"))==NULL)
		{
			sprintf(buf,"popen(%s) failed: %s\n",cmd,strerror(errno));
			ipcLog("%s",buf);

			// uh oh
			ipcSendMessage(pid,msqid,msg->rsvp,C_ACK,buf);
			ipcSendMessage(pid,msqid,msg->rsvp,C_EOF,"");
		}

		while(fgets(buf, sizeof(buf), pp)!=NULL)
		{
			// insulate from interrupt system calls
			if(errno && errno==EINTR) continue;

			ipcLog("<< %s\n",buf);

			// strip the newline
			buf[strlen(buf)-1]='\0';

			// return message to requestor
			ipcSendMessage(pid,msqid,msg->rsvp,C_ACK,buf);
		}

		fclose(pp);

		// that's all folks
		ipcSendMessage(pid,msqid,msg->rsvp,C_EOF,"");
	}
}
