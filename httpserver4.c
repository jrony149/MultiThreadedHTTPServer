#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h> // atoi
#include <string.h> // memset
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h> // write
#include <pthread.h>
#include <stdint.h>
#include <getopt.h>
#include "List.h"

#define BUFFER_SIZE 4096

pthread_mutex_t runMutex            = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  threadConditionVar  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  mainConditionVar    = PTHREAD_COND_INITIALIZER;

pthread_mutex_t fileMutex           = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  fileConditionVar    = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logFileOffsetMutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t healthCheckMutex    = PTHREAD_MUTEX_INITIALIZER;

struct threadData {
    List clientSockQ;
    List readQ;
    List writeQ;
    ssize_t globalLogFileOffset;
    //ssize_t localLogFileOffset;
    int logFileDescriptor;
    uint64_t responseCounter;
    uint64_t failCounter;
    uint8_t logFlag;
};

struct httpObject {
    /*
        Create some object 'struct' to keep track of all
        the components related to a HTTP message
        NOTE: There may be more member variables you would want to add
    */
    char requestCode[5];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length; // example: 13
    int status_code; //for logging the response code (put response code here)
    uint8_t buffer[BUFFER_SIZE + 1];//holds the data
    ssize_t totalHeaderBytes; //records the total number of header bytes (including any message body bytes)
    ssize_t hBytesMinusMBytes;//
    ssize_t responseContentLength; //this is the contentLength you're returning with your response.
    char failBuf[100];
    char totalBuf[100];
};

void logFileErrorWriter(struct httpObject* message, char *buffer, struct threadData* data){

    int localLogFileOffset = 0;
    int sizeOfFirstLine    = 0;
    int pwriteReturn       = 0;

    char *endOfFirstLine          = strstr((const char *)message->buffer, (const char *)"\r");
    ptrdiff_t bytesToBeWritten    = endOfFirstLine - (char *)message->buffer;
    message->buffer[bytesToBeWritten] = '\0';
    
    sizeOfFirstLine = sprintf(buffer, "FAIL: %s --- response %d\n========\n", message->buffer, message->status_code);

    pthread_mutex_lock(&logFileOffsetMutex);

    data->globalLogFileOffset += sizeOfFirstLine;
    localLogFileOffset = data->globalLogFileOffset - sizeOfFirstLine;
    
    pthread_mutex_unlock(&logFileOffsetMutex);

    if((pwriteReturn = pwrite(data->logFileDescriptor, buffer, sizeOfFirstLine, localLogFileOffset)) < 0){

        warn("%d", data->logFileDescriptor);
    }

    pthread_mutex_lock(&healthCheckMutex);

    data->responseCounter += 1;
    data->failCounter     += 1;

    pthread_mutex_unlock(&healthCheckMutex);
    return;

}

