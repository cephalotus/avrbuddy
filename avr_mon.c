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
	ipcLog("Proc %d P_MON Terminiate Requested SIG: %s!\n",getpid(),ipcSigName(sig));
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
	logOpen("mon");

	ipcLog("Starting P_MON process!\n");

	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_MON,"/tmp/ipc_application");
	//ipcLog("Got memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=ipcGetMessageQueue(pid,P_MON,"/tmp/ipc_application");

	// notify root process we are here
	ipcNotify(pid,msqid);

	ipcLog("P_MON Process %d On-Line in slot %d\n"
	, pid
	, slot
	);


	ipcLog("CP: P_MON I am Child Pid=%d\n",cpid);
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

		printf("   Welcome To Avrbuddy!\n");
		printf("type help for command list\n\n");
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
			else if(!strcmp(cmd,"help")) 	doHelpCommand();
			else if(!strcmp(cmd,"sql")) 	doMessageCommand(C_SQL,arg);
			else if(!strcmp(cmd,"ps")) 		doPsCommand();
			else if(!strcmp(cmd,"sys")) 	doMessageCommand(C_SYS,arg);
			else if(!strcmp(cmd,"kill")) 	doKillCommand(arg);
			else if(!strcmp(cmd,"start")) 	doStartCommand(arg);
			else printf("%s\n",buf);
		}
		// we should put some code in here
		// to implement protocol for avr sketches
		//ipcDebug(50,"<<[%02d]< %02x\n",i,buf[i]);

		ipcLog("CP: buf[%02d]< %02x\n",i,buf[i]);
	}
}

int
doKillCommand(char *str)
{
int slot=atoi(str);
IPC_DICT *d=&ipc_dict[slot];

	if(slot==0)
	{
		printf("You are killing the whole process group including yourself\n");
		printf("Goodbye!\n");
	}
	else
	{
		printf("killing pid %d\n",d->pid);
	}
	kill(d->pid,SIGTERM);
}

int
doHelpCommand()
{
    printf("help             Print this list.\n");
    printf("ps               show avrbuddy processes.\n");
    printf("sql   statement  Submit and sql statement to P_SQLITE.\n");
    printf("sys   command    Submit a shell command to P_SYSTEM.\n");
    printf("start prog_name  Start an application member i.e start avr_sqlite.\n");
    printf("kill  slot       Kill process in slot from ps.\n");
    printf("                 -- Killing slot 0 kills the whole application group.\n");
    printf("                 -- includding us ;-)\n");
}

doStartCommand(char *prog)
{
char buf[512];
	sprintf(buf,"%s&",prog);
	printf("Starting up %s\n",prog);
	system(buf);
}

int
doPsCommand()
{
IPC_DICT *d;
int i;
	printf("%-10s%-10s%s\n","Slot","Type","Pid");
	printf("%-10s%-10s%s\n","----","--------","-----");
	for(i=0; i<MAX_IPC; i++)
	{
		d=&ipc_dict[i];
		if(!d->pid) continue;
		printf("%-10d%-10s%d\n",i,d->stype,d->pid);
	}
	return;
}

int
doMessageCommand(int cmd, char *arg)
{
IPC_DICT *d;
MSG_BUF *msg;
int n, addr, cslot;
char com_buf[256];
	switch(cmd)
	{
	case C_SQL:
		// send a message to the P_SQLITE process
		addr=ipcGetPidByType(P_SQLITE);
		ipcLog("Send --> from:%d to:%d\n",pid,addr);
		ipcSendMessage(pid, msqid, addr, cmd, arg);
		ipcLog("Sent %s to pid:%d\n",arg,addr);
		ipcLog("Waiting for Reply\n");

		for(;;)
		{
			// wait for reply or signal
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
			case C_EOF :
				printf(">> EOF\n");
				return 0;

			case C_ACK :
				printf(">> %s\n",msg->text);
				break;
			}
		}
		break;
		
	case C_SYS:
		// send a message to the P_SYSTEM process
		addr=ipcGetPidByType(P_SYSTEM);
		ipcLog("Send --> from:%d to:%d\n",pid,addr);
		ipcSendMessage(pid, msqid, addr, cmd, arg);
		ipcLog("Sent %s to pid:%d\n",arg,addr);
		ipcLog("Waiting for Reply\n");

		for(;;)
		{
			// wait for reply or signal
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
