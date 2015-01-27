#include "avr.h"
#include <termio.h>
#include <fcntl.h>

/* values that are set to defaults
** can be changed with command line
** options
*/
static int 
  speed  = B576000
, csize  = CS8 //N81
, parity = 0
, vmin   = 1
, vtime  = 10
, stops  = 1
, fd
, avr_test
, avr_dlev
, avrWrite(int, int, BYTE *)
;

pid_t
  pid
, ppid
, cpid
, slot
;

static IPC_DICT *dict;

static char
  *device = "ttyATH0"
, *ofile  = "/home/yun/log/ttyATH0.out"
, *hexString()
, errmsg[512]
;
static BYTE
  com_buf[256]
, sav_buf[256]
, *read_hil()
, getSum()
;

static struct termio tty;

static char
  *libdir
, fn[40]
, ib[80]
;

void // signal handler
ttyTerminate(int sig)
{
int x,id;
	ipcLog("Proc %d TTY Terminiate Requested SIG: %s!\n",getpid(),ipcSigName(sig));
	if(cpid)
	{
		ipcLog("Killing cpid %d\n",cpid);
		kill(cpid,SIGTERM);

		// don't leave a zombie
		id=wait(&x);

		ipcLog("Child Process %d died With exit code=%d\n",id,x);
	}
	ipcExit(pid,0,0);
}

main(int argc, char *argv[])
{
char *s;
int i, cc;
char tb[128];
FILE *fp;
MSG_BUF *msg;

	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,ttyTerminate);

	ppid=getppid(); 
	pid=getpid();

	/*set up application logging */
	logOpen(basename(device));

	ipcLog("Starting P_TTY process!\n");


	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_TTY,"/tmp/ipc_application");
	//ipcLog("Got Memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=ipcGetMessageQueue(pid,P_TTY,"/tmp/ipc_application");

	/* process command line options */
	if(getOptions(argv)<0)
	{
		sprintf(tb,"Process %d Slot %d option error"
		, pid
		, slot
		);
		ipcLog("Processing Cannot Continue, Waiting to be killed...\n");
		// send message to ROOT
		ipcSendMessage(pid,msqid,ipc_dict[0].pid,C_FAIL,tb);
		// wait for ROOT to kill us
		pause();
	}

	// tell root process we are here
	ipcNotify(pid,msqid);

	ipcLog("TTY Process %d On-Line in slot %d\n"
	, pid
	, slot
	);

	/* open the comm port */
	if((fd=devOpen(device))<0)
	{
		char buf[81];
		ipcLog("Can't open %s Error: %s\n",device,strerror(errno));
		sprintf(buf,"Process %d Slot %d %s"
		, pid
		, slot
		, strerror(errno)
		);

		ipcLog("Processing Cannot Continue, Waiting to be killed...\n");
		// send message to ROOT
		ipcSendMessage(pid,msqid,ipc_dict[0].pid,C_FAIL,buf);
		// wait for ROOT to kill us
		pause();
	}

	ipcLog("Successfully Opened TTY port=%s speed=%d dlev:%d\n"
	, device
	, getSpeed(speed)
	, avr_dlev);

	/* point to our library directory */
	libdir="/tmp";

	cc=0;
	/* ask for heartbeat early on */
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

	/* we are now a parent process with a child on the tty receive
	** our job now is to listen to the message queue for any further
	** instructions that would have us transmit to the tty
	*/
	for(;;)
	{
		/* wait for messages or signals */
		msg=ipcRecvMessage(msqid, pid);
		ipcLog("Got Message from %d [%s] \n",msg->rsvp,msg->text);
	}
}

int
avrWrite(int fd, int cnt, BYTE *s)
{
int cc;
	if((cc=write(fd,s,cnt))<cnt)
	{
		//Uh-Oh!
		ipcLog("Serial Write Error: %s\n",strerror(errno));
	}
	return cc;
}

beChildProcess()
{
int cnt, c, x, i;
char buf[1024];

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

		// read 1 byte
		if((c=read(fd,&buf[i],1))<0)
		{
			if(errno==EINTR) { i--; continue; }
			//Uh-Oh!
			ipcLog("Serial Read Error: %s\n",strerror(errno));

			// send kill signal to parent process
			kill(ppid,SIGTERM);
			ipcExit(pid,errno,0);
		}

		if(x!=1)
		{
			if(x>=0) ipcLog("wanted 1 saw %d\n",x);
			i--;
			continue;
		}

		// we have a character
		switch(buf[i])
		{
		case '\n':	//end of line?
			buf[i]='\0';
			i=-1; // for() will increment it to 0
			ipcLog("Serial Read Line: [%s]\n",buf);
			break;

		}
		// we should put some code in here
		// to implement protocol for avr sketches
		//ipcDebug(50,"<<[%02d]< %02x\n",i,buf[i]);

		ipcLog("<<[%02d]< %02x\n",i,buf[i]);
	}
}

