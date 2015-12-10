/*
 * TransparentProxy.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: michael
 */

#include "TransparentProxy.h"

using namespace std;

pthread_mutex_t portmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;

TransparentProxy::TransparentProxy(string clientSide, string serverSide, int clientSidePort) {
	TransparentProxy::clientSide = clientSide;
	TransparentProxy::serverSide = serverSide;
	TransparentProxy::clientSidePort = clientSidePort;
}

TransparentProxy::~TransparentProxy() {
}

/**
 * Logs a line to the log file in a thread-safe manner
 */
static void log(string line){
	ofstream myfile;
	pthread_mutex_lock(&logmutex);
	myfile.open ("transparentproxy.log", ios::app);
	myfile << line << endl;
	myfile.close();
	pthread_mutex_unlock(&logmutex);
}

/**
 * Generates a port number for the proxy source port in a thread-safe manner
 */
static void getPortNumber(int *portOut){
	static int port = 2000;
	pthread_mutex_lock(&portmutex);
	*portOut = port;
	port++;
	pthread_mutex_unlock(&portmutex);
}

/**
 * A task for a thread to read from one socket and write the results to another
 */
static void* connectSockets(void *args){
	struct communicator *comArgs = (struct communicator*)args;
	char buffer[MAX_REQUEST_SIZE + 1];
	int receivedClient, written;
	//get value of initial control flag
	pthread_mutex_lock(comArgs->signalLock);
	bool local = comArgs->signalLock;
	pthread_mutex_unlock(comArgs->signalLock);
	//loop while the control flag says to
	while(local){
		memset(buffer, 0, MAX_REQUEST_SIZE + 1);
		//read from the source socket
		if((receivedClient = recv(comArgs->source_fd, buffer, MAX_REQUEST_SIZE, 0)) > 0){
			//write the data to the destination
			written = write(comArgs->dest_fd, buffer, receivedClient);
			memset(buffer, 0, MAX_REQUEST_SIZE + 1);
			//if the write failed, then the destination socket is likely closed
			if(written < 0){
				cout << "socket closed. Ending connections"<<endl;
				break;
			}
			//add the bytes we have processed
			*(comArgs->bytesTotal) += written;
		} else{
			//if the read failed, the source socket is likely closed
			cout << "socket closed. Ending connections"<<endl;
			break;
		}
		pthread_mutex_lock(comArgs->signalLock);
		local = comArgs->signalLock;
		pthread_mutex_unlock(comArgs->signalLock);
	}
	//set the flag so that other threads will also stop
	//we only get here if we break out of a loop on a closed socket,
	//or if another thread already toggled the flag
	pthread_mutex_lock(comArgs->signalLock);
	*(comArgs->keepLooping) = false;
	pthread_mutex_unlock(comArgs->signalLock);
	pthread_exit(NULL);
}

/**
 * Task for a thread to handle a connection from a client
 */