void logFileWriter(int64_t totalBytesOut, struct httpObject* message, struct threadData* data, ssize_t fd){


    if(data->logFlag == 1){

        int fd2 = 0;
        if(message->status_code == 200 || message->status_code == 201){
            if ((fd2 = open(message->filename, O_RDONLY)) < 0) {
                warn("%s%s\n", message->filename, "logFileWriter() failed to open file descriptor");

            }
        }
        
        uint8_t buffer[BUFFER_SIZE + 1];
        char buffer2[BUFFER_SIZE + 1];
        char hcBuffer[100 + 1];

        buffer[BUFFER_SIZE]              = '\0';
        buffer2[BUFFER_SIZE]             = '\0';
        hcBuffer[100]                    = '\0';
        int64_t sizeOfFirstLine          = 0;
        int64_t localLogFileOffset       = 0;
        int pwriteReturn                 = 0;
        int numOfFullLinesDestinedForLog = 0;
        int64_t totalBytesWrittenToLog   = 0;
        int readReturn                   = 0;
        int buffer2Offset                = 0;
        uint16_t safeReadVal             = 0;
        uint8_t whileLoopCount           = 0;
        uint64_t offSet                  = 0;
        uint64_t sprintfReturn           = 0;

        
        if(message->status_code == 500){
            logFileErrorWriter(message, buffer2, data);
            if(fd2 != 0){
                close(fd2);
            }
            return;
        }
        if(message->status_code == 400){

            logFileErrorWriter(message, buffer2, data);
            if(fd2 != 0){
                close(fd2);
            }
            return;
        }
        if(message->status_code == 404){
            logFileErrorWriter(message, buffer2, data);
            if(fd2 != 0){
                close(fd2);
            }
            return;
        }
        if(message->status_code == 403){
            logFileErrorWriter(message, buffer2, data);
            if(fd2 != 0){
                close(fd2);
            }
            return;
        }
        if(message->status_code == 418){
            logFileErrorWriter(message, buffer2, data);
            if(fd2 != 0){
                close(fd2);
            }
            return;
        }
        if((strcmp(message->filename, "healthcheck") == 0) && (strcmp(message->requestCode, "GET") == 0)){

            //sizeOfHealthCheckData = sprintf(hcBuffer, "%s %s%s %s", message->failBuf, "0a", message->totalBuf);
            for(uint64_t x = 0; x < strlen(message->failBuf); x++){
                sprintfReturn = sprintf(&hcBuffer[offSet], "%02x ", message->failBuf[x]);
                offSet += sprintfReturn;
            }
            sprintfReturn = 0;
            hcBuffer[offSet] = '\0';
            strcat(hcBuffer, "0a ");
            offSet += 3;
            for(uint64_t x = 0; x < strlen(message->totalBuf); x++){
                if(x == strlen(message->totalBuf)){
                    sprintfReturn = sprintf(&hcBuffer[offSet], "%2x", message->totalBuf[x]);
                    offSet += sprintfReturn;
                    break;
                }

                sprintfReturn = sprintf(&hcBuffer[offSet], "%02x ", message->totalBuf[x]);
                offSet += sprintfReturn;
            }

            sizeOfFirstLine = sprintf(buffer2, "%s /%s length %ld\n%s\n========\n", message->requestCode, message->filename, message->content_length, hcBuffer);

            pthread_mutex_lock(&logFileOffsetMutex);
            data->globalLogFileOffset = data->globalLogFileOffset + sizeOfFirstLine;
            localLogFileOffset = data->globalLogFileOffset - sizeOfFirstLine;
            pthread_mutex_unlock(&logFileOffsetMutex);

            if((pwriteReturn = pwrite(data->logFileDescriptor, buffer2, sizeOfFirstLine, localLogFileOffset)) < 0){
                warn("%d", data->logFileDescriptor);
            }
            if(fd2 != 0){
                close(fd2);
            }

            pthread_mutex_lock(&healthCheckMutex);

            data->responseCounter += 1;

            pthread_mutex_unlock(&healthCheckMutex);

            return;
        }

        if(strcmp(message->requestCode, "HEAD") == 0){

            sizeOfFirstLine = sprintf(buffer2, "%s /%s length %ld\n========\n", message->requestCode, message->filename, totalBytesOut);
            pthread_mutex_lock(&logFileOffsetMutex);
            data->globalLogFileOffset = data->globalLogFileOffset + sizeOfFirstLine;
            localLogFileOffset = data->globalLogFileOffset - sizeOfFirstLine;
            pthread_mutex_unlock(&logFileOffsetMutex);

            if((pwriteReturn = pwrite(data->logFileDescriptor, buffer2, sizeOfFirstLine, localLogFileOffset)) < 0){
                warn("%d", data->logFileDescriptor);
            }
            if(fd != 2){
                close(fd2);
            }

            pthread_mutex_lock(&healthCheckMutex);

            data->responseCounter += 1;

            pthread_mutex_unlock(&healthCheckMutex);

            return;
        }
        if((message->status_code == 200 || message->status_code == 201) && totalBytesOut == 0){
            
            sizeOfFirstLine = sprintf(buffer2, "%s /%s length %ld\n========\n", message->requestCode, message->filename, totalBytesOut);
            pthread_mutex_lock(&logFileOffsetMutex);
            data->globalLogFileOffset += sizeOfFirstLine;
            localLogFileOffset = data->globalLogFileOffset - sizeOfFirstLine;
            pthread_mutex_unlock(&logFileOffsetMutex);

            if((pwriteReturn = pwrite(data->logFileDescriptor, buffer2, sizeOfFirstLine, localLogFileOffset)) < 0){
                warn("%d%s\n", data->logFileDescriptor, " pwrite in logFileWriter() function failed to write data to log.");
            }

            if(fd2 != 0){
                close(fd2);
            }

            pthread_mutex_lock(&healthCheckMutex);
            data->responseCounter += 1;
            pthread_mutex_unlock(&healthCheckMutex);


            return;
        }

        if((message->status_code == 200 || message->status_code == 201) && totalBytesOut != 0){

            sizeOfFirstLine = sprintf(buffer2, "%s /%s length %ld\n", message->requestCode, message->filename, totalBytesOut);
            buffer2Offset += sizeOfFirstLine;

            pthread_mutex_lock(&logFileOffsetMutex);

            if((totalBytesOut/20 >= 1) && ((totalBytesOut % 20) == 0)){//you have a flat number of lines.  No remainder.

                numOfFullLinesDestinedForLog = (totalBytesOut/20);
                data->globalLogFileOffset += sizeOfFirstLine + (69*numOfFullLinesDestinedForLog) + 9;
                localLogFileOffset = data->globalLogFileOffset - (sizeOfFirstLine + (69*numOfFullLinesDestinedForLog) + 9);

            }

            else if((totalBytesOut/20) == 0){//you have less than 1 line

                data->globalLogFileOffset += sizeOfFirstLine + (3*totalBytesOut) + 18;
                localLogFileOffset = data->globalLogFileOffset  - (sizeOfFirstLine + (3*totalBytesOut) + 18);
                
            }

            else if ((totalBytesOut/20 >= 1) && (totalBytesOut%20) != 0){//you have 1 line or more w/remainder

                numOfFullLinesDestinedForLog = (totalBytesOut/20);
                data->globalLogFileOffset += sizeOfFirstLine + (69*numOfFullLinesDestinedForLog) + (3*(totalBytesOut%20)) + 18;
                localLogFileOffset = data->globalLogFileOffset - (sizeOfFirstLine + (69*numOfFullLinesDestinedForLog) + (3*(totalBytesOut%20)) + 18);
                
            }
            
            pthread_mutex_unlock(&logFileOffsetMutex);

            while(totalBytesWrittenToLog < totalBytesOut){

                safeReadVal = 20 * ((BUFFER_SIZE - (sizeOfFirstLine))/ 69);//number of lines we can safely put into our converted buffer
                
                if((readReturn = read(fd2, buffer, safeReadVal)) < 0){
                    warn("%ld", fd);
                }
                sizeOfFirstLine = 0;
                for(int64_t x = 0; x < readReturn; x++){

                    if(x == 0 && whileLoopCount == 0){

                        sprintfReturn = sprintf((buffer2 + buffer2Offset), "%08d", 0);
                        buffer2Offset += sprintfReturn;
                        whileLoopCount = 1;
                    }

                    else if((totalBytesWrittenToLog >= 20) && ((totalBytesWrittenToLog%20) == 0)){

                        sprintfReturn = sprintf((buffer2 + buffer2Offset), "\n%08ld", totalBytesWrittenToLog);
                        buffer2Offset += sprintfReturn;
                    }
                    
                    sprintfReturn = sprintf((buffer2 + buffer2Offset), " %02x", buffer[x]);
                    totalBytesWrittenToLog += 1;
                    buffer2Offset += sprintfReturn;
                    
                }

                sizeOfFirstLine = 0;

                if(totalBytesWrittenToLog == totalBytesOut){
        
                    sprintfReturn = sprintf((buffer2 + buffer2Offset), "%s", "\n========\n");
                    buffer2Offset += 10;
                }

                if((pwriteReturn = pwrite(data->logFileDescriptor, buffer2, buffer2Offset, (localLogFileOffset))) < 0){
                    warn("%d", data->logFileDescriptor);
                }


                localLogFileOffset += buffer2Offset;
                buffer2Offset = 0;

            }

        }

        pthread_mutex_lock(&healthCheckMutex);
        data->responseCounter += 1;
        pthread_mutex_unlock(&healthCheckMutex);

        if(fd2 != 0){
            close(fd2);
        }
        
    }

    return;
}

