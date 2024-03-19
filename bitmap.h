#ifndef BITMAP_H
#define BITMAP_H

#define MAX_BITMAP_BLOCKS 1888

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t bitmap_size;        // Size of the bitmap in bytes
    int32_t block_size;         // Size of each block
    int32_t num_blocks;         // Number of blocks 
    uint8_t *free_blocks;       // Pointer to dynamically allocated memory for bitmap
} Bitmap;

Bitmap* create_bitmap(int memory_size, int block_size);
void initialize_free_blocks(Bitmap *bitmap);
bool is_block_free(Bitmap *bitmap, int block_index);
void allocate_block(Bitmap *bitmap, int block_index);
void free_block(Bitmap *bitmap, int block_index);
int find_free_blocks_of_size(Bitmap *bitmap, int block_size);
void free_bitmap(Bitmap *bitmap);



#endif // BITMAP_H