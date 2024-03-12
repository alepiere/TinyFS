#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define BLOCKSIZE 256

int openDisk(char *filename, int nBytes){
    int fd;
    if(nBytes < BLOCKSIZE){
        fprintf(stderr, "Error: Disk size must be at least BLOCKSIZE bytes.\n");
        return -1;
    }

    off_t diskSize = nBytes - (nBytes % BLOCKSIZE);
    if (diskSize == 0) {
        diskSize = BLOCKSIZE; // Ensure disk size is at least BLOCKSIZE bytes
    }

    // Open file
    if (nBytes == 0) {
        // Open existing file without truncating
        fd = open(filename, O_RDWR);
    } else {
        // Open file with truncation to specified size
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    }

    // Check if file open was successful
    if (fd == -1) {
        perror("Error opening file");
        return -1;
    }

    // Truncate file size to calculated disk size
    if (ftruncate(fd, diskSize) == -1) {
        perror("Error truncating file");
        close(fd);
        return -1;
    }

    return fd;
}

int closeDisk(int disk){
   if (fcntl(disk, F_GETFD) != -1) {
      close(disk);
   }
   return 0;
}

int readBlock(int disk, int bNum, void *block){
    int flags = fcntl(disk, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    int offset = bNum * BLOCKSIZE;
    if (lseek(disk, offset, SEEK_SET) == -1) {
        return -1;
    }
    int bytesRead = read(disk, block, BLOCKSIZE);
    if (bytesRead == -1) {
        return -1;
    } else if (bytesRead < BLOCKSIZE) {
        fprintf(stderr, "Error: bytes read less than blocksize.\n");
        return -1;
    }
    return 0;
}
    
int writeBlock(int disk, int bNum, void *block){
    int flags = fcntl(disk, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    int offset = bNum * BLOCKSIZE;
    if (lseek(disk, offset, SEEK_SET) == -1) {
        return -1;
    }
    int bytesWritten = write(disk, block, BLOCKSIZE);
    if (bytesWritten == -1) {
        return -1;
    } else if (bytesWritten < BLOCKSIZE) {
        fprintf(stderr, "Error: bytes written less than blocksize.\n");
        return -1;
    }
    return 0;
}