void messageHandler(ssize_t client_sockd, ssize_t fd, struct httpObject* message, struct threadData* data){

    int sprintfReturn  = 0;
    int sprintfReturn2 = 0;

    if((strcmp(message->filename, "healthcheck") == 0) && (strcmp(message->requestCode, "GET") == 0) && (data->logFlag == 1)){


        int64_t Bytes = 0;
        memset(&message->buffer, 0, sizeof(message->buffer));

        pthread_mutex_lock(&healthCheckMutex);

        sprintfReturn  = sprintf(message->failBuf, "%ld", data->failCounter);//writing the failCounter and the total responseCounter into bufs for later use in logging.
        sprintfReturn2 = sprintf(message->totalBuf, "%ld", data->responseCounter);
        
        message->failBuf[sprintfReturn]   = '\0';
        message->totalBuf[sprintfReturn2] = '\0';

        Bytes = sprintf((char *)message->buffer, "%ld\n%ld", data->failCounter, data->responseCounter);

        pthread_mutex_unlock(&healthCheckMutex);
        
        if(dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n%s", Bytes, message->buffer) < 0){
            warn("%ld%s", client_sockd, "dprintf in messageHandler healthcheck case failed to print.");
            return;
        }
        

        return;

    }
    if(message->status_code == 400){

        if(dprintf(client_sockd, "%s", "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n") < 0){
            warn("%ld", client_sockd);
            return;
        }
        close(client_sockd);
        if(fd != 0){
            close(fd);
        }
            
    }

    if(message->status_code == 403){

        if(dprintf(client_sockd, "%s", "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n") < 0){
            warn("%ld", client_sockd);
        }
        close(client_sockd);
        if(fd != 0){
            close(fd);
        }

    }

    if(message->status_code == 404){

        if(dprintf(client_sockd, "%s", "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n") < 0){
            warn("%ld", client_sockd);
        }
        close(client_sockd);
        if(fd != 0){
            close(fd);
        }
    }

    if(message->status_code == 500){

        if(dprintf(client_sockd, "%s", "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n") < 0){
            warn("%ld", client_sockd);
        }
        close(client_sockd);
        if(fd != 0){
            close(fd);
        }
    }

    if(message->status_code == 418){

        if(dprintf(client_sockd, "%s", "HTTP/1.1 418 I'm a teapot\r\nContent-Length: 0\r\n\r\n") < 0){
            warn("%ld", client_sockd);
        }
        close(client_sockd);
        if(fd != 0){
            close(fd);
        }
    }

    if(message->status_code == 200){

       if((dprintf(client_sockd, "%s%ld%s", "HTTP/1.1 200 OK\r\nContent-Length: ", message->responseContentLength, "\r\n\r\n")) < 0){
           warn("%ld", client_sockd);
       }
       if (strcmp(message->requestCode, "PUT") == 0 || strcmp(message->requestCode, "HEAD") == 0){
           close(client_sockd);
           if(fd != 0){
               
              close(fd);
           }
       }

    }

    if(message->status_code == 201){

        if((dprintf(client_sockd, "%s%ld%s", "HTTP/1.1 201 Created\r\nContent-Length: ", message->responseContentLength, "\r\n\r\n")) < 0){
            warn("%ld", client_sockd);
        }
        if(strcmp(message->requestCode, "PUT") == 0 || strcmp(message->requestCode, "HEAD") == 0){
            close(client_sockd);
            close(fd);
        }
    }
}

