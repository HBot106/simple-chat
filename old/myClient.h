#include "shared.h"

void setupConnection(int argc, char * argv[]);
void promptUser();
void waitForAction();
void receivePacketClient();
void parseUserInput();
void checkArgs(int argc, char* argv[]);

void flag02_IN();
void flag03_IN();
void flag04_IN(uint8_t* receiveBuffer);
void flag05_IN(struct commonChatHeader_t* header);

void flag07_IN(uint8_t* receiveBuffer);
void flag09_IN();
void flag11_IN(uint8_t* receiveBuffer);
void flag12_IN(uint8_t* receiveBuffer);
void flag13_IN();

void flag01_OUT(char* handle);
void flag04_SENDER(char* inputBuffer, int bufferPosition);