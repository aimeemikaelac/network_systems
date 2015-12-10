# ECEN 5023: Network Systems
Programming assignment 3: Simple web proxy

Submission by: Michael Coughlin

Nov. 19, 2015

## Source code
The source code for this assignment is contained in the following files:

* webserver.cpp: The main logic of the proxy, including the main() function, 
  network connections and connection handling.

* webserver.h: The corresponding header file for this source file.

* webserver_util.cpp: A set of helper functions for the webserver for things such 
  as sending error codes or writing files to a socket.

* webserver_util.h: The header file for this source file.

* ConcurrentQueue.cpp: A simple implementation of a concurrent queue for pthreads. 
  Is used to allow for requests to be provided to a worker thread in order, to ensure
  that requests are handled int he order they are received.

* ConcurrentQueue.h: The header file for this source file.

* Makefile: A makefile with which to compile the provided source code into an executable
  called **proxyserver**

* README.md: This file. A readme file formatted for markdown.

## Compilation
To compile the source into an executable, simply run the provided makefile in the directory
containing the source code be executing *make* or *make all*. This can also be performed by
executing the same commands that the makefile would run:
```
	g++ -std=c++11   -c -o webserver.o webserver.cpp
	g++ -std=c++11   -c -o webserver_util.o webserver_util.cpp
	g++ -std=c++11   -c -o ConcurrentQueue.o ConcurrentQueue.cpp
	g++ -std=c++11 webserver.o webserver_util.o ConcurrentQueue.o -o proxyserver -lpthread
```
Please note that compilation requires a compiler that supports C11.

To clean the project and remove all output files, execute *make clean*, or execute directly:
```
	rm -f *.o
	rm -f proxyserver
```

## Usage
The program is compiled to the **proxyserver** executable.

The options are provided here:
```
	Usage: proxyserver <port> <timeout>
	Options:
		<port> - service port
		<timeout> - cache timeout, in seconds
```
Please note that this program is intended to only be used on a Linux system,
as the system calls are Linux-kernel dependent.

## Design Decisions

This assignment using the code from assignment #1. To implement multi-processing,
a pthread is spawned for each connection and the request is put onto a concurrent
queue, where a single worker thread processes the work received in-order, in the 
exact same manner as previous assignments. The worker thread extracts the URL from
the request from the proxy client, extracts the destination port if present, performs
a DNS query and opens a TCP connection to the destination if possible. The entire
request from the client is copied to this socket unmodified, other than the removal
of any keep-alive headers. Any response is stored in a hash table stored by the request
URL along with the time that it was received. If a client request arrives with a URL
for an entry in the hash table, it is returned to the client if the differnce between
the current time and the stored time for the entry is less than the timeout, rather
than performed a network operation to retrieve it.
