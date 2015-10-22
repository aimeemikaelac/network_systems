/*
 * DFSServer.cpp
 *
 *  Created on: Oct 21, 2015
 *      Author: michael
 */

#include "DFSServer.h"

using namespace std;

DFSServer::DFSServer(string dir, int port, string userTablePath) {
	string actualTable = dir;
	actualTable.append("/");
	actualTable.append(userTablePath);
	parseUserTable(actualTable);
	runServer(dir, port);
}

DFSServer::~DFSServer() {
	// TODO Auto-generated destructor stub
}



void DFSServer::runServer(string directory, int port){
	if(chdir(directory.c_str()) < 0){
		cout << "Could not access the requested directory: "<<directory<<endl;
		return;
	}
	ConcurrentQueue queue;
	workerArgs *args = new workerArgs;
	args->workQueue = queue;
	args->userTable = userTable;
	//spawn worker thread
	pthread_t* worker_thread = (pthread_t*)malloc(sizeof(pthread_t));
	int rc = pthread_create(worker_thread, NULL, worker, (void*)args);
	if(rc){
		cout << "Error creating worker thread " << endl;
		return;
	}
	//now listen on the port forever
	serverListen(port, queue, handleConnection);
}

void DFSServer::parseUserTable(string table){
	int exists = 0, accessible = 0;
	checkFilepathExistsAccessible(table, &exists, &accessible);
	if(exists && accessible){
		ifstream file(table.c_str());
		string line;
		while(!file.eof()){
			if(getline(file, line)){
				char tempLine[line.npos];
				strcpy(tempLine, line.c_str());
				char *user = strtok(tempLine, " ");
				char *password = strtok(NULL, " ");
				if(userTable.count(user) == 0){
					userTable[string(user)] = list<string>;
				}
				list<string> current = userTable[string(user)];
				current.push_back(string(password));
			}
		}
	}
}

//void printHelp(){
//	cout << "Usage: dfs [-f FILE] [-p PORT] [-h]" << endl;
//	cout << "Options:" << endl;
//	cout << "  -f FILE\tSpecify a configuration file. If the file cannot be opened, reasonable defaults will be assumed. See README for format. The default is ./dfs.conf."<<endl;
//	cout << "  -p PORT\tSpecify a service port. Will be ignored if port is set in a configuration file. The default is 8000." <<endl;
//	cout << "  -h\t\tPrint this message and exit."<<endl;
//}

int main(int argc, char** argv){
	int opt;
	string userTableFile = "./dfs.conf";
	if(argc<3){
		cout << "Incorrect number of arguments" <<endl;
	}
	string directory(argv[1]);
	int port = atoi(argv[2]);
	DFSServer server(directory, port, userTableFile);
//	bool regularDirectory = false
//	if(argc > 1){
//		while(optind < argc){
//			if((opt = getopt(argc, argv, "f:d:p:h")) != -1){
//				switch(opt){
//					case 'f':
//						configFile = string(optarg);
//						break;
//					case 'p':
//						port = atoi(optarg);
//						break;
//					case 'h':
//						printHelp();
//						return -1;
//					case '?':
//						printHelp();
//						return -1;
//				}
//			} else{
//
//			}
//		}
//	}
}
