#include "avr.h"

int 
  debug_level
;

static int 
  prog_cnt
, event_id
, prog_id[25]
, ipc_slot
;

char 
  **split(char *s, char k)
,  *libdir="/home/yun/lib";
;

void // signal handler
sigDeadChild(int sig)
{
int status; pid_t pid;
	if((pid=wait(&status))>0)
	{
		ipcLog("Child %d is Dead\n",pid);
		ipcSlotClear(pid);
	}

	// reset the signal
	signal(SIGCLD,sigDeadChild);
}

void // signal handler
sigTerminate(int sig)
{
int status, slot, kflag;
pid_t pid;

	ipcLog("Init Proc %d Terminiate Requestd! SIG: %s\n",getpid(),ipcSigName(sig));

	// kill all processes in the application group, 
	// 0 is me, so start at slot number 1
	for(kflag=slot=1; slot<MAX_IPC; slot++)
	{
		// skip empty slots
		if(!ipc_dict[slot].pid) continue;
		kflag++;

		pid=ipc_dict[slot].pid;

		ipcLog("Killing Child Process %d in Ipc Slot %d\n",pid,slot);

		// kill and wait to prevent zombie process
		kill(pid,SIGTERM); 

		if((pid=waitpid(pid,&status,WNOHANG))>0)
		{
			ipcLog("Child %d is Dead\n",pid);
		}
	}
	if(!kflag)
	{
		ipcLog("No Slots Occupied by Sub-Processes\n");
	}

	ipcLog("Freeing Shared Memory segment\n");
	shfree();

	ipcLog("Removing Message Queue, id=%d\n",msqid);
	rm_queue(msqid);

	// we should be the last one standing.
	ipcExit(0,0);
}

main(char **ac, int av)
{
char **aa;
char ib[128], fn[41];

FILE *fp;
MSG_BUF *msg;
int pid=getpid();

	logOpen("init");

	ipcLog("Starting Avr Application Group!\n");

	/* divorce ourselves from the process
	** group that we started in so that
	** we are immune to death if the
	** process group leader dies
	** see daemon.c
	*/
	daemonize();

	ipc_slot=getSharedMemory(P_ROOT,"/tmp/ipc_application");
	ipcLog("Shared Memory at %x slot=%d\n",ipc_dict,ipc_slot);

	ipc_dict[ipc_slot].pid=pid;
	ipc_dict[ipc_slot].type=P_ROOT;

	msqid=getMessageQueue(P_ROOT,"/tmp/ipc_application");

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
			aa=split(ib,' ');

			startAvr(aa,&prog_id[prog_cnt++]);
		}
		fclose(fp);
	}

	// set signal handlers
	signal(SIGTERM,sigTerminate);
	signal(SIGCLD ,sigDeadChild);

	for(;;) // until we get SIGTERM
	{
		// wait for messages or signals
		msg=recvMessage(msqid, pid);
		ipcLog("Message from %d [%s] \n",msg->rsvp,msg->text);
	}
}

startAvr(char **a, int *id)
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
	ipcLog("Starting up [%s]\n",wb);

	ipcLog("PROG: %s\n",prog);

	for(i=0; a[i]; i++)
	{
		ipcLog("ARG: %s\n",a[i]);
	}

	switch((*id=fk=fork()))
	{
		case -1:
			ipcLog("Fork Failed !! THE END !\n"); 
			break;

		case 0:
			execv(prog,a);

			// if we get here, execv failed
			ipcLog("Can't execute [%s] Error %d\n",prog,errno);

			// kill child just in case
			kill(fk,SIGTERM);

		default:
			break;
	}

	/* we may be called back, so
	** let things stabilize a bit
	*/
	sleep(1);
}

char ** // split string into an array
split(char *s, char k)
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
