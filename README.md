# MultiThreadedHTTPServer

Currently only handles GET, PUT, and HEAD requests.
To Spin Up Server:

Navigate to directory where server lives.
Type "Make" into terminal to build server.
Spin up server by following these instructions:

- If the option for logging is to be enabled, simply provide the "-l (logFileName)" flag as an argument.
- If a particular number of threads is to be used, simply provide the "-N (threadCountNumber)" flag as an argument.

-The binary executable "httpserver4" created by the included Makefile can be run by simply typing "./httpserver portnumber arg2 arg3" where "portnumber"
is the desired port number for client connection.  The portnumber argument is required.  The other two, i.e., the logging argument and the number of threads agrument are optional.  If no logging flag is provided, the server will run without logging.  If no thread count number is provided, the server will run with the default number of threads which is 4.

-This server is capable of returning "healthcheck" data to the client.  To perform a health check as a client, simply make a requests of the form "GET /healthcheck".  For example, one could use the curl command in the following way (if one wanted to test the healthcheck functionality on the same macine the server was running on):

curl -v localhost:(serverPortNumber)\healthcheck

The server will return the healthcheck data as a ratio of the number of requests unsuccecssfully serviced over the number of requests successfully serviced.
