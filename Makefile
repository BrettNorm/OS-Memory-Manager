CC = gcc
CFLAGS = -g -I -std=gnu99 -D_POSIX_C_SOURCE=200809 -D_SVID_SOURCE -lpthread -lrt -std=c99 
all: oss process

oss: config.h oss.c
	$(CC) -o $@ $^ $(CFLAGS)

process: config.h process.c
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm oss process systemlog.txt