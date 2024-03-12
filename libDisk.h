#ifndef LIBDISK_H
#define LIBDISK_H

#include <stdio.h>

#define BLOCKSIZE 256

int openDisk(char *filename, int nBytes);
int readBlock(int disk, int bNum, void *block);
int writeBlock(int disk, int bNum, void *block);
int closeDisk(int disk);

#endif /* LIBDISK_H */