int blackListCheck(struct httpObject* message, ssize_t client_sockd, struct threadData* data) {

  uint8_t dash = 45;
  uint8_t undScore = 95;

  if (strlen(message->filename) > 27) {

    message->status_code = 400;
    messageHandler(client_sockd, 0, message, data);
    logFileWriter(0, message, data, 0);


    return 1;
  }

  for (uint64_t x = 0; x < strlen(message->filename); x++) {

    if (!isalnum(message->filename[x]) && message->filename[x] != dash && message->filename[x] != undScore) {
        message->status_code = 400;
        messageHandler(client_sockd, 0, message, data);
        logFileWriter(0, message, data, 0);

        return 1;
    }
  }

  return 0;
}

int8_t parse_request_header(struct httpObject* message, ssize_t client_sockd, struct threadData* data){


    message->content_length = 0;
    char *rnAddr            = strstr((const char *)message->buffer, "\r\n");
    char dummy1[20];
    char dummy2[20];
    char Key[80];
    char Value[2048];
    char header[BUFFER_SIZE + 1];
    header[BUFFER_SIZE]     = '\0';
    memcpy(header, message->buffer, BUFFER_SIZE);
    char *rCode             = "";
    char *version           = "";

    if((sscanf(header, "%s /%s %s\r\n", message->requestCode, message->filename, message->httpversion) == 3)){

        rCode = message->requestCode;
        version = message->httpversion;
        if(strcmp(version, "HTTP/1.1") != 0){

            message->status_code = 400;
            messageHandler(client_sockd, 0, message, data);
            logFileWriter(0, message, data, 0);
        
            return 1;
        }

        if((strcmp(rCode, "PUT") != 0) && (strcmp(rCode, "GET") != 0) && (strcmp(rCode, "HEAD") != 0)){
            message->status_code = 400;
            messageHandler(client_sockd, 0, message, data);
            logFileWriter(0, message, data, 0);
        
            return 1;
        }
    
    }

    if((sscanf(header, "%s /%s %s\r\n", message->requestCode, message->filename, message->httpversion) < 3)){
        message->status_code = 400;
        messageHandler(client_sockd, 0, message, data);
        logFileWriter(0, message, data, 0);
        
        return 1;
    }    

    while ((rnAddr != NULL) && ((sscanf(rnAddr, "\r\n%[^ :]: %s%[\r]%[\n]", Key, Value, dummy1, dummy2)) == 4)) {

        if(strcmp(Key, "Content-Length") == 0){

            if(strcmp(rCode, "PUT") != 0){
                message->status_code = 400;
                messageHandler(client_sockd, 0, message, data);
                logFileWriter(0, message, data, 0);
                return 1;
            }

            for (uint64_t x = 0; x < strlen(Value); x++) { // check to see if it is completely composed of digits

                if (!isdigit(Value[x])) {
                    message->status_code = 400;
                    messageHandler(client_sockd, 0, message, data);
                    logFileWriter(0, message, data, 0);
                    return 1; //If a non-numerical char is found the Content-Length value, return 1
                }
            }
            message->content_length = atoi(Value); 
        }
        
        if (strstr((const char *)rnAddr, "\r\n") != NULL) {
            rnAddr = rnAddr + 1;
        }
        rnAddr = strstr((const char *)rnAddr, "\r\n");

    }

    char *currentAddr       = strstr((const char *)rnAddr, (const char *)rnAddr);
    char *addrOfEndOfHeader = strstr((const char *)rnAddr, (const char *)"\r\n\r\n");

    if( (strcmp(rCode, "PUT") != 0) && (strcmp(rCode, "HEAD") != 0) && (strcmp(rCode, "GET") != 0) ){

        message->status_code = 400;
        messageHandler(client_sockd, 0, message, data);
        logFileWriter(0, message, data, 0);
        
        return 1;
    }

    if (rnAddr != NULL && currentAddr == addrOfEndOfHeader) {
        return 0;
    }

    message->status_code = 400;
    messageHandler(client_sockd, 0, message, data);
    logFileWriter(0, message, data, 0);

    return 1; //If the overall structure of the header is mal-formed, return 1.
}


