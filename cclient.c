#include "cclient.h"

int SERVER_SOCKET_FD;

//used to force blocking while waiting for handle confirmation from server
int IS_BLOCKED;

//hold input handle for check and connection setup
char USER_HANDLE[MAX_HANDLE_SIZE];

int main(int argc, char* argv[])
{
	checkArgs(argc, argv);
	setupConnection(argc, argv);
	waitForAction();
	
	return 0;
}

void setupConnection(int argc, char * argv[])
{
	/* set up the TCP Client socket  */
	SERVER_SOCKET_FD = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	//get the handle argument
	strcpy(USER_HANDLE, argv[1]);

	//check for handle validity
	if (isdigit(USER_HANDLE[0]))
	{
		perror("Invalid handle, handle starts with a number");
		exit (-1);
	}

	//confirm handle with server
	flag01_OUT(USER_HANDLE);
}

void promptUser()
{
	fprintf(stdout, "$: ");
	fflush(stdout);
}

void waitForAction()
{
	int fdMax = 0;

	//setup fd set for the select() call
	fd_set selectionSet;

	while (1)
	{	
		//setup fd set for the select() call
		FD_ZERO(&selectionSet);

		//add stdin FD (0) to the set used by select()
		FD_SET(0, &selectionSet);

		//add the server socket to the set used by select()
		FD_SET(SERVER_SOCKET_FD, &selectionSet);
		fdMax = SERVER_SOCKET_FD;

		if (!IS_BLOCKED)
		{
			promptUser();
		}

		//waits for one of the file descriptors in the selection set to become ready for reading
		if(select((fdMax + 1), &selectionSet, NULL, NULL, NULL) < 0)
		{
			perror("select()");
		}

		if (FD_ISSET(0, &selectionSet))
		{
			if (!IS_BLOCKED)
			{
				//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "received user input");
				fflush(stdout);
				parseUserInput();
			}
		}

		if (FD_ISSET(SERVER_SOCKET_FD, &selectionSet))
		{
			//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "received packet from server");
			receivePacketClient(&selectionSet);
		}
	}
}

void receivePacketClient(fd_set* selectionSet)
{
	//buffer will hold data from recv()
	uint8_t receiveBuffer[MAXBUF] = "";

	uint16_t expectedLength_NetOrder;

	//get the first two bytes of the packet so we know how long the rest is
	uint16_t receivedLength = recv(SERVER_SOCKET_FD, &expectedLength_NetOrder, 2, MSG_PEEK);
	if (receivedLength < 0)
	{
		perror("recv call");
		exit(-1);
	}
	// unexpected connection drop
	else if (receivedLength == 0)
	{
		//send the deregistration acknowledgement
		headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_09);

		//close socket and clear it from the selection set
		close(SERVER_SOCKET_FD);
		FD_CLR(SERVER_SOCKET_FD, selectionSet);
		perror("connection to server terminated unexpectedly");
		exit(0);
	}
	
	//translate the expected length to host order
	uint16_t expectedLength = ntohs(expectedLength_NetOrder);

	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "bytes read is", receivedLength);
	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "packet length is", expectedLength);

	//fill buffer with the rest of the packet
	receivedLength = recv(SERVER_SOCKET_FD, receiveBuffer, expectedLength, MSG_WAITALL);
	if (receivedLength < 0)
	{
		perror("recv call");
		exit(-1);
	}
	// unexpected connection drop
	else if (receivedLength == 0)
	{
		//send the deregistration acknowledgement
		headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_09);

		//close socket and clear it from the selection set
		close(SERVER_SOCKET_FD);
		FD_CLR(SERVER_SOCKET_FD, selectionSet);
		perror("connection to server terminated unexpectedly");
		exit(0);
	}
	
	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "received length is", receivedLength);

	// cast buffer as a common chat header
	struct commonChatHeader_t* header = (struct commonChatHeader_t*)&receiveBuffer;

	if (IS_BLOCKED)
	{
		switch (header->flag)
		{
			//handle registration success
			case FLAG_02:
				flag02_IN();
				break;
			//handle registration failure
			case FLAG_03:
				flag03_IN();
				break;
			//handles from list command one by one
			case FLAG_12:
				flag12_IN(receiveBuffer);
				break;
			//list command completion
			case FLAG_13:
				flag13_IN();
				break;
			default:
				break;
		}
	}
	else
	{
		//determine how to handle packet based on flag
		switch (header->flag)
		{
			//handle registration success
			case FLAG_02:
				flag02_IN();
				break;
			//handle registration failure
			case FLAG_03:
				flag03_IN();
				break;
			//broadcast
			case FLAG_04:
				flag04_IN(receiveBuffer);
				break;
			//message
			case FLAG_05:
				flag05_IN(receiveBuffer);
				break;
			//destination handle error
			case FLAG_07:
				flag07_IN(receiveBuffer);
				break;
			//exit confirmation
			case FLAG_09:
				flag09_IN();
				break;
			//number of handles from list command
			case FLAG_11:
				flag11_IN(receiveBuffer);
				break;
			//handles from list command one by one
			case FLAG_12:
				flag12_IN(receiveBuffer);
				break;
			//list command completion
			case FLAG_13:
				flag13_IN();
				break;
			default:
				break;
		}
	}
}

