.SUFFIXES: .c .ec
.ec.c:
	esql -e $<

#CINC=-I/export/development/hil/src/inc -I/opt/informix/inf7.30.uc9/incl/esql -I/export/home/hil/src/inc
#DLIB=-L../lib -lfm -lcurses
SQLITE_LIB=/usr/lib/libsqlite3.so.0
#CFLAGS=$(CINC) -g 
CFLAGS=

BIN=/home/yun/bin/

PRI_OBJS=avr_init.o avr_ipc.o avr_log.o avr_daemon.o
AVR_OBJS=avr_tty.o avr_ipc.o avr_log.o
AVR_STAT=avr_stat.o avr_memory.o avr_log.o
AVR_SQLITE=avr_sqlite.o avr_ipc.o avr_log.o
MON_OBJS=avr_mon.o avr_memory.o avr_log.o
LOD_OBJS=avr_load.o avr_memory.o avr_log.o

all: $(BIN)avr_tty $(BIN)avr_init $(BIN)avr_sqlite

$(AVR_PUMP) $(MON_OBJS) $(PRI_OBJS) $(AVR_OBJS): avr.h

$(BIN)avr_tty: $(AVR_OBJS)
	cc $(IFLAGS) $(AVR_OBJS) $(LDFLAGS) -o$@
	chmod 4755 $@

$(BIN)avr_load: $(LOD_OBJS)
	cc $(IFLAGS) $(LOD_OBJS) $(LDFLAGS) -o$@
	chmod 4755 $@

$(BIN)avr_mon: $(MON_OBJS)
	cc $(IFLAGS) $(MON_OBJS) $(LDFLAGS) -s -o$@

$(BIN)avr_init: $(PRI_OBJS)
	cc $(IFLAGS) $(PRI_OBJS) $(LDFLAGS) -o$@
	chmod 4755 $@

$(BIN)avr_sqlite: $(AVR_SQLITE)
	cc $(IFLAGS) $(AVR_SQLITE) -L/usr/lib -lsqlite3 $(LDFLAGS) -o$@
	chmod 4755 $@

run:
	avr_init

clean:
	-pkill avr_init
	sleep 2
	-rm ../log/*
	-rm *.o
