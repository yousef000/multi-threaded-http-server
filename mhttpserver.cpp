#include <iostream>
#include <queue>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomanip>
#include <ctime>

using namespace std;
#define BUFFSIZE 16 * 1024
int SIZE = 4;
queue<int> requests;
int OFFSET = 0;
int logfd = 0;

pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t conditionMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sharedVarMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t conditionCond = PTHREAD_COND_INITIALIZER;

int isDirectory(const char *path);
void response200(int clientSocket);
void response201(int clientSocket);
void response400(int clientSocket);
void response403(int clientSocket);
void response404(int clientSocket);
void response500(int clientSocket);
void responseContentLength(int clientSocket, int length);
bool checkIfFileExist(char*filename);
bool checkReadPermission(char*filename);
bool checkWritePermission(char*filename);
void writeFileInput(char*f, int clientSocket, char buffer[]);
void parseHeader(char buffer[], char * httpMethod, char * filename, int * contentLength);
void GetRequest(char * filename, int clientSocket);
void PutRequest(char * filename, int clientSocket, int length);
void *processRequests(void* arguments);
void initWorkers(pthread_t * workers, int size);
void reserveSpaceToWriteLog(char * filename, char * httpMethod, int length, int response);
void writeLog(char * buffer, int length, char * filename, char * httpMethod, int offset, int response);
void parseArguments(int argc, char * argv[]);

