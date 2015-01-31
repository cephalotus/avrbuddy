#include <sqlite3.h> 
#include "avr.h"

static sqlite3 *db;
static char *zErrMsg;

// this can be changed with a command line aruement
// i.e avr_yun /tmp/tmp.db
static char *yundb="/home/yun/lib/yun.db";

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
char reply[1024];
char *s,delim;
int i;
	printf("COLS: %d\n",cols);

	// iterate over the columns in the row;
	for(s=reply,i=0; i<cols; i++)
	{
		if(i==cols-1)
			delim='\0';
		else
			delim='|';
		printf("COL: %d\n",i);
		printf("col %d: %s = %s\n",i,colnames[i],fields[i]?fields[i]:"NULL");
		s+=sprintf(s,"%s%c",fields[i]?fields[i]:"NULL",(i==cols-1)?'\0':'|');
	}
	printf("Reply: %s\n",reply);
	return 0;
}

int 
main(int argc, char* argv[])
{
	int rc;
	char *sql;
	const char *data = "Callback function called";
	char buf[81];


	if((rc=sqlite3_open(yundb, &db))!=0)
	{
		printf("Can't open database: %s ERROR:%s\n",yundb,sqlite3_errmsg(db));
		exit(0);
	}

	printf("Opened %s successfully\n",yundb);

	sql="select * from pins";
	sql=".tables";
	// Execute SQL statement
	if((rc=sqlite3_exec(db, sql, callback, 0, &zErrMsg)) != SQLITE_OK )
	{
		printf("SQL error: %s\n", zErrMsg);
	}
	else
	{
		printf("Sql executed successfully\n");
	}
}