getSpeed(int baud)
{
	switch(baud)
	{
	case B300		: return(300);
	case B600		: return(600);
	case B1200		: return(1200);
	case B1800		: return(1800);
	case B2400		: return(2400);
	case B4800		: return(4800);
	case B9600		: return(9600);
	case B19200		: return(19200);
	case B38400		: return(38400);
	case B57600		: return(57600);
	case B115200	: return(115200);
	case B230400	: return(230400);
	case B460800	: return(460800);
	case B500000	: return(500000);
	case B576000	: return(576000);
	case B921600	: return(921600);
	case B1000000	: return(1000000);
	case B1152000	: return(1152000);
	case B1500000	: return(1500000);
	case B2000000	: return(2000000);
	case B2500000	: return(2500000);
	case B3000000	: return(3000000);
	case B3500000	: return(3500000);
	case B4000000	: return(4000000);
	}
}

devOpen(char *device)
{
int fd;
char dev[80];
extern int errno;

	if(isatty(0) && ioctl(0,TCGETA,&tty))
	{
		ipcLog("Fcntl Error %d\n",errno);
		return -1;
	}

	if((fd=open(device,O_RDWR|O_NDELAY))<0)
	{
		return -1;
	}

	/* set control flags  use RTS/CTS flow */
	tty.c_iflag = IGNPAR|IGNBRK;
	tty.c_lflag = tty.c_oflag = 0;
	tty.c_cflag = speed|csize|stops|parity|CREAD|CLOCAL;
	tty.c_cc[VMIN] = vmin;
	tty.c_cc[VTIME] = vtime;

	if(ioctl(fd,TCSETAW,&tty))
	{
		ipcLog("Fcntl Error %d\n",errno);
		ipcLog("Line %d Fcntl Error %d\n",__LINE__,errno);
	}

	// Ensure blocking reads
	if(fcntl(fd,F_SETFL,(fcntl(fd,F_GETFL)&~O_NDELAY),0))
	{
		ipcLog("Line %d Fcntl Error %d\n",__LINE__,errno);
		return -1;
	}
	return(fd);
}

int
getOptions(char **av)
{
register int i;
char *opt;

	for(i=1; av[i]; i++)
	{
		if(av[i][0]=='-')
		{
			opt=av[i];
			switch(av[i][1])
			{
			case 'T':
				/* test flag */
				avr_test++;
				break;

			case 'd':
				/* debug level */
				avr_dlev=atoi(&av[i][2]);
				break;

			case 's':
				switch(atoi(&av[i][2]))
				{
				case 300		: speed = B300;		break;
				case 600		: speed = B600;		break;
				case 1200		: speed = B1200;	break;
				case 1800		: speed = B1800;	break;
				case 2400		: speed = B2400;	break;
				case 4800		: speed = B4800;	break;
				case 9600		: speed = B9600;	break;
				case 19200		: speed = B19200;	break;
				case 38400		: speed = B38400;	break;
				case 57600		: speed = B57600;	break;
				case 115200		: speed = B115200;	break;
				case 230400		: speed = B230400;	break;
				case 460800		: speed = B460800;	break;
				case 500000		: speed = B500000;	break;
				case 576000		: speed = B576000;	break;
				case 921600		: speed = B921600;	break;
				case 1000000	: speed = B1000000;	break;
				case 1152000	: speed = B1152000;	break;
				case 1500000	: speed = B1500000;	break;
				case 2000000	: speed = B2000000;	break;
				case 2500000	: speed = B2500000;	break;
				case 3000000	: speed = B3000000;	break;
				case 3500000	: speed = B3500000;	break;
				case 4000000	: speed = B4000000;	break;
				default   		: 
					ipcLog("%s not a legal speed\n",&av[i][2]);
					ipcExit(pid,0,0);
				}
				break;

			case 'c':
				switch(atoi(&av[i][2]))
				{
					case 7 : csize = CS7;
					case 8 : csize = CS8;
				}
				break;

			case 'p':
				if(av[i][2]=='e')
					parity=PARENB;
				else if(av[i][2]=='o')
					parity=PARENB|PARODD;
				break;

			case 'o':
				ofile=&av[i][2];
				break;

			case 'v':
				device=&av[i][2];
				break;

			case 'm':
				vmin=atoi(&av[i][2]);
				break;

			case 't':
				vtime=atoi(&av[i][2]);
				break;

			default:
				ipcLog("%s not a valid option \n",opt);
				return -1;
			}
		}
	}
	return 0;
}

char *
hexString(BYTE *s, int n)
{
static char tb[256];
int l;
register int i;
	if(n>255)
	{
		sprintf(tb,"STRING LEN %d ???\n",n);
		return(tb);
	}
	l=sprintf(tb,"[ ");
	for(i=0; i<n; i++) l+=sprintf(tb+l,"%02x ",s[i]);
	sprintf(tb+l,"]");
	return(tb);
}