static void* handleConnection(void *handlerArgsStruct){
	int clientSrcPort, clientDstPort, serverFd, serverConnectionSourcePort, clientToServerBytes = 0, serverToClientBytes = 0;
	char clientDstPortBuf[100], iptablesBuffer[200], buffer[80], line[200];
	bool bound = false, proceed = true;
	time_t rawtime;
	struct tm * timeinfo;
	struct sockaddr_in clientSrcPortInfo, clientSrcIpInfo, clientOriginalDst, proxy_to_server_addr;
	struct addrinfo hints, *res;
	pthread_mutex_t signalLock;
	pthread_t clientToServer, serverToClient;
	pthread_attr_t attr1, attr2;
	void* status;
	volatile bool signal = true;
	struct communicator clientToServerArgs, serverToClientArgs;

	struct connectionArgs *handlerArgs= (struct connectionArgs*)(handlerArgsStruct);

	//get client src port
	socklen_t clientInfoLen = sizeof(sockaddr_in);
	if(getsockname(handlerArgs->connection_fd, (struct sockaddr *)&clientSrcPortInfo, &clientInfoLen) == -1){
		cout << "Could not get client src port information" << endl;
		proceed = false;
	}
	clientSrcPort = clientSrcPortInfo.sin_port;

	//get client src IP
	if(getpeername(handlerArgs->connection_fd, (sockaddr *)(&clientSrcIpInfo), &clientInfoLen) < 0){
		cout << "Could not get client src IP information" << endl;
		proceed = false;
	}
	string clientSrcIp = string(inet_ntoa(clientSrcIpInfo.sin_addr));

	//get client dest IP and port
	if(getsockopt(handlerArgs->connection_fd, SOL_IP, SO_ORIGINAL_DST, &clientOriginalDst, &clientInfoLen) < 0){
		cout << "Could not get original client destination"<<endl;
		proceed = false;
	}
	string clientDestIp = string(inet_ntoa(clientOriginalDst.sin_addr));
	clientDstPort = clientOriginalDst.sin_port;

	//create connection to client destination
	// first, load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	memset(clientDstPortBuf, 0, 100);
	sprintf(clientDstPortBuf, "%i", ntohs(clientDstPort));

	//lookup destination info
	if(getaddrinfo(clientDestIp.c_str(), clientDstPortBuf, &hints, &res) != 0){
		cout << "getaddrinfo failed for getting client destination"<<endl;
		proceed = false;
	}

	//create socket for connection to server
	if(proceed && (serverFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0){
		cout << "Could not get socket to connect to server"<<endl;
		proceed = false;
	}

	//setup struct for binding to a specific port number
	proxy_to_server_addr.sin_family = AF_INET;
	proxy_to_server_addr.sin_addr.s_addr = inet_addr(handlerArgs->serverSideIp.c_str());//INADDR_ANY

	while(!bound && proceed){
		//get a port number that is not in use by another thread
		getPortNumber(&serverConnectionSourcePort);
		proxy_to_server_addr.sin_port = htons(serverConnectionSourcePort);
		//bind to this port number so we can create an iptables rule using it
		if(bind(serverFd, (struct sockaddr*)&proxy_to_server_addr, sizeof(sockaddr_in)) < 0){
			cout << "Could not bind to: "<<handlerArgs->serverSideIp<<":"<<serverConnectionSourcePort<<endl;
		} else{
			//if this fails, then another process is using this port. try again
			//TODO: stop when we run out of port numbers (unlikely to happen)
			bound = true;
		}
	}

	//write an ip tables rule to SNAT all traffic to the client's dest
	memset(iptablesBuffer, 0 , 200);
	sprintf(iptablesBuffer, "iptables -t nat -A POSTROUTING -p tcp -d %s --sport %i -j SNAT --to-source %s", clientDestIp.c_str(), serverConnectionSourcePort, clientSrcIp.c_str());
	cout << "Writing iptables rule: "<<iptablesBuffer<<endl;
	system(iptablesBuffer);

	//connect to the server
	if(proceed && connect(serverFd, res->ai_addr, res->ai_addrlen) == -1){
		cout << "Could not connect to server"<<endl;
		proceed = false;
	}

	cout << "Handling connection from: " << clientSrcIp << ":" << ntohs(clientSrcPort) << " to: " << clientDestIp << ":" << ntohs(clientDstPort) << endl;

	//set time for logging
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(buffer,80,"%I:%M:%S",timeinfo);
	string date(buffer);

	//only proxy connection if a connection exists - e.g. no errors encountered before
	if(proceed){
		//setup scaffolding for threads
		pthread_mutex_init(&signalLock, NULL);
		pthread_attr_init(&attr1);
		pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_JOINABLE);
		pthread_attr_init(&attr2);
		pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_JOINABLE);

		clientToServerArgs.keepLooping = &signal;
		clientToServerArgs.signalLock = &signalLock;
		clientToServerArgs.source_fd = handlerArgs->connection_fd;
		clientToServerArgs.dest_fd = serverFd;
		clientToServerArgs.bytesTotal = &clientToServerBytes;

		serverToClientArgs.keepLooping = &signal;
		serverToClientArgs.signalLock = &signalLock;
		serverToClientArgs.source_fd = serverFd;
		serverToClientArgs.dest_fd = handlerArgs->connection_fd;
		serverToClientArgs.bytesTotal = &serverToClientBytes;

		//create threads to forward traffic
		pthread_create(&clientToServer, &attr1, connectSockets, (void*)(&clientToServerArgs));
		pthread_create(&serverToClient, &attr2, connectSockets, (void*)(&serverToClientArgs));

		//wait for threads to exit - when the connection ends
		pthread_join(clientToServer, &status);
		pthread_join(serverToClient, &status);

		//log this connection
		sprintf(line, "%s %s %i %s %i %i", date.c_str(), clientSrcIp.c_str(), clientSrcPort, clientDestIp.c_str(), clientDstPort, clientToServerBytes + serverToClientBytes);
		log(string(line));

		close(serverFd);
	} else{
		cout << "Error proxying connection"<<endl;
	}
	close(handlerArgs->connection_fd);
	pthread_exit(NULL);
}


