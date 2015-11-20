# ECEN 5023: Network Systems
Programming assignment 1: Simple web server

Submission by: Michael Coughlin

Sept. 20, 2015

## Source code
The source code for this assignment is contained in the following files:

* webserver.cpp: The main logic of the webserver, including the main() function, 
  configuration option parsing, network connections and connection handling.

* webserver.h: The corresponding header file for this source file.

* webserver_util.cpp: A set of helper functions for the webserver for things such 
  as sending error codes or writing files to a socket.

* webserver_util.h: The header file for this source file.

* ConcurrentQueue.cpp: A simple implementation of a concurrent queue for pthreads. 
  Is used to allow for requests to be provided to a worker thread in order, to ensure
  that requests are handled int he order they are received.

* ConcurrentQueue.h: The header file for this source file.

* Makefile: A makefile with which to compile the provided source code into an executable
  called **webserver**

* README.md: This file. A readme file formatted for markdown.

## Compilation
To compile the source into an executable, simply run the provided makefile in the directory
containing the source code be executing *make* or *make all*. This can also be performed by
executing the same commands that the makefile would run:
```
	g++ -c -o webserver.o webserver.cpp
	g++ -c -o webserver_util.o webserver_util.cpp
	g++ -c -o ConcurrentQueue.o ConcurrentQueue.cpp
	g++ webserver.o webserver_util.o ConcurrentQueue.o -o webserver -lpthread
```

To clean the project and remove all output files, execute *make clean*, or execute directly:
```
	rm -f *.o
	rm -f webserver
```

## Usage
The program is compiled to the **webserver** executable. To view the usage options, run:
```
	./webserver -h
```

The options are also provided here:
```
	Usage: webserver [-f FILE] [-p PORT] [-h]
	Options:
	  -f FILE	Specify a configuration file. If the file cannot be opened, reasonable 
	  		defaults will be assumed. See below for format. The default is ./ws.conf.
	  -p PORT	Specify a service port. Will be ignored if port is set in a configuration
			file. The default is 8080.
	  -h		Print the help message and exit.
```
Please not that this program is intended to only be used on Linux system, as the file path
lookups depend on Linux file path separators and file system calls. 


## Configuration Options
The configuration file follows the same format as is specified by the assignment. Below are
the options:

* To set the service port:
```
	Listen <port>
```

* To set the document root:
```
	DocumentRoot "<path_to_root>"
```

* To set the directory indices:
```
	DirectoryIndex <list_of_index_files>
```

* To specify content-types, any number of the following, where <file-type> is a file extension:
```
	<file-type> <content-type>
```

An example configuration file is provided below:
```
file ws.conf:
	#service port number
	Listen  8004

	#document root
	DocumentRoot  "/home/sha2/www"

	#default web page
	DirectoryIndex index.html index.htm index.ws

	#Content-Type which the server handles
	.html  text/html
	.txt  text/plain
	.png  image/png
	.gif  image/gif
```
