CC = gcc
CFLAGS = -Wall -Werror -Wextra
LDFLAGS = -lpthread

all:
	$(CC) hw2Server.c -o server ${CFLAGS} ${LDFLAGS}
	$(CC) hw2client.c -o client ${CFLAGS} 
	mkdir server_folder
	mv server ./server_folder 
clean:
	rm -fr server_folder server client test.*	
