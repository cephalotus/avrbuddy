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
 
#define MAX_IPC		15
#define TRACE fprintf(stderr,"FN: %s LN:%d\n",__FILE__,__LINE__);
 
/* IPC Entity Types*/
#define P_ROOT		(1<<0)
#define P_AVR		(1<<1)
#define P_SQLITE	(1<<2)


/* IPC Command Definitions */
#define C_LOGIN		(1<<0)
#define C_LOGOUT	(1<<1)

typedef void (*sighandler_t)(int);
 
typedef struct 
{
	pid_t pid;
	int   type;
	int   dav;
	int   online;
	int   test_flag;
	char  tty[81];
	char  test_buffer[255];
}IPC_DICT
;
typedef struct 
{
	long type;
	int  rsvp;
	int  len;
	int  cmd;
	char text[200];
} MSG_BUF
;
MSG_BUF *
  recvMessage(int msqid, long mtype)
;
typedef unsigned char
  BYTE
;
extern IPC_DICT 
  *ipc_dict
;
extern char 
  *getenv()
, *ipcCmdName(int cmd)
;
extern int
  ipc_dlev
, ipc_dup
, errno
, msqid
, shMemAlloc(int ptype, const char *token)
, sendMessage(int msqid, long mtype, int cmd, char *txt)
, ipcLog(const char *format, ...)
, ipcDetail(const char* format, ...)
, ipcDebug(int lev, const char*format,...)
;
extern long 
  shmid
;
extern void
  usrExit(int err, int sig)
, logClose()
, initNotify()
;
