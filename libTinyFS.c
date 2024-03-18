#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"

int mounted = 0; // 1 if file system is mounted, 0 if not

int tfs_mkfs(char *filename, int nBytes)
{
    /* Makes a blank TinyFS file system of size nBytes on the unix file
    specified by ‘filename’. This function should use the emulated disk
    library to open the specified unix file, and upon success, format the
    file to be a mountable disk. This includes initializing all data to 0x00,
    setting magic numbers, initializing and writing the superblock and
    inodes, etc. Must return a specified success/error code. */
    superblock sb;
    sb.type = SUPERBLOCK;
    sb.magic = 0x44;
    sb.blockSize = BLOCKSIZE;
    sb.nBlocks = nBytes / BLOCKSIZE;

    int fd = openDisk(filename, nBytes);
    if (fd < 0)
    {
        fprintf(stderr, "Error: Unable to open disk file.\n");
        return INVLD_BLK_SIZE;
    }
    else
    {
        unsigned char data = 0x01; // Modify the data to be written
        if (write(fd, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        if (write(fd, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        // Initialize the rest of the data to 0x00
        unsigned char zeroData = 0x00;
        // calculate the disk size and write the rest of the data
        off_t diskSize = nBytes - (nBytes % BLOCKSIZE);
        int remainingBytes = diskSize - sizeof(data) * 2;
        for (int i = 0; i < remainingBytes; i++)
        {
            if (write(fd, &zeroData, sizeof(zeroData)) != sizeof(zeroData))
            {
                fprintf(stderr, "Error: Unable to write physical data.\n");
                return WRITE_ERROR;
            }
        }
        
        
    }
}

int tfs_mount(char *diskname){
    //check if already mounted
    if (mounted){
        fprintf(stderr, "Error: File system already mounted.\n");
        return MOUNTED_ERROR;
    }
    
    int disk = openDisk(diskname, 0);
    if (disk < 0) {
      return DISK_ERROR;
    }

    char superblock_data[BLOCKSIZE];
    if (readBlock(disk, 0, superblock_data) == -1) {
        fprintf(stderr, "Error: Unable to read superblock from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }

    //check for magic number
    if (superblock_data[1] != 0x44) {
        fprintf(stderr, "Error: Incorrect magic number. Not a TinyFS file system.\n");
        closeDisk(disk);
        return MAGIC_NUMBER_ERROR;
    }
    mounted = 1;
    printf("File system mounted successfully: %s\n", diskname);
    closeDisk(disk);
    return 0;
}

int tfs_unmount(void){
/* tfs_mount(char *diskname) “mounts” a TinyFS file system located within
‘diskname’. tfs_unmount(void) “unmounts” the currently mounted file
system. As part of the mount operation, tfs_mount should verify the file
system is the correct type. In tinyFS, only one file system may be
mounted at a time. Use tfs_unmount to cleanly unmount the currently
mounted file system. Must return a specified success/error code. */
    if (!mounted){
        fprintf(stderr, "Error: No file system mounted.\n");
        return MOUNTED_ERROR;
    }
    mounted = 0;
    printf("File system unmounted successfully.\n");
    return 0;
}

fileDescriptor tfs_openFile(char *name);
/* Creates or Opens a file for reading and writing on the currently
mounted file system. Creates a dynamic resource table entry for the file,
and returns a file descriptor (integer) that can be used to reference
this entry while the filesystem is mounted. */

int tfs_closeFile(fileDescriptor FD){
/* Closes the file, de-allocates all system resources, and removes table
entry */
    int result = close(FD);
}


int tfs_writeFile(fileDescriptor FD, char *buffer, int size);
/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire
file’s content, to the file system. Previous content (if any) will be 
completely lost. Sets the file pointer to 0 (the start of file) when
done. Returns success/error codes. */

int tfs_deleteFile(fileDescriptor FD);
/* deletes a file and marks its blocks as free on disk. */

int tfs_readByte(fileDescriptor FD, char *buffer);
/* reads one byte from the file and copies it to buffer, using the
current file pointer location and incrementing it by one upon success.
If the file pointer is already past the end of the file then
tfs_readByte() should return an error and not increment the file pointer.
*/

int tfs_seek(fileDescriptor FD, int offset);
/* change the file pointer location to offset (absolute). Returns
success/error codes.*/
