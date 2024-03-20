#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"
#include "fdLL.c"
#include "bitmap.c"
#include <sys/fcntl.h>

int mounted = 0;     // 1 if file system is mounted, 0 if not
char *currMountedFS; // Name of the currently mounted file system
int fds = 1;         // File descriptor counter
int disk = -1;       // File descriptor for disk
FileEntry *openFileTable = NULL;

Bitmap *readBitmap(int disk)
{
    // Go to the location where the bitmap is stored on disk
    if (lseek(disk, 4, SEEK_SET) == -1)
    {
        fprintf(stderr, "Error: Unable to seek to index 4.\n");
        closeDisk(disk);
        return NULL;
    }

    // Read the bitmap structure from disk
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (read(disk, bitmap, sizeof(Bitmap)) != sizeof(Bitmap))
    {
        fprintf(stderr, "Error: Unable to read bitmap data.\n");
        closeDisk(disk);
        free(bitmap);
        return NULL;
    }

    // Allocate memory for free blocks
    bitmap->free_blocks = (uint8_t *)malloc(bitmap->bitmap_size);
    if (bitmap->free_blocks == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for bitmap contents.\n");
        closeDisk(disk);
        free(bitmap);
        return NULL;
    }

    // Read the bitmap contents from disk
    if (read(disk, bitmap->free_blocks, bitmap->bitmap_size) != bitmap->bitmap_size)
    {
        fprintf(stderr, "Error: Unable to read bitmap contents.\n");
        closeDisk(disk);
        free(bitmap->free_blocks);
        free(bitmap);
        return NULL;
    }

    return bitmap;
}

