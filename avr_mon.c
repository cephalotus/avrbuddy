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

void // signal handler
shellTimeout(int sig)
{
	ipcLog("P_MON SIG: %s!\n",ipcSigName(sig));
	printf("Request Timed Out!\n");
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

		system("tput clear");
		printf("\n\n\n\n\n\n");
		printf("    Welcome to Stephen Heggood's avrbuddy application!\n\n");
		printf("    Type help for command list.\n\n");
		printf("    Most shell commands will work from prompt (vi will not).\n\n");
		printf("    If you want shell commands as seen by co-proccess using avr_system,\n");
		printf("    you must prepend the sys command i.e mon> sys date\n\n");
		printf("    sys <command> relies on avr_system process\n");
		printf("    sql <command> relies on avr_sqlite process\n");
		printf("    ps  to ensure these are running\n\n");


		for(;;)
		{
			printf("<mon>$ ");
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
			else if(!strcmp(cmd,"log")) 	doLogCommand(arg);
			else if(!strcmp(cmd,"txt")) 	doTxtCommand(arg);
			else 
			{
				char tb[1024];
				sprintf(tb,"%s %s",cmd,arg);
				//printf("Trying shell command %s\n",tb);
				doMessageCommand(C_SYS,tb);
			}
		}
		// we should put some code in here
		// to implement protocol for avr sketches
		//ipcDebug(50,"<<[%02d]< %02x\n",i,buf[i]);

		ipcLog("CP: buf[%02d]< %02x\n",i,buf[i]);
	}
}

doTxtCommand()
{
int i;
	for(i=0; i<MAX_TEXT; i++)
	{
		if(!ipc_text[i].from_pid) continue;
		printf("pid:%d->%d  txt:%s\n"
		, ipc_text[i].from_pid
		, ipc_text[i].to_pid
		, ipc_text[i].text
		);
	}
}

int
doLogCommand(char *arg)
{
FILE *pp;
char buf[1024];
	if(!*arg)
	{
		if((pp=popen("/bin/ls /home/yun/log","r"))==NULL)
		{
			printf("popen error: %s\n",strerror(errno));
			return errno;
		}
		while(fgets(buf, sizeof(buf), pp)!=NULL)
		{
			printf("%s",buf);
		}
		fclose(pp);
		return 0;
	}
	sprintf(buf,"/bin/cat /home/yun/log/%s",arg);

	//fprintf(stderr,"popen(%s)\n",buf);

	if((pp=popen(buf,"r"))==NULL)
	{
		printf("popen error: %s\n",strerror(errno));
		return errno;
	}
	while(fgets(buf, sizeof(buf), pp)!=NULL)
	{
		printf("%s",buf);
	}
	fclose(pp);
	return 0;
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
	switch(cmd)
	{
	case C_SQL: getMessageReply(P_SQLITE,cmd,arg);
		break;

	case C_SYS: getMessageReply(P_SYSTEM,cmd,arg);
		break;
	}
}

int
getMessageReply(int mtype, int cmd, char *arg)
{
IPC_DICT *d;
MSG_BUF *msg;
IPC_TEXT *txt;
int n, addr, cslot;
char com_buf[256];

	addr=ipcGetPidByType(mtype);

	ipcLog("Send --> from:%d to:%d\n",pid,addr);

	ipcSendMessage(pid, msqid, addr, cmd, arg);

	ipcLog("Sent %s to pid:%d\n",arg,addr);

	ipcLog("Waiting for Reply\n");

	signal(SIGALRM,shellTimeout); alarm(2);

	// wait for reply or signal
	msg=ipcRecvMessage(msqid, pid);

	alarm(0);

	// point to sender's dictionary entry
	d=&ipc_dict[msg->slot];


	ipcLog("Reply From: %d  Slot: %d type: %s cmd: %s [%s]\n"
	, msg->rsvp
	, msg->slot
	, d->stype
	, msg->scmd
	, msg->text
	);
	switch(msg->cmd)
	{
	case C_ERR :
		printf("%s\n",msg->text);
		return -1;

	case C_EOF :
		//printf("EOF\n");
		while((txt=ipcGetText(msg->rsvp,pid))!=NULL)
		{
			printf("%s\n",txt->text);
			/*
			ipcLog("%d->%d  %s\n"
			, txt->from_pid
			, txt->to_pid
			, txt->text
			);
			*/

			// release the text slot
			txt->from_pid=txt->to_pid=0;
		}
		return 0;

	case C_ACK :
		printf("%s\n",msg->text);
		break;
	}
	return 0;
}
