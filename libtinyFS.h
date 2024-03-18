/* The default size of the disk and file system block */
#define BLOCKSIZE 256
/* Your program should use a 10240 Byte disk size giving you 40 blocks
total. This is a default size. You must be able to support different
possible values */
#define DEFAULT_DISK_SIZE 10240
/* use this name for a default emulated disk file name */
#define DEFAULT_DISK_NAME “tinyFSDisk”
/* use as a special type to keep track of files */
typedef int fileDescriptor;


typedef struct SuperBlock {
    int type;
    int magic;
    int blockSize;
    int nBlocks;
    int rootInode;
    int freeNodes;
} superblock;



//block types
#define EMPTY 0
#define SUPERBLOCK 1
#define INODE 2
#define FILE_EXTENT 3
#define FREE_BLOCK 4