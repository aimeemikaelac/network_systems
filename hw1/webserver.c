#include <stdio.h>
#include <stdlib.h>

static unsigned servicePort;

static char* documentRoot;

static 

int readline(File *configFile, char* buffer, size_t bufferLength){
	if(fgets(buffer, bufferLength, configFile) != NULL){
		buffer[bufferLength-1] = '\0';
		return 0;
	} else{
		return -1;
	}
}

int parseLine(char* line, size_t bufferLength){
	



//parse the config file indicated by file name
int parseConfigFile(char* fileName){
	size_t bufferLength = 1024;
	char[bufferLength] line;
	File *configFile = NULL;
//	ifstream configFile;
//	configFile.open(fileName);
//	if(!configFile.is_open()){
	if((configFile = fopen(fileName)) == NULL){
		printf("\nCould not open config file");
//		cout << endl << "Could not open config file" << endl;
		return -1;
	}
	
	while(readLine(configFile, line, bufferLength){
		parseLine(line, 1024);
	}
}



int main(){

}


