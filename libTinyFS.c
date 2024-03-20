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
#include <string.h>

int mounted = 0;     // 1 if file system is mounted, 0 if not
char *currMountedFS; // Name of the currently mounted file system
int fds = 1;         // File descriptor counter
int disk = -1;       // File descriptor for disk
FileEntry *openFileTable = NULL;

Bitmap *readBitmap(int disk)
{
    char blockData[BLOCKSIZE];
    if (readBlock(disk, 0, blockData) == -1)
    {
        fprintf(stderr, "Error: Unable to read block from disk.\n");
        closeDisk(disk);
        return NULL;
    }
    printf("Block Data (in bytes) for readbitmap: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02X ", (unsigned char)blockData[i]);
    }
    printf("\n");
    diskInfo *diskInfo = getDiskByFD(disk);
    FILE *file_pointer = diskInfo->file_pointer;
    // Go to the location where the bitmap is stored on disk
    if (fseek(file_pointer, 4, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: Unable to seek to index 4.\n");
        closeDisk(disk);
        return NULL;
    }

    // Read the bitmap structure from disk
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (fread(bitmap, sizeof(Bitmap), 1, file_pointer) != 1)
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
    if (fread(bitmap->free_blocks, bitmap->bitmap_size, 1, file_pointer) != 1)
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
    char *blockData = (char*)malloc(BLOCKSIZE);
    if (readBlock(disk, 0, blockData) == -1)
    {
        fprintf(stderr, "Error: Unable to read block from disk.\n");
        closeDisk(disk);
        return -1;
    }
    printf("Block Data (in bytes) for writeBitmap: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02X ", (unsigned char)blockData[i]);
    }
    printf("\n");
    if (bitmap->bitmap_size + sizeof(Bitmap) > 232)
    {
        fprintf(stderr, "Error: Bitmap size too large.\n");
        closeDisk(disk);
        return BITMAP_SIZE_ERROR;
    }
    if (memcpy(&blockData[4], bitmap, sizeof(Bitmap)) == NULL)
    {
        fprintf(stderr, "Error: Unable to copy bitmap data.\n");
        closeDisk(disk);
        return -1;
    }
    if (memcpy(&blockData[4 + sizeof(Bitmap)], bitmap->free_blocks, bitmap->bitmap_size) == NULL)
    {
        fprintf(stderr, "Error: Unable to copy bitmap contents.\n");
        closeDisk(disk);
        return -1;
    }
    printf("Block Data (in bytes) after memcpy: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02X ", (unsigned char)blockData[i]);
    }
    printf("\n");
    if (writeBlock(disk, 0, blockData) == -1)
    {
        fprintf(stderr, "Error: Unable to write block to disk.\n");
        closeDisk(disk);
        return -1;
    }
    printf("\n");
    // diskInfo *diskInfo = getDiskByFD(disk);
    // FILE *file_pointer = diskInfo->file_pointer;
    // Go back to index 4(location where bitmap should be written to disk)
    // if (fseek(file_pointer, 4, SEEK_SET) != 0)
    // {
    //     fprintf(stderr, "Error: Unable to seek to index 4.\n");
    //     closeDisk(disk);
    //     return SEEK_ERROR;
    // }

    printf("sizeof bitmap = %zu\n", sizeof(Bitmap));
    printf("Bitmap size: %d\n", bitmap->bitmap_size);
    printf("Bitmap contents: ");
    for (int i = 0; i < bitmap->bitmap_size; i++)
    {
        printf("%d ", bitmap->free_blocks[i]);
    }
    printf("\n");
    // if (fwrite(bitmap, sizeof(Bitmap), 1, file_pointer) != 1)
    // {
    //     fprintf(stderr, "Error: Unable to write bitmap data.\n");
    //     closeDisk(disk);
    //     return WRITE_ERROR;
    // }
    // printf("Bitmap data written.\n");

    // if (fwrite(bitmap->free_blocks, bitmap->bitmap_size, 1, file_pointer) != 1)
    // {
    //     fprintf(stderr, "Error: Unable to write bitmap data.\n");
    //     closeDisk(disk);
    //     return WRITE_ERROR;
    // }
    // printf("Bitmap contents written.\n");
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
    int num_blocks = nBytes / BLOCKSIZE;
    if (disk < 0)
    {
        fprintf(stderr, "Error: Unable to open disk file.\n");
        return INVLD_BLK_SIZE;
    }
    else
    {
        char *blockdata = calloc(1, BLOCKSIZE);
        memset(&blockdata[0], 4, 1);
        memset(&blockdata[1], 0x44, 1);
        for (int i = 0; i < num_blocks; i++)
        {
            printf("WRITING A FILE BLOCK");
            if (writeBlock(disk, i, blockdata) == -1)
            {
                fprintf(stderr, "Error: Unable to write block to disk.\n");
                closeDisk(disk);
                return DISK_ERROR;
            }
        }
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02X ", blockdata[i]);
        }
        printf("\n");
        blockdata = calloc(1, BLOCKSIZE);
        memset(&blockdata[0], 1, 1);
        memset(&blockdata[1], 0x44, 1);
        // manually write superblock to disk
        if (writeBlock(disk, 0, blockdata) == -1)
        {
            fprintf(stderr, "Error: Unable to write block to disk.\n");
            closeDisk(disk);
            return DISK_ERROR;
        }
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02X ", blockdata[i]);
        }
        printf("\n");
        blockdata = calloc(1, BLOCKSIZE);
        memset(&blockdata[0], 2, 1);
        memset(&blockdata[1], 0x44, 1);
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02X ", blockdata[i]);
        }
        printf("\n");
        // manually write root directory to disk
        if (writeBlock(disk, 1, blockdata) == -1)
        {
            fprintf(stderr, "Error: Unable to write block to disk.\n");
            closeDisk(disk);
            return DISK_ERROR;
        }
        // Start the bitmap
        int diskSize = nBytes;
        Bitmap *bitmap = create_bitmap(diskSize - BLOCKSIZE, BLOCKSIZE);
        size_t data_size = sizeof(Bitmap) + bitmap->bitmap_size;
        // bit map only able to hold max of 232 * 8 blocks worth of data
        // 1856 should be enough blocks to store all the data on our system and if we need more in the future
        // we can alter how bitmap is store structurally
        int returnStatus = writeBitmap(disk, bitmap);
        if (returnStatus != 1)
        {
            return returnStatus; // set to a specific error code later
        }
    }
    return 1; // make meaningfull succss error code
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
    printf("before return\n");
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
    if (bitmap == NULL)
    {
        fprintf(stderr, "Error: Unable to read bitmap from disk.\n");
        return DISK_READ_ERROR;
    }
    int free_block = find_free_blocks_of_size(bitmap, 1);
    if (free_block == -2)
    {
        fprintf(stderr, "Error: No free blocks available.\n");
        return FREE_BLOCK_ERROR;
    }
    allocate_block(bitmap, free_block);
    writeBitmap(disk, bitmap);
    // multiply by 256 to get index relative to the disk for later calculations
    // we store it as a 16 bit number so we can store up to 65536 blocks
    // seek to root directory block where data is written
    // if (lseek(disk, ROOT_DIRECTORY_LOC + 4, SEEK_SET) == -1)
    // {
    //     fprintf(stderr, "Error: Unable to seek to root directory block.\n");
    //     closeDisk(disk);
    //     return SEEK_ERROR;
    // }

    int inode_index = free_block;
    FileEntry *newFileEntry = createFileEntry(name, fd, inode_index);

    // ensure that inode is written as two bytes so we can write up to block 65536 for inodes
    uint16_t byte_inode = (uint16_t)inode_index;
    // we will write to inode mappings (inode offset) to bytes after 4th byte(index 4 onward)
    uint16_t value;
    unsigned char buffer[sizeof(uint16_t)];
    disk = openDisk(currMountedFS, 0);
    int flags = fcntl(disk, F_GETFD);
    if (flags == -1)
    {
        perror("fcntl");
        return -1; // Error occurred
    }
    printf("file is open\n"); // File is open
    char *inodeData = malloc(BLOCKSIZE);
    if (readBlock(disk, 1, inodeData) == -1)
    {
        fprintf(stderr, "Error: Unable to read inode data from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    for (int i = 0; i < 250; i += 2)
    {
        // + 4 to skip the first 4 bytes of the block which is meta data
        unsigned char byte1 = inodeData[i + 4];
        unsigned char byte2 = inodeData[i + 5];
        int combinedInt = (byte1 << 8) | byte2;
        printf("combined int is %d\n", combinedInt);
        if (combinedInt == 0)
        {
            printf("found free inode\n");
            inodeData[i + 4] = byte_inode >> 8;
            inodeData[i + 5] = byte_inode & 0xFF;
            writeBlock(disk, 1, inodeData);
        }
        // if (lseek(disk, 2, SEEK_CUR) == -1)
        // {
        //     fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
        //     return SEEK_ERROR;
        // }
        // off_t currentOffset = lseek(disk, 0, SEEK_CUR); // Get current offset
        // if (lseek(disk, 1, SEEK_CUR) == -1)
        //{
        //     fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
        //     return SEEK_ERROR;
        // }
        // if (currentOffset == -1)
        // {
        //     fprintf(stderr, "Error: Unable to get current file offset.\n");
        //     return SEEK_ERROR;
        // }
        // int datasize = read(disk, buffer, sizeof(buffer));
        // printf("buffer data is %s and size is %d and other size is %d\n", buffer, sizeof(value), datasize);
        // if (datasize != sizeof(buffer))
        // {
        //     fprintf(stderr, "buffer data is %s and size is %d and other size is %d\n", buffer, sizeof(value), datasize);
        //     fprintf(stderr, "Error: Unable to read physical data at offset %ld. %d\n", currentOffset, disk);
        //     return DISK_READ_ERROR;
        // }
        // if (value == 0x0000)
        // {
        //     // The next two bits are not 0x00
        //     // so write new inode index to this location
        //     if (lseek(disk, -2, SEEK_CUR) == -1)
        //     {
        //         fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
        //         return SEEK_ERROR;
        //     }
        //     if (write(disk, &byte_inode, sizeof(byte_inode)) != sizeof(byte_inode))
        //     {
        //         fprintf(stderr, "Error: Unable to write physical data.\n");
        //         return WRITE_ERROR;
        //     }
        //     break;
        // }
    }
    // implement logic if we run out of space for inodes by allocating a new block for more inode space

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

int tfs_writeFile(fileDescriptor FD, char *buffer, int size)
{
    /* Writes buffer ‘buffer’ of size ‘size’, which represents an entire
    file’s content, to the file system. Previous content (if any) will be
    completely lost. Sets the file pointer to 0 (the start of file) when
    done. Returns success/error codes. */
    // subtract 4 from block_size
    Bitmap *bitmap = readBitmap(disk);
    int num_blocks = (size + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);
    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    // check if there is data already written to the file and if so deallocate it
    if (file->file_size > 0)
    {
        int prev_num_blocks = (file->file_size + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);
        // deallocate previous blocks allocated to a file
        free_num_blocks(bitmap, file->inode_index, prev_num_blocks);
        // update file size to be 0 now temporarily until we write new data
        file->file_size = 0;
    }
    // find free blocks for new data for file
    int free_block = find_free_blocks_of_size(bitmap, num_blocks);
    if (free_block == -2)
    {
        fprintf(stderr, "Error: No free blocks available.\n");
        return FREE_BLOCK_ERROR;
    }
    // set write block in terms of disk index
    int write_offset = free_block;
    int data_written = 0;
    int offset = 0;
    char blockdata[BLOCKSIZE];
    while (data_written <= size)
    {
        memset(&blockdata[0], 3, 1);
        memcpy(&blockdata[4], buffer + offset, 252);
        data_written += BLOCKSIZE - 4;
        if (data_written < size)
        {
            // set the location of the next block
            memset(&blockdata[2], write_offset + 1, 1);
        }
        if (writeBlock(disk, write_offset, blockdata) == -1)
        {
            fprintf(stderr, "Error: Unable to write block to disk.\n");
            closeDisk(disk);
            return DISK_ERROR;
        }
        write_offset += BLOCKSIZE;
    }
    return 1;
}

int tfs_deleteFile(fileDescriptor FD)
{
    /* deletes a file and marks its blocks as free on disk. */
    if (!mounted)
    {
        return MOUNTED_ERROR;
    }
    Bitmap *bitmap = readBitmap(disk);
    FileEntry *deleteMe = findFileEntryByFD(openFileTable, FD);
    free_num_blocks(bitmap, deleteMe->inode_index, deleteMe->file_size);
    writeBitmap(disk, bitmap);
    tfs_closeFile(FD); // remove from open file table and free memory
    return 1;
}

int tfs_readByte(fileDescriptor FD, char *buffer)
{
    /* reads one byte from the file and copies it to buffer, using the
    current file pointer location and incrementing it by one upon success.
    If the file pointer is already past the end of the file then
    tfs_readByte() should return an error and not increment the file pointer.
    */
    if (!mounted)
    {
        return MOUNTED_ERROR;
    }

    // Find the file entry in the open file table
    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    return 1; // make success message thats meaningful
}

int tfs_seek(fileDescriptor FD, int offset)
{
    /* change the file pointer location to offset (absolute). Returns
    success/error codes.*/
    if (!mounted)
    {
        return MOUNTED_ERROR;
    }
    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    file->offset = offset;
    return 1; // make success message thats meainful
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
    return 1;
}