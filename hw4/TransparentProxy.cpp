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

static void log(string line){
	ofstream myfile;
	pthread_mutex_lock(&logmutex);
	myfile.open ("transparentproxy.log", ios::ate | ios::out);
	myfile << line << endl;
	myfile.close();
	pthread_mutex_unlock(&logmutex);
}

static void getPortNumber(int *portOut){
	static int port = 2000;
	pthread_mutex_lock(&portmutex);
	*portOut = port;
	port++;
	pthread_mutex_unlock(&portmutex);
}

static void* connectSockets(void *args){
	struct communicator *comArgs = (struct communicator*)args;
	char buffer[MAX_REQUEST_SIZE + 1];
	//loop while the client and server connection are open
	pthread_mutex_lock(comArgs->signalLock);
	bool local = comArgs->signalLock;
	pthread_mutex_unlock(comArgs->signalLock);
	while(local){
		memset(buffer, 0, MAX_REQUEST_SIZE + 1);
		int receivedClient;
		int written;
		if((receivedClient = recv(comArgs->source_fd, buffer, MAX_REQUEST_SIZE, 0)) > 0){
			written = write(comArgs->dest_fd, buffer, receivedClient);
			memset(buffer, 0, MAX_REQUEST_SIZE + 1);
			if(written < 0){
				cout << "Dest socket closed. Ending connections"<<endl;
				pthread_mutex_lock(comArgs->signalLock);
				*(comArgs->keepLooping) = false;
				pthread_mutex_unlock(comArgs->signalLock);
				break;
			}
		}
		pthread_mutex_lock(comArgs->signalLock);
		local = comArgs->signalLock;
		pthread_mutex_unlock(comArgs->signalLock);
	}
	pthread_exit(NULL);
}


