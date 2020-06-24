# Makefile for CPE464 tcp assignment
# written by Hugh Smith - April 2017

CC= gcc
CFLAGS= -g -Wall
LIBS = 

#append a 32 to executable name if on a 32 bit machine
FILE = 
ARCH = $(shell arch)
ifeq ("$(ARCH)", "i686")
	FILE = 32
endif

all:  $(FILE) cclient$(FILE) server$(FILE)

32: 
	# to make life easier - automatically deletes .o's that were most likely created on a 64 bit machine
	# assumes you are doing most of your development on 64 bit machines
	rm -f *.o
	
cclient$(FILE): cclient.c shared.c networks.o gethostbyname6.o
	$(CC) $(CFLAGS) -o cclient$(FILE) cclient.c shared.c networks.o gethostbyname6.o $(LIBS)

server$(FILE): server.c shared.c networks.o gethostbyname6.o
	$(CC) $(CFLAGS) -o server$(FILE) server.c shared.c networks.o gethostbyname6.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o



