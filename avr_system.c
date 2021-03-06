#include "avr.h"
#include <libgen.h>

extern IPC_DICT *ipc_dict;
static IPC_DICT *dict;
static struct termio tty;

void // signal handler
sysTerminate(int sig)
{
int x,id;
	ipcLog("Proc %d P_SYSTEM Terminiate Requested SIG: %s!\n",pid,ipcSigName(sig));
	ipcExit(0,0);
}

int 
main(int argc, char* argv[])
{
    int pp[2], cc, c, status, cslot; 
	char *part, **aa, buf[1024];
	IPC_DICT *d;
	MSG_BUF *msg;

	signal(SIGINT, sysTerminate);
	signal(SIGTERM,sysTerminate);

	/* set up application logging */
	logOpen("system");

	ipcLog("Starting P_SYSTEM process!\n");
	chdir("/home/yun");
	ipcLog("Chdir: %s\n",strerror(errno));

	/* pick up shared memory environment */
	ipcGetIpcResources(P_SYSTEM,"/tmp/ipc_application");

	//ipcLog("Got memory at %x ipc_slot=%d\n",ipc_dict,ipc_slot);
	dict=&ipc_dict[ipc_slot];

	// notify root process we are here
	ipcNotify(pid);

	ipcLog("P_SYSTEM Process %d On-Line in ipc_slot %d\n"
	, pid
	, ipc_slot
	);

	for(;;)
	{
	FILE *pp;
	char cmd[1024];
		ipcLog("Waiting for Message\n");
		// wait for messages or signals
		msg=ipcRecvMessage();

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

		// save the command
		strcpy(cmd,msg->text);

		// make an array for execvp
		aa=ipcSplit(msg->text,' ');
		if(!aa[0])
		{
			ipcSendMessage(msg->rsvp,C_ERR,"No cmd line argument!");
			continue;
		}
		if(!strcmp(aa[0],"cd"))
		{
			if(chdir(aa[1])<0)
			{
				sprintf(buf,"cd failed: %s",strerror(errno));
				ipcSendMessage(msg->rsvp,C_ERR,buf);
			}
			ipcSendMessage(msg->rsvp,C_EOF,"cd Ok");
			continue;
		}

		if((pp=popen(cmd,"r"))==NULL)
		{
			// uh oh
			sprintf(buf,"popen(%s) failed: %s\n",cmd,strerror(errno));
			ipcLog("%s",buf);
			ipcSendMessage(msg->rsvp,C_ERR,buf);
		}

		while(fgets(buf,sizeof(buf),pp)!=NULL)
		{
			// insulate from interrupt system calls
			if(errno && errno==EINTR) 
			{
				ipcLog("fgets Interrupted!\n");
				continue;
			}
			// strip the newline
			buf[strlen(buf)-1]='\0';

			// ipcLog("<< %s\n",buf);

			ipcAddText(msg->rsvp,buf); // to pid, text
		}

		// close the pipe to command
		fclose(pp);

		// that's all folks
		ipcSendMessage(msg->rsvp,C_EOF,"Success");
	}
}
