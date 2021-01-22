# MultiThreadedHTTPServer

This is a multi-threaded HTTP server written in the C programming language.
This server includes a number of features that can be manipulated using command line arguments - these features are:  
  
1). Full logging of request data.  This server will fully log all requests it handles (whether write request or read request) including the full content of the file
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
  
  This server also has a health check functionality.  Example:  sending a curl request of the type "curl -v <server_address>/healthcheck" will return the number of successfully   services requests over the number of unsuccessfully services requests.  An "unsuccessfully serviced request" is any request that results in no data being returned.               HTTP status codes returns in the 400's and 500's constitute "unsuccessfully serviced requests".
  
  Further Notes:  
    
  -Currently, this server only handles GET, PUT, and HEAD requests.  
  -I am experiencing a bug in which the health check functionality of the server causes a segfault after one request, but only in the docker container.  
   I developed the server on a Virtual Machine running Ubuntu, version 18.04.5 and the server runs in that environment with no issues.   
   Unfortunately, although the docker container is running the exact same O.S. version, the segfault does occur when the server is running within the container.  
   I am currently unsure as to why this is happening, but I am working on fixing it.  All other server functionality within the container is working as expected, however.
   
 Feel free to use the "docker exec -it /bin/bash" command from a separate terminal in order to open a terminal within the docker container to observe the logging file.  
 One last note: because this was an academic project, some strange restrictions were placed on the code, and files requested in any capacity (whether read or write) can consist                 of only the following characters: a-z, A-Z, 0-9, -, and _.  Also, their names cannot be longer than 27 characters.
 Example of a good file name:    
 FiLeNaMe_-  
 Example of a bad file name:  
 FiLeNaMe.!~
 
 
 My apologies for the health check bug, but happy file reading and writing!
 
  
  
  
 
 
  
