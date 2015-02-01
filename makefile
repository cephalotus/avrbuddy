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
	SQLITE_LIB=/usr/lib/libsqlite3.a -ldl -lpthread
endif
#$(info $(CPU))
#$(info SQLITE_LIB: $(SQLITE_LIB))


AVR_INIT =avr_init.o   avr_ipc.o avr_log.o avr_daemon.o
AVR_TTY  =avr_tty.o    avr_ipc.o avr_log.o
AVR_LITE =avr_sqlite.o avr_ipc.o avr_log.o
AVR_MON=avr_mon.o  avr_ipc.o avr_log.o
AVR_SYSTEM =avr_system.o   avr_ipc.o avr_log.o

all: $(BIN)avr_tty \
$(BIN)avr_init \
$(BIN)avr_sqlite \
$(BIN)avr_mon \
$(BIN)avr_system 

$(AVR_INIT) $(AVR_TTY) $(AVR_LITE) $(AVR_MON) $(AVR_SYSTEM): avr.h

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

$(BIN)avr_mon: $(AVR_MON)
	cc $(IFLAGS) $(AVR_MON) $(LDFLAGS) -o$@
	chmod 4755 $@
	strip $@

$(BIN)avr_system: $(AVR_SYSTEM)
	cc $(IFLAGS) $(AVR_SYSTEM) -o$@
	chmod 4755 $@
	strip $@

lite: lite.c
	cc $(IFLAGS) $? $(SQLITE_LIB) $(LDFLAGS) -o$@

run:
	avr_init

kill:
	pkill avr_init
	make clean

vi:
	vi ../log/*

log_tty:
	tail -f ../log/tty*

log_sqlite:
	tail -f ../log/sqlite*

clean:
	-rm ../log/*

status:
	@git status

git:
	@git status

commit:
	@git commit -a

# origin is just the "name" of the remote.  Standard convention is origin. I could've 
# named it github.  I thought at the time origin was fixed as is remote add.
# git remote add orign https://github.com/cephalotus/avrbuddy.git (uses ssh as transport)
#
push:
	git push -u origin master
