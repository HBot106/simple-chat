#include "server.h"

int LARGEST_SOCKET;
struct socketDescriptor_t* HEAD_SOCKET;
struct socketDescriptor_t* TAIL_SOCKET;

int main(int argc, char *argv[])
{	
	int portNumber = checkArgs(argc, argv);
	int serverSocket = tcpServerSetup(portNumber);

	waitForConnection(serverSocket);
	
	/* close the sockets */
	close(serverSocket);

	return 0;
}

void waitForConnection(int serverSocket)
{
	//setup fd set for the select() call
	fd_set selectionSet;

	//a linked-list of socketDescriptor structs will hold socket numbers and their associated handles
	HEAD_SOCKET = (struct socketDescriptor_t*)malloc(sizeof(struct socketDescriptor_t));
	if (!HEAD_SOCKET)
	{
		perror("malloc error");
		exit(-1);
	}
	//at init, tail = head
	TAIL_SOCKET = HEAD_SOCKET;

	//setup the server socket as the head of the linked-list
	HEAD_SOCKET->fileDescriptor = serverSocket;
	strcpy(HEAD_SOCKET->handle, "Server Socket");
	HEAD_SOCKET->next_socketDescriptor = NULL;
	HEAD_SOCKET->previous_socketDescriptor = NULL;

	while (1)
	{
		resetSelectionSet(&selectionSet);

		//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "waiting for connection, select()");

		if(select((LARGEST_SOCKET+1), &selectionSet, NULL, NULL, NULL) < 0)
		{
			perror("select()");
		}

		//check all sockets for activity recursively
		checkForActiveSocket(HEAD_SOCKET, &selectionSet);
	}
}

void resetSelectionSet(fd_set* selectionSet)
{
	//maintain position in the linked-list
	struct socketDescriptor_t* currentSocket = HEAD_SOCKET;

	FD_ZERO(selectionSet);

	//add the server socket to the set used by select()
	FD_SET(currentSocket->fileDescriptor, selectionSet);
	if (currentSocket->fileDescriptor > LARGEST_SOCKET)
	{
		LARGEST_SOCKET = currentSocket->fileDescriptor;
	}
	
	//while the linked-list continues
	while(currentSocket->next_socketDescriptor)
	{
		currentSocket = currentSocket->next_socketDescriptor;
		
		//add the current socket to the set used by select()
		FD_SET(currentSocket->fileDescriptor, selectionSet);
		if (currentSocket->fileDescriptor > LARGEST_SOCKET)
		{
			LARGEST_SOCKET = currentSocket->fileDescriptor;
		}
	}
}

void checkForActiveSocket(struct socketDescriptor_t* currentSocket, fd_set* selectionSet)
{
	//fprintf(stdout, "%s: %s: %s\n", "\n[DEBUG]", "Linked-List", currentSocket->handle);
	if (FD_ISSET(currentSocket->fileDescriptor, selectionSet))
	{
		if (!strcmp(currentSocket->handle, "Server Socket"))
		//the server socket is active; accept new connection
		{
			//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "accept new connection");
			// allcoate space for new linked-list node
			struct socketDescriptor_t* newClientSocket = (struct socketDescriptor_t*)malloc(sizeof(struct socketDescriptor_t));
			if (!newClientSocket)
			{
				perror("malloc error");
				exit(-1);
			}

			//set values in the socketDescriptor struct
			newClientSocket->fileDescriptor = tcpAccept(currentSocket->fileDescriptor, DEBUG_FLAG);
			//initialize handle as null
			//strcpy(newClientSocket->handle, "\0");
			
			//add to and maintain the linked-list
			newClientSocket->next_socketDescriptor = NULL;
			newClientSocket->previous_socketDescriptor = TAIL_SOCKET;
			TAIL_SOCKET->next_socketDescriptor = newClientSocket;
			TAIL_SOCKET = newClientSocket;

			//add the new socket to the set used by select()
			//FD_SET(newClientSocket->fileDescriptor, selectionSet);
			if (newClientSocket->fileDescriptor > LARGEST_SOCKET)
			{
				LARGEST_SOCKET = newClientSocket->fileDescriptor;
			}
		}
		else
		// a client socket; handle packet
		{
			//fprintf(stdout, "%s: %s from: %i\n", "\n[DEBUG]", "received packet", currentSocket->fileDescriptor);
			receivePacketServer(currentSocket, selectionSet);
		}
	}
	//recursively check next socket
	if (currentSocket->next_socketDescriptor)
	{
		checkForActiveSocket(currentSocket->next_socketDescriptor, selectionSet);
	}
}

