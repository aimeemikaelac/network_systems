/*
 * DFSClient.cpp
 *
 *  Created on: Oct 22, 2015
 *      Author: michael
 */

#include "DFSClient.h"

using namespace std;

DFSClient::DFSClient(string configFile) {
	parseConfig(configFile);
	serverHashedPassword = (unsigned char*)malloc(SHA256_DIGEST_LENGTH);
	serverHashPerformed = false;
	cli();
}

DFSClient::~DFSClient() {
	// TODO Auto-generated destructor stub
}

void DFSClient::parseConfig(string file){
	int exists =0, accessible = 0;
	checkFilepathExistsAccessible(file, &exists, &accessible);
	//check if we can access the file
	if(exists == 0 || accessible == 0){
		cout << "Cannot access config file at: " << file <<endl;
		exit(-1);
	}
	ifstream fileStream(file.c_str());
	string line;
	int numServers = 0;
	while(!fileStream.eof()){
		if(getline(fileStream, line)){
			char tempLine[line.length()];
			strcpy(tempLine, line.c_str());
			char* firstElement = strtok(tempLine, " ");
			if(firstElement == NULL){
				continue;
			}
			if(strcmp("Server", firstElement) == 0){
				char* serverName = strtok(NULL, " ");
				if(serverName == NULL){
					continue;
				}
				char* ipAndPort = strtok(NULL, " ");
				if(ipAndPort == NULL){
					continue;
				}
				char* ip = strtok(ipAndPort, ":");
				if(ip == NULL){
					continue;
				}
				char* port = strtok(NULL, ":");
				if(port == NULL){
					continue;
				}
				serverToIpMap[string(serverName)] = string(ip);
				serverToPortMap[string(serverName)] = (int)strtol(port, NULL, 10);
				numServers++;
			} else if(strcmp("Username:", firstElement) == 0){
				char* username = strtok(NULL, " ");
				if(username == NULL){
					exit(-1);
				}
				user = string(username);
			} else if(strcmp("Password:", firstElement) == 0){
				char* pass = strtok(NULL, " ");
				if(pass == NULL){
					exit(-1);
				}
				password = string(pass);
			} else{
				continue;
			}
		}
	}
	if(numServers > 4){
		cout << "Too many servers for assignment" <<endl;
	}
	if(numServers < 4){
		cout << "Too few servers for assignment" <<endl;
	}
}

void DFSClient::cli(){
	string line;
	string response;
	while(true){
		cout << ">> ";
		getline(cin, line);
		if(cin.eof() == 1){
			return;
		}
		response =  parseCommand(line);
		cout << response << endl;
	}
}

fileNamePair DFSClient::getFilePart(string fileName){
	char temp[fileName.length()];
	string baseFile("");
	strcpy(temp, fileName.c_str());
	char *current;
	char *prev = "";
	current = strtok(temp, ".");
	bool skip = true;
	while(current != NULL){
		baseFile.append(prev);
		prev = current;
		current = strtok(NULL, ".");
		if(current != NULL && !skip){
			baseFile.append(".");
		}
		skip = false;
	}
	fileNamePair pair;
	pair.part = -1;
	pair.fullFilePartName = "";
	pair.baseFile = "";
	pair.server = "";
	if(prev == NULL){
		return pair;
	}
	char *end;
	long partNum = strtol(prev, &end, 10);
	if(strcmp(end, "") != 0){
		return pair;
	}
	pair.baseFile = baseFile;
	pair.fullFilePartName = fileName;
	pair.part = (int)partNum;
	return pair;
}

