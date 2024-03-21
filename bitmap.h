#ifndef BITMAP_H
#define BITMAP_H

#define MAX_BITMAP_BLOCKS 1888

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    int bitmap_size;        // Size of the bitmap in bytes
    int num_blocks;         // Number of blocks
    unsigned char *free_blocks; // Pointer to dynamically allocated memory for bitmap
} Bitmap;

Bitmap *create_bitmap(int bitmap_size, int num_blocks, unsigned char* free_blocks);
void initialize_free_blocks(Bitmap *bitmap);
bool is_block_free(Bitmap *bitmap, int block_index);
void allocate_block(Bitmap *bitmap, int block_index);
void free_block(Bitmap *bitmap, int block_index);
int find_free_blocks_of_size(Bitmap *bitmap, int block_size);
void free_bitmap(Bitmap *bitmap);

#endif // BITMAP_H