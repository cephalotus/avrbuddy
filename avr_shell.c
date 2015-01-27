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
char *s, buf[81];
FILE *fp;
MSG_BUF *msg;

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

	switch(cpid=fork())
	{
		case -1:
			ipcLog("Can't fork error: %s\n",strerror(errno));
			ipcExit(pid,errno,0);

		case 0:
			break;

		default:
			beChildProcess();
			break;
	}

	// we are now a parent process with a child on the tty receive
	// our job now is to listen to the message queue for any further
	// instructions that would have us transmit to the tty
	//
	ipcLog("PP: SHELL I am Parent Pid=%d\n",pid);
	for(;;)
	{
		/* wait for messages or signals */
		msg=ipcRecvMessage(msqid, pid);
		ipcLog("PP: Got Message from %d [%s] \n",msg->rsvp,msg->text);
	}
}

beChildProcess()
{
int cnt, c, x, i;
char buf[1024];

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
			if(!strcmp(buf,"exit")) 
			{
				printf("Goodbye!\n");
				shellTerminate(0);
			}
			else if(!strcmp(buf,"msg")) 
				doMessageCommand();
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
doMessageCommand()
{
IPC_DICT *d;
int ix=0;
char buf[81];
	printf("root: %d\n",ipc_head->proot);
	printf("txmsg: %d\n",ipc_head->txmsg);
	printf("rxmsg: %d\n\n",ipc_head->rxmsg);

	printf("--Choices--\n");
	for(ix=0; ix<MAX_IPC; ix++)
	{
		if(!ipc_dict[ix].pid) continue;
		d=&ipc_dict[ix];
		printf("%d) %d\t%s\n",ix+1,d->pid,d->stype);
	}
	printf("Choose: ");
	fflush(stdout);
	fgets(buf,sizeof(buf),stdin);
	buf[strlen(buf)-1]='\0';
	ix=atoi(buf);
	if(ix>0&&ix<MAX_IPC)
	{
		printf("You chose pid %d\n",ipc_dict[ix-1].pid);
		printf("Message: ");
		fgets(buf,sizeof(buf),stdin);
		buf[strlen(buf)-1]='\0';
		printf("You Entered: %s\n",buf);
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