receivedResponse DFSClient::writeStringToServer(string ip, int port, unsigned char* message, int messageLength){
	int rc, socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server;
	if(socket_fd < 0){
		cout << "Error opening socket file descriptor" << endl;
		exit(-1);
	}
	struct in_addr inet_temp;
	if(inet_aton(ip.c_str(), &inet_temp) == -1){
		cout << "Invalid ip address: "<<ip<<endl;
		exit(-1);
	}
	receivedResponse response;
	response.contents = NULL;
	response.length = 0;
	server.sin_addr.s_addr = inet_addr(ip.c_str());
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	rc = connect(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if(rc < 0){
		cout << "Could not connect to server: "<<ip<<":"<<port<<endl;
		response.contents =(unsigned char*)"Error: Failed to connect";
		response.length = strlen("Error: Failed to connect");
		return response;
	}
	rc = send(socket_fd, message, messageLength, 0);
	if(rc < 0){
		cout << "Could not send data to server: "<<ip<<":"<<port<<endl;
		response.contents =(unsigned char*)"Error: Failed to send";
		response.length = strlen("Error: Failed to send");
		return response;
	}
	char buffer[BUFFER_SIZE+1];
	response.length = 0;
	int bytesReceived;
	do{
		memset(buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket_fd, buffer, BUFFER_SIZE, 0);
		if(bytesReceived < 0){
			cout << "Error receiving from socket"<<endl;
			response.contents =(unsigned char*)"Error: Failed to receive";
			response.length = strlen("Error: Failed to receive");
			return response;
		}
		response.contents = (unsigned char*)realloc(response.contents, bytesReceived+response.length);
		memcpy(response.contents+response.length, buffer, bytesReceived);
		response.length = bytesReceived+response.length;
	} while(bytesReceived > 0);
	return response;
}

void DFSClient::getSha256String(unsigned char* input, int inputLength, unsigned char* out){
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, input, inputLength);
	SHA256_Final(out, &sha256);
}

void DFSClient::getSha256Key(unsigned char* out){
	getSha256String((unsigned char*)password.c_str(), password.length(), out);
}

void DFSClient::getRandomIv(unsigned char* iv){
	memset(iv, 0, 17);
	if(!RAND_bytes(iv, 16)){
		cout << "OpenSSl random number generator encountered an error"<<endl;
		exit(-1);
	}
}

encryptedFile* DFSClient::readAndEncrypt(string filePath){
	ifstream file(filePath.c_str());
	//read whole file contents into string
	string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	unsigned char keyBuf[SHA256_DIGEST_LENGTH];
	encryptedFile *encFile = (encryptedFile*)malloc(sizeof(encryptedFile));
	encFile->iv = (unsigned char*)malloc(17);
	getSha256Key(keyBuf);
	getRandomIv(encFile->iv);
	int encryptedBytes;
	int finalEncryptedBytes;
	unsigned char *ciphertext = (unsigned char*)malloc(contents.length() + 64);
	memset(ciphertext, 0, contents.length() + 64);
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_256_ctr(), NULL, keyBuf, encFile->iv);
	EVP_EncryptUpdate(&ctx, ciphertext, &encryptedBytes, (unsigned char*)contents.c_str(), contents.length());
	EVP_EncryptFinal(&ctx, ciphertext+encryptedBytes, &finalEncryptedBytes);
	encFile->contents = ciphertext;
	encFile->contentLength = encryptedBytes + finalEncryptedBytes;
	return encFile;
}

void DFSClient::decryptAndWrite(string filePath, encryptedFile encryptedContents){
	int exists =0, accesible=0;
	checkFilepathExistsAccessible(filePath, &exists, &accesible);
	if(exists == 1 && accesible == 0){
		cout << "\nCannot access file path"<<endl;
		return;
	}
	int i;

	unsigned char plaintext[encryptedContents.contentLength + 16];
	memset(plaintext, 0, encryptedContents.contentLength + 16);
	EVP_CIPHER_CTX ctx;
	unsigned char key[SHA256_DIGEST_LENGTH];
	getSha256Key(key);
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_256_ctr(), NULL, key, encryptedContents.iv);
	int decryptedBytes;
	int finalDecryptedbytes;
	EVP_DecryptUpdate(&ctx, plaintext, &decryptedBytes, (unsigned char*)encryptedContents.contents, encryptedContents.contentLength);
	EVP_DecryptFinal(&ctx, plaintext+decryptedBytes, &finalDecryptedbytes);
	ofstream out;
	out.open(filePath.c_str(), ofstream::out|ofstream::trunc);
	out.write((char*)plaintext, decryptedBytes+finalDecryptedbytes);
	out.flush();
	out.close();
}

void DFSClient::getServerPasswordHash(unsigned char* out, int outLength){
#ifdef SERVER_PT_PASSWORD
	memcpy(out, password.c_str(), outLength);
#else
	if(serverHashPerformed){
		memcpy(out, serverHashedPassword, outLength);
	}
	unsigned char hash[SHA256_DIGEST_LENGTH];
	getSha256Key(hash);
	int i;
	for(i=0; i<HASH_ROUNDS; i++){
		getSha256String(hash, SHA256_DIGEST_LENGTH, hash);
	}
	serverHashPerformed = true;
	memcpy(out, serverHashedPassword, outLength);
#endif
}