int read_http_request(ssize_t client_sockd, struct httpObject* message, struct threadData* data) {
    

    message->buffer[BUFFER_SIZE] = '\0';
    
    ssize_t bytes  = 0;
    ssize_t offSet = 0;
    ssize_t endOfHeader = 0;

    while ((bytes = recv(client_sockd, &message->buffer[offSet], BUFFER_SIZE, 0)) > 0){

        endOfHeader = (ssize_t)strstr((const char *)message->buffer, "\r\n\r\n");

        if(!endOfHeader){
            offSet = offSet + bytes;
        }

        if(endOfHeader){
            break;
        }
    }

    if (!endOfHeader){//If we've broken out of the loop, and the end of the header cannot be validated ...

        if(bytes == -1){
            message->status_code = 500;
            messageHandler(client_sockd, 0, message, data);//If bytes == -1, then there was server error; throw 500
            logFileWriter(0, message, data, 0);
            return 1;
            
        }

        else{
            message->status_code = 400;
            messageHandler(client_sockd, 0, message, data); //...anything else is a bad request; throw 400.
            logFileWriter(0, message, data, 0);
            return 1;
            
        }
    }

    else if (endOfHeader && (offSet == 0)){ //This will be useful when you want to account for message body bytes in the header recv()
        message->totalHeaderBytes = bytes;
    }

    else if (endOfHeader && (offSet > 0)){
        message->totalHeaderBytes = offSet;
    }

    return 0;
}

int headerBytesDetector(struct httpObject* message){
    
    char *endOfHeader = strstr((const char *)message->buffer, (const char *)"\r\n\r\n"); //doing the math ...
    ptrdiff_t bytesInHeader    = endOfHeader - (char *)message->buffer;
    message->hBytesMinusMBytes = bytesInHeader;
    int64_t messageBodyBytes   = (message->totalHeaderBytes) - (bytesInHeader + 4);

    if((bytesInHeader + 4) != message->totalHeaderBytes){//using the math...handles the case in which there are bytes after the \r\n\r\n in the final header recv()
        return messageBodyBytes;
        
    }

    return 0;

}

