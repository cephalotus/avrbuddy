#include "avr.h"
#include <termio.h>
#include <fcntl.h>

static int
  speed
, vmin
, vtime
, csize
, stops
, parity
, fd
, pid
, ppid
, cpid
, slot
, avr_test
, avr_dlev
, sav_cnt
;
static IPC_DICT 
  *dict
;
extern int
  errno
;
static char
  *device
, *ofile
, *hexString()
, errmsg[512]
;
static BYTE
  com_buf[256]
, sav_buf[256]
, *read_hil()
, getSum()
;
static struct termio
  tty
;
static char
  *libdir
, fn[40]
, ib[80]
;
void // signal handler
ttyTerminate()
{
	ipcLog("TTY Terminiate Request!\n");
	usrExit(0,SIGTERM);
}

main(int argc, char *argv[])
{
char *s;
int cc, loop_flag;
register int i;
char tb[128];
FILE *fp;

	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,ttyTerminate);

	/* process command line options */
	getOptions(argv); 
	ppid=getppid(); 
	pid=getpid();

	/*set up application logging */
	logOpen(device);

	ipcLog("Starting P_AVR process!\n");

	/* pick up shared memory environment */
	slot=getSharedMemory(P_AVR,"/tmp/ipc_application");
	ipcLog("Got memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=getMessageQueue(P_AVR,"/tmp/ipc_application");

	strcpy(dict->tty,device);
	time(&dict->online);

	ipcLog("AVR Process %d On-Line in slot %d\n"
	, pid
	, slot
	);

	/* open the comm port */
	if((fd=devOpen(device))<0)
	{
		ipcLog("bad open err %d on %s\n",errno, device);
		usrExit(0,SIGTERM);
	}

	ipcLog("AVR port=%s speed=%d dlev:%d\n"
	, device
	, getSpeed(speed)
	, avr_dlev);

	/* point to our library directory */
	libdir="/tmp";

	cc=0;
	/* ask for heartbeat early on */
	avr_receive();
	usrExit(0,SIGTERM);
}

avr_receive()
{
int cnt;
	/* read a AVR Message */
	while(read_hil(fd,&cnt,com_buf)==NULL) 
	;;
	return cnt;
}

BYTE *
read_hil(int fd, int *cnt, BYTE *ib)
{
int i;
int x;

	for(i=0; i<255 ; i++)
	{
		if((x=read(fd,&ib[i],1))<0)
		{
			return(NULL);
		}
		if(x!=1)
		{
			if(x>=0) ipcLog("wanted 1 saw %d\n",x);
			i--;
			continue;
		}
		ipcDebug(50,"<<[%02d]< %02x\n",i,ib[i]);
	}
	*cnt=i;
	return(ib);
}

getSpeed(int n)
{
	switch(n){
	case B300:   return(300);
	case B1200:  return(1200);
	case B2400:  return(2400);
	case B4800:  return(4800);
	case B9600:  return(9600);
	case B19200: return(19200);
	}
}

devOpen(char *port)
{
int fd;
char dev[80];
extern int errno;

	sprintf(dev,"/dev/%s",port);

	if(isatty(0) && ioctl(0,TCGETA,&tty))
	{
		ipcLog("Fcntl Error %d\n",errno);
		usrExit(errno,SIGTERM);
	}

	if((fd=open(dev, O_RDWR|O_NDELAY))<0)
	{
		ipcLog("Can't open port %s errno:%d", dev,errno);
		exit(0);
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
		usrExit(errno,SIGTERM);
	}
	if(fcntl(fd,F_SETFL,(fcntl(fd,F_GETFL)&~O_NDELAY),0))
	{
		ipcLog("Line %d Fcntl Error %d\n",__LINE__,errno);
		usrExit(errno,SIGTERM);
	}
	return(fd);
}

getOptions(char **av)
{
register int i;
	/* set hil sio defaults */
	speed  = B9600;
	csize  = CS8;
	parity = 0;
	vmin   = 1;
	vtime  = 10;
	device = "ttya";

	for(i=1; av[i]; i++)
	{
		if(av[i][0]=='-')
		{
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
				case 1200 : speed = B1200; break;
				case 2400 : speed = B2400; break;
				case 4800 : speed = B4800; break;
				case 9600 : speed = B9600; break;
				default   : 
					ipcLog("%s not a legal speed\n",&av[i][2]);
					usrExit(0,SIGTERM);
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
				ofile = &av[i][2];
				break;

			case 'v':
				device = &av[i][2];
				break;

			case 'm':
				vmin = atoi(&av[i][2]);
				break;

			case 't':
				vtime = atoi(&av[i][2]);
				break;
			}
		}
	}
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

avrWrite(int hd, int n, BYTE *s)
{
	write(hd,s,n);
}