void receivePacketServer(struct socketDescriptor_t* socket, fd_set* selectionSet)
{
	//buffer will hold data from recv()
	uint8_t receiveBuffer[MAXBUF];

	uint16_t expectedLength_NetOrder;

	//get the first two bytes of the packet so we know how long the rest is
	uint16_t receivedLength = recv(socket->fileDescriptor, &expectedLength_NetOrder, 2, MSG_PEEK);
	if (receivedLength < 0)
	{
		perror("recv call");
		exit(-1);
	}
	// unexpected connection drop
	else if (receivedLength == 0)
	{
		flag08_IN(socket);
		return;
	}
	
	//translate the expected length to host order
	uint16_t expectedLength = ntohs(expectedLength_NetOrder);

	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "bytes read is", receivedLength);
	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "packet length is", expectedLength);

	//fill buffer with the rest of the packet
	receivedLength = recv(socket->fileDescriptor, receiveBuffer, expectedLength, MSG_WAITALL);
	if (receivedLength < 0)
	{
		perror("recv call");
		exit(-1);
	}
	// unexpected connection drop
	else if (receivedLength == 0)
	{
		flag08_IN(socket);
		return;
	}
	
	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "received length is", receivedLength);

	// cast buffer as a common chat header
	// this pointer will also be used to reference the buffer in subsequent functions
	struct commonChatHeader_t* header = (struct commonChatHeader_t*)&receiveBuffer;

	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "determined flag");
	
	//determine how to handle packet based on flag
	switch (header->flag)
	{
		//handle registration
		case FLAG_01:
			flag01_IN(receiveBuffer, socket);
			break;
		//broadcast
		case FLAG_04:
			flag04_IN(receiveBuffer, socket, expectedLength);
			break;
		//message
		case FLAG_05:
			flag05_IN(receiveBuffer, socket, expectedLength);
			break;
		//handle deregistration
		case FLAG_08:
			flag08_IN(socket);
			break;
		//list handles
		case FLAG_10:
			flag10_IN(socket);
			break;
		default:
			break;
	}
}

//registration packet recieved
void flag01_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 1 in");

	//hold a local copy of the handle string (null terminated)
	char handle[MAX_HANDLE_SIZE];

	uint8_t handleLength = receiveBuffer[3];
	
	//extract the handle
	memcpy(handle, &receiveBuffer[4], handleLength);
	//null terminate the handle for easier printing
	handle[handleLength] = '\0';

	//if the handle is available
	if (isHandleAvailable(handle))
	{
		//add the handle to the socket descriptor struct
		strcpy(socket->handle, handle);
		//send a successful registration response
		headerOnlyPacket_OUT(socket->fileDescriptor, FLAG_02);
	}
	//if the handle is not available
	else
	{
		//send a failed registration response
		headerOnlyPacket_OUT(socket->fileDescriptor, FLAG_03);
	}
}

//broadcast packet recieved
void flag04_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket, uint16_t expectedLength)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 4 in");
	//forward to all registered handles
	flag04_SENDER(receiveBuffer, socket, expectedLength);
}

//utilizes the linked-list to send a broadcast packet to all registered handles
void flag04_SENDER(uint8_t* receiveBuffer, struct socketDescriptor_t* sourceSocket, uint16_t expectedLength)
{
	//keep position in linked-list
	struct socketDescriptor_t* destinationSocket = HEAD_SOCKET;

	//while the list continues
	while(destinationSocket->next_socketDescriptor)
	{
		destinationSocket = destinationSocket->next_socketDescriptor;

		// // if the handle is niether null nor the same as the source handle
		// // therefore accepted sockets which do not yet have a handle, and the source of the broadcast will be skipped
		// if ((strcmp("\0", destinationSocket->handle)) && (strcmp(sourceSocket->handle, destinationSocket->handle)))
		// {
			//forward the broadcast
		flag04_OUT(receiveBuffer, destinationSocket, expectedLength);
		// }

	}
}