void putHandler(ssize_t client_sockd, ssize_t fd, struct httpObject* message, struct threadData* data, uint16_t responseType){



    int fstatReturn = 0;

    struct stat buf;
    fstat(fd, &buf);
    //off_t size               = buf.st_size;
    
    if((fstatReturn = fstat(fd, &buf)) < 0){

        warn("%ld", fd);

        message->status_code = 500;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        
    }
    
    int64_t totalBytesReadFromClient = 0;
    int64_t totalBytesWrittenToFile  = 0;
    int64_t bytesReceived            = 0;
    int64_t writeReturn              = 0;
    int64_t messageBodyBytes         = headerBytesDetector(message);
    
    if (messageBodyBytes > 0){

        totalBytesReadFromClient = messageBodyBytes;

        if((writeReturn = write(fd, &(message->buffer[message->hBytesMinusMBytes + 4]), messageBodyBytes)) <= 0){
            
            warn("%ld", fd);
            message->status_code = 500;
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(0, message, data, 0);
            
        }

        totalBytesWrittenToFile = writeReturn;
    }

    while (totalBytesWrittenToFile < message->content_length) {

      if ((bytesReceived = recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) <= 0) {

        warn("%ld", client_sockd);
        message->status_code = 500;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        return;
    
    }

      totalBytesReadFromClient = totalBytesReadFromClient + bytesReceived;

      if (totalBytesReadFromClient > message->content_length) {

        message->status_code = 400;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        return;
    
      }

      if ((writeReturn = write(fd, message->buffer, bytesReceived)) <= 0) {

        warn("%ld", fd);
        message->status_code = 500;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        return;
        
        
      }

      totalBytesWrittenToFile = totalBytesWrittenToFile + writeReturn;
    }


    if(totalBytesWrittenToFile == message->content_length){
        if(responseType == 201){
            message->status_code = 201;
            message->responseContentLength = 0; //because this was a PUT request, we're not sending any data back, the contentLength is 0.
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(totalBytesWrittenToFile, message, data, fd);
            
            
        }
        if(responseType == 200){
            message->status_code = 200;
            message->responseContentLength = 0;
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(totalBytesWrittenToFile, message, data, fd);
        
            
        }
    }

    if(totalBytesWrittenToFile != message->content_length){
        message->status_code = 418;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
    }

}

void getHandler(ssize_t client_sockd, ssize_t fd, struct httpObject* message, struct threadData* data){

    int64_t totalBytesWrittenToClient = 0;
    int64_t bytesReceived             = 0;
    int64_t writeReturn               = 0;
    int64_t messageBodyBytes          = headerBytesDetector(message);

    struct stat buf;
    fstat(fd, &buf);
    off_t size                        = buf.st_size; //stat the file as clos as possible to this check (size == 0 check)

    if (size == 0){

        message->status_code = 200;
        message->responseContentLength = 0;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        close(client_sockd);
        close(fd);
        
    }

    else if (messageBodyBytes > 0){

        message->status_code = 400;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        return;
        
    }

    else{

        if ((bytesReceived = read(fd, message->buffer, BUFFER_SIZE)) <= 0) {//perform at least one sucessful read before sending 200 OK message to client.
            warn("%ld", fd);

            message->status_code = 500;
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(0, message, data, 0);
            
        }
        else{

            message->status_code = 200;
            message->responseContentLength = size;
            messageHandler(client_sockd, fd, message, data);

            while (totalBytesWrittenToClient < size) {

                if ((writeReturn = write(client_sockd, message->buffer, bytesReceived)) <= 0) {
                    
                    warn("%ld", client_sockd);

                    //close(client_sockd);
                    //close(fd);
                    
                    break;
                }

                totalBytesWrittenToClient = totalBytesWrittenToClient + writeReturn;

                if ((bytesReceived = read(fd, message->buffer, BUFFER_SIZE)) <= 0) { // If you get an error while reading here, there's nothing
                    warn("%ld", fd);
                    //close(client_sockd);
                    //close(fd);                                                          // you can do.  Close the fd and the sockd.
                    
                    break;
                }
            }

            close(client_sockd);
            close(fd);

            logFileWriter(totalBytesWrittenToClient, message, data, fd);

        }
        
    }
}

void headHandler(ssize_t client_sockd, ssize_t fd, struct httpObject* message, struct threadData* data){

    
    struct stat buf;
    fstat(fd, &buf);
    off_t size               = buf.st_size;

    int64_t messageBodyBytes = headerBytesDetector(message);

    if (messageBodyBytes > 0){
        message->status_code = 400;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(0, message, data, 0);
        
    }
    else {
        message->status_code = 200;
        message->responseContentLength = size;
        messageHandler(client_sockd, fd, message, data);
        logFileWriter(size, message, data, 0);
    
    }
    
}

