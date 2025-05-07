#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include "vdisk.h"

// Structure definitions
typedef struct
{
    uint8_t magic[16];         // Magic # for SSFS
    uint32_t num_blocks;       // Total # of blocks
    uint32_t num_inode_blocks; // Number of inode blocks
    uint32_t block_size;       // Block size in bytes (1024)
} superblock_t;

typedef struct
{
    uint8_t valid;                  // 0 if free, 1 if allocated
    uint32_t size;                  // File size in bytes
    uint32_t direct_blocks[4];      // Direct block pointers
    uint32_t indirect_block;        // Single indirect block pointer
    uint32_t double_indirect_block; // Double indirect block pointer
} inode_t;

// Constants
#define BLOCK_SIZE 1024
#define INODE_SIZE 32
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define MAGIC_NUMBER "\xf0\x55\x4c\x49\x45\x47\x45\x49\x4e\x46\x4f\x30\x39\x34\x30\x0f"

// Global variables (extern)
extern bool disk_mounted;
extern DISK disk;
extern superblock_t superblock;
extern uint32_t *block_bitmap;
extern char *mounted_disk;

// Function declarations
int read_inode(int inode_num, inode_t *inode, bool bypass_mount_check);
int write_inode(int inode_num, inode_t *inode);
void free_block(int block_num);
int find_free_block(void);
int get_block_for_offset(inode_t *inode, int offset, bool allocate);

#endif
