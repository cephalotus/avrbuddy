#include <sqlite3.h> 
#include "avr.h"

static IPC_DICT *dict;
static sqlite3 *db;
static char *zErrMsg;

int slot;
pid_t
  pid
, ppid
;
static pid_t rsvp;

// this can be changed with a command line aruement
// i.e avr_yun /tmp/tmp.db
static char *yundb="/home/yun/lib/yun.db";

extern IPC_DICT *ipc_dict;

void // signal handler
dbDterminate(int sig)
{
	ipcLog("P_SQLITE Terminiate Request! SIG: %s\n",ipcSigName(sig));
	if(zErrMsg) sqlite3_free(zErrMsg);
	if(db)      sqlite3_close(db);
	ipcExit(getpid(),0,sig);
}

typedef int (*sqlite3_callback)
(
void*,    /* Data provided in the 4th argument of sqlite3_exec() */
int,      /* The number of columns in row */
char**,   /* An array of strings representing fields in the row */
char**    /* An array of strings representing column names */
);

static int // gets called for each row in the result set
callback(void *NotUsed, int cols, char **fields, char **colnames)
{
char reply[1024];
char *s;
int i;
	//ipcLog("Query has %d cols\n",cols);

	// iterate over the columns in the row;
	for(s=reply,i=0; i<cols; i++)
	{
		//ipcLog("col %d: %s = %s\n",i,colnames[i],fields[i]?fields[i]:"NULL");
		s+=sprintf(s,"%s%c",fields[i]?fields[i]:"NULL",(i==cols-1)?'\0':'|');
	}
	ipcLog("\n");

	ipcLog("SENDING ACK Message: %s\n",reply);
	ipcSendMessage(pid,msqid,rsvp,C_ACK,reply);

	ipcLog("SENDING EOF Message: %s\n","");
	ipcSendMessage(pid,msqid,rsvp,C_EOF,"");
	return 0;
}

int 
main(int argc, char* argv[])
{
	int rc;
	char *sql;
	const char *data = "Callback function called";
	char buf[81];
	MSG_BUF *msg;

	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,dbDterminate);

	// set thes globally
	ppid=getppid(); 
	pid=getpid();

	/*set up application logging */
	logOpen("sqlite");

	ipcLog("Starting P_SQLITE process!\n");

	if(argc==2) 
	{
		yundb=argv[1];
		ipcLog("Set database [%s] from argv[1]\n",yundb);
	}
	else
	{
		ipcLog("No Database specified!\n");
		ipcLog("Using default database [%s]\n",yundb);
	}

	/* pick up shared memory environment */
	slot=ipcGetSharedMemory(pid,P_SQLITE,"/tmp/ipc_application");
	//ipcLog("Got Memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=ipcGetMessageQueue(pid,P_SQLITE,"/tmp/ipc_application");

	// tell root process we are here
	ipcNotify(pid,msqid);

	if((rc=sqlite3_open(yundb, &db))!=0)
	{
		ipcLog("Can't open database: %s ERROR:%s\n",yundb,sqlite3_errmsg(db));
		sprintf(buf,"Process %d Slot %d %s"
		, pid
		, slot
		, sqlite3_errmsg(db)
		);

		ipcLog("Processing Cannot Continue, Waiting to be killed...\n");

		// send message to ROOT
		ipcLog("FAIL=%d\n",C_FAIL);
		ipcSendMessage(pid,msqid,ipc_dict[0].pid,C_FAIL,buf);
		// wait for ROOT to kill us
		pause();
	}

	ipcLog("Opened %s successfully\n",yundb);

	for(;;) // until we get SIGTERM
	{
		/* wait for messages or signals */
		msg=ipcRecvMessage(msqid, pid);
		rsvp=msg->rsvp;

		ipcLog("Message From %d [%s] \n",msg->rsvp,msg->text);

		// Execute SQL statement
		if((rc=sqlite3_exec(db, msg->text, callback, 0, &zErrMsg)) != SQLITE_OK )
		{
			ipcLog("SQL error: %s\n", zErrMsg);
			ipcSendMessage(pid,msqid,msg->rsvp,C_ACK,zErrMsg);
		}
		else
		{
			ipcLog("SQL success\n");
		}
		ipcSendMessage(pid,msqid,msg->rsvp,C_EOF,"");
	}
}
