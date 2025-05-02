#ifndef FS_HELPERS_H
#define FS_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include "fs.h" // Include the main fs.h

// Inode structure (32 bytes) - needs to be accessible to helper functions
typedef struct
{
    uint8_t valid;                  // 0 if free, 1 if allocated
    uint32_t size;                  // File size in bytes
    uint32_t direct_blocks[4];      // Direct block pointers
    uint32_t indirect_block;        // Single indirect block pointer
    uint32_t double_indirect_block; // Double indirect block pointer
} inode_t;

// Function declarations for helper functions
int read_inode(int inode_num, inode_t *inode);
int write_inode(int inode_num, inode_t *inode);
int find_free_block(void);
void free_block(int block_num);
int get_block_for_offset(inode_t *inode, int offset, bool allocate);
int initialize_block(int block_num);

#endif // FS_HELPERS_H
