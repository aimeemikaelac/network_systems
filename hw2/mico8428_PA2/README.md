# ECEN 5023: Network Systems
Programming Assignment 2: Distributed file server/client

Submission by: Michael Coughlin

Oct. 25, 2015

## Source code
The source code for this assignment is contained in the following files:

* DFSClient.cpp: The main logic of the DFS client, including the main() function, 
  configuration option parsing, network connections and request handling.

* DFSClient.h: The corresponding header file for this source file.

* DFSServer.cpp: The main logic of the DFS server, including the main() function, 
  configuration option parsing, network connections and connection handling.

* DFSServer.h: The corresponding header file for this source file.

* util.cpp: A set of helper functions for the DFS client/server for things such 
  as sending error codes or writing files to a socket.

* util.h: The header file for this source file.

* ConcurrentQueue.cpp: A simple implementation of a concurrent queue for pthreads. 
  Is used to allow for requests to be provided to a worker thread in order, to ensure
  that requests are handled int he order they are received.

* ConcurrentQueue.h: The header file for this source file.

* Makefile: A makefile with which to compile the provided source code into two executables
  called **dfs** and **dfc**

* README.md: This file. A readme file formatted for markdown.

## Compilation
To compile the source into an executable, simply run the provided makefile in the directory
containing the source code be executing *make* or *make all*. This can also be performed by
executing the same commands that the makefile would run:
```
        g++ -g   -c -o util.o util.cpp
        g++ -g   -c -o ConcurrentQueue.o ConcurrentQueue.cpp
        g++ -g   -c -o DFSClient.o DFSClient.cpp
        g++ util.o ConcurrentQueue.o DFSClient.o -o dfc -g -lpthread -lssl -lcrypto
        g++ -g   -c -o DFSServer.o DFSServer.cpp
        g++ util.o ConcurrentQueue.o DFSServer.o -o dfs -g -lpthread -lssl -lcrypto
```

To clean the project and remove all output files, execute *make clean*, or execute directly:
```
        rm -f *.o
        rm -f dfc
        rm -f dfs
```

## Usage
The client is run by executing the **dfc** executable with a configuration file as the only argument.
Sample configuration files are provided in **dfc_alice.conf**, **dfc_bob2.conf** and **dfc_bob.conf**.

Example usage:
```
        ./dfc dfc.conf.alice
```

The server is run by executing the **dfc** executable with a directory as the first argument and a port
number as the second argument. The server reads a file **dfc.conf** in the same directory as the executable
to determine valid user names. Without the file, the server will have no user information, so no users
will be able to access the server. An example configuration file is available in **dfs.conf** with corresponding
information to the client configuration files.

Example usage:
```
        ./dfs DFS/ 9001
        ./dfs . 9002
```