/**
 * Runs a proxy that forwards requests from ip1 to ip2 and then routes backwards
 */
int TransparentProxy::runProxy(){
	//need to listen on the clientSide ip address
	//any connections will be handled directly by a spawned thread
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;
	char service[100], iptablesBuffer[200];

	struct sockaddr_in server_addr;
	sprintf(service, "%i", clientSidePort);

	//setup struct to create socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(clientSide.c_str());
	server_addr.sin_port = htons(clientSidePort);

	cout << "Running server" << endl;

	//request socket from system
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		return socket_fd;
	}

	//set socket reusable from previous program run
	int yes = 1;
	status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//bind to socket
	status = bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));
	if(status < 0){
		cout << "Error binding on socket" << endl;
		return status;
	} else{
		cout << "Running server on TCP port: "<<clientSidePort<<endl;
	}

	//listen on socket with a backlog of 10 connections
	status = listen(socket_fd, 100);
	if(status < 0){
		cout << "Error listening on bound socket" << endl;
		return status;
	} else{
		cout << "Listening on TCP socket" << endl;
	}


	//write iptables rule to DNAT all traffic on eth1 to this process
	memset(iptablesBuffer, 0 , 200);
	sprintf(iptablesBuffer, "iptables -t nat -A PREROUTING -p tcp -i eth1 -j DNAT --to-destination %s:%i", clientSide.c_str(), clientSidePort);
	cout << "Writing iptables rule: "<<iptablesBuffer<<endl;
	system(iptablesBuffer);
	memset(iptablesBuffer, 0, 200);

	while(true){
		//accept connections until program terminated
		char clientIp[INET_ADDRSTRLEN];
		cout << "Accepting new connections" << endl;

		struct sockaddr connection_addr;
		socklen_t connection_addr_size = sizeof(sockaddr);

		//block on accept until new connection
		new_connection_fd = accept(socket_fd, &connection_addr, &connection_addr_size);
		if(new_connection_fd < 0){
			cout << "Error accepting connection" <<endl;
		}

		//calculate ip of client
		int ip = ((struct sockaddr_in*)(&connection_addr))->sin_addr.s_addr;
		inet_ntop(AF_INET, &ip, clientIp, INET_ADDRSTRLEN);
		cout << "Accepted new connection from: " << string(clientIp) << " in socket: " << new_connection_fd << endl;

		//spin off new pthread to handle connection

		//setup thread and inputs
		pthread_t* current_connection_thread = (pthread_t*)malloc(sizeof(pthread_t));
		struct connectionArgs *args = new connectionArgs;
		args->connection_fd = new_connection_fd;
		args->clientSideIp = clientSide;
		args->clientSidePort = clientSidePort;
		args->serverSideIp = serverSide;

		//spawn thread. we do not wait on these, since we do not care when they finish
		//these threads handle closing the socket themselves
		rc = pthread_create(current_connection_thread, NULL, handleConnection, (void*)args);
		if(rc){
			cout << "Error creating thread: "<<thread_ids<<endl;
			return -1;
		}
	}
	return 0;
}

int main(int argc, char **argv){
	if(argc != 4){
		cout << "Need to have arguments: <client-side interface ip> <listen port> <server-side interface ip>"<<endl;
		return -1;
	}
	string clientSide(argv[1]);
	int clientSidePort = atoi(argv[2]);
	string serverSideIp(argv[3]);
	TransparentProxy proxy(clientSide, serverSideIp, clientSidePort);
	proxy.runProxy();
	return 0;
}
