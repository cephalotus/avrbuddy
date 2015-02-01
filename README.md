
This project is not yet ready for prime time and is a work in progress. Some things described here may incomplete or not fully implemented, but the framework is in place and stable. I have this on github to maybe create Arduino-Yun community interest. Collaborators are welcome.

Author: steve@heggood.com

Project Overview:

This is an application for the Arduino-Yun that serves as an adjucnt for existing interconnects between the two Yun processors.

It consists of a process group interconnected via  shared memory and message queue. A lead process avr_init is run to startup the application. It first establishes a shared memory segment and a message queue, then starts up other utility processes defined in avr.config.  Currently I have three that startup in my configuration:

1. avr_tty -- establishes a serial connection between ATmega32u4 Seral1 and linux dev/ATH0.
2. avr_sqlite -- establishes an sqlite database that be accessed from the ATmega32u4 via avr_tty, or the other processes. 3. avr_system -- listens for requests for shell commands such as date or ls and returns result to requesting process.

avr_mon is run on demand from linux shell to monitor the app.  Currently provides help, ps, sql, sys, and kill.

See the README file in the repository that details the application.
See the INSTALL file in the repository that outlines the application environment.
