#include "avr.h"

extern int errno;
void sysError(const char *fmt,...);

daemonize()
{
int childpid, fd;
	if(getppid()==1) goto out;

	/*
	 * If we were not started in the background, fork and
	 * let the parent exit.  This also guarantees the first child
	 * is not a process group leader.
	 */

	if((childpid=fork()) < 0) 
		sysError("can't fork first child");

	else if(childpid > 0)       
		exit(0);	/* parent */

	/*
	 * First child process.
	 *
	 * Disassociate from controlling terminal and process group.
	 * Ensure the process can't reacquire a new controlling terminal.
	 */

	if(setpgrp()<0) sysError("can't change process group");

	signal(SIGHUP,SIG_IGN);	/* immune from pgrp leader death */

	switch(childpid=fork())
	{
	case -1 :
		sysError("can't fork second child");
		exit(errno);
	case 0 : 
		break;
	default :
		exit(0); /* first child */
	}
out:
	/* second child */
	/* probably got set to EBADF from a close */
	errno=0;		

	/*  make sure we aren't on a mounted filesystem */
	chdir("/");

	/* Clear any inherited file mode creation mask.  */
	umask(0);
}

void
sysError(const char *fmt,...)
{
va_list	args;
char tb[128];

	va_start(args,fmt);
	vsprintf(tb, fmt, args);
	va_end(args);
	ipcLog("%s ",tb);

	if(errno)
		ipcLog("%s\n",strerror(errno));
	else
		ipcLog("\n");
}
