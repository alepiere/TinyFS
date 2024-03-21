TARGET   = TinyFSDemo
CC       = gcc
CCFLAGS  = 
LDFLAGS  = -lm
SOURCES = libDisk.c libTinyFS.c tinyFSDemo.c
INCLUDES = $(wildcard *.h)
OBJECTS  = $(SOURCES:.c=.o)
DISKS = $(wildcard *.dsk)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(INCLUDES)
	$(CC) $(CCFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS) $(DISKS)
