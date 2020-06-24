#include "shared.h"

struct socketDescriptor_t
{
    int fileDescriptor;
    char handle[MAX_HANDLE_SIZE];
    struct socketDescriptor_t* next_socketDescriptor;
    struct socketDescriptor_t* previous_socketDescriptor;
} __attribute__((packed));

void waitForConnection(int serverSocket);
void checkForActiveSocket(struct socketDescriptor_t* currentSocket, fd_set* selectionSet);
void receivePacketServer(struct socketDescriptor_t* socket, fd_set* selectionSet);
void resetSelectionSet(fd_set* selectionSet);
void flag01_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket);
void flag04_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket, uint16_t expectedLength);
void flag04_SENDER(uint8_t* receiveBuffer, struct socketDescriptor_t* sourceSocket, uint16_t expectedLength);
void flag04_OUT(uint8_t* receiveBuffer, struct socketDescriptor_t* destinationSocket, uint16_t expectedLength);
void flag05_IN(uint8_t* receiveBuffer, struct socketDescriptor_t* socket, uint16_t expectedLength);
void flag05_OUT(uint8_t* receiveBuffer, struct socketDescriptor_t* validDestination, uint16_t expectedLength);
void flag07_OUT(struct socketDescriptor_t* socket, char* invalidHandle, uint8_t invalidHandleLength);
void flag08_IN(struct socketDescriptor_t* socket);
void flag10_IN(struct socketDescriptor_t* socket);
void flag11_OUT(struct socketDescriptor_t* socket, uint32_t count);
void flag12_SENDER(struct socketDescriptor_t* sendSocket);
void flag12_OUT(struct socketDescriptor_t* sendSocket, struct socketDescriptor_t* handleSocket);
uint32_t handleCount(void);
int isHandleAvailable(char* testHandle);
struct socketDescriptor_t* isHandleValid(char* testHandle);
int checkArgs(int argc, char *argv[]);

