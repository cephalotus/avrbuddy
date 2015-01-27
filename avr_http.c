#include "avr.h"
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>

static char
  *time_string(long t)
, *makeBody(int *len)
, *getAvrStats(int *len)
, *makeHeader(int bodylen, char *timestr, int *hdrlen)
;

static char 
  buf[2048]
, *host_arg
;
static pid_t
  ppid
, cpid
, pid
;
static int
  cmd_opt
, port_arg
, slot
;
void // signal handler
sigDeadChild(int sig)
{
int status; pid_t cpid;
	if((cpid=waitpid(cpid,&status,WNOHANG))>0)
	{
		ipcLog("Child %d is Dead\n",cpid);
		ipcClearSlot(cpid);
	}

	// reset the signal
	signal(SIGCLD,sigDeadChild);
}

void // signal handler
httpTerminate(int sig)
{
int x,id;
	ipcLog("Proc %d TTY Terminiate Requested SIG: %s!\n",getpid(),ipcSigName(sig));
	if(cpid)
	{
		ipcLog("Killing cpid %d\n",cpid);
		kill(cpid,SIGTERM);
		sigDeadChild(15);
	}
	ipcExit(pid,0,0);
}

main(int argc,char** argv)
{
int reuse_addr = 1;  /* Used so we can re-bind to our port */
int sock, nk, cid;
int i, cc;

struct sockaddr_in addr;
struct sockaddr_in ss;
struct sockaddr_in new_socket;

static struct sockaddr cli_addr ;
socklen_t clilen=sizeof(cli_addr);
unsigned int len, addr_len;
char ib[80], *libdir, *s, tb[128];
FILE *fp;
MSG_BUF *msg;
IPC_DICT *d;

	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,httpTerminate);

	ppid=getppid(); 
	pid=getpid();

	/*set up application logging */
	logOpen(basename("http"));

	ipcLog("Starting P_HTTP process!\n");


	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_TTY,"/tmp/ipc_application");
	d=&ipc_dict[slot];
	msqid=ipcGetMessageQueue(pid,P_TTY,"/tmp/ipc_application");
	ipcNotify(pid,msqid);
	ipcLog("TTY Process %d On-Line in slot %d\n",pid,slot);

	// set host and port for http requests
	host_arg="localhost";
	port_arg=8100;

	signal(SIGTERM,httpTerminate);
   
	if((sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))<=0)
	{
		ipcLog("tcpio: can't open stream socket error: %s\n",strerror(errno));
	}

	/* So that we can re-bind to it without TIME_WAIT problems */
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&reuse_addr,sizeof(reuse_addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port_arg);

	if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))<0)
	{
		ipcLog("LN: %d server: can't bind local address error %s\n",__LINE__,strerror(errno));
		close(sock);
		httpTerminate(0);
	}

	listen(sock,1);

	signal(SIGCLD,sigDeadChild);

	for(;;)
	{ /*  Wait for a client to connect  */

		/* args: int, struct sockaddr* , int 
		** get a new socket handle for child to use
		*/
		if((nk=accept(sock,&cli_addr,&clilen))<0)
		{
			if(errno==EINTR) continue;
			httpTerminate(EINTR);
		}

		/* 
		 * we have a new connection
		 * fork a child to handle it
		 */
		switch(cid=fork())
		{
		case -1 :			/* un-forkable */
			ipcLog("server: fork error");
			httpTerminate(0);

		case 0: 			/* child process */
			close(sock);
			tcpio(nk);
			break;

		default :           /* parent process */
			close(nk);		
			cpid=cid;
			break;
		}
	}
}


/*
** child runs thiis process
*/
tcpio(int sk)
{
int len, hdrlen, bodylen, tnsk, cc, cid;
register int i, e;
long now;
struct tm *tm;
unsigned char etx[]={ 0x0a, 0x0d, 0x0a, 0x00 };
unsigned char *eot="\r\n";
unsigned char end[4];
unsigned char *reply;
unsigned char c;
char buf[8192];
char get[1024];
char *body;
char *header;
char timestr[128];
char *weekday[]={"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL };
char *months[]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL };

	/* read from the tcp socket */
	for(;;)
	{
		i=e=0;
		get[0]='\0';
		while((cc=read(sk,&c,1))==1) 
		{
			//need 0a, 0d, 0a to end
			if(c=='\r' || c=='\n')
			{
				end[e++]=c;
				if(e==3)
				{
					break;
				}
				buf[i]='\0';
				//fprintf(stderr,"<<[%02x]\n",c);
				if(i)
				{
					//fprintf(stderr,"<< buf [%s]\n",buf);
					if(!memcmp(buf,"GET",3))
					{
					int y;
						strcpy(get,buf+5);
						for(y=0; y<strlen(get)-1; y++)
						{
							if(get[y]==' ')
							{
								get[y]='\0';
								break;
							}
						}
					}
				}
				i=0; continue;
			}
			e=0;
			if(c<' ')
			{
				//fprintf(stderr,"<<[%02x]\n",c);
				continue;
			}
			buf[i++]=c;
		}
		if(cc<0)
		{
			ipcLog("Fatal Error:%s\n",strerror(errno));
		}

		time(&now);
		tm=gmtime(&now);

		sprintf(timestr,"%s, %02d %s %04d %02d:%02d:%02d GMT"
		, weekday[tm->tm_wday]
		, tm->tm_wday
		, months[tm->tm_mon]
		, tm->tm_year+1900
		, tm->tm_hour
		, tm->tm_min
		, tm->tm_sec
		);

		//body=makeBody(&bodylen);
		body=getAvrStats(&bodylen);
		header=makeHeader(bodylen,timestr,&hdrlen);

		write(sk,header,hdrlen);
		write(sk,eot,2);
		write(sk,body,bodylen);
		ipcLog("Request GET=%s : Response: %d bytes\n",get,bodylen);
	}
	return;
}