static void* handleConnection(void *handlerArgsStruct){
	struct connectionArgs *handlerArgs= (struct connectionArgs*)(handlerArgsStruct);
	struct sockaddr_in clientSrcPortInfo;
	socklen_t clientInfoLen = sizeof(sockaddr_in);
	if(getsockname(handlerArgs->connection_fd, (struct sockaddr *)&clientSrcPortInfo, &clientInfoLen) == -1){
		cout << "Could not get client src port information" << endl;
		//fprintf(stderr, "%s\n", explain_getsockname(handlerArgs->connection_fd, &clientSrcPortInfo, &clientInfoLen));
		//exit(-1);
	}
	int clientSrcPort = clientSrcPortInfo.sin_port;
	cout << "Source port: "<<ntohs(clientSrcPort)<<endl;
	struct sockaddr_in clientSrcIpInfo;
	if(getpeername(handlerArgs->connection_fd, (sockaddr *)(&clientSrcIpInfo), &clientInfoLen) < 0){
		cout << "Could not get client src IP information" << endl;
	}
	string clientSrcIp = string(inet_ntoa(clientSrcIpInfo.sin_addr));
	struct sockaddr_in clientOriginalDst;
	if(getsockopt(handlerArgs->connection_fd, SOL_IP, SO_ORIGINAL_DST, &clientOriginalDst, &clientInfoLen) < 0){
		cout << "Could not get original client destination"<<endl;
	}
	string clientDestIp = string(inet_ntoa(clientOriginalDst.sin_addr));
	int clientDstPort = clientOriginalDst.sin_port;

	int serverFd;
	struct addrinfo hints, *res;

	// first, load up address structs with getaddrinfo():
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char clientDstPortBuf[100];
	memset(clientDstPortBuf, 0, 100);
	sprintf(clientDstPortBuf, "%i", ntohs(clientDstPort));
	
	if(getaddrinfo(clientDestIp.c_str(), clientDstPortBuf, &hints, &res) != 0){
		cout << "getaddrinfo failed for getting client destination"<<endl;
	}

	cout << "Client destination: "<<clientDestIp<<":"<<clientDstPortBuf<<endl;

	if((serverFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0){
		cout << "Could not get socket to connect to server"<<endl;
		exit(-1);
	}

	//bind to socket

	bool bound = false;
	int serverConnectionSourcePort;
	struct sockaddr_in proxy_to_server_addr;

	//setup struct to create socket
	proxy_to_server_addr.sin_family = AF_INET;
	proxy_to_server_addr.sin_addr.s_addr = inet_addr(handlerArgs->serverSideIp.c_str());//INADDR_ANY

	while(!bound){
		getPortNumber(&serverConnectionSourcePort);
		proxy_to_server_addr.sin_port = htons(serverConnectionSourcePort);
		if(bind(serverFd, (struct sockaddr*)&proxy_to_server_addr, sizeof(sockaddr_in)) < 0){
			cout << "Could not bind to: "<<handlerArgs->serverSideIp<<":"<<serverConnectionSourcePort<<endl;
		} else{
			bound = true;
			cout << "Successfully bound to: "<<handlerArgs->serverSideIp<<":"<<serverConnectionSourcePort<<endl;
		}
	}

	cout << "ServerConnectionSourcePort: "<<serverConnectionSourcePort<<endl;

	char iptablesBuffer[200];
	memset(iptablesBuffer, 0 , 200);

	sprintf(iptablesBuffer, "iptables -t nat -A POSTROUTING -p tcp -d %s --sport %i -j SNAT --to-source %s", clientDestIp.c_str(), serverConnectionSourcePort, clientSrcIp.c_str());
	cout << "Writing iptables rule: "<<iptablesBuffer<<endl;

	system(iptablesBuffer);


	if(connect(serverFd, res->ai_addr, res->ai_addrlen) == -1){
		cout << "Could not connect to server"<<endl;
		pthread_exit(NULL);
	}

	cout << "Handling connection from: " << clientSrcIp << ":" << ntohs(clientSrcPort) << " to: " << clientDestIp << ":" << ntohs(clientDstPort) << endl;
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];

	time (&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer,80,"%I:%M:%S",timeinfo);
	string date(buffer);


	volatile bool signal = true;
	pthread_mutex_t signalLock;

	pthread_mutex_init(&signalLock, NULL);

	pthread_t clientToServer;
	pthread_attr_t attr1;
	pthread_attr_init(&attr1);
	pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_JOINABLE);

	pthread_t serverToClient;
	pthread_attr_t attr2;
	pthread_attr_init(&attr2);
	pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_JOINABLE);

	struct communicator clientToServerArgs;
	int clientToServerBytes = 0;
	clientToServerArgs.keepLooping = &signal;
	clientToServerArgs.signalLock = &signalLock;
	clientToServerArgs.source_fd = handlerArgs->connection_fd;
	clientToServerArgs.dest_fd = serverFd;
	clientToServerArgs.bytesTotal = &clientToServerBytes;

	struct communicator serverToClientArgs;
	int serverToClientBytes = 0;
	serverToClientArgs.keepLooping = &signal;
	serverToClientArgs.signalLock = &signalLock;
	serverToClientArgs.source_fd = serverFd;
	serverToClientArgs.dest_fd = handlerArgs->connection_fd;
	serverToClientArgs.bytesTotal = &serverToClientBytes;

	pthread_create(&clientToServer, &attr1, connectSockets, (void*)(&clientToServerArgs));
	pthread_create(&serverToClient, &attr2, connectSockets, (void*)(&serverToClientArgs));

	void* status;
	pthread_join(clientToServer, &status);
	pthread_join(serverToClient, &status);

	char line[200];
	sprintf(line, "%s %s %i %s %i %i", date.c_str(), clientSrcIp.c_str(), clientSrcPort, clientDestIp.c_str(), clientDstPort, clientToServerBytes + serverToClientBytes);

	cout << "Logging: "<< line << endl;

	log(string(line));

	close(handlerArgs->connection_fd);
	close(serverFd);
	pthread_exit(NULL);
}


/**
 * Runs a proxy that forwards requests from ip1 to ip2 and then routes backwards
 */
int TransparentProxy::runProxy(){
	//need to listen on the clientSide ip address
	//any connections will be handled directly by a spawned thread
	int socket_fd, status, new_connection_fd, rc, thread_ids = 0;

	list<pthread_t*> threads;
	struct sockaddr_in server_addr;
	char service[100];
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


	char iptablesBuffer[200];
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
		thread_ids++;
		threads.push_back(current_connection_thread);
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
