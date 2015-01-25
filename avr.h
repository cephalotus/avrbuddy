#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/signal.h>
#include <sys/types.h>
 
/* Keep this number as low as your application can bear
** as the amount of shared memory allocated is proportional
** to the maximum number of process the ipc tapestry supports
** It would be wastful to allocate slots for 100 processes
** if only 10 were ever used
*/
#define MAX_IPC		5

#define TRACE fprintf(stderr,"FN: %s LN:%d\n",__FILE__,__LINE__);
 
/* IPC Entity Types*/
#define P_ROOT		(1<<0)
#define P_AVR		(1<<1)
#define P_SQLITE	(1<<2)


/* IPC Command Definitions */
#define C_LOGIN		(1<<0)
#define C_LOGOUT	(1<<1)

/* keep the compiler from bitching about handler functions
typedef void (*sighandler_t)(int);
 
/* Ipc Dictionary structre
** One for each 'anticipated' process
** will be allocated in shared memory
** so that the processes can 'peek' at
** each other
*/
typedef struct 
{
	pid_t pid;
	int   type;
	int   online;
	char  ibc_buf[255];
}IPC_DICT
;

extern IPC_DICT *ipc_dict;

/* The IPC message queue buffer
** Remember, char[] is a block of raw memory bytes.
** Any data type can be memcpy'd into it andcast 
** on the recieve side.  256 bytes should be 
** ample for most objects, ergo text[256].
** Consider the limits of the memory on the AR9331
** when increasing this number.
*/
typedef struct 
{
	long type;
	int  rsvp;
	int  len;
	int  cmd;
	char text[256]; 
} MSG_BUF
;

MSG_BUF *recvMessage(int msqid, long mtype) ;

typedef unsigned char BYTE;

extern char *ipcCmdName(int cmd);

extern int
  ipc_dlev
, ipc_dup
, msqid
, shMemAlloc(int ptype, const char *token)
, sendMessage(int msqid, long mtype, int cmd, char *txt)
, ipcLog(const char *format, ...)
, ipcDetail(const char* format, ...)
, ipcDebug(int lev, const char*format,...)
;

extern long shmid;

extern void
  usrExit(int err, int sig)
, logClose()
, initNotify()
;