char *
makeHeader(int bodylen, char *timestr, int *hdrlen)
{
	static char hdrBuff[2048];
	int len;
	char *s=hdrBuff;
	s+=sprintf(s,"HTTP/1.0 200 OK\r\n");
	s+=sprintf(s,"Date: %s\r\n",timestr);
	s+=sprintf(s,"Server: Avr Ajax Service\r\n");
	s+=sprintf(s,"Content-Type: text/XML\r\n");
	s+=sprintf(s,"Content-Length: %d\r\n",bodylen);
	s+=sprintf(s,"Last-Modified: %s\r\n",timestr);
	*hdrlen=strlen(hdrBuff);
	return(hdrBuff);
}

char *makeBody(int *len)
{
	static char bodyBuff[204800];
	*len=sprintf(bodyBuff,"<html> <body> <h1>This is a Test</h1> </body> </html>");
	return(bodyBuff);
}

void
gopts(char *av[])
{
register int i;
	for(i=1; av[i]; i++)
	{
		if(av[i][0]=='-')
		{
			switch(av[i][1])
			{
				case 'v' : 
					cmd_opt='v';
					break;

				case 'h' : 
					 host_arg=&av[i][2];
					break;

				case 'p' : 
					 port_arg=atoi(&av[i][2]);
					break;

				default :
					ipcLog("Invalid Option : %s\n",&av[i][2]);
					httpTerminate(-1);
			}
		}
	}
}

char *
getAvrStats(int *len)
{
int c,x=0;
int avr_cnt;
register int i;
int oos, spid; 
long now;
struct localtime *tm;
char *s1, *s2;
static char xmlbuf[204800];
char *xml=xmlbuf;
struct utsname uts;
IPC_DICT *d;


	// get host info
	uname(&uts);

	// current time
	time(&now);

	// count the avr_links 
	for(i=avr_cnt=0; i<MAX_IPC; i++){
		if(ipc_dict[i].pid==0) break;
		/*
		printf("slot %d pid %d type %d\n"
		, i
		, ipc_dict[i].pid
		, ipc_dict[i].type
		);
		*/
		avr_cnt++;
	}
	xml+=sprintf(xml,"<?xml version='1.0' encoding='utf-8' ?>\n");
	xml+=sprintf(xml,"<docRoot>\n");
	xml+=sprintf(xml,"<nodename>%s</nodename>\n",uts.nodename);
	xml+=sprintf(xml," <tick>%d</tick>\n", now);

	if(!avr_cnt)
	{
		xml+=sprintf(xml,"</docRoot>\n");
		httpTerminate(0);
	}

	xml+=sprintf(xml," <avrprocs>\n");
	for(i=0; i<MAX_IPC; i++)
	{
		if(ipc_dict[i].pid!=0) break;
		{
			d=&ipc_dict[i];
			xml+=sprintf(xml,"  <avrproc>\n");
			xml+=sprintf(xml,"   <ipcslot>%d</ipclot>\n",i);
			xml+=sprintf(xml,"   <procid>%d</procid>\n",ipc_dict[i].pid);
			xml+=sprintf(xml,"   <proctype>%d</proctype>\n",ipc_dict[i].type);
			xml+=sprintf(xml,"   <procname>%d</procname>\n",ipc_dict[i].stype);
			xml+=sprintf(xml,"  </avrproc>\n");
		}
	}
	xml+=sprintf(xml," </avrprocs>\n");
	xml+=sprintf(xml,"</docRoot>\n");
	*len=strlen(xmlbuf);
	return xmlbuf;
}

char *
time_string(long t)
{
static char rb[40];
if(!t) { return ""; }

struct tm *tm;
	tm=localtime(&t);
	sprintf(rb,"%02d:%02d:%02d"
	, tm->tm_hour
	, tm->tm_min
	, tm->tm_sec
	);
	return(rb);
}
