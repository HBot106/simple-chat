#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include <sys/select.h> 

#include "networks.h"

#define FLAG_01 0x01
#define FLAG_02 0x02
#define FLAG_03 0x03
#define FLAG_04 0x04
#define FLAG_05 0x05
#define FLAG_06 0x06
#define FLAG_07 0x07
#define FLAG_08 0x08
#define FLAG_09 0x09
#define FLAG_10 0x0A
#define FLAG_11 0x0B
#define FLAG_12 0x0C
#define FLAG_13 0x0D

#define MAXBUF 1500
#define DEBUG_FLAG 1

#define MAX_HANDLE_SIZE 100
#define MAX_NUM_HANDLES 10
#define MAX_MESSAGE_LENGTH 200

#define CHAT_HEADER_LENGTH 3

struct commonChatHeader_t
{
    uint16_t length;
    uint8_t flag;
    char* additionalFields; // "additionalFields" points to the byte after the flag of the common header
} __attribute__((packed));

void headerOnlyPacket_OUT(int socketFD, int flag);