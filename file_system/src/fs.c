#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "include/fs.h"
#include "include/vdisk.h"
#include "include/error.h"

/* File system data structures */

// Super block structure
typedef struct {
    uint8_t magic[16];  // Magic number for SSFS
    uint32_t num_blocks;  // Total number of blocks
    uint32_t num_inode_blocks;  // Number of inode blocks
    uint32_t block_size;  // Block size in bytes (1024)
} superblock_t;

// Inode structure (32 bytes)
typedef struct {
    uint8_t valid;  // 0 if free, 1 if allocated
    uint32_t size;  // File size in bytes
    uint32_t direct_blocks[4];  // Direct block pointers
    uint32_t indirect_block;  // Single indirect block pointer
    uint32_t double_indirect_block;  // Double indirect block pointer
} inode_t;

// File system state
static bool disk_mounted = false;
static superblock_t superblock;
static uint32_t *block_bitmap = NULL;  // For tracking free blocks
static char *mounted_disk = NULL;

/* Implementation of core functions */

int format(char *disk_name, int inodes) {
    // Implementation of format function
    // ...
}

int mount(char *disk_name) {
    // Implementation of mount function
    // ...
}

int unmount(void) {
    // Implementation of unmount function
    // ...
}

int create(void) {
    // Implementation of create function
    // ...
}

int delete(int inode_num) {
    // Implementation of delete function
    // ...
}

int stat(int inode_num) {
    // Implementation of stat function
    // ...
}

int read(int inode_num, uint8_t *data, int len, int offset) {
    // Implementation of read function
    // ...
}

int write(int inode_num, uint8_t *data, int len, int offset) {
    // Implementation of write function
    // ...
}

/* Helper functions */
// Add helper functions as needed...