string DFSClient::getServerPasswordHashString(unsigned char* hash){
#ifdef SERVER_PT_PASSWORD
	return string((char*)hash);
#else
	string out("");
	int i;
	for(i=0; i<SHA256_DIGEST_LENGTH; i++){
		char buf[100];
		sprintf(buf, "%02x", hash[i]);
		out.append(buf);
	}
	return out;
#endif
}

string DFSClient::getServerPasswordHashStringDirect(){
#ifdef SERVER_PT_PASSWORD
	unsigned char hash[password.length() + 1];
	memset(hash, 0, password.length()+1);
	getServerPasswordHash(hash, password.length());
	return getServerPasswordHashString(hash);
#else
	unsigned char hash[SHA256_DIGEST_LENGTH];
	getServerPasswordHash(hash, SHA256_DIGEST_LENGTH);
	return getServerPasswordHashString(hash);
#endif
}

string DFSClient::writeListToServer(string ip, int port){
	string message = "LIST " + user + " " + getServerPasswordHashStringDirect();
	receivedResponse response = writeStringToServer(ip, port, (unsigned char*)message.c_str(), message.length());
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

string DFSClient::writeMkdirToServer(string ip, int port, string folder){
	string mkdirMessage = "MKDIR "+user+" "+getServerPasswordHashStringDirect()+" "+folder;
	receivedResponse response = writeStringToServer(ip, port, (unsigned char*)mkdirMessage.c_str(), mkdirMessage.length());
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

receivedResponse DFSClient::writeGetToServer(string ip, int port, string file){
	string getMessage = "GET "+user+" "+getServerPasswordHashStringDirect()+" "+file;
	return writeStringToServer(ip, port, (unsigned char*)getMessage.c_str(), getMessage.length());
}

string DFSClient::writePutToServer(string ip, int port, string fileName, unsigned char* fileContents, int contentsLength){
	char length[100];
	memset(length, 0, 100);
	sprintf(length, "%d", contentsLength);
	string putMessage = "PUT " +user + " "+ getServerPasswordHashStringDirect()+" "+fileName+" "+string(length)+"\n";
	unsigned char totalPutMessage[putMessage.length() + contentsLength];
	memcpy(totalPutMessage, putMessage.c_str(), putMessage.length());
	memcpy(totalPutMessage + putMessage.length(), fileContents, contentsLength);
	receivedResponse response = writeStringToServer(ip, port, totalPutMessage, putMessage.length() + contentsLength);
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

int DFSClient::parseListResponse(string server, string message){
	char temp[message.length()];
	strcpy(temp, message.c_str());
	char *totalFilesHeader = strtok(temp, " ");
	if(message.find("Error:") == 0){
		//encountered error
		return 0;
	}
	if(message.find("Invalid Username/Password. Please try again.")==0){
		cout << "Invalid Username/Password. Please try again."<<endl;
		return 0;
	}
	if(totalFilesHeader == NULL || (strcmp("Total_Files:", totalFilesHeader) != 0)){
		cout << "Could not parse total files header in LIST response" << endl;
		return -1;
	}
	char *totalFilesNumChar = strtok(NULL, " ");
	char *end;
	int totalFilesNum = strtol(totalFilesNumChar, &end, 10);
	if(totalFilesNum == 0){
		return 0;
	}

	string whitespaces (" \t\f\v\n\r");
	string current(message);
	char *lineChar = NULL;
	do{
		char temp2[current.length()];
		strcpy(temp2, current.c_str());
		lineChar = strtok(temp2, "\n");
		if(lineChar == NULL){
			break;
		}
		string line(lineChar);
		if(line.find("Total_Files:") != 0 && line.find_first_not_of(whitespaces) == 0 && line.find("Files:") != 0){
			string searchLine = line;
			if(line.find("/") != line.npos){
				searchLine = string(line, line.find("/") +1);
			}
			struct fileNamePair pair = getFilePart(searchLine);
			if(filesToParts.count(pair.baseFile) == 0){
				filesToParts[pair.baseFile] = vector<fileNamePair>();
			}
			vector<struct fileNamePair> currentInfo = filesToParts[pair.baseFile];
			pair.server = server;
			currentInfo.push_back(pair);
			filesToParts[pair.baseFile] = currentInfo;
		}
		string next(current, current.find("\n") + 1);
		current = next;
	} while(current.find("\n") != current.npos);
	return totalFilesNum;
}

int DFSClient::numParts(string file){
	vector<struct fileNamePair> currentFileParts = filesToParts[file];
	vector<struct fileNamePair>::iterator fileIterator;
	int currentPart;
	int sum = 0;
	for(currentPart = 1; currentPart<=4; currentPart++){
		for(fileIterator = currentFileParts.begin(); fileIterator != currentFileParts.end(); fileIterator++){
			fileNamePair currentPair = *fileIterator;
			if(currentPair.part == currentPart){
				sum++;
				break;
			}
		}
	}
	return sum;
}

string DFSClient::sendList(){
	filesToParts.clear();
	map<string, string>::iterator it;
	for(it = serverToIpMap.begin(); it != serverToIpMap.end(); it++){
		string server = (string)it->first;
		string ip = (string)it->second;
		int port = (int)serverToPortMap[server];
		string response = writeListToServer(ip, port);
		parseListResponse(server, response);
	}
	string message = "LIST\n\n";
	map<string, vector<struct fileNamePair> >::iterator it2;
	for(it2 = filesToParts.begin(); it2 != filesToParts.end(); it2++){
		int parts= numParts(it2->first);
		message.append(it2->first);
		if(parts != 4){
			message.append(" [incomplete]");
		}
		message.append("\n");
	}
	return message;
}

string DFSClient::sendGet(string filePath){
	//clear current info on files and rebuild, in case someone dropped out
	sendList();
	int numCurrentParts = numParts(filePath);
	if(numCurrentParts < 4){
		return "Not enough parts to rebuild file";
	}
	vector<struct fileNamePair> currentFileInfo = filesToParts[filePath];
	int i;
	string totalHashes[4];
	set<string> ivs;
	string response("");
	encryptedFile encFile;
	encFile.contents = NULL;
	encFile.contentLength = 0;
	for(i=1; i<=4; i++){
		vector<string> servers;
		vector<struct fileNamePair>::iterator it;
		for(it = currentFileInfo.begin(); it != currentFileInfo.end(); it++){
			struct fileNamePair currentPair = *it;
			if(currentPair.part == i){
				servers.push_back(currentPair.server);
			}
		}
		vector<string>::iterator it2;
		receivedResponse file;
		bool success = false;
		for(it2=servers.begin(); it2!=servers.end(); it2++){
			bool hashMatched = true;
			string ip = serverToIpMap[(string)(*it2)];
			int port = serverToPortMap[(string)(*it2)];
			char partChar[100];
			sprintf(partChar, "%d", i);
			file = writeGetToServer(ip, port, filePath+"."+string(partChar));
			char fileTemp[file.length+1];
			memset(fileTemp, 0, file.length+1);
			memcpy(fileTemp, file.contents, file.length);
			string fileStr(fileTemp);
			string originalFileStr(fileStr);
			if(fileStr.find("Error") == 0){
				cout << "Error receiving file on server: "<<*it2 <<": "<<fileStr<<endl;
				continue;
			}
			unsigned char currentHash[MD5_DIGEST_LENGTH+1];
			memset(currentHash, 0, MD5_DIGEST_LENGTH+1);

			string currentTotalHash(string(fileStr.c_str()), 0, fileStr.find("\n"));
			totalHashes[i-1] = currentTotalHash;

			fileStr = string(string(fileStr.c_str()), fileStr.find("\n")+1);

			string currentPartHash(fileStr, 0, fileStr.find("\n"));
			string currentIv(fileStr, fileStr.find("\n")+1, 32);

			ivs.insert(currentIv);

			//the offset from the begging of the received file to the raw encrypted data
			//is the size of the file minus the stores total hash size, partial hash size and iv size,
			//and the 3 newlines separating these values
			int newlineOffset = fileStr.find("\n") + currentPartHash.length() + currentTotalHash.length() + 3;
			int j;
			//subtract one from the raw size to get rid of the trailing newline
			int rawFileSize = file.length - newlineOffset-1;
			unsigned char rawFile[rawFileSize];
			memcpy(rawFile, file.contents+newlineOffset, rawFileSize);
//			fileStr = string(fileStr, 0, fileStr.find("\n"));
//			int lastNewline = file.find_last_of("\n");
//			file = file.substr(0, lastNewline);
//			cout << "File: \n"<<file<<endl;
			MD5(rawFile, rawFileSize, currentHash);
			string currentHashStr = md5ToHexString((unsigned char*)currentHash);
			if(currentHashStr.compare(currentPartHash) != 0){
				hashMatched = false;
				response.append("Hash mismatch. Received part hash: "+currentHashStr+
						" Stored hash in file: "+currentPartHash+"\n");
			}
			if(originalFileStr.find("Error") != 0 && hashMatched){
				encFile.contents = (unsigned char*)realloc(encFile.contents, encFile.contentLength+rawFileSize);
				memcpy(encFile.contents + encFile.contentLength, rawFile, rawFileSize);
				encFile.contentLength+=rawFileSize;
				success = true;
				break;
			}
		}
		if(!success){
			char iStr[100];
			sprintf(iStr, "%d", i);
			return response + "Error receiving part: "+string(iStr);
		}
	}
	unsigned char totalHash[MD5_DIGEST_LENGTH+1];
	memset(totalHash, 0, MD5_DIGEST_LENGTH+1);
	MD5(encFile.contents, encFile.contentLength, totalHash);
	string totalHashStr = md5ToHexString((unsigned char*)totalHash);
	bool allMatched = true;
	for(i=0; i<4; i++){
		if(totalHashStr.compare(totalHashes[i]) != 0){
			allMatched = false;
			char iStr[100];
			sprintf(iStr, "%d", i+1);
			response.append("\nHash from part "+string(iStr)+"does not match total hash");
		}
	}
	if(!allMatched){
		response.append("\nTotal hash did not match for all parts");
	}
	if(ivs.size() != 1){
		response.append("\nIncorrect number of ivs encountered. Decryption may fail");
	}
	set<string>::iterator beginning = ivs.begin();
	encFile.iv = (unsigned char*)malloc(17);
	hexToIvString(*beginning, encFile.iv);
	decryptAndWrite(filePath, encFile);
	return string(response+"Did GET on: "+filePath);
}

void DFSClient::hexToIvString(string ivHex, unsigned char* iv){
//	string iv("");
	unsigned char ivHexChar[100];
	memset(ivHexChar, 0, 100);
	memset(iv, 0, 17);
	strcpy((char*)ivHexChar, ivHex.c_str());
	int i;
	for(i=0; i<16; i++){
		unsigned char tempBuf[3];
		memset(tempBuf, 0, 3);
		tempBuf[0] = ivHexChar[i*2];
		tempBuf[1] = ivHexChar[i*2+1];
		iv[i] = (unsigned char)strtoul((char*)tempBuf, NULL, 16);
	}
}

string DFSClient::ivToHexString(unsigned char* iv){
	int i;
	string out("");
	unsigned char* ivChar = (unsigned char*)iv;
	for(i=0; i<16; i++){
		char buf[100];
		memset(buf, 0, 100);
		sprintf(buf, "%02x", ivChar[i]);
		out.append(buf);
	}
	return out;
}

string DFSClient::md5ToHexString(unsigned char* hash){
	int i;
	string out("");
	unsigned char* hashChar = (unsigned char*)hash;
	for(i=0; i<MD5_DIGEST_LENGTH; i++){
		char buf[100];
		memset(buf, 0, 100);
		sprintf(buf, "%02x", hashChar[i]);
		out.append(buf);
	}
	return out;
}

string DFSClient::sendPut(string filePath){
	int exists=0, accessible=0;
	int i,j;
	checkFilepathExistsAccessible(filePath, &exists, &accessible);
	if(!(exists==1 && accessible== 1)){
		return "Could not access file to upload";
	}
	unsigned char totalDigest[MD5_DIGEST_LENGTH+1];
	unsigned char part1Digest[MD5_DIGEST_LENGTH+1];
	unsigned char part2Digest[MD5_DIGEST_LENGTH+1];
	unsigned char part3Digest[MD5_DIGEST_LENGTH+1];
	unsigned char part4Digest[MD5_DIGEST_LENGTH+1];
	memset(totalDigest, 0, MD5_DIGEST_LENGTH+1);
	memset(part1Digest, 0, MD5_DIGEST_LENGTH+1);
	memset(part2Digest, 0, MD5_DIGEST_LENGTH+1);
	memset(part3Digest, 0, MD5_DIGEST_LENGTH+1);
	memset(part4Digest, 0, MD5_DIGEST_LENGTH+1);

	encryptedFile *encryptedContents = readAndEncrypt(filePath);

	int length = encryptedContents->contentLength;
	unsigned char part1[length/4];
	memcpy(part1, encryptedContents->contents, length/4);
	unsigned char part2[length/4];
	memcpy(part2, encryptedContents->contents+length/4, length/4);
	unsigned char part3[length/4];
	memcpy(part3, encryptedContents->contents+2*(length/4), length/4);
	unsigned char part4[length/4];
	int remaining = encryptedContents->contentLength - 3*(length/4);
	memcpy(part4, encryptedContents->contents+3*(length/4), remaining);
	MD5(encryptedContents->contents, encryptedContents->contentLength, totalDigest);
	MD5(part1, length/4, part1Digest);
	MD5(part2, length/4, part2Digest);
	MD5(part3, length/4, part3Digest);
	MD5(part4, remaining, part4Digest);
	int partsToServer[4][2];
	memset(partsToServer, 0, 8*sizeof(int));
	int partsSchemeIndex = (int)(totalDigest[MD5_DIGEST_LENGTH-1]) % 4;
	string message("");

	string response("");
	for(i=0; i<4; i++){
		for(j=0; j<2; j++){
			string server;
			unsigned char* partToSend;
			unsigned char* digestToSend;
			int partLength = length/4;
			switch(i){
				case 0:
					server = "DFS1";
					break;
				case 1:
					server = "DFS2";
					break;
				case 2:
					server = "DFS3";
					break;
				case 3:
					server = "DFS4";
					break;
			}
			int part =serverOrder[partsSchemeIndex][i][j];
			switch(part){
				case 1:
					partToSend = part1;
					digestToSend = part1Digest;
					break;
				case 2:
					partToSend = part2;
					digestToSend = part2Digest;
					break;
				case 3:
					partToSend = part3;
					digestToSend = part3Digest;
					break;
				case 4:
					partToSend = part4;
					digestToSend = part4Digest;
					partLength = remaining;
					break;
			}
			if(serverToIpMap.find(server) == serverToIpMap.end()){
				message.append("Could not find server "+server+" in configured servers\n");
			}
			char partStr[100];
			sprintf(partStr, "%d", part);
			string fileNameToSend = filePath +"."+string(partStr);//+ md5ToHexString(totalDigest)+"."
			string ip = serverToIpMap[server];
			int port = serverToPortMap[server];
			string partWithHashes = md5ToHexString(totalDigest) + "\n" +
					md5ToHexString(digestToSend)+"\n"+
					ivToHexString(encryptedContents->iv)+"\n";
			unsigned char totalPartWithHashes[partWithHashes.length()+partLength];
			memcpy(totalPartWithHashes, partWithHashes.c_str(), partWithHashes.length());
			memcpy(totalPartWithHashes+partWithHashes.length(), partToSend, partLength);
			string serverResponse = writePutToServer(ip, port, fileNameToSend, totalPartWithHashes, partWithHashes.length()+partLength);
			if(serverResponse.length()>0){
				response.append("\n"+serverResponse);
			}
		}
	}
	return string("Did PUT on: "+filePath+"\nAnd received response: "+response);
}

string DFSClient::sendMkdir(string filePath){
	map<string, string>::iterator it;
	for(it = serverToIpMap.begin(); it != serverToIpMap.end(); it++){
		string server = (string)it->first;
		string ip = (string)it->second;
		int port = (int)serverToPortMap[server];
		string response = writeMkdirToServer(ip, port, filePath);
		if(response.find("Created directory") == response.npos){
			cout << "Error creating directory on server: "+server<<endl;
		}
	}
	return string("Did MKDIR on: ").append(filePath);
}

string DFSClient::parseCommand(string command){
	char temp[command.length()];
	strcpy(temp, command.c_str());
	char* inCommand = strtok(temp, " ");
	if(inCommand == NULL){
		return string("Invalid command: ").append(command);
	}
	RequestOption commandOption = getCommand(inCommand);
	if(commandOption == NONE){
		return string("Invalid command: ").append(command);
	}
	string response;
	char* filePath = strtok(NULL, " ");
	char* lastArg = filePath != NULL ? strtok(NULL, " ") : NULL;
	switch(commandOption){
		case LIST:
			response = filePath != NULL ? string("Invalid argument to LIST: ").append(filePath) : sendList();
			break;
		case GET:
			if(lastArg != NULL){
				response = string("GET does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendGet(string(filePath)) : string("GET requires a file path");
			}
			break;
		case PUT:
			if(lastArg != NULL){
				response = string("PUT does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendPut(string(filePath)) : string("PUT requires a file path");
			}
			break;
		case MKDIR:
			if(lastArg != NULL){
				response = string("MKDIR does not take a third argument: ").append(lastArg);
			} else{
				response = filePath != NULL ? sendMkdir(string(filePath)) : string("MKDIR requires a file path");
			}
			break;
		default:
			response = string("Invalid command: ").append(command);
	}
	return response;
}

int main(int argc, char **argv){
	if(argc != 2){
		cout << "Need the conf file as the only argument";
	}
	DFSClient client(argv[1]);
}











