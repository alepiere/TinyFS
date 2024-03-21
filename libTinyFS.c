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
#include <time.h>

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
    // printf("sizeof bitmap = %zu\n", sizeof(Bitmap));
    // printf("Bitmap size: %d\n", bitmap->bitmap_size);
    // printf("Bitmap contents: ");
    // for (int i = 0; i < bitmap->bitmap_size; i++)
    // {
    //     printf("%d ", bitmap->free_blocks[i]);
    // }
    // printf("\n");
    if (write(disk, bitmap, sizeof(Bitmap)) != sizeof(Bitmap))
    {
        fprintf(stderr, "Error: Unable to write bitmap data.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
    // printf("Bitmap data written.\n");
    if (write(disk, bitmap->free_blocks, bitmap->bitmap_size) != bitmap->bitmap_size)
    {
        fprintf(stderr, "Error: Unable to write bitmap data.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
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
    time_t t;
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
    printf("file descriptor %d created for file %s\n", fd, name);

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
    // now create contents for inode
    unsigned char inode[BLOCKSIZE];
    if (readBlock(disk, inode_index, inode) == -1)
    {
        fprintf(stderr, "Error: Unable to read inode from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    inode[0] = 0x02; // Set the first byte to 0x02 to represent inode
    // INODE STRUCTURE [0] = 0x02, [1] = 0x44, [2] = block number of first block of file, [3] = empty [4-12] = file name and byte 12 will be null character
    //  for is file name is exactly 8 characters long
    if (strlen(name) > 8)
    {
        fprintf(stderr, "Error: File name exceeds the maximum limit of 8 characters.\n");
        return NAME_LENGTH_ERROR;
    }
    for (int i = 4; i < 12; i++)
    {
        inode[i] = name[i - 4];
    }
    printf("inode contents: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02x ", inode[i]);
    }
    printf("\n");
    // inode[12] will be null character which blocks are set to by default
    // inode[13] and [inode 14] will be file size which are set to 0/null again by default

    // inode[15] will be creation timestamp (8 bytes)
    time(&t);
    struct tm *local_time = localtime(&t);

    printf("AAA Current local time: %02d:%02d:%02d\n", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    printf("Size of time_t: %zu bytes\n", sizeof(t));
    printf("size of tm_hour is %zu\n", sizeof(local_time->tm_hour));
    printf("size of tm_min is %zu\n", sizeof(local_time->tm_min));
    printf("size of tm_sec is %zu\n", sizeof(local_time->tm_sec));
    // Convert tm_hour to bytes
    unsigned char hour_bytes[sizeof(local_time->tm_hour)];
    for (int i = 0; i < sizeof(local_time->tm_hour); i++)
    {
        hour_bytes[i] = *((unsigned char *)&local_time->tm_hour + i);
    }

    // Write hour_bytes to inode starting at i = 15
    for (int i = 0; i < sizeof(local_time->tm_hour); i++)
    {
        inode[i + 15] = hour_bytes[i];
    }
    // Convert tm_min to bytes
    unsigned char min_bytes[sizeof(local_time->tm_min)];
    for (int i = 0; i < sizeof(local_time->tm_min); i++)
    {
        min_bytes[i] = *((unsigned char *)&local_time->tm_min + i);
    }

    // Write min_bytes to inode starting at i = 19
    for (int i = 0; i < sizeof(local_time->tm_min); i++)
    {
        inode[i + 19] = min_bytes[i];
    }

    // Convert tm_sec to bytes
    unsigned char sec_bytes[sizeof(local_time->tm_sec)];
    for (int i = 0; i < sizeof(local_time->tm_sec); i++)
    {
        sec_bytes[i] = *((unsigned char *)&local_time->tm_sec + i);
    }

    // Write sec_bytes to inode starting at i = 23
    for (int i = 0; i < sizeof(local_time->tm_sec); i++)
    {
        inode[i + 23] = sec_bytes[i];
    }
    if (writeBlock(disk, inode_index, inode) == -1)
    {
        fprintf(stderr, "Error: Unable to write inode to disk.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
    unsigned char testInode[BLOCKSIZE];
    if (readBlock(disk, inode_index, testInode) == -1)
    {
        fprintf(stderr, "Error: Unable to read root directory from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    // Print the contents of inodes in bytes
    printf("inode contents with CREATION TIME: and inode index is %d\n", inode_index);
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02x ", testInode[i]);
    }
    printf("\n");

    // // creation time
    // for (int i = 15; i < 15 + sizeof(time_t); i++)
    // {
    //     inode[i] = *((unsigned char *)&t + i); // Store each byte of time_t starting at byte 15
    // }

    // // modification time
    // for (int i = 24; i < 24 + sizeof(time_t); i++)
    // {
    //     inode[i] = *((unsigned char *)&t + i); // Store each byte of time_t starting at byte 15
    // }

    // // access time
    // for (int i = 33; i < 33 + sizeof(time_t); i++)
    // {
    //     inode[i] = *((unsigned char *)&t + i); // Store each byte of time_t starting at byte 15
    // }
    if (writeBlock(disk, 1, rootDirectory) == -1)
    {
        fprintf(stderr, "Error: Unable to write root directory to disk.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }
    if (readBlock(disk, 1, rootDirectory) == -1)
    {
        fprintf(stderr, "Error: Unable to read root directory from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    // Print the contents of rootDirectory in bytes
    printf("Root Directory contents: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02x ", rootDirectory[i]);
    }
    printf("\n");

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

    insertFileEntry(&openFileTable, newFileEntry);

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
    int num_blocks = (size + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);
    // Iterate through all fileEntries in the openFileTable and print the fd
    printf("File Descriptors in openFileTable: ");
    FileEntry *allFileTable = openFileTable;
    while (allFileTable != NULL)
    {
        printf("%d ", allFileTable->fileDescriptor);
        allFileTable = allFileTable->next;
    }
    printf("\n");

    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    if (file == NULL)
    {
        fprintf(stderr, "Error: File not found in open file table.\n");
        return FILE_NOT_FOUND_ERROR;
    }
    // check if there is data already written to the file and if so deallocate it
    if (file->file_size > 0)
    {
        printf("deleting old data RN\n");
        int file_index = file->file_index;
        int prev_num_blocks = (file->file_size + (BLOCKSIZE - 4) - 1) / (BLOCKSIZE - 4);
        printf("prev num blocks is %d and file_index is %d for fd %d\n", prev_num_blocks, file_index, file->fileDescriptor);
        char freeBlock[BLOCKSIZE];
        freeBlock[0] = 0x04;
        freeBlock[1] = 0x44;
        for (int i = 2; i < BLOCKSIZE; i++)
        {
            freeBlock[i] = 0x00;
        }
        for (int i = 0; i < prev_num_blocks; i++)
        {
            if (writeBlock(disk, file_index + i, freeBlock) == -1)
            {
                fprintf(stderr, "Error: Unable to write free block to disk.\n");
                closeDisk(disk);
                return WRITE_ERROR;
            }
            free_block(mountedBitmap, file->file_index + i);
        }
        // deallocate previous blocks allocated to a file
        free_num_blocks(mountedBitmap, file->inode_index, prev_num_blocks);
        // update file size to be 0 now temporarily until we write new data
        file->file_size = 0;
    }
    // find free blocks for new data for file
    int free_block = find_free_blocks_of_size(mountedBitmap, num_blocks);
    file->file_index = free_block;
    if (free_block == -2)
    {
        fprintf(stderr, "Error: No free blocks available.\n");
        return FREE_BLOCK_ERROR;
    }
    unsigned char fileContent[BLOCKSIZE];
    if (readBlock(disk, free_block, fileContent) == -1)
    {
        fprintf(stderr, "Error: Unable to read file content from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    // write the data (which is 4 less than blocksize because 4 bytes used for metadata)
    int i;
    int remaining_size = size;
    int chunk_size = 252;
    int offset = 4;
    // calculate the block number of next block to link blocks in a file system
    int next_block = free_block + 1;
    int blocks_written = 1;
    while (remaining_size > 0)
    {
        int current_chunk_size = (remaining_size < chunk_size) ? remaining_size : chunk_size;
        printf("current chunk size is %d\n", current_chunk_size);
        for (i = 0; i < current_chunk_size; i++)
        {
            fileContent[offset + i] = buffer[i];
        }
        // offset += current_chunk_size;
        remaining_size -= current_chunk_size;
        printf("remaining size is %d\n", remaining_size);

        // Fill the remaining space in the block with 0x00 if current_chunk_size is less than 252
        if (current_chunk_size < chunk_size)
        {
            for (i = current_chunk_size; i < chunk_size; i++)
            {
                fileContent[offset + i] = 0x00;
                // printf("i is %d\n", i+offset);
            }
        }
        if (remaining_size != 0)
        {
            if (next_block > 255)
            {
                fprintf(stderr, "next block size needs to be less than 255 to fit on byte.\n");
                closeDisk(disk);
                return -4; // make an error code
            }
            // set the link to next block for file data if there is still more data to be written
            fileContent[2] = (unsigned char)next_block;
        }

        // write the modified fileContent back to disk
        if (writeBlock(disk, free_block, fileContent) == -1)
        {
            fprintf(stderr, "Error: Unable to write file content to disk.\n");
            closeDisk(disk);
            return WRITE_ERROR;
        }
        // Print the block data written in bytes
        printf("Block %d data written: ", blocks_written++);
        for (i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02x ", fileContent[i]);
        }
        printf("\n");
    }
    file->file_size = size;
    printf("after editing file size\n");
    unsigned char inode[BLOCKSIZE];
    if (readBlock(disk, file->inode_index, inode) == -1)
    {
        fprintf(stderr, "Error: Unable to read inode from disk.\n");
        closeDisk(disk);
        return DISK_READ_ERROR;
    }
    printf("block read\n");
    if (free_block > 255)
    {
        fprintf(stderr, "next block size needs to be less than 255 to fit on byte.\n");
        closeDisk(disk);
        return -4; // make an error code
    }
    // set the pointer to beginning of file in inode block
    inode[2] = (unsigned char)free_block;
    if (size > 65535)
    {
        fprintf(stderr, "file size needs to be less than 65535 to fit on 2 bytes.\n");
        closeDisk(disk);
        return -4; // make an error code
    }
    // split size into two unsigned char bytes
    unsigned char size_byte1 = (unsigned char)(size >> 8);
    unsigned char size_byte2 = (unsigned char)size;

    // write size bytes to index 12 and 13 in inode
    inode[13] = size_byte1;
    inode[14] = size_byte2;

    // write updated inode back to disk
    printf("writing inode back to disk\n");
    printf("inode_index is %d\n", file->inode_index);
    if (writeBlock(disk, file->inode_index, inode) == -1)
    {
        fprintf(stderr, "Error: Unable to write inode to disk.\n");
        closeDisk(disk);
        return WRITE_ERROR;
    }

    // set write block in terms of disk index
    // int write_offset = free_block * 256;
    // int data_written = 0;
    // while (data_written <= size)
    // {
    //     if (lseek(disk, write_offset, SEEK_SET) == -1)
    //     {
    //         fprintf(stderr, "Error: Unable to seek to root directory block.\n");
    //         closeDisk(disk);
    //         return SEEK_ERROR;
    //     }
    //     unsigned char data = 0x03; // 3 for file extent block
    //     if (write(disk, &data, sizeof(data)) != sizeof(data))
    //     {
    //         fprintf(stderr, "Error: Unable to write physical data.\n");
    //         return WRITE_ERROR;
    //     }
    //     // index to the where data starts (index 4)
    //     if (lseek(disk, 3, SEEK_CUR) == -1)
    //     {
    //         fprintf(stderr, "Error: Unable to offset file descriptor pointer.\n");
    //         return SEEK_ERROR;
    //     }
    //     // write the data (which is 4 less than blocksize because 4 bytes used for metadata)
    //     if (write(disk, buffer, BLOCKSIZE - 4) != BLOCKSIZE - 4)
    //     {
    //         fprintf(stderr, "Error: Unable to write physical data.\n");
    //         return WRITE_ERROR;
    //     }
    //     data_written += BLOCKSIZE - 4;
    //     if (data_written < size)
    //     {
    //         if (lseek(disk, write_offset + 2, SEEK_SET) == -1)
    //         {
    //             fprintf(stderr, "Error: Unable to seek to next block position.\n");
    //             closeDisk(disk);
    //             return SEEK_ERROR;
    //         }
    //         // write block index divided by 256 so it takes less bits to store
    //         uint16_t next_block = (uint16_t)(write_offset / 256 + 1);
    //         if (write(disk, &next_block, sizeof(next_block)) != sizeof(next_block))
    //         {
    //             fprintf(stderr, "Error: Unable to write next block index data.\n");
    //             return WRITE_ERROR;
    //         }
    //     }
    //     write_offset += BLOCKSIZE;
    // }
    printf("returning in this function\n");
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
    // Figure out what block the file pointer is in (file pointer = fileindex + offset)
    int start_pointer = file->file_index;
    int file_pointer = (file->offset / 252 * 256) + file->offset % 252;
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

// EXTRA CREDIT :,)

// Timestamps (10%)
time_t tfs_readFileInfo(fileDescriptor FD)
{
    /* returns the file’s creation time or all info
        should be stored on the INODE*/

    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    if (file == NULL)
    {
        fprintf(stderr, "Error: File not found.\n");
        return FILE_NOT_FOUND_ERROR;
    }
    int inode_ind = file->inode_index;
    printf("inode index is %d\n", inode_ind);
    unsigned char inodeBlock[BLOCKSIZE];
    // Print the contents of the inodeBlock
    printf("inodeBlock contents in time thing: ");
    for (int i = 0; i < BLOCKSIZE; i++)
    {
        printf("%02x ", inodeBlock[i]);
    }
    printf("\n");
    readBlock(disk, inode_ind, inodeBlock);
    // Read the unsigned bytes from inodeBlock[15] to inodeBlock[18]
    unsigned char hour_bytes[4];
    for (int i = 0; i < 4; i++)
    {
        hour_bytes[i] = inodeBlock[15 + i];
    }
    unsigned char min_bytes[4];
    for (int i = 0; i < 4; i++)
    {
        hour_bytes[i] = inodeBlock[19 + i];
    }
    unsigned char sec_bytes[4];
    for (int i = 0; i < 4; i++)
    {
        hour_bytes[i] = inodeBlock[23 + i];
    }
    // Convert the bytes to local_time->tm_hour
    struct tm local_time;
    local_time.tm_hour = *((int *)hour_bytes);
    local_time.tm_min = *((int *)min_bytes);
    local_time.tm_sec = *((int *)sec_bytes);
    printf("AAA Current local time: %02d:%02d:%02d\n", local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

    // Print the local_time->tm_hour
    // printf("Local time hour: %d\n", local_time.tm_hour);

    // time_t creation_time = *((time_t *)(inodeBlock[15]));
    // printf("File creation time: %s\n", ctime(&creation_time));

    // time_t modification_time = *((time_t *)(inodeBlock[24]));
    // printf("File creation time: %s\n", ctime(&modification_time));

    // time_t access_time = *((time_t *)(inodeBlock[33]));
    // printf("File creation time: %s\n", ctime(&access_time));

    // Return -1 if file not found
    return 0;
}

// Directory listing and file renaming (10%)
int tfs_rename(fileDescriptor FD, char *newName)
{
    /* renames a file. New name should be passed in. File has to be open. */
    FileEntry *file = findFileEntryByFD(openFileTable, FD);
    if (file == NULL)
    {
        fprintf(stderr, "Error: File not found.\n");
        return FILE_NOT_FOUND_ERROR;
    }
    strcpy(file->filename, newName);
    printf("File renamed successfully.\n");
    return 1;
}

int tfs_readdir()
{
    /* lists all the files and directories on the disk, print the
    list to stdout -- Note: if you don’t have hierarchical directories, this just reads
    the root directory aka “all files” */
    int j;
    if (!mounted)
    {
        fprintf(stderr, "Error: No file system mounted.\n");
        return MOUNTED_ERROR;
    }
    unsigned char rootDirectory[BLOCKSIZE];
    readBlock(disk, 1, rootDirectory);
    for (int i = 4; i < 251; i += 2)
    {
        // need two bytes to write up to block 65535 for inodes
        int value = (rootDirectory[i] << 8) | rootDirectory[i + 1];
        // if value is 0 (unallocated) then thats where next inode mapping will be
        if (value == 0)
        {
            break;
        }
        printf("value is %d\n", value);
        unsigned char inodeBlock[BLOCKSIZE];
        readBlock(disk, value, inodeBlock);
        char filename[9];
        printf("Inode contents: ");
        for (int i = 0; i < BLOCKSIZE; i++)
        {
            printf("%02x ", inodeBlock[i]);
        }
        printf("\n");
        for (j = 0; j < 8 && inodeBlock[j + 4] != 0x00; j++)
        {
            filename[j] = inodeBlock[j + 4];
        }
        filename[j] = '\0'; // Null-terminate the string
        printf("File name: %s\n", filename);
    }

    return 1;
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
    char fileContent[] = "Test file dataelllu333333333312345678765456787656787656787656787656787656781234567890123456789123456789123456789012345678912345678923456789012345678912345678902345678902345678912345678902345678923456789345654676567j98938242331513513531534531513534534567867876789878978787678767876567867865676567876";
    printf("Length of fileContent: %zu\n", strlen(fileContent));
    char *ptr = fileContent;
    int fd = tfs_openFile("testfile");
    tfs_readFileInfo(fd);
    // printf("fd is %d for testfile\n", fd);
    // int len = strlen(fileContent);
    // tfs_writeFile(fd, ptr, len);
    // printf("file written correctly\n");
    // int fd2 = tfs_openFile("testfil3");
    // char testData[] = "Test file d";
    // char *newptr = testData;
    // tfs_writeFile(fd, newptr, 12);
    // tfs_writeFile(fd2, newptr, 12);
    // tfs_writeFile(fd2, ptr, len);
    // tfs_writeFile(fd, ptr, len);
    // tfs_openFile("testfile");
    // tfs_openFile("testfil3");
    // tfs_openFile("e");
    // tfs_readFileInfo(fd);
    // printf("TinyFS file system created successfully.\n");
    // tfs_readdir();
    return 1;
}