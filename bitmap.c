#include "bitmap.h"
#include <stdlib.h>

// funcion to initialize bit map
Bitmap *create_bitmap(int bitmap_size, int num_blocks, unsigned char *free_blocks)
{
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (bitmap != NULL)
    {
        // can store 8 blocks in the 8 bits in one byte and
        bitmap->bitmap_size = bitmap_size;
        bitmap->num_blocks = num_blocks;
        printf("bitmap size: %d\n", bitmap->bitmap_size);
        printf("num_blocks: %d\n", bitmap->num_blocks);
        // Allocate memory for free blocks by number of bytes
        //Basically have an optional parameter to pass in freeblocks if there already is one and if not make one
        if (free_blocks == NULL){
            bitmap->free_blocks = (unsigned char *)malloc((bitmap->num_blocks + 7) / 8);
            initialize_free_blocks(bitmap);
            allocate_block(bitmap, 0); // super block goes here
            allocate_block(bitmap, 1); // directory block goes here
        }
        else{
            //initalization steps
            bitmap->free_blocks = free_blocks;
        }
        if ((bitmap->free_blocks) == NULL)
        {
            free(bitmap);
            return NULL;
        }
        // int free_block = find_free_blocks_of_size(bitmap, 12);
        // printf("Free block: %d\n", free_block);
    }
    return bitmap;
}

// function to initialize the free blocks
void initialize_free_blocks(Bitmap *bitmap)
{
    int num_blocks = bitmap->num_blocks;
    int num_bytes = (num_blocks + 7) / 8;
    for (int i = 0; i < num_bytes; i++)
    {
        bitmap->free_blocks[i] = 0xFF; // All blocks are set to 1 indicating they are free
    }
}

// function to check if block is free
bool is_block_free(Bitmap *bitmap, int block_index)
{
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    return (bitmap->free_blocks[byte_index] >> bit_index) & 1;
}

// function to mark block as allocated
void allocate_block(Bitmap *bitmap, int block_index)
{
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    bitmap->free_blocks[byte_index] &= ~(1 << bit_index); // Set the block to 0 to indicate allocated
}

// function to free a block
void free_block(Bitmap *bitmap, int block_index)
{
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    bitmap->free_blocks[byte_index] |= (1 << bit_index); // Set the block to 1 to indicate now free
}

void free_num_blocks(Bitmap *bitmap, int start_block_index, int num_blocks)
{
    for (int i = 0; i < num_blocks; i++)
    {
        int block_index = start_block_index + i;
        int byte_index = block_index / 8;
        int bit_index = block_index % 8;
        bitmap->free_blocks[byte_index] |= (1 << bit_index); // Set the block to 1 to indicate now free
    }
}

// function to see if there are contigious blocks of memory of a set size
int find_free_blocks_of_size(Bitmap *bitmap, int block_size)
{
    int free_blocks = 0;
    int num_blocks = bitmap->num_blocks;
    for (int i = 0; i < num_blocks; i++)
    {
        if (is_block_free(bitmap, i))
        {
            free_blocks++;
            if (free_blocks == block_size)
            {
                return i - block_size + 1;
            }
        }
        else
        {
            free_blocks = 0;
        }
    }
    return -2; // make valuable error code for no free blocks found
}