void process_request(ssize_t client_sockd, struct httpObject* message, struct threadData* data) { //this function will handle opening the file, and all responsibilities that come with that task,
    
    struct stat stats;                                                                   //and will farm the actual reading and writing work out to separate functions for GET, HEAD, and PUT.
    
    if((strcmp(message->filename, "healthcheck") == 0) && (data->logFlag == 0)){
        message->status_code = 404;
        messageHandler(client_sockd, 0, message, data);
        return;
    }

    if((strcmp(message->filename, "healthcheck") == 0) && ((strcmp(message->requestCode, "PUT") == 0) || (strcmp(message->requestCode, "HEAD") == 0))){

        message->status_code = 403;
        messageHandler(client_sockd, 0, message, data);
        logFileWriter(0, message, data, 0);
        return;
    }

    int findReturn1 = 0;
    int findReturn2 = 0;
    ssize_t fd = 0;
    
    if(strcmp(message->requestCode, "PUT") == 0){

        stat(message->filename, &stats);
        
        if((stats.st_mode & S_IWUSR) != S_IWUSR && errno != ENOENT){
            
            message->status_code = 403;
            messageHandler(client_sockd, 0, message, data);
            logFileWriter(0, message, data, 0);
            return;
        }
        
        pthread_mutex_lock(&fileMutex); //  the enqeue lock

        while(((findReturn1 = find(data->writeQ, (void *)message->filename)) != -1) || ((findReturn2 = find(data->readQ, (void *)message->filename)) != -1)){

            pthread_cond_wait(&fileConditionVar, &fileMutex);
        }

        append(data->writeQ, (void *)message->filename);
        pthread_mutex_unlock(&fileMutex);

        if ((fd = open(message->filename, O_RDWR | O_TRUNC)) < 0) { // if opening fails, the file must not exist, so ...

            if ((fd = open(message->filename, O_CREAT | O_WRONLY | O_TRUNC, 0600)) < 0) { // create the file ...

                if (errno == EACCES) {

                    message->status_code = 403;
                    messageHandler(client_sockd, fd, message, data);
                    logFileWriter(0, message, data, 0);
                    
                }

                else {

                    message->status_code = 500;
                    messageHandler(client_sockd, fd, message, data);
                    logFileWriter(0, message, data, 0);
                    
                }

            }

            putHandler(client_sockd, fd, message, data, 201);

            pthread_mutex_lock(&fileMutex); //the dequeue lock
            findDelete(data->writeQ, (void *)message->filename);
            pthread_mutex_unlock(&fileMutex);
            pthread_cond_broadcast(&fileConditionVar);

            return;
        }

        putHandler(client_sockd, fd, message, data, 200);

        pthread_mutex_lock(&fileMutex); //the dequeue lock
        findDelete(data->writeQ, (void *)message->filename);
        pthread_mutex_unlock(&fileMutex);
        pthread_cond_broadcast(&fileConditionVar);

        return;

    }//close PUT case

    if(strcmp(message->requestCode, "GET") == 0){

        stat(message->filename, &stats);

        if(errno == ENOENT){

            message->status_code = 404;
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(0, message, data, 0);
            return;
        }

        if((stats.st_mode & S_IRUSR) != S_IRUSR){

            message->status_code = 403;
            messageHandler(client_sockd, 0, message, data);
            logFileWriter(0, message, data, 0);
            return;

        }

        pthread_mutex_lock(&fileMutex);

        while((findReturn2 = find(data->writeQ, (void *)message->filename)) != -1){

            pthread_cond_wait(&fileConditionVar, &fileMutex);
        }

        append(data->readQ, (void *)message->filename);
        pthread_mutex_unlock(&fileMutex);

        if ((fd = open(message->filename, O_RDONLY)) < 0) { //These checks are now redundant because of the stat checks above, but so what.

            if (errno == EACCES) {
                message->status_code = 403;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);
            }
            else if (errno == ENOENT) {
                message->status_code = 404;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);
                
            }

            else {
                message->status_code = 500;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);
            
            }
            
        }
        
        getHandler(client_sockd, fd, message, data);

        pthread_mutex_lock(&fileMutex);
        findDelete(data->readQ, (void *)message->filename);//the deqeue lock
        pthread_mutex_unlock(&fileMutex);
        pthread_cond_broadcast(&fileConditionVar);

        return;

    }//close GET case

    if(strcmp(message->requestCode, "HEAD") == 0){

        stat(message->filename, &stats);

        if(errno == ENOENT){

            message->status_code = 404;
            messageHandler(client_sockd, fd, message, data);
            logFileWriter(0, message, data, 0);
            return;
        }

        if((stats.st_mode & S_IRUSR) != S_IRUSR){

            message->status_code = 403;
            messageHandler(client_sockd, 0, message, data);
            logFileWriter(0, message, data, 0);
            return;


        }

        pthread_mutex_lock(&fileMutex);

         while((findReturn2 = find(data->writeQ, (void *)message->filename)) != -1){

            pthread_cond_wait(&fileConditionVar, &fileMutex);
        }

        append(data->readQ, (void *)message->filename);
        pthread_mutex_unlock(&fileMutex);

        if ((fd = open(message->filename, O_RDONLY)) < 0) {

            if (errno == EACCES) {
                message->status_code = 403;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);

            }
            else if (errno == ENOENT) {
                message->status_code = 404;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);
                
            }

            else {
                message->status_code = 500;
                messageHandler(client_sockd, fd, message, data);
                logFileWriter(0, message, data, 0);
            }
            
        }

        headHandler(client_sockd, fd, message, data);

        pthread_mutex_lock(&fileMutex);//the deqeue lock
        findDelete(data->readQ, (void *)message->filename);
        pthread_mutex_unlock(&fileMutex);
        pthread_cond_broadcast(&fileConditionVar);
        
    }//close head case

    return;
}