void flag02_IN()
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 2 in");
	IS_BLOCKED = 0;
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "no longer blocking");
}

void flag03_IN()
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 3 in");
	fprintf(stderr, "Handle already in use: %s", USER_HANDLE);
	exit(-1);
}

void flag04_IN(uint8_t* receiveBuffer)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 4 in");
	//local copies of the handle strings (null terminated)
	char sourceHandle[MAX_HANDLE_SIZE];
	char myMessage[201];

	uint8_t sourceHandleLength = receiveBuffer[3];
	
	//extract the sourceHandle
	memcpy(sourceHandle, &receiveBuffer[4], sourceHandleLength);
	//null terminate the handle for easier printing
	sourceHandle[sourceHandleLength] = '\0';

	//message should point to the start of the message
	strcpy(myMessage, (char*)&receiveBuffer[4 + sourceHandleLength]);
	fprintf(stdout, "%s: %s\n", sourceHandle, myMessage);
}

void flag05_IN(uint8_t* receiveBuffer)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 5 in");
	//local copies of the handle strings (null terminated)
	char sourceHandle[MAX_HANDLE_SIZE];
	char myMessage[201];

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

	//maintains position in the header
	char* locationInHeader = (char*)&receiveBuffer[5 + sourceHandleLength];

	//loop through and extract each destination handle
	int i;
	for (i = 0; i < numberOfDestinationHandles; i++)
	{
		//needed to modify locationInHeader dynamically
		currentHandleLength = (uint8_t)*locationInHeader;
		
		//move one byte, to the location of the next handle
		locationInHeader += 1;
		
		//move the length of the handle, to the next handle's length
		locationInHeader += currentHandleLength;
	}

	//after the loop, locationInHeader should point to the start of the message
	strcpy(myMessage, (char*)locationInHeader);
	fprintf(stdout, "%s: %s\n", sourceHandle, myMessage);
}

void flag07_IN(uint8_t* receiveBuffer)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 7 in");
	//hold a local copy of the handle string (null terminated)
	char handle[MAX_HANDLE_SIZE];


	uint8_t handleLength = receiveBuffer[3];
	
	//extract the handle
	memcpy(handle, &receiveBuffer[4], handleLength);
	//null terminate the handle for easier printing
	handle[handleLength] = '\0';

	fprintf(stderr, "Client with handle %s does not exist\n", handle);
}

void flag09_IN()
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 9 in");
	close(SERVER_SOCKET_FD);
	exit(0);
}

void flag11_IN(uint8_t* receiveBuffer)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 11 in");
	uint32_t numOfHandles_NetOrder = *(uint32_t*)&receiveBuffer[3];
	uint32_t numOfHandles = ntohl(numOfHandles_NetOrder);

	fprintf(stdout, "Number of clients: %u\n", numOfHandles);

	IS_BLOCKED = 1;
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "currently blocking");
}

void flag12_IN(uint8_t* receiveBuffer)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 12 in");
	//hold a local copy of the handle string (null terminated)
	char handle[MAX_HANDLE_SIZE];

	// "header->additionalFields" points to the byte after the flag of the common header
	// in this case, that is the length of the handle
	uint8_t handleLength = receiveBuffer[3];
	
	//extract the handle
	memcpy(handle, &receiveBuffer[4], handleLength);
	//null terminate the handle for easier printing
	handle[handleLength] = '\0';

	fprintf(stderr, "\t%s\n", handle);
}

void flag13_IN()
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 13 in");
	IS_BLOCKED = 0;
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "no longer blocking");
}

