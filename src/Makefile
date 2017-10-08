CC            =gcc
CFLAGS        +=  -g -Wall

all: server1-totobar server2-totobar server3-totobar

server1-totobar : server1-totobar.c
	$(CC) $(CFLAGS)  server1-totobar.c -o ../bin/server1-totobar -lpthread

server2-totobar : server2-totobar.c
	$(CC) $(CFLAGS)  server2-totobar.c -o ../bin/server2-totobar -lpthread

server3-totobar : server3-totobar.c
	$(CC) $(CFLAGS)  server3-totobar.c -o ../bin/server3-totobar -lpthread
