#include "avr.h"
#include "time.h"

int 
  ipc_dlev
, ipc_dup
;
static FILE 
  *fp
;
static 
  struct tm *tm
;
static 
  int dlast
;
static 
  long now
;
static char 
   logname[128]
,  logbase[128]
, *logdir="/home/yun/log"
; 

void
makeLogBase(char *proc)
{
	// logbase used for future logname.time
	sprintf(logbase,"%s/%s",logdir,proc);
}

int
newLog()
{
	time(&now); 
	tm=localtime(&now);

	// save for log rotation
	dlast=tm->tm_mday;

	sprintf(logname,"%s.%02d%02d"
	,logbase
	,tm->tm_mon+1
	,tm->tm_mday
	);

	logClose(); /* close any old log */

	umask(0);

	if((fp=fopen(logname,"a"))==NULL)
	{
		fp=stderr;
		fprintf(fp,"Can't open log file %s -- using stderr\n",logname);
		return(errno);
	}
	return 0;
}
 
logOpen(char *proc)
{
int fd;

	makeLogBase(proc);
	newLog();

	// get file descriptor
	fd=fileno(fp);

	// sometimes we want to do this
	// when using pipes between processes
	if(ipc_dup && fp != stderr)
	{
		close(1); dup(fd);
		close(2); dup(fd);
	}
	return(fd);
}

void
logClose()
{
	if(fp && fp != stderr) fclose(fp);
	fp=NULL;
}

void
ipcFatalExit(pid_t pid, const char*format,...)
{
va_list args;
char buf[256];
	va_start(args,format); 
	vsprintf(buf,format,args);
	va_end(args);

	// send message to ROOT
	ipcLog("ERROR_WAIT: ipcSendMessage(pid=%d, msqid=%d,ipc_dict[0].pid=%d,C_FAIL,buf=%s)\n"
	, pid
	, msqid
	, ipc_dict[0].pid
	, buf
	);

	ipcSendMessage(pid,msqid,ipc_dict[0].pid,C_FAIL,buf);
	// wait to be killed
	pause();
}

int
ipcDebug(int lev, const char*format,...)
{
va_list args;
char tb[256];
	if(ipc_dlev<lev) return(0);
	if(!fp) return(0);
	va_start(args,format); 
	vsprintf(tb,format,args);
	va_end(args);
	ipcLog("%s",tb);
}

int
ipcDetail(const char* format, ...)
{
va_list args;
char tb[256];

	va_start(args,format); 
	vsprintf(tb,format,args);
	va_end(args);

	ipcLog("%s",tb);
}

int
ipcLog(const char *format, ...)
{
va_list args;
char tb[256];
struct tm *tm;
long now;

	time(&now), tm=localtime(&now);
	ipcValidateLog(tm);

	/* time logs */
	fprintf(fp,"%02d:%02d:%02d "
	, tm->tm_hour
	, tm->tm_min
	, tm->tm_sec
	);

	va_start(args,format); 
	vfprintf(fp,format,args);
	va_end(args);
	fflush(fp);
}

int
ipcRawLog(const char *format, ...)
{
va_list args;
char tb[256];
struct tm *tm;
long now;

	time(&now), tm=localtime(&now);
	ipcValidateLog(tm);

	va_start(args,format); 
	vfprintf(fp,format,args);
	va_end(args);
	fflush(fp);
}

int
ipcValidateLog(struct tm *tm)
{
char tb[256];

	if(!fp)
	{
		sprintf(tb,"unk_%d",getpid());
		logOpen(tb);
	}
	if(!fp) return(0);

	if(dlast != tm->tm_mday)
	{
		newLog();
	}
}