int main(int argc, char * argv[]){
    //int start = clock();

    int serverSocket, clientSocket;
    parseArguments(argc, argv);
    pthread_t workers[SIZE];
   
    if((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
        warn("Socket connection failed!");
    }
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    if(argv[optind] != NULL)
        address.sin_port = htons(atoi(argv[optind+1])); 
    else{
        address.sin_port = htons(8080);
    }
    if(::bind(serverSocket, (struct sockaddr *) &address, sizeof(address)) < 0)
        warn("Bind");
    
    // create worker threads that sleeps initially, and wakes up when request is made
    // workers get access of requests through global queue called requests
    initWorkers(workers, SIZE);
    warn("Serversocket %d", serverSocket);
    if(::listen(serverSocket, 5) < 0)
        warn("Listen");

    while(true){
        clientSocket = ::accept(serverSocket, (struct sockaddr *) &address, (socklen_t *) &addrlen);
        if(clientSocket < 0){
            warn("Accept");
            break;
        }
            
        
        pthread_mutex_lock(&queueMutex);
        //if threads are sleeping, wake them up
        if(requests.empty()){
            requests.push(clientSocket);
            pthread_cond_broadcast(&conditionCond);
        }
        //if threads are not sleeping, just push the clientSocket
        else{
            requests.push(clientSocket);
        }
        pthread_mutex_unlock(&queueMutex);

    }
    //int end = clock();
    //warn("time of execution: %f", (end-start)/double(CLOCKS_PER_SEC)*1000);
    return 0;
}
int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}
void response200(int clientSocket){
    char reply[BUFFSIZE] = "200 OK\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void response201(int clientSocket){
    char reply[BUFFSIZE] = "201 Created\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void response400(int clientSocket){
    char reply[BUFFSIZE] = "400 Bad Request\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void response403(int clientSocket){
    char reply[BUFFSIZE] = "403 Forbidden\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void response404(int clientSocket){
    char reply[BUFFSIZE] = "404 Not Found\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void response500(int clientSocket){
    char reply[BUFFSIZE] = "500 Internal Server Error\r\n";
    send(clientSocket, reply, strlen(reply), 0);
}
void responseContentLength(int clientSocket, int length){
    char reply[BUFFSIZE] = "Content-length: ";
    string s = to_string(length) + "\r\n\r\n";
    char l[BUFFSIZE];
    strcpy(l, s.c_str());
    send(clientSocket, reply, strlen(reply), 0);
    send(clientSocket, l, strlen(l), 0);
}
bool checkIfFileExist(char*filename){
    if(access(filename, F_OK) != -1)
        return true;
    else{
        return false;
    }
}
bool checkReadPermission(char*filename){
    if(access(filename, R_OK) != -1)
        return true;
    else{
        return false;
    }
}
bool checkWritePermission(char*filename){
    if(access(filename, W_OK) != -1)
        return true;
    else{
        return false;
    }
}
void parseArguments(int argc, char * argv[]){
    int c;
    while((c = getopt(argc, argv, "N:I:")) != -1){
        switch(c){
            case('N'):
                SIZE = atoi(optarg);
                break;
            case('I'):
                if((logfd = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0){
                    warn("%s", optarg);
                }
                break;
            case('?'):  
                break; 
            default:
                abort();
        }
        
    }
    
}
void writeFileInput(char*f, int clientSocket, char buffer[]){
    warn("get req started");
    pthread_mutex_lock(&sharedVarMutex);
    warn("get locked");
    int fd, length = 0;
    char httpMethod[4] = "GET";
    int response = 200;
    
    bool read_ok = checkReadPermission(f);
    if((fd = open(f, O_RDONLY)) < 0){
        response404(clientSocket);
        response = 404;
    }
    if(read_ok == false && response != 404){
        response403(clientSocket);
        response = 403;
    }
    if(isDirectory(f) == 1) {
        errno = EISDIR;
        response400(clientSocket);
        response = 400;
    }
    if((length = read(fd, buffer, BUFFSIZE)) >= 0){
        response200(clientSocket);
        responseContentLength(clientSocket, length);
        send(clientSocket, buffer, length, 0);
    }
    close(fd);
    warn("get req finished");

    /* The program will need to use synchronization between 
     * threads to reserve the space, but should need no 
     * synchronization to actually write the log information, 
     * since no other thread is writing to that location. */
    
    if(logfd != 0){
        int startingPoint = OFFSET;
        reserveSpaceToWriteLog(f, httpMethod, length, response);
        pthread_mutex_unlock(&sharedVarMutex);
        warn("get unlocked");
        writeLog(buffer, length, f, httpMethod, startingPoint, response);
    }
    else{
        pthread_mutex_unlock(&sharedVarMutex);
    }
    
}
void parseHeader(char buffer[], char * httpMethod, char * filename, int * contentLength){
    char http[100];
    sscanf(buffer, "%s %s %s %d", httpMethod, filename, http, contentLength);
}
void GetRequest(char * filename, int clientSocket){
    if(filename[0] == '/')
        filename++;
    char buffer[BUFFSIZE];
    writeFileInput(filename, clientSocket, buffer);
}
void PutRequest(char * filename, int clientSocket, int length){
    warn("Put req started");
    pthread_mutex_lock(&sharedVarMutex);
    warn("put locked");
    int fd;
    char httpMethod[4] = "PUT";
    int response = 200;
    if(filename[0] == '/')
        filename++;
    char buffer[BUFFSIZE];
    bool file_exist = checkIfFileExist(filename);
    if(file_exist){
        bool write_ok = checkWritePermission(filename);
        if(write_ok == false){
            response403(clientSocket);
            response = 403;
        }
    }
    if((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0){
        warn("%s", filename);
    }
    char inputToWrite[BUFFSIZE];
    if((length = read(clientSocket, buffer, BUFFSIZE)) >= 0){
        sscanf(buffer, "%[^/1.1]", inputToWrite);
        write(fd, inputToWrite, length);
        
    }
    if(file_exist)
        response200(clientSocket);
    else if(response != 403){
        response201(clientSocket);
    }
    responseContentLength(clientSocket, 0);
    close(fd);
    warn("Put req finished");

    /* The program will need to use synchronization between 
     * threads to reserve the space, but should need no 
     * synchronization to actually write the log information, 
     * since no other thread is writing to that location. */
    
    if(logfd != 0){
        int startingPoint = OFFSET;
        reserveSpaceToWriteLog(filename, httpMethod, length, response);
        pthread_mutex_unlock(&sharedVarMutex);
        warn("put unlocked");
        writeLog(buffer, length, filename, httpMethod, startingPoint, response);
    }
    else{
        pthread_mutex_unlock(&sharedVarMutex);
    }
   

}
void reserveSpaceToWriteLog(char * filename, char * httpMethod, int length, int response){
    if(response == 200){
        if(length == 0)
            OFFSET += strlen(httpMethod) + strlen(filename) + 10 + (strlen(to_string(length).c_str())) + 8;
        else if((length%20) == 0 && length >= 20)
            OFFSET += strlen(httpMethod) + strlen(filename) + 10 + (strlen(to_string(length).c_str())) + length*2 + 9*(length/20) + 11 + 19*(length/20);
        else{
            OFFSET += strlen(httpMethod) + strlen(filename) + 10 + (strlen(to_string(length).c_str())) + length*2 + 9*(length/20)+9 + 13 + 19*(length/20) + (length%20);
        } 
    }
    else{
        OFFSET += 6 + strlen(httpMethod) + strlen(filename) + (strlen(to_string(response).c_str())) + 35;
    }
}
void writeLog(char * buffer, int length, char * filename, char * httpMethod, int offset, int response){
    warn("Offset before writing %d", offset);
    warn("response %d\n", response);
    char header[BUFFSIZE];
    if(response == 200){
        if(length == 0)
            sprintf(header, "%s %s length %d", httpMethod, filename, length);
        else{
            sprintf(header, "%s %s length %d\n00000000 ", httpMethod, filename, length);
        }
        pwrite(logfd, header, strlen(header), offset);
        offset += strlen(header);
        char paddedBytes[BUFFSIZE];
        char charAsHex[BUFFSIZE];
        for(int i = 0; i < length; i++){
            sprintf(&charAsHex[i*2], "%02x", buffer[i]);
            if((i+1)%20 == 0){
                pwrite(logfd, &charAsHex[i*2], 2, offset);
                offset += 2;
                pwrite(logfd, " ", 1, offset);
                offset += 1;
                if((i+1) != length){
                    sprintf(paddedBytes, "\n%08d ", i+1);
                    pwrite(logfd, paddedBytes, strlen(paddedBytes), offset);
                    offset += strlen(paddedBytes);
                }
            }
            else{
                pwrite(logfd, &charAsHex[i*2], 2, offset);
                offset += 2;
                pwrite(logfd, " ", 1, offset);
                offset += 1;
            }
        }
        pwrite(logfd, "\n=======\n", 9, offset);
        offset += 9;
    }
    else{
        sprintf(header, "FAIL: %s %s HTTP/1.1 --- response %d\\n\n=======\n", httpMethod, filename, response);
        pwrite(logfd, header, strlen(header), offset);
        offset += strlen(header);
    }
    warn("Offset after writing %d", offset);
}
void *processRequests(void *){
    int length;
    char httpMethod[3];
    char filename[27];
    int contentLength = 0;
    int clientSocket;
    char buffer[BUFFSIZE];

    while(true){
        clientSocket = -1;
        pthread_mutex_lock(&conditionMutex);
        if(requests.empty()){
            pthread_cond_wait(&conditionCond, &conditionMutex);
        }
        else{
            clientSocket = requests.front();
            requests.pop();
        }
        pthread_mutex_unlock(&conditionMutex);

        if(clientSocket >= 0){
            warn("clientsocket recv");
        
            length = read(clientSocket, buffer, BUFFSIZE);
            parseHeader(buffer, httpMethod, filename, &contentLength);
            warn("buffer changed");
            if((filename[0] == '/' && strlen(filename) != 28) || (filename[0] != '/' && strlen(filename) != 27)){
                response400(clientSocket);
                responseContentLength(clientSocket, 0);
                pthread_mutex_lock(&sharedVarMutex);
                int startingPoint = OFFSET;
                reserveSpaceToWriteLog(filename, httpMethod, length, 400);
                pthread_mutex_unlock(&sharedVarMutex);
                warn("put unlocked");
                writeLog(buffer, length, filename, httpMethod, startingPoint, 400);
            }
            else if(strcmp(httpMethod, "GET") == 0)
                GetRequest(filename, clientSocket);
            else if(strcmp(httpMethod, "PUT") == 0)
                PutRequest(filename, clientSocket, length);
            else{
                response500(clientSocket);
            }
         
            close(clientSocket);

        }
        


    }

}
void initWorkers(pthread_t * workers, int size){
    for(int i = 0; i < size; i++){
        warn("i:%d ", i);
        if(pthread_create(&workers[i], NULL, &processRequests, NULL))
            warn("Thread");
    }
}