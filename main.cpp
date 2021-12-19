#include <stdio.h>
#include <iostream>
#include "stdint.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <regex>
#include <string.h>
#include <map>
#include <vector>

#include <fstream>
#include <sstream>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IGNORE_CHAR '.'
#define HTTP_BADREQUEST "HTTP/1.1 400 Bad Request\r\n\r\n <div>400 Bad Request</div>"
#define HTTP_NOTFOUND "HTTP/1.1 404 Not Found\r\n\r\n <div>404 Not Found</div>"
#define BACKLOG 10 // how many pending connections queue will hold
#define MYPORT "80" // the port users will be connecting to


using std::string;
using std::map;
using std::regex;
int yes = 1; //It's needed. Trust me.


int getaddrinfo(
	const char *node, // e.g. "www.example.com" or IP
	const char *service, // e.g. "http" or port number
	const struct addrinfo *hints,
	struct addrinfo **res
);


// get sockaddr, but protocol agnostic
void *get_in_addr(struct sockaddr *sa) {
	if(sa->sa_family == AF_INET) {
		//handle ipv4
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	//handle ipv6
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


enum class Method {
	GET = 1,
	POST = 2,
	PUT = 3,
	DELETE = 4,
	HEAD = 5,
};


Method parseMethod(string method) {
	if(method == "GET") {
		return Method::GET;
	} else if(method == "POST") {
		return Method::POST;
	} else if(method == "PUT") {
		return Method::PUT;
	} else if(method == "DELETE") {
		return Method::DELETE;
	} else if(method == "HEAD") {
		return Method::HEAD;
	} else {
		return Method::HEAD;
	}
}


string parseMethod(Method method) {
	string m;
	switch((int)method) {
		case 1:
			m = "GET";
			break;
		case 2:
			m = "POST";
			break;
		case 3:
			m = "PUT";
			break;
		case 4:
			m = "DELETE";
			break;
		case 5:
			m = "HEAD";
			break;
		default:
			m = "HEAD";
	};
	return m;
};


map<string, string> mimetype_map;

void initializeMimeMap() {
	mimetype_map["default"] = "text/plain";
	mimetype_map["html"] = "text/html";
	mimetype_map["css"] = "text/css";
	mimetype_map["js"] = "text/js";
	mimetype_map["jpeg"] = "image/jpeg";
	mimetype_map["png"] = "image/png";
}


//Gets the MIME type of specified resource
string getMimeType(string filename) {
	string extension = filename.substr(filename.find('.')+1);
	return strlen(mimetype_map[extension].c_str()) != 0 ? 
		mimetype_map[extension] 
		: 
		mimetype_map["default"];
};


bool exists (string filename) {
  struct stat buffer;   
	if(stat (filename.c_str(), &buffer) == 0) {
		if(buffer.st_mode & S_IFREG) {
			return true;
		};
	}
	return false;
}


void translatePath(string &path) {
	if(path.substr(0, 2) == "./") {
		//I want to be able to use it both ways, let's see if it's that bad of an idea
		path = path.substr(2);
		return;
	}
	if(path == "/") {
		if(exists("index.html")) {
			path = "index.html";
			return;
		}
	}
	if(path.at(0) == '/') {
		path = path.substr(1);
	}
	if(path.find('.') == -1) {
		if(path.back() == '/') {
			if(exists(path+"index.html")) {
				path += "index.html";
			}
		}
		if(exists(path+"/index.html")) {
			path += "/index.html";
		}
	}
};


struct requestModel {
	Method method;
	string proto;
	string path;
	string path_params;
	map<string, string> headers;
	string body;
};


//https://regexr.com/
int parseRequest(string request, requestModel &result) {
	map<string, string> headers;
	regex firstLineR = regex(R"(^(.*)\S)");
	regex headersR = regex(R"([A-Z].*: (.*)\S)");
	regex bodyR = regex(R"((.&)$)");
	std::smatch firstLineM;
	if(!regex_search(request, firstLineM, firstLineR)) {
		return -1;
	}
	string firstLine = firstLineM.str();
	int methodEndsAt = firstLine.find(' ');
	int pathEndsAt = firstLine.substr(methodEndsAt+1).find(' ');
	string method = firstLine.substr(0, methodEndsAt);
	
	string fullPath = firstLine.substr(methodEndsAt+1, pathEndsAt);
	int qindex = fullPath.find("?");
	string path = fullPath.substr(0, fullPath.find("?"));
	translatePath(path);
	string path_params = fullPath.substr(fullPath.find("?")+1);

	string proto = firstLine.substr(methodEndsAt + 1 + pathEndsAt + 1); 
	// cumulated offset
	auto headers_begin = std::sregex_iterator(request.begin(), request.end(), headersR);
	auto headers_end = std::sregex_iterator();
	int length = distance(headers_begin, headers_end);
	string lastHeader;
	for(std::sregex_iterator i = headers_begin; i != headers_end; i++) {
		lastHeader = (*i).str();
		int colonIndex = lastHeader.find(":");
		headers[lastHeader.substr(0, colonIndex)] = lastHeader.substr(colonIndex+1);
	}
	string body;
	std::smatch bodyM;
	regex_search(request, bodyM, bodyR);
	if(!(bodyM.str() == lastHeader)) { //I'm that lazy
		body = bodyM.str();
	}

	result.method = parseMethod(method);
	result.path = path;
	result.path_params = path_params;
	result.headers = headers;
	result.proto = proto;
	result.body = body;
	return 0;
}


// Populates content string with the file content
int loadFile(string path, string &content) {
	std::ifstream theFile; //ifstream read, ofstream write
	std::stringstream buff;
	theFile.open(path.c_str()); 
	// PÓŁ GODZINY NIE DZIALALO ZAPOMNIAŁEM ŻE TRZEBA OTWORZYĆ PLIK WTF
	if(theFile.good()) {
		buff << theFile.rdbuf();
		content = buff.str();
		return 0; //success
	}
	return -1; //failure
}


//Checks path.
//if it's a folder increments path by index.html and checks for it, 
//if it's not then it populates content string with file's data.
bool isThereSuchFile(string &path, string &content) {
  struct stat buffer;
	if(stat (path.c_str(), &buffer) == 0) {
		if(buffer.st_mode & S_IFDIR) {
			//handle dir
			if(path.back() == '/') {
				path += "index.html";
			} else {
				path += "/index.html";
			}
			return isThereSuchFile(path, content);
		}
		else if(buffer.st_mode & S_IFREG) {
			//handle a file
			loadFile(path, content);
			return true;
		}
		else {
			//handle other
			return false;
		}
	};
	return false;
}


class SocketServer {
	// Pure socket setup, 0 implementation in diet.
	public:
		int serverSocketFileDescriptor, foreignSocketFileDescriptor;
		socklen_t sin_size;
		std::string port = MYPORT;
		struct addrinfo hints, *servinfo, *p;
		struct sockaddr_storage their_addr;
		char s[INET6_ADDRSTRLEN];
		
		virtual void handleIncomingData(std::string incomingData) {};
		//The stuff that is being done on every request
		
		virtual void bindToLocalAddress();
		void setup();
		//even more setup
		int mainLoop();
		SocketServer() {};
};


void SocketServer::setup() {
	hints.ai_family = AF_UNSPEC; //Accept both IPV4 and IPV6
	hints.ai_socktype = SOCK_STREAM; //Accept TCP
	hints.ai_flags = AI_PASSIVE;  //Do not initiate connection.
	memset(&hints, 0, sizeof hints);
	bindToLocalAddress();
	printf("server: waiting for connections to %s...\n", this->port.c_str());
}


int SocketServer::mainLoop() {
	if(listen(serverSocketFileDescriptor, foreignSocketFileDescriptor) == -1) {
		perror("listen");
		exit(1);
	};
	while(true) {
		sin_size = sizeof their_addr;
		foreignSocketFileDescriptor = accept(serverSocketFileDescriptor, (struct sockaddr *)&their_addr, &sin_size);
		if(!fork()) {
			std::string incomingData;
			char buff[8096];
			int bytesRead;
			if(foreignSocketFileDescriptor == -1) {
				perror("accept");
				continue;
			}
			inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
			printf("server: got connection from %s\n", s);
			if((bytesRead = read(foreignSocketFileDescriptor, buff, 8096)) > 0) {
				//TO IMPROVE
				if(bytesRead == -1) {
					perror("read");
				}
				incomingData += buff;
			}
			handleIncomingData(incomingData);
		}
		close(foreignSocketFileDescriptor); // done using it
	}
	close(serverSocketFileDescriptor); // no need to listen anymore
	return 0;
};


void SocketServer::bindToLocalAddress() {
	int addrinfoStatus;
	if((addrinfoStatus = getaddrinfo(NULL, this->port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrinfoStatus)); 
	}
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if((serverSocketFileDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		//set socket options: should reuse: 1 (yes)
		if(setsockopt(serverSocketFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if(bind(serverSocketFileDescriptor, p->ai_addr, p->ai_addrlen) == -1) {
			close(serverSocketFileDescriptor);
			perror("server: bind");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo);
	if (p == NULL) {
		perror("listen");
		exit(1);
	}
	if (listen(serverSocketFileDescriptor, BACKLOG) == -1) {
		perror("listen");
		exit(-1);
	}
}


class Server: public SocketServer {
	public:
		map<Method, map<string, string (*)(requestModel request)>> endpoints;

		static string GETFileHandler(requestModel request);
		void createGetFileEndpoint(string path);
		void getEndpointsFromDirectory(string pathToDirectory);
		void on(Method method, string path, string (*)(requestModel request));
		void handleIncomingData(string incomingData) override;
		Server() {
			//setup
			setup();
		}		
};


void Server::handleIncomingData(std::string incomingData) {
	string resp;
	struct requestModel request;
	if(parseRequest(incomingData, request) == -1) {
		//REDUCE THE AMOUNT OF ENDPOINTS PLZ
		resp = HTTP_BADREQUEST;
		if(send(foreignSocketFileDescriptor, resp.c_str(), resp.size(), 0) == -1) {
			perror("send");
			return;
		};
	};
	if(endpoints[request.method][request.path] != 0) {
		resp = endpoints[request.method][request.path](request);
	} else {
		resp = HTTP_NOTFOUND;
	}
	if(send(foreignSocketFileDescriptor, resp.c_str(), resp.size(), 0) == -1) {
		perror("send");
	};
}


string Server::GETFileHandler(requestModel request) {
	string response;
	string fileContent;
	translatePath(request.path);
	string mimetype = getMimeType(request.path);
	if(loadFile(request.path, fileContent) == 0) {
		response = 
			"HTTP/1.1 200 OK\r\n"
			"Server: MyServer!\r\n"
			"Connection: Kepp-Alive\r\n"
			"Content-Type: "+mimetype+"\r\n"
			"\r\n" + fileContent;
		return response;
	};
	return HTTP_NOTFOUND;
}


string getFilenameFromPath(string path) {
	int len = strlen(path.c_str());
	string result;
	string result2;
	for(int i = path.size(); i > 0; i--) {
		if(path[i] == '/') {
			break;
		}
		result += path[i];
	}
	int l = result.size();
	for(int i{}; i != l; i++) {
		result2 += result[l - i - 1];
	}
	return result2;
}


void Server::createGetFileEndpoint(string path) {
	translatePath(path);
	std::cout << "initializing GET endpoint " << path << std::endl;
	endpoints[Method::GET][path] = GETFileHandler;
}


void Server::getEndpointsFromDirectory(string path) {
	DIR *directory;
	struct dirent *entry;
	struct stat statBuffer;
	if(stat (path.c_str(), &statBuffer) == 0) {
		if((directory = opendir(path.c_str()))) {
			while((entry = readdir(directory))) {
				if(
						strcmp(entry->d_name, "..") != 0 && 
						strcmp(entry->d_name, ".") != 0 &&
						entry->d_name[0] != IGNORE_CHAR
					) {
					// strcmp cuz compiler complains about === 
					// Given path leads to a directory. let the fun begin ^^
					getEndpointsFromDirectory(path+"/"+entry->d_name);
				}
			}
			closedir(directory);
		} else if(statBuffer.st_mode & S_IFREG) {
			// Given path leads to a file
			if(getFilenameFromPath(path)[0] != IGNORE_CHAR) {
				createGetFileEndpoint(path);
			}
			return;
		}
		else {
			//Given path doesn't lead to neither file, nor directory
			return;
		}
	}	
	//Such item doesn't exist
}


typedef string (*handlerFunction)(requestModel request);


void Server::on(Method method, string path, handlerFunction handler){
	string m = parseMethod(method);
	std::cout << "initializing " << m << " endpoint " << path.substr(1) << std::endl;
	endpoints[method][path.substr(1)] = handler;
};


string endpointFunction(requestModel request) {
	std::cout << "testing the endpoint" << std::endl;
	return "a";
};


int main(int argc, char *argv[]) {
	initializeMimeMap();
	//cout << __cplusplus << endl; //14
	Server s;
	s.getEndpointsFromDirectory(".");
	s.on(Method::POST, "/endpoint", endpointFunction);
	//s.on(METHOD, "PATH", functionHandler)
	s.mainLoop();
	return 0;
};