int writeBitmap(int disk, Bitmap *bitmap)
{
    // Go back to index 4(location where bitmap should be written to disk)
    if (lseek(disk, 4, SEEK_SET) == -1)
    {
        fprintf(stderr, "Error: Unable to seek to index 4.\n");
        closeDisk(disk);
        return SEEK_ERROR;
    }

    if (bitmap->bitmap_size + sizeof(Bitmap) > 232)
    {
        fprintf(stderr, "Error: Bitmap size too large.\n");
        closeDisk(disk);
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
    if (write(disk, bitmap, sizeof(Bitmap)) != sizeof(Bitmap))
    {
        fprintf(stderr, "Error: Unable to write bitmap data.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
    printf("Bitmap data written.\n");
    if (write(disk, bitmap->free_blocks, bitmap->bitmap_size) != bitmap->bitmap_size)
    {
        fprintf(stderr, "Error: Unable to write bitmap data.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
    printf("Bitmap contents written.\n");
    return 1;
}

int tfs_mkfs(char *filename, int nBytes)
{
    /* Makes a blank TinyFS file system of size nBytes on the unix file
    specified by ‘filename’. This function should use the emulated disk
    library to open the specified unix file, and upon success, format the
    file to be a mountable disk. This includes initializing all data to 0x00,
    setting magic numbers, initializing and writing the superblock and
    inodes, etc. Must return a specified success/error code. */
    disk = openDisk(filename, nBytes);
    if (disk < 0)
    {
        fprintf(stderr, "Error: Unable to open disk file.\n");
        return INVLD_BLK_SIZE;
    }
    else
    {
        unsigned char data = 0x01; // Modify the data to be written
        if (write(disk, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        data = 0x44; // Modify data to write to be magic number
        if (write(disk, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        // Initialize the rest of the data to 0x00
        unsigned char zeroData = 0x00;
        // put magic number and headers in free blocks LOOOOOOOOOOOOOOOOOOOK
        // calculate the disk size and write the rest of the data
        off_t diskSize = nBytes;
        // 4 is next empty block (block type = 0, magic num = 1, block address = 2, empty = 3)
        int remainingBytes = diskSize - 2;
        for (int i = 0; i < remainingBytes; i++)
        {
            if (write(disk, &zeroData, sizeof(zeroData)) != sizeof(zeroData))
            {
                fprintf(stderr, "Error: Unable to write physical data.\n");
                return WRITE_ERROR;
            }
        }

        // Start the bitmap
        Bitmap *bitmap = create_bitmap(diskSize - BLOCKSIZE, BLOCKSIZE);
        size_t data_size = sizeof(Bitmap) + bitmap->bitmap_size;
        // bit map only able to hold max of 232 * 8 blocks worth of data
        // 1856 should be enough blocks to store all the data on our system and if we need more in the future
        // we can alter how bitmap is store structurally
        return writeBitmap(disk, bitmap);
        // make success code for mkfs
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

    disk = openDisk(diskname, 0);
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

fileDescriptor tfs_openFile(char *name)
{
    /* Creates or Opens a file for reading and writing on the currently
    mounted file system. Creates a dynamic resource table entry for the file,
    and returns a file descriptor (integer) that can be used to reference
    this entry while the filesystem is mounted. */
    if (!mounted)
    {
        fprintf(stderr, "Error: No file system mounted.\n");
        return MOUNTED_ERROR; // Or define an appropriate error code
    }

    // Check if the file already exists in the dynamic resource table
    FileEntry *current = openFileTable;
    while (current != NULL)
    {
        if (strcmp(current->filename, name) == 0)
        {
            // File already exists, return its file descriptor
            return current->fileDescriptor;
        }
        current = current->next;
    }

    // File does not exist, creates a dynamic resource table entry for the file, returns a file descriptor
    int fd = fds++;

    Bitmap *bitmap = readBitmap(disk);
    int free_block = find_free_blocks_of_size(bitmap, 1);
    if (free_block == -2)
    {
        fprintf(stderr, "Error: No free blocks available.\n");
        return FREE_BLOCK_ERROR;
    }
    allocate_block(bitmap, free_block);
    writeBitmap(disk, bitmap);
    // multiply by 256 to get index relative to the disk for later calculations
    // we store it as
    int inode_index = free_block;
    FileEntry *newFileEntry = createFileEntry(name, fd, inode_index);

    if (lseek(disk, ROOT_DIRECTORY_LOC, SEEK_SET) == -1)
    {
        fprintf(stderr, "Error: Unable to seek to root directory block.\n");
        closeDisk(disk);
        return SEEK_ERROR;
    }
    unsigned char data = 0x02; // 2 for inode block
    if (write(disk, &data, sizeof(data)) != sizeof(data))
    {
        fprintf(stderr, "Error: Unable to write physical data.\n");
        return WRITE_ERROR;
    }
    data = 0x44; // Modify data to write to be magic number
    if (write(disk, &data, sizeof(data)) != sizeof(data))
    {
        fprintf(stderr, "Error: Unable to write physical data.\n");
        return WRITE_ERROR;
    }
    if (lseek(disk, 2, SEEK_CUR) == -1)
    {
        fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
        return SEEK_ERROR;
    }
    // ensure that inode is written as two bytes so we can write up to block 65536 for inodes
    uint16_t byte_inode = (uint16_t)inode_index;
    // we will write to inode mappings (inode offset) to bytes after 4th byte(index 4 onward)
    for (int i = 0; i < 250; i += 2)
    {
        uint16_t value;
        if (read(disk, &value, sizeof(value)) != sizeof(value))
        {
            fprintf(stderr, "Error: Unable to read physical data.\n");
            return DISK_READ_ERROR;
        }
        if (value == 0x0000)
        {
            // The next two bits are not 0x00
            // so write new inode index to this location
            if (lseek(disk, -2, SEEK_CUR) == -1)
            {
                fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
                return SEEK_ERROR;
            }
            if (write(disk, &byte_inode, sizeof(byte_inode)) != sizeof(byte_inode))
            {
                fprintf(stderr, "Error: Unable to write physical data.\n");
                return WRITE_ERROR;
            }
            break;
        }
    }
    //implement logic if we run out of space for inodes by allocating a new block for more inode space

    insertFileEntry(openFileTable, newFileEntry);

    /* find first free location to place an inode block */
    /* add a new entry in drt that refers to this filename
     *     returns a fileDescriptor (temp->id) */
    return fd;
}

int tfs_closeFile(fileDescriptor FD)
{
    /* Closes the file, de-allocates all system resources, and removes table
    entry */

    int result = deleteFileEntry(openFileTable, FD);
    return result;
}

int tfs_writeFile(fileDescriptor FD, char *buffer, int size){
/* Writes buffer ‘buffer’ of size ‘size’, which represents an entire
file’s content, to the file system. Previous content (if any) will be
completely lost. Sets the file pointer to 0 (the start of file) when
done. Returns success/error codes. */
    Bitmap *bitmap = readBitmap(disk);
    int num_blocks = (size + BLOCKSIZE - 1) / BLOCKSIZE;
    return 1;
}

int tfs_deleteFile(fileDescriptor FD);
/* deletes a file and marks its blocks as free on disk. */

int tfs_readByte(fileDescriptor FD, char *buffer);
/* reads one byte from the file and copies it to buffer, using the
current file pointer location and incrementing it by one upon success.
If the file pointer is already past the end of the file then
tfs_readByte() should return an error and not increment the file pointer.
*/

int tfs_seek(fileDescriptor FD, int offset)
{
    /* change the file pointer location to offset (absolute). Returns
    success/error codes.*/
    if (!mounted)
    {
        return MOUNTED_ERROR;
    }

    FileEntry *current = openFileTable;
    fileDescriptor fd = openDisk(currMountedFS, 0);

    while (current != NULL)
    {
        if (current->fileDescriptor == FD)
        {
            if (lseek(current->fileDescriptor, offset, SEEK_SET) == -1)
            {
                return SEEK_ERROR;
            }
            closeDisk(FD);
            return 1;
        }
        current = current->next;
    }
    return SEEK_FAIL;
}

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
    tfs_mount("tinyfs_disk");
    tfs_openFile("testfile");
    printf("TinyFS file system created successfully.\n");
}