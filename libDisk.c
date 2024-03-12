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
    if (nBytes == 0) {
        // Open existing file without truncating
        fd = open(filename, O_RDWR);
    } else {
        // Open file with truncation to specified size
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
        lseek(fd, nBytes, SEEK_SET);
        write(fd, "\n", 1);
    }

    // Check if file open was successful
    if (fd == -1) {
        perror("Error opening file");
        return -1;
    }
    
    return fd;
}

int closeDisk(int disk); /* self explanatory */

int readBlock(int disk, int bNum, void *block);

int writeBlock(int disk, int bNum, void *block);