void parseUserInput()
{
	//buffer to hold the user input and a position tracker
	char inputBuffer[MAXBUF] = "";
	int bufferPosition = 0;

	//fill the buffer from stdin
	if (!fgets(inputBuffer, MAXBUF, stdin))
	{
		perror("fgets error");
		exit(-1);
	}
	//fflush(stdin);

	uint8_t length = 0;
	while (inputBuffer[length] != '\n')
	{
		length++;
	}
	inputBuffer[length] = '\0';
	inputBuffer[MAXBUF - 1] = '\0';

	//find first occurance of '%', this will be the begining of the user command.
	for (bufferPosition = 0; bufferPosition < MAXBUF; bufferPosition++)
	{
		if (inputBuffer[bufferPosition] == '%')
		{
			break;
		}
	}

	// next character will identify the command type
	bufferPosition++;
	switch(inputBuffer[bufferPosition])
	{
		case 'M':
			//message
			flag05_OUT(inputBuffer, bufferPosition);
			break;
		case 'm':
			//message
			flag05_OUT(inputBuffer, bufferPosition);
			break;
		case 'B':
			//broadcast
			flag04_SENDER(inputBuffer, bufferPosition);
			break;
		case 'b':
			//broadcast
			flag04_SENDER(inputBuffer, bufferPosition);
			break;
		case 'L':
			//list
			headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_10);
			break;
		case 'l':
			//list
			headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_10);
			break;
		case 'E':
			//exit
			headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_08);
			break;
		case 'e':
			//exit
			headerOnlyPacket_OUT(SERVER_SOCKET_FD, FLAG_08);
			break;
		default:
			break;
	}
}

void flag01_OUT(char* handle)
{
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 1 out");
	//build packet
	uint8_t handleLength = strlen(handle);
	uint16_t sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength);
	uint16_t sendLength_NetOrder = htons(sendLength);
	uint8_t sendBuf[sendLength];
	uint8_t sendFlag = FLAG_01;

    //copy to sending buffer
	memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
	memcpy(&sendBuf[2], &sendFlag, 1);
	memcpy(&sendBuf[3], &handleLength, 1);
	memcpy(&sendBuf[4], handle, handleLength);

	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "packet length is", (ntohs(sendLength_NetOrder)));
	//fprintf(stdout, "%s: %s: %u\n", "\n[DEBUG]", "packet length is", sendLength);

	//send to destination
	if (send(SERVER_SOCKET_FD, sendBuf, sendLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}

	IS_BLOCKED = 1;
	//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "currently blocking");
}

void flag04_SENDER(char* inputBuffer, int bufferPosition)
{
	//skip the leading space
	bufferPosition += 2;

	//need to determine message length before assigning these
	uint16_t sendLength;
	uint16_t sendLength_NetOrder;

	//build packet
	uint8_t sendBuf[MAXBUF];
	uint8_t handleLength = strlen(USER_HANDLE);
	uint8_t sendFlag = FLAG_04;

	char myNull = '\0';

	//important for splitting up long messages
	uint16_t messageLength = strlen(&inputBuffer[bufferPosition]);

	//while the message is longer than the max, repeatedly send the max, sliding the bufferPosition forward
	while (messageLength > MAX_MESSAGE_LENGTH)
	{
		//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 4 out");
		//limit send length by MAX_MESSAGE_LENGTH
		sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength + MAX_MESSAGE_LENGTH);
		sendLength_NetOrder = htons(sendLength);
		
		//copy to sending buffer
		memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
		memcpy(&sendBuf[2], &sendFlag, 1);
		memcpy(&sendBuf[3], &handleLength, 1);
		memcpy(&sendBuf[4], USER_HANDLE, handleLength);
		memcpy(&sendBuf[4 + handleLength], &inputBuffer[bufferPosition], MAX_MESSAGE_LENGTH);
		memcpy(&sendBuf[4 + handleLength + MAX_MESSAGE_LENGTH], &myNull, 1);

		//send to destination
		if (send(SERVER_SOCKET_FD, sendBuf, sendLength, 0) < 0)
		{
			perror("send call");
			exit(-1);
		}

		//slide position in buffer, so that the loop can send the next message chunk
		bufferPosition += MAX_MESSAGE_LENGTH;
		messageLength = strlen(&inputBuffer[bufferPosition]);
	}
	//original message was short enough, or this is the final in a batch of messages
	if (messageLength <= MAX_MESSAGE_LENGTH)
	{
		//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 4 out");
		//it does not need to be split further
		sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength + messageLength);
		sendLength_NetOrder = htons(sendLength);

		//copy to sending buffer
		memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
		memcpy(&sendBuf[2], &sendFlag, 1);
		memcpy(&sendBuf[3], &handleLength, 1);
		memcpy(&sendBuf[4], USER_HANDLE, handleLength);
		memcpy(&sendBuf[4 + handleLength], &inputBuffer[bufferPosition], messageLength);
		memcpy(&sendBuf[4 + handleLength + messageLength], &myNull, 1);

		//send to destination
		if (send(SERVER_SOCKET_FD, sendBuf, sendLength, 0) < 0)
		{
			perror("send call");
			exit(-1);
		}
	}
}

