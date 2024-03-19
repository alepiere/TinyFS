#include "bitmap.h"
#include <stdlib.h>

// funcion to initialize bit map
Bitmap* create_bitmap( int memory_size, int block_size) {
    Bitmap *bitmap = (Bitmap *)malloc(sizeof(Bitmap));
    if (bitmap != NULL) {
        // can store 8 blocks in the 8 bits in one byte and 
        bitmap->bitmap_size = ((memory_size / block_size) + 7) / 8;
        bitmap->block_size = block_size;
        bitmap->num_blocks = memory_size / block_size;
        printf("bitmap size: %d\n", bitmap->bitmap_size);
        printf("block_size: %d\n", bitmap->block_size);
        printf("num_blocks: %d\n", bitmap->num_blocks);
        // Allocate memory for free blocks by number of bytes
        bitmap->free_blocks = (uint8_t*)malloc((bitmap->num_blocks + 7) / 8);
        if ((bitmap->free_blocks) == NULL) {
            free(bitmap);
            return NULL;
        }
        initialize_free_blocks(bitmap);
        allocate_block(bitmap, 1); // directory block goes here
        free_block(bitmap, 0);
        int free_block = find_free_blocks_of_size(bitmap, 12);
        printf("Free block: %d\n", free_block);
    }
    return bitmap;
}

// function to initialize the free blocks
void initialize_free_blocks(Bitmap *bitmap) {
    int num_blocks = bitmap->num_blocks;
    int num_bytes = (num_blocks + 7) / 8;
    for (int i = 0; i < num_bytes; i++) {
        bitmap->free_blocks[i] = 0xFF; // All blocks are set to 1 indicating they are free
    }
}

// function to check if block is free
bool is_block_free(Bitmap *bitmap, int block_index) {
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    return (bitmap->free_blocks[byte_index] >> bit_index) & 1;
}

// function to mark block as allocated
void allocate_block(Bitmap *bitmap, int block_index) {
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    bitmap->free_blocks[byte_index] &= ~(1 << bit_index); // Set the block to 0 to indicate allocated
}

// function to free a block
void free_block(Bitmap *bitmap, int block_index) {
    int byte_index = block_index / 8;
    int bit_index = block_index % 8;
    bitmap->free_blocks[byte_index] |= (1 << bit_index); // Set the block to 1 to indicate now free
}

// function to see if there are contigious blocks of memory of a set size
int find_free_blocks_of_size(Bitmap *bitmap, int block_size) {
    int free_blocks = 0;
    int num_blocks = bitmap->num_blocks;
    for (int i = 0; i < num_blocks; i++) {
        if (is_block_free(bitmap, i)) {
            free_blocks++;
            if (free_blocks == block_size) {
                return i - block_size + 1;
            }
        } else {
            free_blocks = 0;
        }
    }
    return -2; // make valuable error code for no free blocks found
}