//send broadcast packet
void flag04_OUT(uint8_t* receiveBuffer, struct socketDescriptor_t* destinationSocket, uint16_t expectedLength)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 4 out");
	// no need to rebuild packet
	if (send(destinationSocket->fileDescriptor, receiveBuffer, expectedLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}

//message packet recieved
void flag05_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket, uint16_t expectedLength)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 5 in");
	//local copies of the handle strings (null terminated)
	char sourceHandle[MAX_HANDLE_SIZE];
	char currentHandle[MAX_HANDLE_SIZE];

	//destination count
	uint8_t numberOfDestinationHandles;

	//needed to modify locationInHeader dynamically
	uint8_t currentHandleLength;

	uint8_t sourceHandleLength = receiveBuffer[3];
	
	//extract the sourceHandle
	memcpy(sourceHandle, &receiveBuffer[4], sourceHandleLength);
	//null terminate the handle for easier printing
	sourceHandle[sourceHandleLength] = '\0';

	//extract the numberOfDestinationHandles
	memcpy(&numberOfDestinationHandles, &receiveBuffer[4 + sourceHandleLength], 1);

	//validated destination to send to
	struct socketDescriptor_t* validDestination;

	//maintains position in the header
	char* locationInHeader = (char*)&receiveBuffer[5 + sourceHandleLength];

	//loop through and extract each destination handle
	int i;
	for (i = 0; i < numberOfDestinationHandles; i++)
	{
		//needed to modify locationInHeader dynamically
		currentHandleLength = (uint8_t)*locationInHeader;
		
		//maintains position in the header
		locationInHeader++;

		//extract the destination handle
		memcpy(currentHandle, locationInHeader, currentHandleLength);
		//null terminate the handle for easier printing
		currentHandle[currentHandleLength] = '\0';

		// find the socket descriptor that matches this handle
		validDestination = isHandleValid(currentHandle);

		//if there is one, forward the message to that destination
		if (validDestination)
		{
			flag05_OUT(receiveBuffer, validDestination, expectedLength);
		}
		//otherwise return an invalid handle packet to the source
		else
		{
			flag07_OUT(socket, currentHandle, currentHandleLength);
		}
		
		//move through header
		locationInHeader += currentHandleLength;
	}
}

//send message packet
void flag05_OUT(uint8_t* receiveBuffer, struct socketDescriptor_t* validDestination, uint16_t expectedLength)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 5 out");
	// no need to rebuild packet
	if (send(validDestination->fileDescriptor, receiveBuffer, expectedLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}

//send an invalid handle error to the source of a message
void flag07_OUT(struct socketDescriptor_t* socket, char* invalidHandle, uint8_t invalidHandleLength)
{	
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 7 out");
	//build the packet
	uint8_t sendFlag = FLAG_07;
	uint16_t sendLength = (CHAT_HEADER_LENGTH + 1 + invalidHandleLength);
	uint16_t sendLength_NetOrder = htons(sendLength);
	uint8_t sendBuf[sendLength];
	
	memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
	memcpy(&sendBuf[2], &sendFlag, 1);
	memcpy(&sendBuf[3], &invalidHandleLength, 1);
	memcpy(&sendBuf[4], invalidHandle, invalidHandleLength);

	//send to destination
	if (send(socket->fileDescriptor, sendBuf, sendLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}

//exit packet recieved
void flag08_IN(struct socketDescriptor_t* socket)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 8 in");
	//unlink the socket from the linked-list
	if (socket->previous_socketDescriptor)
	{
		socket->previous_socketDescriptor->next_socketDescriptor = socket->next_socketDescriptor;
	}
	if (socket->next_socketDescriptor)
	{
		socket->next_socketDescriptor->previous_socketDescriptor = socket->previous_socketDescriptor;
	}

	//send the deregistration acknowledgement
	headerOnlyPacket_OUT(socket->fileDescriptor, FLAG_09);

	//close socket and clear it from the selection set
	close(socket->fileDescriptor);

	//free the linked-list node from memory
	free(socket);
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "teardown complete");
}

//list registered handles
void flag10_IN(struct socketDescriptor_t* socket)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 10 in");
	//obtain and send handle count
	flag11_OUT(socket, handleCount());

	//send a response for each known handle
	flag12_SENDER(socket);

	//indicate completion of handle listing
	headerOnlyPacket_OUT(socket->fileDescriptor, FLAG_13);
}

