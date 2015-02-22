#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
 
/* Keep this number as low as your application can bear
** as the amount of shared memory allocated is proportional
** to the maximum number of process the ipc tapestry supports
** It would be wastful to allocate slots for 100 processes
** if only 10 were ever used
*/

//#define TRACE ipcLog("PID: %d FN: %s LN:%d\n",getpid(),__FILE__,__LINE__);
#define TRACE fprintf(stderr,"PID: %d FN: %s LN:%d\n",getpid(),__FILE__,__LINE__);

#define MAX_IPC		5
#define MAX_TEXT	200

/* IPC Entity Types*/
#define P_ROOT		1
#define P_TTY		2
#define P_SQLITE	3
#define P_MON		4
#define P_SYSTEM	5

/* IPC Command Definitions */
#define C_LOGIN		1
#define C_LOGOUT	2
#define C_FAIL		3
#define C_PING		4
#define C_ACK		5
#define C_NAK		6
#define C_EOF		7
#define C_SQL		8
#define C_SYS		9
#define C_ERR		10

/* Ipc Dictionary and Ipc Head structures:
** One IPC_DICT each 'anticipated' process and one IPC_HEAD is required.
**
** A block of shared memory will be allocated which will be the size of
** (MAX_IPC*sizeof(IPC_DICT))+sizeof(IPC_HEAD). The memory is then partitioned
** by setting IPC_DICT pointer and IPC_HEAD pointer to proper memory offsets.
** This way all processes can examine each other's demographics.
*/

typedef struct 
{
	pid_t pid;				// process id of group member
	int   type;				// process type, P_ROOT, P_SQLITE, etc.
	int   online;			// startup time
	char  stype[21];		// string representation of process type
	char  ibc_buf[256];		// utiliy buffers for messaging, etc;
}IPC_DICT
;

extern IPC_DICT *ipc_dict;

typedef struct 
{
	IPC_DICT *dict;			// pointer to ipc_dict
	pid_t proot;			// pid of root process
	int   procs;			// array of process numbers
	int   txmsg;			// number of messages transmited
	int   rxmsg;			// number of messages recieved
}IPC_HEAD
;

extern IPC_HEAD *ipc_head;


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
	int  slot;
	int  len;
	int  cmd;
	char scmd[21];
	char text[256]; 
} MSG_BUF
;

extern MSG_BUF
  *recvMessage(int msqid, long mtype)
;

typedef struct 
{
	int  from_pid;
	int  to_pid;
	char text[256]; 
} IPC_TEXT;
;

extern IPC_TEXT 
  *ipc_text
, *ipcGetText(pid_t from_pid, pid_t to_pid)
;

typedef unsigned char BYTE;

extern char 
  *ipcCmdName(int cmd)
, *ipcSigName(int sig)
, **ipcSplit(char *s, char c)
;

extern int
  ipc_dlev
, ipc_dup
, ipcAddText(pid_t from_pid, pid_t to_pid, const char *text)
, ipcLog(const char *format, ...)
, ipcRawLog(const char *format, ...)
, ipcDetail(const char* format, ...)
, ipcSysError(const char *fmt,...)
, ipcDebug(int lev, const char*format,...)
, ipcGetIpcResources(pid_t pid, int ptype, char *token)
, ipcGetSharedMemory(pid_t pid, int ptype, char *token)
, ipcGetPidByType(int type)
, ipcGetSlotByType(int type)
, ipcGetSlotByPid(pid_t pid)
, ipcAllocSharedMemory(pid_t pid, int ptype, char *token)
, ipcGetMessageQueue(pid_t pid, int ptype, char *token)
, ipcSendMessage(pid_t pid, int msqid, long mtype, int cmd, char *txt)
, ipcCleanMessageQueue(int msqid)
, ipcRemoveMsgQueue(int msqid)

;

extern long // from avr_ipc.c
  shmid 
, msqid
, semid
, ipcGetSemaphore(pid_t pid, int ptype, char *token)
;

// each main declares his own global, main links to us
extern IPC_DICT *ipc_dict;

extern MSG_BUF *ipcRecvMessage(int msqid, pid_t pid);

extern void ipcLogErrorWait(pid_t pid, const char*format,...);

extern void
  ipcFreeSharedMem(pid_t pid)
, ipcClearSlot(pid_t pid)
, ipcNotify(pid_t pid, int qid)
, ipcExit(pid_t pid, int err, int sig)
, logClose()
, initNotify()
;
;
char 
  *ipcTypeName(int ptype)
, *ipcCmdName(int cmd)
, *ipcSigName(int sig)
, *ipcTypeNameByPid(pid_t pid)
;
