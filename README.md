
This project is not yet ready for prime time and is a work in progress. Some things described in the overview are incomplete or not fully
implemented, but the framework is in place and stable. I have this on github to maybe create Arduino-Yun community interest. Collaborators
are welcome.

Author: steve@heggood.com

Project Overview:

This is an application for the Arduino-Yun that serves as an adjucnt for existing interconnects between the two Yun processors.

It consists basically as a process group interconnected via Uinix-flavor shared memory and message queue. A lead process avr_init is run to
startup the application. It first establishes a shared memory segment and a message queue. The shared memory block is overlaid by a cast
to an array of strucures I call slots. The avr_init claims the first slot, leaving the remaining slots for it's children. Each slot contains
the pid and ptype i.e C_SQLITE, startup time, etc. In this way, any child can identfy it's parent or siblings by ptype. The avr_init ptype
is P_ROOT. The others are P_SQLITE, P_HTTP, and P_SHELL.

Next, avr_init reads the file /home/yun/lib/avr.config which contains one line for each process that avr_init should spawn. As each process
starts, it sends a message back to avr_init as a notification that it is on-line. This info is written to avr_init's log file. The avr_init
process is daemonized to divorce it's ancestry so that it is backgrounded and doesn't die with it's parent process. It remains in order to
control the rest of the application group. I remains vigilant of death of children in order to free up shared resouces acquired by the
process on startup.

Process avr_tty opens up /dev/ATH0 as a means to communicate to the avr. A simple protocol must be used in the avr sketch. For example,
commands must be used to match their definition on the linux side i.e C_SQLITE. Sketch would write "C_SQLITE dbname sql-statement\n" to the
serial port.

The receiver always considers the first word from the avr as a command. This is true for both requests and responses from the avr to linux.
Anything you want a server sketch to listen to linux for requests must implement a protocol as well. This satisfies the criteria for the
'avrbuddy' application to route information between co-processes. in the C_SQLITE example I gave, avr_tty first reads a string such as
"C_SQLITE SELECT something FROM database\n". The command string is converted to numeric equivalent to use as a message queue routing address
to avr_sqlite who is always listening for requests on the message queue. The message also contains the pid of avr_tty so that the query
result can be sent back to avr_tty whose 'from address' will be the pid of avr_tty. Note that database service is not limited to avr_tty.
Any process in the group or process that temporarily joins the group i.e. avr_shell may subit C_SQLITE messages to the queue.

Process avr_sqlite is a service process. It uses a default sqlite3 database: /home/yun/lib/yun.db. Upon startup the default is opened, and
the process continuously waits for messages in the queue, and responds to requests.

Process avr_http provies a VERY limited http service, basically GET and respond. It runs on an arbitray port specified as a command line
option. Any responses made are currently ebedded in the avr_http code. I expect to expand it's functionality to serve files from
/home/yun/www.

Process avr_shell connects stdio to the message queue and shared memory. I am writing a simple parser to implement a command set. For
example, ls may list the process types and id's. Then, one could do things like "C_SQLITE select * from table. The exit command or ^C exits.

Example avr.config file:
spawn process to provide database services

avr_sqlite /home/yun/lib/yun.db
spawn process for connection to Avr Serial1

                                                                                                                            2,1           Top