void handleServerTasks(int client_sockd, struct threadData* data){

    

   
    int parseReturn     = 0;
    int blackListReturn = 0;
    struct httpObject message;
    
    memset(message.buffer, 0, sizeof(message.buffer));

    if(read_http_request(client_sockd, &message, data) == 1){
        return;
    }

    
    if((parseReturn = parse_request_header(&message, client_sockd, data)) == 1){

    
        return;
    }

    if((blackListReturn = blackListCheck(&message, client_sockd, data)) == 1){


        return;
        
    }

    process_request(client_sockd, &message, data);



}

void * threadRunner(void *arg){

    struct threadData *parms;
    parms = (struct threadData*)arg;
    int client_sockd;
    while(1){

        pthread_mutex_lock(&runMutex);

        while(length(parms->clientSockQ) == 0){
            pthread_cond_wait(&threadConditionVar, &runMutex);
        }
        client_sockd = (intptr_t)back(parms->clientSockQ);
        deleteBack(parms->clientSockQ);
        pthread_cond_signal(&mainConditionVar);

        pthread_mutex_unlock(&runMutex);

        handleServerTasks(client_sockd, parms);

    }

    return NULL;
}


int main(int argc, char** argv) {

    if (argc < 2) {

        fprintf(stderr, "Minimum requirements for running server: port number.\n");
        return -1;
    }

    int option;
    int threadNumber = 4;
    char *logFileName = NULL;
    int logFd = 0;

    //handling command line args w/getOpt

    while((option = getopt(argc, argv, "l:N:")) != -1){
        switch(option){
            case 'l':
                logFileName = optarg;
                break;
            case 'N':
                for(uint8_t x = 0; x < strlen(optarg); x++){
                    if(!isdigit(optarg[x])){
                        fprintf(stderr, "Error. Argument must be an integer.  Server will run on default thread count (4)\n");
                        break;
                    }
                }
                threadNumber = atoi(optarg);
                break;
            default:
                printf("%s\n", "hello from default");    
        }
    }

    if(logFileName != NULL){

        if ((logFd = open(logFileName, O_CREAT | O_WRONLY | O_TRUNC, 0600)) < 0) { // create the log file ...
            warn("%s%s\n", logFileName, " Log file failed to open in main()"); // not sure what to do here ... 
        }
    }

   

    char* port = argv[optind];
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);


    if (server_sockd < 0) {
        perror("socket");
    }
    int enable = 1;

    /*
        This allows you to avoid: 'Bind: Address Already in Use' error
    */
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    /*
        Bind server address to socket that is open
    */
    ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

    /*
        Listen for incoming connections
    */
    if((ret = listen(server_sockd, SOMAXCONN)) < 0){
        warn("%d", server_sockd);
    } // 5 should be enough, if not use SOMAXCONN

    if (ret < 0) {
        return EXIT_FAILURE;
    }

    struct sockaddr client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    intptr_t client_sockd;

    struct threadData dat;

    dat.clientSockQ         = newList();
    dat.readQ               = newList();
    dat.writeQ              = newList();
    dat.logFileDescriptor   = logFd;
    dat.globalLogFileOffset = 0;
    dat.responseCounter     = 0;
    dat.failCounter         = 0;
    dat.logFlag             = 0;
    if(logFileName != NULL){
        dat.logFlag = 1;
    }

    pthread_t threadPool[threadNumber]; //creating what amounts to an array of thread variables.

    for(int x = 0; x < threadNumber; x++){ //initializing those thread variables (creating them).

        pthread_create(&threadPool[x],NULL, threadRunner, &dat);
    }

    while(1){

        printf("[+] server is waiting...\n");
    
        if ((client_sockd = accept(server_sockd, &client_addr, &client_addrlen))  >= 0){

            pthread_mutex_lock(&runMutex);
            while(length(dat.clientSockQ) > threadNumber){
                pthread_cond_wait(&mainConditionVar, &runMutex);
            }
            prepend(dat.clientSockQ, (void *)client_sockd);
            pthread_cond_signal(&threadConditionVar);
            pthread_mutex_unlock(&runMutex);  

        }

    }

    return 0;
}
