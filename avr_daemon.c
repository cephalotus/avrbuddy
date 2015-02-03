#include "avr.h"

extern int errno;

daemonize()
{
int childpid, fd;

	if(getppid()==1) goto out;

	/*
	 * If we were not started in the background, fork and
	 * let the parent exit.  This also guarantees the first child
	 * is not a process group leader.
	 */

	if((childpid=fork())<0) 
	{
		ipcSysError("can't fork first child");
	}
	else if(childpid>0)     
	{
		exit(0);	// parent
	}

	/*
	 * First child process.
	 *
	 * Disassociate from controlling terminal and process group.
	 * Ensure the process can't reacquire a new controlling terminal.
	 */

	if(setpgrp()<0) 
	{
		ipcSysError("can't change process group");
	}

	signal(SIGHUP,SIG_IGN);	// immune from pgrp leader death

	switch(childpid=fork())
	{
	case -1 :
		ipcSysError("can't fork second child");
		exit(errno);

	case 0 : 
		break;

	default :
		exit(0); // first child
	}
out:
	// Second child; Probably got set to EBADF from a close
	errno=0;		

	// Make sure we aren't on a user mounted filesystem
	chdir("/");

	// Clear any inherited file mode creation mask
	umask(0);
}
// vim:cin:ai:sts=4 ts=4 sw=4 ft=cpp
