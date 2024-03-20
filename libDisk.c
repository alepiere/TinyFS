#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "libDisk.h"

#define BLOCKSIZE 256

diskNode *head;
int num_disks = 0;
FILE *file_pointer = NULL;

int openDisk(char *filename, int nBytes)
{
    char *perms = "r+";
    int disk;
    FILE *file_pointer;
    diskInfo diskInfo;

    if (nBytes < BLOCKSIZE && nBytes != 0)
    {
        fprintf(stderr, "Error: Disk size must be at least BLOCKSIZE bytes.\n");
        return -1;
    }

    off_t diskSize = nBytes - (nBytes % BLOCKSIZE);
    if (diskSize == 0)
    {
        diskSize = BLOCKSIZE; // Ensure disk size is at least BLOCKSIZE bytes
    }

    // Open file
    if (nBytes == 0)
    {
        // Open existing file without truncating
        perms = 
        fd = open(filename, O_RDWR);
    }
    else
    {
        // Open file with truncation to specified size
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }

    // Check if file open was successful
    if (fd == -1)
    {
        perror("Error opening file");
        return -1;
    }

    // Truncate file size to calculated disk size
    if (ftruncate(fd, diskSize) == -1)
    {
        perror("Error truncating file");
        close(fd);
        return -1;
    }
    file_pointer = fdopen(fd, "r+");
    if (file_pointer == NULL)
    {
        perror("Error opening file pointer");
        close(fd);
        return -1;
    }
    printf("File pointer is %p\n", file_pointer);
    return fd;
}

void insertDisk(diskInfo *diskinfo){
    diskNode *newNode = (diskNode *)malloc(sizeof(diskNode));
    newNode->diskinfo = diskinfo;
    newNode->next = head;
    head = newNode;
    num_disks++;
}

int closeDisk(int disk)
{
    if (fcntl(disk, F_GETFD) != -1)
    {
        close(disk);
    }
    file_pointer = NULL;
    return 0;
}

int readBlock(int disk, int bNum, void *block)
{
    printf("File pointer is %p\n", file_pointer);
    char* blockdata = block;
    // for (int i = 0; i < BLOCKSIZE; i++)
    // {
    //     printf("%02X ", blockdata[i]);
    // }
    int flags = fcntl(disk, F_GETFL);
    if (flags == -1)
    {
        return -1;
    }
    int offset = bNum * BLOCKSIZE;
    if (fseek(file_pointer, offset, SEEK_SET) == -1)
    {
        return -1;
    }
    // if (lseek(disk, offset, SEEK_SET) == -1)
    // {
    //     return -1;
    // }
    size_t bytesRead = fread(block, 1, BLOCKSIZE, file_pointer);;
    if (bytesRead == -1)
    {
        return -1;
    }
    else if (bytesRead < BLOCKSIZE)
    {
        fprintf(stderr, "Error: bytes read less than blocksize.\n %d", bytesRead);
        return -1;
    }
    return 0;
}

int writeBlock(int disk, int bNum, void *block)
{   
    printf("File pointer is %p\n", file_pointer);
    char* blockdata = block;
    // for (int i = 0; i < BLOCKSIZE; i++)
    // {
    //     printf("%02X ", blockdata[i]);
    // }
    int flags = fcntl(disk, F_GETFL);
    if (flags == -1)
    {
        return -1;
    }
    int offset = bNum * BLOCKSIZE;
    if (fseek(file_pointer, offset, SEEK_SET) == -1)
    {
        return -1;
    }
    int bytesWritten = fwrite(block, 1, BLOCKSIZE, file_pointer);
    printf("bytesWritten: %d\n", bytesWritten);
    if (bytesWritten == -1)
    {
        return -1;
    }
    else if (bytesWritten < BLOCKSIZE)
    {
        fprintf(stderr, "Error: bytes written less than blocksize.%d\n", bytesWritten);
        return -1;
    }
    return 0;
}