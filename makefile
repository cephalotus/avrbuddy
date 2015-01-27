#
# We must define _GNU_SOURCE to get some of the
# msgctl structes and flags to work see: man 2 msgctl
# for details
#
# chmod 4755 forces setuid in permissions so that
# the executable runs as user yun.
#
# strip the executable to save space on the yun
#
.c.o:
	$(CC) -c $(CFLAGS) -D_GNU_SOURCE $< -o $@

DFLAGS=-D_GNU_SOURCE
CFLAGS=-O

BIN=/home/yun/bin/
#determine which sqlite3 library to use
CPU=$(shell uname -m)
ifeq "$(CPU)" "i686"
	SQLITE_LIB=-lsqlite3
else
	SQLITE_LIB=-lsqlite3 -ldl -lpthread
endif
$(info $(CPU))
$(info SQLITE_LIB: $(SQLITE_LIB))

#ifdef CATEGORY
#Linux gizmo.heggood.com 3.17.8-300.fc21.i686+PAE #1 SMP Thu Jan 8 23:49:59 UTC 2015 i686 i686 i386 GNU/Linux
#ifdef TEST
#$(CATEGORY):
#    whatever
#else
#$(info TEST not defined)
#else
#$(info CATEGORY not defined)
#endif

AVR_INIT =avr_init.o   avr_ipc.o avr_log.o avr_daemon.o
AVR_TTY  =avr_tty.o    avr_ipc.o avr_log.o
AVR_LITE =avr_sqlite.o avr_ipc.o avr_log.o
AVR_SHELL=avr_shell.o  avr_ipc.o avr_log.o
AVR_MON  =avr_mon.o    avr_ipc.o avr_log.o

all: $(BIN)avr_tty \
$(BIN)avr_init \
$(BIN)avr_sqlite \
$(BIN)avr_shell \
$(BIN)avr_mon 

$(AVR_INIT) $(AVR_TTY) $(AVR_LITE) $(AVR_SHELL) : avr.h

$(BIN)avr_init: $(AVR_INIT)
	cc $(IFLAGS) $(AVR_INIT) $(LDFLAGS) -o$@
	chmod 4755 $@
	strip $@

$(BIN)avr_tty: $(AVR_TTY)
	cc $(IFLAGS) $(AVR_TTY) $(LDFLAGS) -o$@
	chmod 4755 $@
	strip $@

$(BIN)avr_sqlite: $(AVR_LITE)
	cc $(IFLAGS) $(AVR_LITE) $(SQLITE_LIB) $(LDFLAGS) -o$@
	chmod 4755 $@
	strip $@

$(BIN)avr_shell: $(AVR_SHELL)
	cc $(IFLAGS) $(AVR_SHELL) $(LDFLAGS) -o$@
	chmod 4755 $@
	strip $@

$(BIN)avr_mon: $(AVR_MON)
	cc $(IFLAGS) $(AVR_MON) $(LDFLAGS)  -lncurses -o$@
	chmod 4755 $@
	strip $@

test: test.c
	cc $? -o $@

run:
	avr_init

kill:
	pkill avr_init

vi:
	vi ../log/*

clean:
	-rm ../log/*
	-rm *.o

status:
	git status

stat:
	git status

commit:
	git commit -a
