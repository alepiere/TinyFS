CC = gcc
CFLAGS = -Wall -g
PROG = TinyFSDemo
OBJS = libTinyFS.o libDisk.o tinyFSDemo.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

tinyFSDemo.o: tinyFSDemo.c libTinyFS.h libTinyFS.h TinyFS_errno.h
libTinyFS.o: libTinyFS.c libTinyFS.h libTinyFS.h libDisk.h TinyFS_errno.h
libDisk.o: libDisk.c libDisk.h libTinyFS.h TinyFS_errno.h

clean:
	rm -f $(PROG) $(OBJS)