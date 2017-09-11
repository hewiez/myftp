all:server

CC=gcc

server:server.o function.o
	$(CC) -o server server.o function.o

server.o:server.c server.h

function.o:function.c server.h

.PHONY:clean

clean:
	rm -rf *.o 
