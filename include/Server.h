#pragma once
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <functional>
#include <string.h>
#include <map>
#include <vector>

#include <fstream>
#include <sstream>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 10 // how many pending connections queue will hold
#define DEFAULT_PORT 8080 // the port users will be connecting to
#define IGNORE_CHAR '.'
#define HTTP_BADREQUEST "HTTP/1.1 400 Bad Request\r\n\r\n <div>400 Bad Request</div>"
#define HTTP_NOTFOUND "HTTP/1.1 404 Not Found\r\n\r\n <div>404 Not Found</div>"





using std::string;
using std::map;
int yes = 1; //It's needed. Trust me.

enum class Method;
struct requestModel;
struct responseModel;
map<string, string> mimetype_map;


void *get_in_addr(struct sockaddr *sa);
/* 
 * A function that returns appropriate sockaddr info struct for ipv4 and ipv6
 */
string parseMethod(Method method); 
/*
 * A function that returns string version of given method used in responses
 */
Method parseMethod(string method);

/*
 * A function that returns enum version of given method used in functions
 */
void initializeMimeMap();
/*
 * A function that returns enum version of given method used in functions
 */
string getMimeType(string filename);
/*
 * A function that returns string with appropriate mime type for given file
 */ 
bool exists (string filename);
/*
 * A function that returns whether a specified file exists or not
 */ 
void translatePath(string &path);
/*
 * A function that unifies paths between requests and filesystem
 */ 
int parseRequest(string request, requestModel &result);
/*
 * A function that parses request string into a request struct
 * returns 0 on success and -1 on failure
 */ 
int loadFile(string path, string &content);
/*
 * A function that loads file content into a string
 * returns 0 on success and -1 on failure
 */ 
bool isThereSuchFile(string &path, string &content);
/*
 * A function that maps files in specified directory
 */ 



class SocketServer {
	// Pure socket setup, 0 implementation in diet.
	private:
		struct addrinfo hints, *servinfo, *p;
		// structures for holding info about server for binding
		struct sockaddr_storage their_addr;
		// structure for holding info about remote client
		char s[INET6_ADDRSTRLEN];

	protected: 
		int serverSocketFileDescriptor, foreignSocketFileDescriptor;
		// file descriptor for socket. In Unix everything is a file
		socklen_t sin_size;
	
	public:
		int port = DEFAULT_PORT;
		virtual void handleIncomingData(std::string incomingData) {};
		//The stuff that is being done on every request
		virtual void bindToLocalAddress();
		void setup();
		//even more setup
		int mainLoop();
		SocketServer() {};
};

template <typename Context>
using handlerFunction = std::function<responseModel(requestModel, Context)>;
typedef string pathString;

std::map<pathString, pathString> pathDictionary;

template <typename Context>
class Server: public SocketServer {
	private:
		std::map<
			Method, 
			std::map<
				pathString, 
				handlerFunction<Context> 
			>
		> endpoints;
		std::map<pathString, std::vector<Method>> preflight;
		
		//Internal use only, does not add record to pathDictionary
		//Other methods are call this one.
		void createEndpoint(Method method, pathString path, handlerFunction<Context>);
		
		void serveDirectory(string pathToDirectory, int skip);
		void createGetFileEndpoint(string path, int skip);
	public:
		
		Context context;
		static responseModel GETFileHandler(requestModel request, Context context);
		responseModel preflightResponse(requestModel r);
		void createGetFileEndpoint(string path);
		void serveDirectory(string pathToDirectory);
		void on(Method method, pathString path, handlerFunction<Context>);
		
		void handleIncomingData(string incomingData) override;
		Server<Context>(Context &Extern) {
			setup();
			context = Extern;
		}
};

	/*
	 * f
	 * pqxx::result g = s.context.query("SELECT * FROM todo");
	for (auto const &row: g) {
		for (auto const &field: row) {
			std::cout << field << std::endl;
		}
	}
	*/
