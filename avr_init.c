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
checkDeath()
{
int x,id;
	id=wait(&x);
	ipcLog("Child process %d died child exit code=%d\n",id,x);

	// reset the signal
	signal(SIGCLD,checkDeath);
}

void // signal handler
timeout()
{
	signal(SIGALRM,timeout);
	alarm(10);
}

void // signal handler
ipcTerminate()
{
int i, cpid;

	ipcLog("Terminiate Request! SIGTERM\n");
	ipcLog("Killing chldren\n");

	/* kill all the other processes in the
	** application group, 0 is us, start at 1
	*/
	for(i=1; i<MAX_IPC; i++)
	{
		// skip empty slots
		if(!ipc_dict[i].pid) continue;

		cpid=ipc_dict[i].pid;

		ipcLog("Killing pid %d in slot %d\n",cpid,i);
		kill((short)cpid,SIGTERM);
	}
	/* give the children time to clean up before
	** we remove the shared memory segment in
	** usrExit()
	*/
	sleep(3);

	ipcLog("Freeing Shared Memory segment\n");
	shfree();

	ipcLog("Removing Message Queue, id=%d\n",msqid);
	rm_queue(msqid);
	usrExit(0,SIGTERM);
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
	ipcLog("Got memory at %x slot=%d\n",ipc_dict,ipc_slot);

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
	signal(SIGINT ,SIG_IGN);
	signal(SIGTERM,ipcTerminate);
	signal(SIGCLD ,checkDeath);
	signal(SIGALRM,timeout);

	for(;;) // until we get SIGTERM
	{
		/* wait for messages or signals */
		msg=recvMessage(msqid, pid);
		ipcLog("Got Message from %d [%s] \n",msg->rsvp,msg->text);
	}
}

startAvr(char **a, int *id)
{
int i,l,fk,qsize;
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
			kill((short)fk,SIGTERM);

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
