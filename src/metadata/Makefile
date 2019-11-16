CFLAGS=-std=gnu99 -Wall -Werror -pedantic
CC=gcc

all: meta

meta: meta.c meta.h
	$(CC) $(CFLAGS)  -o meta meta.c -lsqlite3

clean:
	rm -f meta *.o