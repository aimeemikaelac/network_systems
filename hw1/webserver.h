//Webserver.h
#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <list>
#include <wctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include "ConcurrentQueue.h"


class Webserver{
	public:
		Webserver(std::string);
		~Webserver();
		int runServer();

		enum ConfigOptions{ServicePort, DocumentRoot, DirectoryIndex, ContentType };

	private:
		int parseFile(std::string);
		bool parseOption(std::string, ConfigOptions);

		struct connectionArguments;

		enum ConfigOptions;

		//private class variables
		std::string fileName;
		std::string documentRoot;
		std::set<std::string> documentIndex;
		std::map<std::string, std::string> contentTypes;
		unsigned servicePort;
};
