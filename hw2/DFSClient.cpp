/*
 * DFSClient.cpp
 *
 *  Created on: Oct 22, 2015
 *      Author: Michael Coughlin
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
}

/**
 * Parse the configuration file pointed to by the input.
 * Will exit the program on certain parse errors, as these errors
 * indicate lack of unrecoverable information, e.g. no password specified
 */
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

/**
 * Run the interface for the program and parse valid commands
 */
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

/**
 * Determine which part of the total file a file is based on the file name
 * of the file segment
 */
fileNamePair DFSClient::getFilePart(string fileName){
	char temp[fileName.length()];
	char *current, *end, *prev = "";
	fileNamePair pair;
	bool skip = true;
	long partNum;

	string baseFile("");
	strcpy(temp, fileName.c_str());
	current = strtok(temp, ".");

	while(current != NULL){
		baseFile.append(prev);
		prev = current;
		current = strtok(NULL, ".");
		if(current != NULL && !skip){
			baseFile.append(".");
		}
		skip = false;
	}

	pair.part = -1;
	pair.fullFilePartName = "";
	pair.baseFile = "";
	pair.server = "";

	if(prev == NULL){
		return pair;
	}

	partNum = strtol(prev, &end, 10);
	if(strcmp(end, "") != 0){
		return pair;
	}

	pair.baseFile = baseFile;
	pair.fullFilePartName = fileName;
	pair.part = (int)partNum;
	return pair;
}

/**
 * Write a char array of a certain length to an ip address and port, and return the response
 * as a char array and length
 */
receivedResponse DFSClient::writeStringToServer(string ip, int port, unsigned char* message, int messageLength){
	struct sockaddr_in server;
	char buffer[BUFFER_SIZE+1];
	receivedResponse response;
	response.contents = NULL;
	response.length = 0;
	struct in_addr inet_temp;

	//request a TCP socket
	int rc, bytesReceived, socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0){
		//if we cannot even open a socket, there is no point in continuing
		cout << "Error opening socket file descriptor" << endl;
		exit(-1);
	}

	if(inet_aton(ip.c_str(), &inet_temp) == -1){
		//if an ip is invalid, then the config file is not correct. therefore no point in continuing
		cout << "Invalid ip address: "<<ip<<endl;
		exit(-1);
	}

	//setup connection struct
	server.sin_addr.s_addr = inet_addr(ip.c_str());
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	//connect to ip:port destination
	rc = connect(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if(rc < 0){
		//if we cannot connect, then the calling function should handle this
		//put the error message into the returned response
		//TODO: have specific error-code field
		cout << "Could not connect to server: "<<ip<<":"<<port<<endl;
		response.contents =(unsigned char*)"Error: Failed to connect";
		response.length = strlen("Error: Failed to connect");
		return response;
	}

	//send the input data
	rc = send(socket_fd, message, messageLength, 0);
	if(rc < 0){
		//same as before, if an error occurs, caller should handle
		cout << "Could not send data to server: "<<ip<<":"<<port<<endl;
		response.contents =(unsigned char*)"Error: Failed to send";
		response.length = strlen("Error: Failed to send");
		return response;
	}

	response.length = 0;
	do{
		//receive data and store in output struct
		memset(buffer, 0, BUFFER_SIZE+1);
		bytesReceived = recv(socket_fd, buffer, BUFFER_SIZE, 0);
		if(bytesReceived < 0){
			//if we receive no data, caller should handle
			cout << "Error receiving from socket"<<endl;
			response.contents =(unsigned char*)"Error: Failed to receive";
			response.length = strlen("Error: Failed to receive");
			return response;
		}
		//since data may be encrypted bytes, we cannot use a c_str/std::string as a '\0' byte
		//may be significant, rather than just a null terminator. therefore, treat received
		//data as raw bytes
		response.contents = (unsigned char*)realloc(response.contents, bytesReceived+response.length);
		memcpy(response.contents+response.length, buffer, bytesReceived);
		response.length = bytesReceived+response.length;
	} while(bytesReceived > 0);
	return response;
}

