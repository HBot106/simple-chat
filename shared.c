#include "shared.h"

//method to send a packet only consisting of the common chat header
void headerOnlyPacket_OUT(int socketFD, int flag)
{
	//build packet
	uint16_t sendLength = CHAT_HEADER_LENGTH;
	uint16_t sendLength_NetOrder = htons(sendLength);
	uint8_t sendBuf[sendLength];
	uint8_t sendFlag;
	
	//set flag field based on argument
	switch (flag)
	{
		case 2:
			sendFlag = FLAG_02;
			break;
		case 3:
			sendFlag = FLAG_03;
			break;
		case 8:
			sendFlag = FLAG_08;
			break;
		case 9:
			sendFlag = FLAG_09;
			break;
		case 10:
			sendFlag = FLAG_10;
			break;
		case 13:
			sendFlag = FLAG_13;
			break;
		default:
			break;
	}

    //copy to sending buffer
	memcpy(&sendBuf[0], &sendLength_NetOrder, 2);
	memcpy(&sendBuf[2], &sendFlag, 1);

    //fprintf(stdout, "%s: %s %i %s\n", "\n[DEBUG]", "flag", flag, "out");

	//send to destination
	if (send(socketFD, sendBuf, sendLength, 0) < 0)
	{
		perror("send call");
		exit(-1);
	}
}