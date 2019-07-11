CFLAGS=-Wall -Wextra -g
CC=gcc

default: clean server client

server:
	$(CC) $(CFLAGS) -pthread -o server server.c

client:
	$(CC) $(CFLAGS) -pthread -o client client.c

clean:
	rm -rf server client *.zip *.file *.dSYM
