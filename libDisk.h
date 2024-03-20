#ifndef LIBDISK_H
#define LIBDISK_H

#include <stdio.h>

#define BLOCKSIZE 256

typedef struct diskInfo {
    int disk;
    FILE *file_pointer;
    int size;
    int status;
} diskInfo;

typedef struct diskNode {
    diskInfo *diskinfo;
    struct diskNode *next;
} diskNode;

int openDisk(char *filename, int nBytes);
int readBlock(int disk, int bNum, void *block);
int writeBlock(int disk, int bNum, void *block);
int closeDisk(int disk);
void insertDisk(diskInfo *diskinfo);
diskInfo *getDiskByFD(int disk);
diskInfo *createDiskInfo(int disk, FILE *file_pointer, int size, int status);

#endif /* LIBDISK_H */