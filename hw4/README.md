# ECEN 5023: Network Systems
Programming assignment 4: Transparent proxy

Submission by: Michael Coughlin

Dec. 9, 2015

## Source code
The source code for this assignment is contained in the following files:

* TransparentProxy.cpp: The main logic of the proxy, including the main() function, 
  network connections and connection handling.

* TransparentProxy.h: The corresponding header file for this source file.

* Makefile: A makefile with which to compile the provided source code into an executable
  called **transparentproxy**

* README.md: This file. A readme file formatted for markdown.

## Compilation
To compile the source into an executable, simply run the provided makefile in the directory
containing the source code be executing *make* or *make all*. This can also be performed by
executing the same commands that the makefile would run:
```
	g++    -c -o TransparentProxy.o TransparentProxy.cpp
	g++  TransparentProxy.o -o transparentproxy -lpthread
```
Please note that compilation requires a compiler that supports C11.

To clean the project and remove all output files, execute *make clean*, or execute directly:
```
	rm -f *.o
	rm -f transparentproxy
```

## Usage
The program is compiled to the **transparentproxy** executable.

The options are provided here:
```
	Usage: transparentproxy <client-side interface ip> <listen port> <server-side interface ip>
	Options:
		<client-side interface ip> - IP address of the interface facing the client. The interface
									 itself is hard-coded to eth1
		<listen port>			   - the port for the proxy to listen on
		<server-side interface ip> - IP address of the interface facing the server. Is the IP
									 address of eth2
```
Please note that this program is intended to only be used on a Linux system,
as the system calls are Linux-kernel dependent. Also, this program wil only work
on the VM I used for development or a similar VM such that the eth1 interface is
connected to the client and the eth2 interface is connected to the server. Note
that the VMs also need to be configured correctly in order for the network
connections to work. The program must also be run with root priveledges in order
to be able to make calls to iptables, and all iptables rules should be flushed
from all VMs prior to running.

## Design Decisions

This assignment uses iptables to redirect and rewrite network connections to the
running instance of the program, even though iptables can be used without another
program to implement the same functionality in a much quicker and more elegant
solution. However, since the assignment appears to ask for a program, this program
listens on a server socket and programs iptables to DNAT traffic from the client-facing
interface to this socket. All connections are analyzed and a new connection is
made to the connection's original destination. iptables is then used to SNAT this
new connection to spoof the IP address of the original sender so that the server
does not know that it is communicating with the proxy. Each connection is handled
by a thread, and this thread spawns two threads to handle the communication from
server-to-client and client-to-server, respectively. Each connection also binds to
a unique port number and writes to the log file in a thread-safe manner. The
connection is terminated when either the client socket or server socket is closed,
and managed using a shared concurrent variable between the threads.
