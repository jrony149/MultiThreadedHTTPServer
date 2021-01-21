# MultiThreadedHTTPServer

Currently only handles GET, PUT, and HEAD requests.
To Spin Up Server:

Navigate to directory where server lives.
Type "Make" into terminal to build server.
Spin up server by following these instructions:

- If the option for logging is to be enabled, simply provide the "-l (logFileName)" flag as an argument.
- If a particular number of threads is to be used, simply provide the "-N (threadCountNumber)" flag as an argument.

- The binary executable "httpserver4" created by the included Makefile can be run by simply typing "./httpserver portnumber arg2 arg3" where "portnumber"
is the desired port number for client connection.  The portnumber argument is required.  The other two, i.e., the logging argument and the number of threads agrument are optional.  If no logging flag is provided, the server will run without logging.  If no thread count number is provided, the server will run with the default number of threads which is 4.

- This server is capable of returning "healthcheck" data to the client.  To perform a health check as a client, simply make a requests of the form "GET /healthcheck".  For example, one could use the curl command in the following way (if one wanted to test the healthcheck functionality on the same macine the server was running on):

  curl -v localhost:(serverPortNumber)\healthcheck

  The server will return the healthcheck data as a ratio of the number of requests unsuccecssfully serviced over the number of requests successfully serviced.
 
 -----------------------------------------------------------------------------------------------------------------------------------------------------------------
  
  
This is a multi-threaded HTTP server written in the C programming language.
This server includes a number of features that can be activated using command line arguments - these features are:  
  
1). Full logging of request data.  This server will fully log all files it handles (whether via write request or read request) including the full content of the file
in Hexadecimal (0x).  
  
2). The number of threads the server uses to carry out servicing of requests.  The user can set the number of "worker threads" the server uses.
I have included the necessary files for containerization of this server using Docker.  
  
TO RUN IN DOCKER CONTAINER:  
  Clone the repository.  
  First run the "buld_image.sh" file.  This file will create the Docker subnet "serverNet" as well as build the Docker image "mts" on your local machine.  
  Then you must run the "runserver.sh" file.  This file will take three command line arguments.  The first command line argument must be the port on which you want the docker     container to listen on your local machine.  The second command line argument must be the name of the logging file for the logging feature. And the third command line argument   must be the number of threads you want the server to utilize.  Experiment with this number and try to discover an optimal number of threads!    
    
  Example:    
    
  To build image:    
  $ ./build_image.sh    
  To run container:    
  $ ./runserver.sh <port_number_you_want_mapped_to_container> <name_of_logging_file> <number_of_threads_you_want_server_to_generate>    
  
  Once you are done using the multi-threaded HTTP server, to stop the container "mts", remove the container "mts", and to remove the network "serverNet", simply open a second     terminal / command prompt and navigate to the local repository.  Run the "stop_remove.sh" file.  This file will stop the container "mts", remove the container "mts", and         remove the network "serverNet".    
  
  
 
 
  
