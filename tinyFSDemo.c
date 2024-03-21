#include "libTinyFS.h"
#include <stdio.h>

int main(void) {
    int disk = tfs_mkfs("tinyFSDisk", DEFAULT_DISK_SIZE);
    tfs_mount("tinyFSDisk");
    printf("Disk number: %d\n", disk);

    printf("Opening testfile\n");
    int fd = tfs_openFile("testfile");
    printf("File descriptor: %d\n", fd);
    printf("Opening testfil3\n");
    int fd2 = tfs_openFile("testfil3");
    printf("Read the file creation time for testfile\n");
    tfs_readFileInfo(fd);
    tfs_readdir();

    char testData[] = "Test file d";
    char *newptr = testData;
    tfs_writeFile(fd, newptr, 11);

    tfs_deleteFile(fd);
    printf("Deleted testfile and then running readdir\n");
    tfs_readdir();
    return 1;
}