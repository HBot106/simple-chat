
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#define BACKLOG 10

// for the server side
int tcpServerSetup(int portNumber);
int tcpAccept(int server_socket, int debugFlag);

// for the client side
int tcpClientSetup(char * serverName, char * port, int debugFlag);


#endif
