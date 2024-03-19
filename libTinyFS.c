#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"
#include "fdLL.c"
#include "bitmap.c"

int mounted = 0; // 1 if file system is mounted, 0 if not
char *currMountedFS; // Name of the currently mounted file system
int fds = 1; // File descriptor counter

FileEntry *openFileTable = NULL;

int tfs_mkfs(char *filename, int nBytes)
{
    /* Makes a blank TinyFS file system of size nBytes on the unix file
    specified by ‘filename’. This function should use the emulated disk
    library to open the specified unix file, and upon success, format the
    file to be a mountable disk. This includes initializing all data to 0x00,
    setting magic numbers, initializing and writing the superblock and
    inodes, etc. Must return a specified success/error code. */
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
        data = 0x44; // Modify data to write to be magic number
        if (write(fd, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        // Initialize the rest of the data to 0x00
        unsigned char zeroData = 0x00;
        // calculate the disk size and write the rest of the data
        off_t diskSize = nBytes;
        // 4 is next empty block (block type = 0, magic num = 1, block address = 2, empty = 3)
        int remainingBytes = diskSize - 2;
        for (int i = 0; i < remainingBytes; i++)
        {
            if (write(fd, &zeroData, sizeof(zeroData)) != sizeof(zeroData))
            {
                fprintf(stderr, "Error: Unable to write physical data.\n");
                return WRITE_ERROR;
            }
        }
        // Go back to index 4
        if (lseek(fd, 4, SEEK_SET) == -1)
        {
            fprintf(stderr, "Error: Unable to seek to index 4.\n");
            closeDisk(fd);
            return SEEK_ERROR;
        }

        // Start the bitmap
        Bitmap *bitmap = create_bitmap(diskSize - BLOCKSIZE, BLOCKSIZE);
        size_t data_size = sizeof(Bitmap) + bitmap->bitmap_size;
        // bit map only able to hold max of 232 * 8 blocks worth of data
        // 1856 should be enough blocks to store all the data on our system and if we need more in the future
        // we can alter how bitmap is store structurally

        if (bitmap->bitmap_size + sizeof(Bitmap) > 232)
        {
            fprintf(stderr, "Error: Bitmap size too large.\n");
            closeDisk(fd);
            return BITMAP_SIZE_ERROR;
        }
        printf("sizeof bitmap = %zu\n", sizeof(Bitmap));
        printf("Bitmap size: %d\n", bitmap->bitmap_size);
        printf("Bitmap contents: ");
        for (int i = 0; i < bitmap->bitmap_size; i++)
        {
            printf("%d ", bitmap->free_blocks[i]);
        }
        printf("\n");
        if (write(fd, bitmap, sizeof(Bitmap)) != sizeof(Bitmap))
        {
            fprintf(stderr, "Error: Unable to write bitmap data.\n");
            closeDisk(fd);
            return WRITE_ERROR;
        }
        printf("Bitmap data written.\n");
        printf("data_size %zu", data_size);
        if (write(fd, bitmap->free_blocks, bitmap->bitmap_size) != bitmap->bitmap_size)
        {
            fprintf(stderr, "Error: Unable to write bitmap data.\n");
            closeDisk(fd);
            return WRITE_ERROR;
        }
        printf("Bitmap contents written.\n");
        return 1; // make success code for mkfs
    }
}

int tfs_mount(char *diskname)
{
    // check if already mounted
    if (mounted)
    {
        fprintf(stderr, "Error: File system already mounted.\n");
        return MOUNTED_ERROR;
    }

    int disk = openDisk(diskname, 0);
    if (disk < 0)
    {
        return DISK_ERROR;
    }

    char superblock_data[BLOCKSIZE];
    if (readBlock(disk, 0, superblock_data) == -1)
    {
        fprintf(stderr, "Error: Unable to read superblock from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }

    // check for magic number
    if (superblock_data[1] != 0x44)
    {
        fprintf(stderr, "Error: Incorrect magic number. Not a TinyFS file system.\n");
        closeDisk(disk);
        return MAGIC_NUMBER_ERROR;
    }

    mounted = 1;
    printf("File system mounted successfully: %s\n", diskname);
    currMountedFS = (char *)malloc(strlen(diskname));
    strcpy(currMountedFS, diskname);
    closeDisk(disk);
    return 0;
}

int tfs_unmount(void)
{
    /* tfs_mount(char *diskname) “mounts” a TinyFS file system located within
    ‘diskname’. tfs_unmount(void) “unmounts” the currently mounted file
    system. As part of the mount operation, tfs_mount should verify the file
    system is the correct type. In tinyFS, only one file system may be
    mounted at a time. Use tfs_unmount to cleanly unmount the currently
    mounted file system. Must return a specified success/error code. */
    if (!mounted)
    {
        fprintf(stderr, "Error: No file system mounted.\n");
        return MOUNTED_ERROR;
    }
    mounted = 0;
    printf("File system unmounted successfully.\n");
    return 0;
}

fileDescriptor tfs_openFile(char *name){
/* Creates or Opens a file for reading and writing on the currently
mounted file system. Creates a dynamic resource table entry for the file,
and returns a file descriptor (integer) that can be used to reference
this entry while the filesystem is mounted. */
    if (!mounted) {
        fprintf(stderr, "Error: No file system mounted.\n");
        return MOUNTED_ERROR; // Or define an appropriate error code
    }

    // Check if the file already exists in the dynamic resource table
    FileEntry *current = openFileTable;
    while (current != NULL) {
        if (strcmp(current->filename, name) == 0) {
            // File already exists, return its file descriptor
            return current->fileDescriptor;
        }
        current = current->next;
    }
    
     // File does not exist, create a new FileEntry for the file
     // Creates a dynamic resource table entry for the file, and returns a file descriptor
    int fd = fds++;
    insertFileEntry(FileEntry **head, FileEntry *newFileEntry)


    /* find first free location to place an inode block */
    /* add a new entry in drt that refers to this filename
	 *     returns a fileDescriptor (temp->id) */
    close(fd);
}

int tfs_closeFile(fileDescriptor FD)
{
    /* Closes the file, de-allocates all system resources, and removes table
    entry */
    int result = close(FD);
    return result;
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

int main()
{
    // Create a TinyFS file system with a specified size
    char filename[] = "tinyfs_disk";
    int disk_size = 4096; // 2 KB disk size
    int result = tfs_mkfs(filename, disk_size);

    // Check if tfs_mkfs succeeded
    if (result != 1)
    {
        fprintf(stderr, "Error: Failed to create TinyFS file system.\n");
        return 1;
    }
    printf("TinyFS file system created successfully.\n");
}