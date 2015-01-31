#include "avr.h"
#include <termio.h>
#include <fcntl.h>

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
shellTerminate(int sig)
{
int x,id;
	ipcLog("Proc %d SHELL Terminiate Requested SIG: %s!\n",getpid(),ipcSigName(sig));
	ipcLog("Proc %d SHELL cpid=%d\n",cpid);
	if(cpid)
	{
		ipcLog("Killing Child SHELL Process %d\n",cpid);
		kill(cpid,SIGTERM);

		// don't leave a zombie
		id=wait(&x);

		ipcLog("Child Process %d died With exit code=%d\n",id,x);
	}
	ipcExit(getpid(),0,0);
}

main(int argc, char *argv[])
{
char *s, *cmd, *arg, buf[1024];
FILE *fp;
int cnt, c, x, i;

	signal(SIGINT, shellTerminate);
	signal(SIGTERM,shellTerminate);

	ppid=getppid(); 
	pid=getpid();

	/* set up application logging */
	logOpen("shell");

	ipcLog("Starting P_SHELL process!\n");

	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_SHELL,"/tmp/ipc_application");
	//ipcLog("Got memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=ipcGetMessageQueue(pid,P_SHELL,"/tmp/ipc_application");

	// notify root process we are here
	ipcNotify(pid,msqid);

	ipcLog("SHELL Process %d On-Line in slot %d\n"
	, pid
	, slot
	);


	ipcLog("CP: SHELL I am Child Pid=%d\n",cpid);
	// we inherited signal traps from parent

	for(i=0;;i++)
	{
		// don't allow buffer over-run
		if(i==1023)
		{

			buf[i]='\0';
			buf[--i]='\n';
			ipcLog("Serial Over-run Line: [%s]\n",buf);
			i=-1;
			continue;
		}

		printf("Welcome To Avrbuddy!\n\n");
		for(;;)
		{
			printf(">> ");
			fgets(buf, sizeof(buf), stdin);
			buf[strlen(buf)-1]='\0';

			// get command and argument
			for(cmd=s=buf; *s && *s!=' '; s++)
			;;
			*s++='\0';
			arg=s;
			if(!strcmp(cmd,"exit")||!strcmp(cmd,"quit"))
			{
				printf("Goodbye!\n");
				shellTerminate(0);
			}
			else if(!strcmp(cmd,"sql")) 
				doMessageCommand(C_SQL,arg);
			else
				printf("%s\n",buf);
		}
		// we should put some code in here
		// to implement protocol for avr sketches
		//ipcDebug(50,"<<[%02d]< %02x\n",i,buf[i]);

		ipcLog("CP: buf[%02d]< %02x\n",i,buf[i]);
	}
}

int
doMessageCommand(int cmd, char *arg)
{
IPC_DICT *d;
MSG_BUF *msg;
int n, id, cslot;
char com_buf[256];
	switch(cmd)
	{
	case C_SQL:
		// send a message to the C_SQLITE process
		id=ipcGetPidByType(P_SQLITE);
		ipcLog("Send --> from:%d to:%d\n",pid,id);
		ipcSendMessage(pid, msqid, id, C_SQL, arg);
		ipcLog("Sent %s to pid:%d\n",arg,id);
		ipcLog("Waiting for Reply\n");

		for(;;)
		{
			// wait for reply or signal
			msg=ipcRecvMessage(msqid, pid);

			// point to sender's dictionary entry
			d=&ipc_dict[msg->slot];

			// compute slot number
			cslot=d-ipc_dict;

			ipcLog("Message from:%d  Slot:%d type:%s cmd: %s msg:%s\n"
			, msg->rsvp
			, cslot
			, d->stype
			, msg->scmd
			, msg->text
			);
			switch(msg->cmd)
			{
			case C_EOF :
				printf(">> EOF\n");
				return 0;

			case C_ACK :
				printf(">> %s\n",msg->text);
				break;
			}
		}
	}
}

int
ttyWrite(int cnt, BYTE *s)
{
int cc;
	if((cc=write(1,s,cnt))<cnt)
	{
		//Uh-Oh!
		ipcLog("Stdout Write Error: %s\n",strerror(errno));
	}
	return cc;
}

