#include "libTinyFS.h"
#include <stdio.h>

int main(void) {
    int disk = tfs_mkfs("tinyFSDisk", DEFAULT_DISK_SIZE);
    tfs_mount("tinyFSDisk");
    printf("Disk number: %d\n", disk);

    printf("Opening testfile\n");
    int fd = tfs_openFile("testfile");
    printf("File descriptor: %d\n", fd);
    tfs_readdir();
    return 1;
}