/**
 * Perform a sha256 hash on an input char array of a certain length and out the result
 * in out. Out should be of at least SHA256_DIGEST_LENGTH bytes, or a segfault might occur
 */
void DFSClient::getSha256String(unsigned char* input, int inputLength, unsigned char* out){
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, input, inputLength);
	SHA256_Final(out, &sha256);
}

/**
 * Get a sha256 hash of the password variable
 */
void DFSClient::getSha256Key(unsigned char* out){
	getSha256String((unsigned char*)password.c_str(), password.length(), out);
}

/**
 * Get a random 17 byte array from openssl and store in iv, for use as an iv for AES
 * iv should be at least 17 bytes
 */
void DFSClient::getRandomIv(unsigned char* iv){
	memset(iv, 0, 17);
	if(!RAND_bytes(iv, 16)){
		cout << "OpenSSl random number generator encountered an error"<<endl;
		exit(-1);
	}
}

/**
 * Read a file from the file system and encrypt using a sha256 hash of the password
 * and AES_256_ctr with a random iv
 */
encryptedFile* DFSClient::readAndEncrypt(string filePath){
	int encryptedBytes, finalEncryptedBytes;
	ifstream file(filePath.c_str());

	//read whole file contents into string
	string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	unsigned char keyBuf[SHA256_DIGEST_LENGTH];

	//setup the output struct
	encryptedFile *encFile = (encryptedFile*)malloc(sizeof(encryptedFile));
	encFile->iv = (unsigned char*)malloc(17);
	unsigned char *ciphertext = (unsigned char*)malloc(contents.length() + 64);
	memset(ciphertext, 0, contents.length() + 64);

	//get the encryption key
	getSha256Key(keyBuf);
	getRandomIv(encFile->iv);

	//encrypt the file contents
	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_EncryptInit_ex(&ctx, EVP_aes_256_ctr(), NULL, keyBuf, encFile->iv);
	EVP_EncryptUpdate(&ctx, ciphertext, &encryptedBytes, (unsigned char*)contents.c_str(), contents.length());
	EVP_EncryptFinal(&ctx, ciphertext+encryptedBytes, &finalEncryptedBytes);

	//return the encrypted data
	encFile->contents = ciphertext;
	encFile->contentLength = encryptedBytes + finalEncryptedBytes;
	return encFile;
}

/**
 * Decrypt a char array using a sha256 hash of the password and AES_256_ctr and write
 * to a location on the file system
 */
void DFSClient::decryptAndWrite(string filePath, encryptedFile encryptedContents){
	int exists =0, accesible=0, decryptedBytes, finalDecryptedbytes;
	ofstream out;
	//check if destination exists and is not accessible
	checkFilepathExistsAccessible(filePath, &exists, &accesible);
	if(exists == 1 && accesible == 0){
		cout << "\nCannot access file path"<<endl;
		return;
	}

	//prepare the plaintext array
	unsigned char plaintext[encryptedContents.contentLength + 16];
	memset(plaintext, 0, encryptedContents.contentLength + 16);
	EVP_CIPHER_CTX ctx;

	//get the decryption key
	unsigned char key[SHA256_DIGEST_LENGTH];
	getSha256Key(key);

	//decrypt the data
	EVP_CIPHER_CTX_init(&ctx);
	EVP_DecryptInit_ex(&ctx, EVP_aes_256_ctr(), NULL, key, encryptedContents.iv);
	EVP_DecryptUpdate(&ctx, plaintext, &decryptedBytes, encryptedContents.contents, encryptedContents.contentLength);
	EVP_DecryptFinal(&ctx, plaintext+decryptedBytes, &finalDecryptedbytes);

	//write data to file
	//clobber any existing data
	out.open(filePath.c_str(), ofstream::out|ofstream::trunc);
	out.write((char*)plaintext, decryptedBytes+finalDecryptedbytes);
	out.flush();
	out.close();
}

/**
 * Get the password to be sent to the server. Depending on the operating mode,
 * the password is the plaintext password from the config file of is the output
 * of HASH_ROUNDS of applying sha256 to the password
 */
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