//send handle count to socket
void flag11_OUT(struct socketDescriptor_t* socket, uint32_t count)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 11 out");
	//build packet
	uint16_t sendLength = (CHAT_HEADER_LENGTH + 4);
	uint16_t sendLength_NetOrder = htons(sendLength);
	uint8_t sendBuf[sendLength];
	uint8_t sendFlag = FLAG_11;
	uint32_t handleCount_NetOrder = htonl(count);
	
	memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
	memcpy(&sendBuf[2], &sendFlag, 1);
	memcpy(&sendBuf[3], &handleCount_NetOrder, 4);

	//send to destination
	if (send(socket->fileDescriptor, sendBuf, sendLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}

//sends a response for each known handle to the source of the flag_10 packet
void flag12_SENDER(struct socketDescriptor_t* sendSocket)
{
	//keep position in linked-list
	struct socketDescriptor_t* handleSocket = HEAD_SOCKET;

	//while the list continues
	while(handleSocket->next_socketDescriptor)
	{
		handleSocket = handleSocket->next_socketDescriptor;
		
		//if the handle is not null, send the appropriate flag 12 back to the source
		if (strcmp("\0", handleSocket->handle))
		{
			flag12_OUT(sendSocket, handleSocket);
		}
	}
}

//message to be sent for each handle
void flag12_OUT(struct socketDescriptor_t* sendSocket, struct socketDescriptor_t* handleSocket)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 12 out");
	//build packet
	uint8_t sendFlag = FLAG_12;
	uint8_t handleLength = strlen(handleSocket->handle);
	uint16_t sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength);
	uint16_t sendLength_NetOrder = htons(sendLength);
	uint8_t sendBuf[sendLength];
	
	memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
	memcpy(&sendBuf[2], &sendFlag, 1);
	memcpy(&sendBuf[3], &handleLength, 1);
	memcpy(&sendBuf[4], &handleSocket->handle, handleLength);

	//send to destination
	if (send(sendSocket->fileDescriptor, sendBuf, sendLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}

//goes through the linked list from the head, counting the number of nodes
uint32_t handleCount(void)
{
	// the head node represents the server socket, I don't count it
	uint32_t count = 0;
	struct socketDescriptor_t* currentSocket = HEAD_SOCKET;

	while(currentSocket->next_socketDescriptor)
	{
		currentSocket = currentSocket->next_socketDescriptor;

		//if the handle is not NULL, increment the counter
		if (strcmp("\0", currentSocket->handle))
		{
			count++;
		}
	}
	//fprintf(stdout, "%s: %s: %i\n", "\n[DEBUG]", "count", count);
	return count;
}

//goes through the linked list from the head, checking for the availability of a new handle, "testhandle"
int isHandleAvailable(char* testHandle)
{
	//handle assumed to be available
	int available = 1;
	//maintain position in the linked-list
	struct socketDescriptor_t* currentSocket = HEAD_SOCKET;

	//check against the server socket (HEAD) handle, if someone used this it would confuse my program
	if (!strcmp(testHandle, currentSocket->handle))
	{
		available = 0;
	}

	//while the linked-list continues
	while(currentSocket->next_socketDescriptor)
	{
		currentSocket = currentSocket->next_socketDescriptor;
		//if the test handle matches a known handle, it is marked as unavailable
		if (!strcmp(testHandle, currentSocket->handle))
		{
			available = 0;
		}
	}
	return available;
}

struct socketDescriptor_t* isHandleValid(char* testHandle)
{
	//handle is assumed to be invalid
	struct socketDescriptor_t* valid = NULL;
	//maintain position in the linked-list
	struct socketDescriptor_t* currentSocket = HEAD_SOCKET;

	//while the linked-list continues
	while(currentSocket->next_socketDescriptor)
	{
		currentSocket = currentSocket->next_socketDescriptor;
		//if the test handle matches that of a socket descriptor, it a valid destination for the message
		if (!strcmp(testHandle, currentSocket->handle))
		{
			//this should work as there should not be duplicate handles
			valid = currentSocket;
		}
	}
	return valid;
}

// Checks args and returns port number
int checkArgs(int argc, char *argv[])
{
	int portNumber = 0;

	//usage
	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	//set port number
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	return portNumber;
}

