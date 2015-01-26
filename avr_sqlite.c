#include <sqlite3.h> 
#include "avr.h"

static IPC_DICT *dict;
static sqlite3 *db;
static char *zErrMsg;

static int
  pid
, ppid
, slot
;

void // signal handler
dbDterminate(int sig)
{
	ipcLog("SQLITE Terminiate Request! SIG: %s\n",ipcSigName(sig));
	if(zErrMsg) sqlite3_free(zErrMsg);
	if(db)      sqlite3_close(db);
	ipcExit(0,sig);
}

typedef int (*sqlite3_callback)
(
void*,    /* Data provided in the 4th argument of sqlite3_exec() */
int,      /* The number of columns in row */
char**,   /* An array of strings representing fields in the row */
char**    /* An array of strings representing column names */
);

static int // gets called for each row int the result set
callback(void *NotUsed, int cols, char **fields, char **colnames)
{
	int i;
	// iterate over the and columns
	for(i=0; i<cols; i++)
	{
		ipcLog("col %d: %s = %s\n",i,colnames[i],fields[i]?fields[i]:"NULL");
	}
	ipcLog("\n");
	return 0;
}

int 
main(int argc, char* argv[])
{
	int rc;
	char *sql;
	const char *yundb="/home/yun/lib/yun.db";
	const char *data = "Callback function called";
	MSG_BUF *msg;

	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,dbDterminate);

	ppid=getppid(); 
	pid=getpid();

	/*set up application logging */
	logOpen("sqlite");

	ipcLog("Starting P_SQLITE process!\n");

	/* pick up shared memory environment */
	slot=getSharedMemory(P_SQLITE,"/tmp/ipc_application");
	ipcLog("Got Memory at %x slot=%d\n",ipc_dict,slot);
	dict=&ipc_dict[slot];

	msqid=getMessageQueue(P_SQLITE,"/tmp/ipc_application");

	// tell root process we are here
	initNotify(msqid);

	ipcLog("SQLITE Process %d On-Line in slot %d\n", pid, slot);

	if((rc=sqlite3_open(yundb, &db))!=0)
	{
		ipcLog("Can't open database: %s\n", sqlite3_errmsg(db));
		dbDterminate(SIGTERM);
	}

	ipcLog("Opened %s successfully\n",yundb);

	// set signal handlers
	signal(SIGINT ,SIG_IGN);
	signal(SIGTERM,dbDterminate);

	for(;;) // until we get SIGTERM
	{
		/* wait for messages or signals */
		msg=recvMessage(msqid, pid);

		ipcLog("Message from %d [%s] \n",msg->rsvp,msg->text);

		// Execute SQL statement
		if((rc=sqlite3_exec(db, sql, callback, 0, &zErrMsg)) != SQLITE_OK )
		{
			ipcLog("SQL error: %s\n", zErrMsg);
		}
		else
		{
			ipcLog("Sql executed successfully\n");
		}
	}
}