/**
 * Returns a c++ std::string containing the contents of the input sha256 hash
 * Assumes that it is either a c_str of the password or is a valid hash of correct
 * length, depending on the configuration. Will either directly construct a std::string
 * from the input or will create a string containing a hexadecimal representation of the
 * sha256 hash
 */
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

/**
 * Directly creates a string using either the c_str of the password
 * or a sha256 hash of the password, depending on the configuration
 * using getServerPasswordHash() and getServerPasswordHashString()
 * Output will either be a std::string of the password's c_str or
 * a hexadecimal representation of the sha256 hash of the password's
 * c_str
 */
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

/**
 * Write a LIST command to a ip:port tcp destination and return the
 * response as a std::string by constructing one using the received
 * char array as the input
 */
string DFSClient::writeListToServer(string ip, int port){
	string message = "LIST " + user + " " + getServerPasswordHashStringDirect();
	receivedResponse response = writeStringToServer(ip, port, (unsigned char*)message.c_str(), message.length());

	//convert the response to a string, as it contains only ascii values
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

/**
 * Write an MKDIR command to a ip:port tcp destination and return the
 * response as a std::string by constructing one using the received
 * char array as the input
 */
string DFSClient::writeMkdirToServer(string ip, int port, string folder){
	string mkdirMessage = "MKDIR "+user+" "+getServerPasswordHashStringDirect()+" "+folder;
	receivedResponse response = writeStringToServer(ip, port, (unsigned char*)mkdirMessage.c_str(), mkdirMessage.length());

	//convert the response to a string, as it contains only ascii values
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

/**
 * Write a GET command to a ip:port tcp destination and return the
 * response as a char array and length, as the response will be encrypted
 * and may not form a valid c_str
 */
receivedResponse DFSClient::writeGetToServer(string ip, int port, string file){
	string getMessage = "GET "+user+" "+getServerPasswordHashStringDirect()+" "+file;
	return writeStringToServer(ip, port, (unsigned char*)getMessage.c_str(), getMessage.length());
}

/**
 * Write a PUT command to a ip:port tcp destination and return the
 * response as a std::string by constructing one using the received
 * char array as the input
 */
string DFSClient::writePutToServer(string ip, int port, string fileName, unsigned char* fileContents, int contentsLength){
	//convert length from int to string
	char length[100];
	memset(length, 0, 100);
	sprintf(length, "%d", contentsLength);

	//construct string of everything but the file contents
	string putMessage = "PUT " +user + " "+ getServerPasswordHashStringDirect()+" "+fileName+" "+string(length)+"\n";

	//create an array to hold everything
	unsigned char totalPutMessage[putMessage.length() + contentsLength];
	//copy the command message to the array
	memcpy(totalPutMessage, putMessage.c_str(), putMessage.length());
	//copy the file contents to after the command
	memcpy(totalPutMessage + putMessage.length(), fileContents, contentsLength);

	//write the array and receive response
	receivedResponse response = writeStringToServer(ip, port, totalPutMessage, putMessage.length() + contentsLength);

	//convert the response to a string, as it contains only ascii values
	unsigned char outChar[response.length +1];
	memset(outChar, 0, response.length +1);
	memcpy(outChar, response.contents, response.length);
	return string((char*)outChar);
}

/**
 * Parse the response from a server to a LIST command, returning the total
 * number of files that a server identifies as holding. Also populates
 * the map of file names to file parts with the received information
 * from a single server
 */
int DFSClient::parseListResponse(string server, string message){
	char temp[message.length()];
	char *totalFilesNumChar, *end, *lineChar = NULL;
	int totalFilesNum;
	string whitespaces (" \t\f\v\n\r"), current(message);

	strcpy(temp, message.c_str());
	char *totalFilesHeader = strtok(temp, " ");
	//check for errors, which will be at the start of the string according to the protocol
	if(message.find("Error:") == 0){
		return 0;
	}

	//check if server did not like username/password combo
	if(message.find("Invalid Username/Password. Please try again.")==0){
		cout << "Invalid Username/Password. Please try again."<<endl;
		return 0;
	}

	//check if expected headers are present and in the right place
	if(totalFilesHeader == NULL || (strcmp("Total_Files:", totalFilesHeader) != 0)){
		cout << "Could not parse total files header in LIST response" << endl;
		return -1;
	}

	//check number of files servers claim to have
	totalFilesNumChar = strtok(NULL, " ");
	totalFilesNum = strtol(totalFilesNumChar, &end, 10);
	//if there are no files on the servers for us, then we can stop now
	if(totalFilesNum == 0){
		return 0;
	}

	//loop throught the response and parse each file info
	do{
		char temp2[current.length()];
		strcpy(temp2, current.c_str());
		lineChar = strtok(temp2, "\n");
		if(lineChar == NULL){
			break;
		}
		string line(lineChar);

		//if the line is not one of the headers, then parse it. else go on to next line
		if(line.find("Total_Files:") != 0 && line.find_first_not_of(whitespaces) == 0 && line.find("Files:") != 0){
			string searchLine = line;

			//get rid of first forward slash
			if(line.find("/") != line.npos){
				searchLine = string(line, line.find("/") +1);
			}

			//parse the filename to the resulting filename/file part pair
			struct fileNamePair pair = getFilePart(searchLine);
			if(filesToParts.count(pair.baseFile) == 0){
				filesToParts[pair.baseFile] = vector<fileNamePair>();
			}

			//place the info into the file to segment map
			vector<struct fileNamePair> currentInfo = filesToParts[pair.baseFile];
			pair.server = server;
			currentInfo.push_back(pair);
			filesToParts[pair.baseFile] = currentInfo;
		}

		//get next line
		string next(current, current.find("\n") + 1);
		current = next;
	} while(current.find("\n") != current.npos);
	return totalFilesNum;
}

/**
 * Calculate the number of segments are available from live servers based
 * on the info in the files to segments map. returns the total number of parts
 * that are known, e.g. 4 if all 4 different parts are available
 */
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

/**
 * Sends a LIST command, parses the response and returns a string of output to display
 * Determines what files are available from live servers specified in the config file
 * and what segments exist
 */
string DFSClient::sendList(){
	int port, parts;
	string server, ip, response, message="LIST\n\n";
	map<string, string>::iterator serverIt;
	map<string, vector<struct fileNamePair> >::iterator filePartIt;

	//clear any existing info on files, as it may have changed
	filesToParts.clear();

	//loop through all of the configured servers and get info
	for(serverIt = serverToIpMap.begin(); serverIt != serverToIpMap.end(); serverIt++){
		server = (string)serverIt->first;
		ip = (string)serverIt->second;
		port = (int)serverToPortMap[server];
		//receive a server's response and parse it
		response = writeListToServer(ip, port);
		parseListResponse(server, response);
	}

	//using the info from the parsed response, create a string to be printed
	//reflecting this info
	for(filePartIt = filesToParts.begin(); filePartIt != filesToParts.end(); filePartIt++){
		parts = numParts(filePartIt->first);
		message.append(filePartIt->first);
		if(parts != 4){
			message.append(" [incomplete]");
		}
		message.append("\n");
	}
	return message;
}

/**
 * Sends a GET command to a server, writes the received file to the file system
 * and returns a string to be printed. May fail if not enough segments can be found
 * or if a hash mismatch occurs
 */
string DFSClient::sendGet(string filePath){
	int i, numCurrentParts, port;
	string totalHashes[4];
	set<string> ivs;
	string response(""), ip;
	encryptedFile encFile;
	encFile.contents = NULL;
	encFile.contentLength = 0;
	vector<struct fileNamePair>::iterator fileInfoIt;
	vector<string>::iterator serverIt;
	bool success;
	bool hashMatched;
	unsigned char totalHash[MD5_DIGEST_LENGTH+1];
	memset(totalHash, 0, MD5_DIGEST_LENGTH+1);

	//clear current info on files and rebuild, in case someone dropped out
	sendList();
	vector<struct fileNamePair> currentFileInfo = filesToParts[filePath];

	//Check number of file parts
	//if we cannot find all 4 parts somewhere, we cannot continue
	numCurrentParts = numParts(filePath);
	if(numCurrentParts < 4){
		return "Not enough parts to rebuild file";
	}

	//query the correct servers for each part of the file
	for(i=1; i<=4; i++){
		//get a list of server that claim to have this segment
		vector<string> servers;
		for(fileInfoIt = currentFileInfo.begin(); fileInfoIt != currentFileInfo.end(); fileInfoIt++){
			struct fileNamePair currentPair = *fileInfoIt;
			if(currentPair.part == i){
				servers.push_back(currentPair.server);
			}
		}

		receivedResponse file;
		success = false;
		//loop through the list of servers to get the segment. stop after first success
		for(serverIt=servers.begin(); serverIt!=servers.end(); serverIt++){
			hashMatched = true;
			unsigned char currentHash[MD5_DIGEST_LENGTH+1];
			memset(currentHash, 0, MD5_DIGEST_LENGTH+1);

			//get the server info
			ip = serverToIpMap[(string)(*serverIt)];
			port = serverToPortMap[(string)(*serverIt)];

			//convert the part number to a string
			char partChar[100];
			sprintf(partChar, "%d", i);

			//request the file part from the current server
			file = writeGetToServer(ip, port, filePath+"."+string(partChar));

			//make some copies of the response
			char fileTemp[file.length+1];
			memset(fileTemp, 0, file.length+1);
			memcpy(fileTemp, file.contents, file.length);
			string fileStr(fileTemp);
			string originalFileStr(fileStr);

			//check if the server sent us an error. if so, try next server
			if(fileStr.find("Error") == 0){
				cout << "Error receiving file on server: "<<*serverIt <<": "<<fileStr<<endl;
				continue;
			}

			//parse out the stored hash of the complete file
			string currentTotalHash(string(fileStr.c_str()), 0, fileStr.find("\n"));
			totalHashes[i-1] = currentTotalHash;

			//strip out that line
			fileStr = string(string(fileStr.c_str()), fileStr.find("\n")+1);

			//parse out the stored hash of this file segment
			string currentPartHash(fileStr, 0, fileStr.find("\n"));

			//parse out the stored iv for the whole file
			string currentIv(fileStr, fileStr.find("\n")+1, 32);
			ivs.insert(currentIv);

			//now need to get the raw segment contents from the response. since is encrypted,
			//we cannot make a c_str, as the value are not ascii
			//the offset from the beginning of the received file to the raw encrypted data
			//is the size of the file minus the stores total hash size, partial hash size and iv size,
			//and the 3 newlines separating these values
			int newlineOffset = fileStr.find("\n") + currentPartHash.length() + currentTotalHash.length() + 3;
			//subtract one from the raw size to get rid of the trailing newline
			int rawFileSize = file.length - newlineOffset-1;
			unsigned char rawFile[rawFileSize];
			memcpy(rawFile, file.contents+newlineOffset, rawFileSize);

			//now take a hash of the segment contents
			MD5(rawFile, rawFileSize, currentHash);

			//compare the hash to the stored hash in the file
			//if it does not match, then we need to go to next server
			string currentHashStr = md5ToHexString((unsigned char*)currentHash);
			if(currentHashStr.compare(currentPartHash) != 0){
				hashMatched = false;
				response.append("Hash mismatch. Received part hash: "+currentHashStr+
						" Stored hash in file: "+currentPartHash+"\n");
			}

			//if not errors were encountered and the part hash matches, then this is likely a valid segment
			//therefore, put it in the correct place
			if(originalFileStr.find("Error") != 0 && hashMatched){
				encFile.contents = (unsigned char*)realloc(encFile.contents, encFile.contentLength+rawFileSize);
				memcpy(encFile.contents + encFile.contentLength, rawFile, rawFileSize);
				encFile.contentLength+=rawFileSize;
				success = true;
				break;
			}
		}

		//if we did not successfully find a part, then we need to stop
		if(!success){
			char iStr[100];
			sprintf(iStr, "%d", i);
			return response + "Error receiving part: "+string(iStr);
		}
	}

	//now check the hash of the entire file reconstituted from its pieces
	MD5(encFile.contents, encFile.contentLength, totalHash);

	//check if all of the stored total hashes match this hash
	string totalHashStr = md5ToHexString((unsigned char*)totalHash);
	bool allMatched = true;
	for(i=0; i<4; i++){
		if(totalHashStr.compare(totalHashes[i]) != 0){
			//TODO: if they don't match, then we should either recover or quit
			//however, we don't currently store info on which servers didn't match
			//and we may not always be able to recover
			allMatched = false;
			char iStr[100];
			sprintf(iStr, "%d", i+1);
			response.append("\nHash from part "+string(iStr)+"does not match total hash");
		}
	}

	//indicate to user that not all of the total hashes matched
	if(!allMatched){
		response.append("\nTotal hash did not match for all parts");
	}

	//indicate to the user that we also received multiple ivs, which also should not happen
	if(ivs.size() != 1){
		response.append("\nIncorrect number of ivs encountered. Decryption may fail");
	}

	//use the first iv for decryption
	set<string>::iterator beginning = ivs.begin();
	encFile.iv = (unsigned char*)malloc(17);
	hexToIvString(*beginning, encFile.iv);

	//decrypt the file and write to file system
	decryptAndWrite(filePath, encFile);
	return string(response+"Did GET on: "+filePath);
}

/**
 * Converts a hexadecimal representation of an iv to a char of length 16 bytes
 * The input iv pointer must point to an array of at least 17 bytes
 */
void DFSClient::hexToIvString(string ivHex, unsigned char* iv){
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

/**
 * Converts an input 17 byte array to a string consisting of its hexadecimal
 * representation
 * TODO: could be refactored with md5ToHex code
 */
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

/**
 * Converts an input array of MD5 hash length into a string of its hexadecimal
 * representation
 * TODO: could be refactored with ivToHex code
 */
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

/**
 * Sends a PUT command to a server by reading and encrypting a file,
 * splitting it into segments and writing it
 * to the configured servers based in the resulting MD5 hash scheme.
 * Also returns a string to be printed
 */
string DFSClient::sendPut(string filePath){
	int exists=0, accessible=0, i,j, partsSchemeIndex, partLength, part, port;
	int partsToServer[4][2];
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
	string message(""), response(""), server, fileNameToSend, ip, serverResponse;
	unsigned char *partToSend, *digestToSend;

	//check if the file exists. if not then we really shouldn't continue
	checkFilepathExistsAccessible(filePath, &exists, &accessible);
	if(!(exists==1 && accessible== 1)){
		return "Could not access file to upload";
	}

	//read file and encrypt it
	encryptedFile *encryptedContents = readAndEncrypt(filePath);

	//split the file into 4 roughly equal parts, with the 4th part getting the overflow
	int length = encryptedContents->contentLength;
	int remaining = encryptedContents->contentLength - 3*(length/4);
	unsigned char part1[length/4];
	unsigned char part2[length/4];
	unsigned char part3[length/4];
	unsigned char part4[remaining];

	//copy the data to the parts
	memcpy(part1, encryptedContents->contents, length/4);
	memcpy(part2, encryptedContents->contents+length/4, length/4);
	memcpy(part3, encryptedContents->contents+2*(length/4), length/4);
	memcpy(part4, encryptedContents->contents+3*(length/4), remaining);

	//take a hash of the whole file
	//use hash of the encrypted data instead of the raw data to avoid leaking info
	//when calculating the part scheme
	MD5(encryptedContents->contents, encryptedContents->contentLength, totalDigest);

	//take a hash of each part
	MD5(part1, length/4, part1Digest);
	MD5(part2, length/4, part2Digest);
	MD5(part3, length/4, part3Digest);
	MD5(part4, remaining, part4Digest);

	//calculate which scheme to use for part distribution
	partsSchemeIndex = (int)(totalDigest[MD5_DIGEST_LENGTH-1]) % 4;
	memset(partsToServer, 0, 8*sizeof(int));

	//write each part to the server indicated by the distribution scheme
	for(i=0; i<4; i++){
		for(j=0; j<2; j++){
			partLength = length/4;

			//first determine what server to send what part to
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
			part = serverOrder[partsSchemeIndex][i][j];
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

			//lookup the server
			if(serverToIpMap.find(server) == serverToIpMap.end()){
				message.append("Could not find server "+server+" in configured servers\n");
			}
			ip = serverToIpMap[server];
			port = serverToPortMap[server];

			//convert the part number to a string
			char partStr[100];
			sprintf(partStr, "%d", part);

			//construct the file name for the server using the name and part number
			fileNameToSend = filePath +"."+string(partStr);//+ md5ToHexString(totalDigest)+"."

			//now construct the string containing the header of the file, including
			//the total file hash, the part hash and the iv for decryption
			string partWithHashes = md5ToHexString(totalDigest) + "\n" +
					md5ToHexString(digestToSend)+"\n"+
					ivToHexString(encryptedContents->iv)+"\n";

			//now create an array to hold the file headers and file content
			unsigned char totalPartWithHashes[partWithHashes.length()+partLength];
			//copy in the file headers
			memcpy(totalPartWithHashes, partWithHashes.c_str(), partWithHashes.length());
			//copy in the file content after the headers
			memcpy(totalPartWithHashes+partWithHashes.length(), partToSend, partLength);

			//write the file to the server and append any response to the returned value
			serverResponse = writePutToServer(ip, port, fileNameToSend, totalPartWithHashes, partWithHashes.length()+partLength);
			if(serverResponse.length()>0){
				response.append("\n"+serverResponse);
			}
		}
	}
	return string("Did PUT on: "+filePath+"\nAnd received response: "+response);
}

/**
 * Sends an MKDIR command and returns a string to being printed
 */
string DFSClient::sendMkdir(string filePath){
	map<string, string>::iterator serverIt;
	string server, ip, response;
	int port;

	for(serverIt = serverToIpMap.begin(); serverIt != serverToIpMap.end(); serverIt++){
		server = (string)serverIt->first;
		ip = (string)serverIt->second;
		port = (int)serverToPortMap[server];
		response = writeMkdirToServer(ip, port, filePath);
		if(response.find("Created directory") == response.npos){
			cout << "Error creating directory on server: "+server<<endl;
		}
	}
	return string("Did MKDIR on: ").append(filePath);
}

/**
 * Parses a string to determine which of the DFS commands it specifies
 */
string DFSClient::parseCommand(string command){
	string response;
	char *filePath, *lastArg;
	char temp[command.length()];

	//make a copy of the command
	strcpy(temp, command.c_str());
	char* inCommand = strtok(temp, " ");

	//command needs to be valid
	if(inCommand == NULL){
		return string("Invalid command: ").append(command);
	}

	//lookup the command
	RequestOption commandOption = getCommand(inCommand);
	if(commandOption == NONE){
		return string("Invalid command: ").append(command);
	}

	//command may contain a file path
	filePath = strtok(NULL, " ");
	lastArg = filePath != NULL ? strtok(NULL, " ") : NULL;

	//parse the command based on what it is
	switch(commandOption){
		case LIST:
			response = filePath != NULL ? "Invalid argument to LIST: " + string(filePath) : sendList();
			break;
		case GET:
			if(lastArg != NULL){
				response = "GET does not take a third argument: " + string(lastArg);
			} else{
				response = filePath != NULL ? sendGet(string(filePath)) : "GET requires a file path";
			}
			break;
		case PUT:
			if(lastArg != NULL){
				response = "PUT does not take a third argument: " + string(lastArg);
			} else{
				response = filePath != NULL ? sendPut(string(filePath)) : "PUT requires a file path";
			}
			break;
		case MKDIR:
			if(lastArg != NULL){
				response = "MKDIR does not take a third argument: " + string(lastArg);
			} else{
				response = filePath != NULL ? sendMkdir(string(filePath)) : "MKDIR requires a file path";
			}
			break;
		default:
			response = "Invalid command: " + command;
	}
	return response;
}

int main(int argc, char **argv){
	if(argc != 2){
		cout << "Need the conf file as the only argument";
	}
	DFSClient client(argv[1]);
}