void flag05_OUT(char* inputBuffer, int bufferPosition)
{
	//skip the leading space
	bufferPosition += 2;

	//need to determine message length before assigning these
	uint16_t sendLength;
	uint16_t sendLength_NetOrder;

	//build packet
	uint8_t sendBuf[MAXBUF];
	uint8_t handleLength = strlen(USER_HANDLE);
	uint8_t sendFlag = FLAG_05;

	uint8_t numberOfHandles;
	uint8_t handles[MAXBUF];

	char myNull = '\0';

	if (isdigit(inputBuffer[bufferPosition]))
	{
		numberOfHandles = atoi(&inputBuffer[bufferPosition]);
		bufferPosition += 2;
	}
	else
	{
		numberOfHandles = 1;
	}

	int handlesPosition = 0;
	int i;
	for (i = 0; i < numberOfHandles; i++)
	{
		uint8_t destHandleLength = 0;
		while (inputBuffer[bufferPosition + destHandleLength] != ' ')
		{
			destHandleLength++;
		}
		memcpy(&handles[handlesPosition], &destHandleLength, 1);
		memcpy(&handles[handlesPosition + 1], &inputBuffer[bufferPosition], destHandleLength);
		bufferPosition += (destHandleLength + 1);
		handlesPosition += (destHandleLength + 1);
	}
	
	//important for splitting up long messages
	uint16_t messageLength = strlen(&inputBuffer[bufferPosition]);

	//while the message is longer than the max, repeatedly send the max, sliding the bufferPosition forward
	while (messageLength > MAX_MESSAGE_LENGTH)
	{
		//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 5 out");
		//limit send length by MAX_MESSAGE_LENGTH
		sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength + 1 + handlesPosition + MAX_MESSAGE_LENGTH);
		sendLength_NetOrder = htons(sendLength);
		
		//copy to sending buffer
		memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
		memcpy(&sendBuf[2], &sendFlag, 1);
		memcpy(&sendBuf[3], &handleLength, 1);
		memcpy(&sendBuf[4], USER_HANDLE, handleLength);
		memcpy(&sendBuf[4 + handleLength], &numberOfHandles, 1);
		memcpy(&sendBuf[4 + handleLength + 1], &handles, handlesPosition);
		memcpy(&sendBuf[4 + handleLength + 1 + handlesPosition], &inputBuffer[bufferPosition], MAX_MESSAGE_LENGTH);
		memcpy(&sendBuf[4 + handleLength + 1 + handlesPosition + MAX_MESSAGE_LENGTH], &myNull, 1);

		//send to destination
		if (send(SERVER_SOCKET_FD, sendBuf, sendLength, 0) < 0)
		{
			perror("send call");
			exit(-1);
		}

		//slide position in buffer, so that the loop can send the next message chunk
		bufferPosition += MAX_MESSAGE_LENGTH;
		messageLength = strlen(&inputBuffer[bufferPosition]);
	}
	//original message was short enough, or this is the final in a batch of messages
	if (messageLength <= MAX_MESSAGE_LENGTH)
	{
		//fprintf(stdout, "%s: %s\n", "\n[DEBUG]", "flag 5 out");
		//it does not need to be split further
		sendLength = (CHAT_HEADER_LENGTH + 1 + handleLength + 1 + handlesPosition + messageLength);
		sendLength_NetOrder = htons(sendLength);

		//copy to sending buffer
		memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
		memcpy(&sendBuf[2], &sendFlag, 1);
		memcpy(&sendBuf[3], &handleLength, 1);
		memcpy(&sendBuf[4], USER_HANDLE, handleLength);
		memcpy(&sendBuf[4 + handleLength], &numberOfHandles, 1);
		memcpy(&sendBuf[4 + handleLength + 1], &handles, handlesPosition);
		memcpy(&sendBuf[4 + handleLength + 1 + handlesPosition], &inputBuffer[bufferPosition], messageLength);
		memcpy(&sendBuf[4 + handleLength + 1 + handlesPosition + messageLength], &myNull, 1);

		//send to destination
		if (send(SERVER_SOCKET_FD, sendBuf, sendLength, 0) < 0)
		{
			perror("send call");
			exit(-1);
		}
	}
}

void checkArgs(int argc, char* argv[])
{
	if (argc != 4)
	{
		printf("usage: %s <handle> <host-name> <port-number> \n", argv[0]);
		exit(1);
	}
}