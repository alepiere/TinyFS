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
Bitmap *mountedBitmap = NULL;
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
        unsigned char superblock[BLOCKSIZE];
        superblock[0] = 0x01; // Set the first byte to 0x01
        superblock[1] = 0x44; // Set the second byte to 0x44 (magic num)
        // Set the remaining blocks to 0x00
        for (int i = 2; i < BLOCKSIZE; i++)
        {
            superblock[i] = 0x00;
        }
        // we will not write superblock yet, because we want to add bitmap to it first
        unsigned char rootDirectory[BLOCKSIZE];
        rootDirectory[0] = 0x02; // Set the first byte to 0x02
        rootDirectory[1] = 0x44; // Set the second byte to 0x44 (magic num)
        for (int i = 2; i < BLOCKSIZE; i++)
        {
            rootDirectory[i] = 0x00;
        }
        printf("Root Directory contents: ");
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02x ", rootDirectory[i]);
        }
        printf("\n");

        if (writeBlock(disk, 1, rootDirectory) == -1)
        {
            fprintf(stderr, "Error: Unable to write root directory to disk.\n");
            closeDisk(disk);
            return WRITE_ERROR;
        }
        unsigned char emptyBlock[BLOCKSIZE];
        emptyBlock[0] = 0x04; // Set the first byte to 0x04
        emptyBlock[1] = 0x44; // Set the second byte to 0x44 (magic num)
        for (int i = 2; i < BLOCKSIZE; i++)
        {
            emptyBlock[i] = 0x00;
        }
        int num_blocks = nBytes / BLOCKSIZE;
        for (int i = 2; i < num_blocks; i++)
        {
            if (writeBlock(disk, i, emptyBlock) == -1)
            {
                fprintf(stderr, "Error: Unable to write empty block to disk.\n");
                closeDisk(disk);
                return WRITE_ERROR;
            }
        }
        int bitmap_size = (((nBytes / BLOCKSIZE) + 7) / 8);
        Bitmap *bitmap = create_bitmap(bitmap_size, num_blocks, NULL);
        // make sure num+blocks is less than max number of blocks we can store in 2 bytes
        if (bitmap_size > 248)
        {
            fprintf(stderr, "Error: Disk size too large to fit on superblock.\n");
            closeDisk(disk);
            return -123; // make an error code
        }
        if (num_blocks > 65535)
        {
            fprintf(stderr, "Error: Disk size too large.\n");
            closeDisk(disk);
            return -123; // make an error code
        }
        unsigned char *bitmap_data = bitmap->free_blocks;
        superblock[4] = (unsigned char)bitmap_size;
        // Split num_blocks into two bytes
        unsigned char num_blocks_high_byte = (num_blocks >> 8) & 0xFF; // Most significant byte
        unsigned char num_blocks_low_byte = num_blocks & 0xFF;         // Least significant byte

        // Write the two bytes to superblock
        superblock[5] = num_blocks_high_byte; // Write most significant byte to index 5
        superblock[6] = num_blocks_low_byte;  // Write least significant byte to index 6

        for (int i = 0; i < bitmap_size; i++)
        {
            printf("writing bitmap data\n");
            superblock[i + 7] = bitmap_data[i]; // Start writing from index 7 onwards
            printf("bitmap data is %d\n", bitmap_data[i]);
        }
        // Print contents of superblock in bytes
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02X ", superblock[i]);
        }
        printf("\n");
        if (writeBlock(disk, 0, superblock) == -1)
        {
            fprintf(stderr, "Error: Unable to write superblock to disk.\n");
            closeDisk(disk);
            return WRITE_ERROR;
        }
        if (writeBlock(disk, 1, rootDirectory) == -1)
        {
            fprintf(stderr, "Error: Unable to write root directory to disk.\n");
            closeDisk(disk);
            return WRITE_ERROR;
        }
        printf("tfs create has all went through\n");
        // size_t data_size = sizeof(Bitmap) + bitmap->bitmap_size;
        // unsigned char data = 0x01; // Modify the data to be written
        // if (write(disk, &data, sizeof(data)) != sizeof(data))
        // {
        //     fprintf(stderr, "Error: Unable to write physical data.\n");
        //     return WRITE_ERROR;
        // }
        // data = 0x44; // Modify data to write to be magic number
        // if (write(disk, &data, sizeof(data)) != sizeof(data))
        // {
        //     fprintf(stderr, "Error: Unable to write physical data.\n");
        //     return WRITE_ERROR;
        // }
        // Initialize the rest of the data to 0x00
        // unsigned char zeroData = 0x00;
        // put magic number and headers in free blocks LOOOOOOOOOOOOOOOOOOOK
        // calculate the disk size and write the rest of the data
        // off_t diskSize = nBytes;
        // 4 is next empty block (block type = 0, magic num = 1, block address = 2, empty = 3)
        // int remainingBytes = diskSize - 2;
        // int start_location = lseek(disk, 0, SEEK_CUR);
        // for (int i = 0; i < remainingBytes; i++)
        // {
        //     if (write(disk, &zeroData, sizeof(zeroData)) != sizeof(zeroData))
        //     {
        //         fprintf(stderr, "Error: Unable to write physical data.\n");
        //         return WRITE_ERROR;
        //     }
        //     off_t currentOffset = lseek(disk, 0, SEEK_CUR); // Get current offset
        //     if (currentOffset == -1)
        //     {
        //         fprintf(stderr, "Error: Unable to get current file offset.\n");
        //         return SEEK_ERROR;
        //     }
        //     // printf("%d bytes of '0x00' written at location %ld with start location %d\n", i + 1, currentOffset, start_location);
        // }

        // Start the bitmap
        // Bitmap *bitmap = create_bitmap(diskSize - BLOCKSIZE, BLOCKSIZE);
        // size_t data_size = sizeof(Bitmap) + bitmap->bitmap_size;
        // bit map only able to hold max of 232 * 8 blocks worth of data
        // 1856 should be enough blocks to store all the data on our system and if we need more in the future
        // we can alter how bitmap is store structurally
        // int returnStatus = writeBitmap(disk, bitmap);
        // if (returnStatus != 1)
        // {
        //     return returnStatus; // set to a specific error code later
        // }
        // if (lseek(disk, ROOT_DIRECTORY_LOC, SEEK_SET) == -1)
        // {
        //     fprintf(stderr, "Error: Unable to seek to root directory block.\n");
        //     closeDisk(disk);
        //     return SEEK_ERROR;
        // }
        // data = 0x02; // 2 for inode block
        // if (write(disk, &data, sizeof(data)) != sizeof(data))
        // {
        //     fprintf(stderr, "Error: Unable to write physical data.\n");
        //     return WRITE_ERROR;
        // }
        // data = 0x44; // Modify data to write to be magic number
        // if (write(disk, &data, sizeof(data)) != sizeof(data))
        // {
        //     fprintf(stderr, "Error: Unable to write physical data.\n");
        //     return WRITE_ERROR;
        // }

        // off_t block_offset = 512;
        // for (int i = 0; block_offset + (256 * i) < nBytes; i++)
        // {
        //     if (lseek(disk, block_offset + (256 * i), SEEK_SET) == -1)
        //     {
        //         fprintf(stderr, "Error: Unable to seek to free block.\n");
        //         closeDisk(disk);
        //         return SEEK_ERROR;
        //     }
        //     // write block type (4 = free block)
        //     data = 0x04;
        //     if (write(disk, &data, sizeof(data)) != sizeof(data))
        //     {
        //         fprintf(stderr, "Error: Unable to write block type to free block.\n");
        //         closeDisk(disk);
        //         return WRITE_ERROR;
        //     }
        //     // write magic number (0x44)
        //     data = 0x44;
        //     if (write(disk, &data, sizeof(data)) != sizeof(data))
        //     {
        //         fprintf(stderr, "Error: Unable to write magic number to free block.\n");
        //         closeDisk(disk);
        //         return WRITE_ERROR;
        //     }
        // }

        // make success code for mkfs
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

    int bitmap_size = superblock_data[4];
    int num_blocks = (superblock_data[5] << 8) | superblock_data[6];
    printf("bitmap size is %d\n", bitmap_size);
    printf("num blocks is %d\n", num_blocks);
    unsigned char *bitmap_data = (unsigned char *)malloc(bitmap_size);
    for (int i = 0; i < bitmap_size; i++)
    {
        bitmap_data[i] = superblock_data[i + 7];
    }
    Bitmap *bitmap = create_bitmap(bitmap_size, num_blocks, bitmap_data);
    mountedBitmap = bitmap;
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

    int free_block = find_free_blocks_of_size(mountedBitmap, 1);
    printf("free block is %d\n", free_block);
    if (free_block == -2)
    {
        fprintf(stderr, "Error: No free blocks available.\n");
        return FREE_BLOCK_ERROR;
    }
    allocate_block(mountedBitmap, free_block);
    printf("block allocated\n");
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
    unsigned char rootDirectory[BLOCKSIZE];
    if (readBlock(disk, 1, rootDirectory) == -1)
    {
        fprintf(stderr, "Error: Unable to read root directory from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    for (int i = 4; i < 251; i += 2)
    {
        // need two bytes to write up to block 65535 for inodes
        int value = (rootDirectory[i] << 8) | rootDirectory[i + 1];
        // if value is 0 (unallocated) then thats where next inode mapping will be
        if (value == 0)
        {
            rootDirectory[i] = (inode_index >> 8) & 0xFF;
            rootDirectory[i + 1] = inode_index & 0xFF;
            break;
        }
    }
    printf("found space for next inode mapping\n");
    // ensure that inode is written as two bytes so we can write up to block 65536 for inodes
    // uint16_t byte_inode = (uint16_t)inode_index;
    // we will write to inode mappings (inode offset) to bytes after 4th byte(index 4 onward)
    // uint16_t value;
    // unsigned char buffer[sizeof(uint16_t)];
    // for (int i = 0; i < 250; i += 2)
    // {
    //     int datasize = read(disk, buffer, sizeof(buffer));
    //     printf("datasize is %d\n", datasize);
    //     if (datasize != sizeof(buffer))
    //     {
    //         fprintf(stderr, "buffer data is %s and size is %d and other size is %d\n", buffer, sizeof(value), datasize);
    //         fprintf(stderr, "Error: Unable to read physical data.\n");
    //         return DISK_READ_ERROR;
    //     }
    //     printf("buffer is %d eee\n", sizeof(datasize));
    //     if (value == 0x0000)
    //     {
    //         // The next two bits are not 0x00
    //         // so write new inode index to this location
    //         if (lseek(disk, -2, SEEK_CUR) == -1)
    //         {
    //             fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
    //             return SEEK_ERROR;
    //         }
    //         if (write(disk, &byte_inode, sizeof(byte_inode)) != sizeof(byte_inode))
    //         {
    //             fprintf(stderr, "Error: Unable to write physical data.\n");
    //             return WRITE_ERROR;
    //         }
    //         break;
    //     }
    // }
    // LOOOOOOOOOOK implement logic if we run out of space for inodes by allocating a new block for more inode space

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
    int write_offset = free_block * 256;
    int data_written = 0;
    while (data_written <= size)
    {
        if (lseek(disk, write_offset, SEEK_SET) == -1)
        {
            fprintf(stderr, "Error: Unable to seek to root directory block.\n");
            closeDisk(disk);
            return SEEK_ERROR;
        }
        unsigned char data = 0x03; // 3 for file extent block
        if (write(disk, &data, sizeof(data)) != sizeof(data))
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        // index to the where data starts (index 4)
        if (lseek(disk, 3, SEEK_CUR) == -1)
        {
            fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
            return SEEK_ERROR;
        }
        // write the data (which is 4 less than blocksize because 4 bytes used for metadata)
        if (write(disk, buffer, BLOCKSIZE - 4) != BLOCKSIZE - 4)
        {
            fprintf(stderr, "Error: Unable to write physical data.\n");
            return WRITE_ERROR;
        }
        data_written += BLOCKSIZE - 4;
        if (data_written < size)
        {
            if (lseek(disk, write_offset + 2, SEEK_SET) == -1)
            {
                fprintf(stderr, "Error: Unable to seek to next block position.\n");
                closeDisk(disk);
                return SEEK_ERROR;
            }
            // write block index divided by 256 so it takes less bits to store
            uint16_t next_block = (uint16_t)(write_offset / 256 + 1);
            if (write(disk, &next_block, sizeof(next_block)) != sizeof(next_block))
            {
                fprintf(stderr, "Error: Unable to write next block index data.\n");
                return WRITE_ERROR;
            }
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
    if (file == NULL)
    {
        return FILE_NOT_FOUND_ERROR;
    }

    // Check if the file pointer is already past the end of the file
    if (file->offset >= file->file_size)
    {
        return END_OF_FILE_ERROR;
    }

    // Seek to the current file pointer location
    off_t file_offset = ROOT_DIRECTORY_LOC + (file->inode_index * BLOCKSIZE) + file->offset;
    if (lseek(disk, file_offset, SEEK_SET) == -1)
    {
        fprintf(stderr, "Error: Unable to seek to file pointer location.\n");
        return SEEK_ERROR;
    }

    // Read one byte from the file and copy it to the buffer
    if (read(disk, buffer, 1) != 1)
    {
        fprintf(stderr, "Error: Unable to read byte from file.\n");
        return READ_ERROR;
    }

    // Increment the file pointer location
    file->offset++;

    return 1; // Success
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