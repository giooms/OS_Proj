#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "include/fs.h"
#include "include/vdisk.h"
#include "include/error.h"

#define BLOCK_SIZE 1024
#define INODE_SIZE 32
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define MAGIC_NUMBER "\xf0\x55\x4c\x49\x45\x47\x45\x49\x4e\x46\x4f\x30\x39\x34\x30\x0f"

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
static DISK disk;  // Defined in vdisk.h
static superblock_t superblock;
static uint32_t *block_bitmap = NULL;  // For tracking free blocks
static char *mounted_disk = NULL;

/* Implementation of core functions */

int format(char *disk_name, int inodes) {
    // 1. Check if disk is already mounted
    // 2. Open disk with vdisk_on
    // 3. Initialize superblock with magic number
    // 4. Calculate number of inode blocks needed
    // 5. Write superblock to disk
    // 6. Initialize inode blocks to zeros
    // 7. Sync and close disk
}

int mount(char *disk_name) {
    // 1. Check if disk is already mounted
    // 2. Open disk with vdisk_on
    // 3. Read superblock
    // 4. Verify magic number
    // 5. Initialize block bitmap (mark used blocks)
    // 6. Set mounted_disk and disk_mounted flag
}

int unmount(void) {
    // 1. Check if disk is mounted
    // 2. Sync disk with vdisk_sync
    // 3. Free resources (block_bitmap, mounted_disk)
    // 4. Call vdisk_off
    // 5. Clear disk_mounted flag
}

int create(void) {
    // 1. Check if disk is mounted
    // 2. Find first available inode
    // 3. Initialize inode (valid=1, size=0)
    // 4. Write inode to disk
    // 5. Return inode number
}

int delete(int inode_num) {
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Free all allocated blocks (direct, indirect, double-indirect)
    // 4. Mark inode as invalid (valid=0)
    // 5. Write inode to disk
}

int stat(int inode_num) {
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Return inode.size
}

int read(int inode_num, uint8_t *data, int len, int offset) {
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Adjust length if needed (don't read past file end)
    // 4. For each block needed:
    //    - Calculate which block contains this offset
    //    - Read block
    //    - Copy appropriate portion to user buffer
    // 5. Return total bytes read
}

int write(int inode_num, uint8_t *data, int len, int offset) {
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. If offset > file size, zero-fill the gap
    // 4. For each block needed:
    //    - Calculate/allocate block for this offset
    //    - Read block if not overwriting entirely
    //    - Write data to block
    //    - Write block to disk
    // 5. Update inode size if needed
    // 6. Write inode to disk
    // 7. Return bytes written
}

/* Helper functions */

// Helper function to read an inode from disk
static int read_inode(int inode_num, inode_t *inode) {
    if (!disk_mounted) return E_DISK_NOT_MOUNTED;
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK) 
        return E_INVALID_INODE;
    
    // Calculate block number and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;
    
    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0) return result;
    
    // Copy inode data
    memcpy(inode, block + offset, INODE_SIZE);
    
    return 0;
}

// Helper function to write an inode to disk
static int write_inode(int inode_num, inode_t *inode) {
    if (!disk_mounted) return E_DISK_NOT_MOUNTED;
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK) 
        return E_INVALID_INODE;
    
    // Calculate block number and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;
    
    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0) return result;
    
    // Update inode data in the block
    memcpy(block + offset, inode, INODE_SIZE);
    
    // Write the block back to disk
    result = vdisk_write(&disk, block_num, block);
    if (result != 0) return result;
    
    return 0;
}

// Helper function to find a free block
static int find_free_block() {
    if (!disk_mounted) return E_DISK_NOT_MOUNTED;
    
    // Start from the first data block (after superblock and inode blocks)
    int first_data_block = 1 + superblock.num_inode_blocks;
    
    // Search for the first available block using first-available strategy
    for (int i = first_data_block; i < superblock.num_blocks; i++) {
        if (block_bitmap[i] == 0) {
            // Mark the block as used
            block_bitmap[i] = 1;
            return i;
        }
    }
    
    return E_OUT_OF_SPACE;  // No free blocks available
}

// Helper function to mark a block as free
static void free_block(int block_num) {
    if (disk_mounted && block_num > 0 && block_num < superblock.num_blocks) {
        // Mark the block as free in the bitmap
        block_bitmap[block_num] = 0;
    }
}

// Helper function to get block number for a specific file offset
static int get_block_for_offset(inode_t *inode, int offset, bool allocate) {
    if (!disk_mounted) return E_DISK_NOT_MOUNTED;
    if (offset < 0) return E_INVALID_OFFSET;
    
    // Calculate which block this offset falls into
    int block_index = offset / BLOCK_SIZE;
    
    // Direct blocks (0-3)
    if (block_index < 4) {
        if (inode->direct_blocks[block_index] == 0 && allocate) {
            // Need to allocate a new block
            int new_block = find_free_block();
            if (new_block < 0) return new_block;  // Error finding free block
            
            // Initialize the new block with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            inode->direct_blocks[block_index] = new_block;
        }
        return inode->direct_blocks[block_index];
    }
    
    // Indirect blocks (4-259)
    block_index -= 4;
    if (block_index < POINTERS_PER_BLOCK) {
        // Check if we have an indirect block
        if (inode->indirect_block == 0) {
            if (!allocate) return 0;  // No block and not allocating
            
            // Allocate new indirect block
            int new_block = find_free_block();
            if (new_block < 0) return new_block;
            
            // Initialize with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            inode->indirect_block = new_block;
        }
        
        // Read the indirect block
        uint8_t indirect_block[BLOCK_SIZE];
        int result = vdisk_read(&disk, inode->indirect_block, indirect_block);
        if (result != 0) return result;
        
        uint32_t *pointers = (uint32_t *)indirect_block;
        
        // Check if we need to allocate a new data block
        if (pointers[block_index] == 0 && allocate) {
            int new_block = find_free_block();
            if (new_block < 0) return new_block;
            
            // Initialize with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            pointers[block_index] = new_block;
            
            // Write the updated indirect block back
            result = vdisk_write(&disk, inode->indirect_block, indirect_block);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
        }
        
        return pointers[block_index];
    }
    
    // Double indirect blocks (260+)
    block_index -= POINTERS_PER_BLOCK;
    if (block_index < POINTERS_PER_BLOCK * POINTERS_PER_BLOCK) {
        // Check if we have a double indirect block
        if (inode->double_indirect_block == 0) {
            if (!allocate) return 0;  // No block and not allocating
            
            // Allocate new double indirect block
            int new_block = find_free_block();
            if (new_block < 0) return new_block;
            
            // Initialize with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            inode->double_indirect_block = new_block;
        }
        
        // Read the double indirect block
        uint8_t double_indirect_block[BLOCK_SIZE];
        int result = vdisk_read(&disk, inode->double_indirect_block, double_indirect_block);
        if (result != 0) return result;
        
        uint32_t *pointers = (uint32_t *)double_indirect_block;
        
        // Calculate which indirect block and entry within that block
        int indirect_index = block_index / POINTERS_PER_BLOCK;
        int entry_index = block_index % POINTERS_PER_BLOCK;
        
        // Check if we need to allocate a new indirect block
        if (pointers[indirect_index] == 0 && allocate) {
            int new_block = find_free_block();
            if (new_block < 0) return new_block;
            
            // Initialize with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            pointers[indirect_index] = new_block;
            
            // Write the updated double indirect block back
            result = vdisk_write(&disk, inode->double_indirect_block, double_indirect_block);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
        } else if (pointers[indirect_index] == 0) {
            return 0; // No block and not allocating
        }
        
        // Read the indirect block
        uint8_t indirect_block[BLOCK_SIZE];
        result = vdisk_read(&disk, pointers[indirect_index], indirect_block);
        if (result != 0) return result;
        
        uint32_t *sub_pointers = (uint32_t *)indirect_block;
        
        // Check if we need to allocate a new data block
        if (sub_pointers[entry_index] == 0 && allocate) {
            int new_block = find_free_block();
            if (new_block < 0) return new_block;
            
            // Initialize with zeros
            uint8_t zeros[BLOCK_SIZE] = {0};
            result = vdisk_write(&disk, new_block, zeros);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
            
            sub_pointers[entry_index] = new_block;
            
            // Write the updated indirect block back
            result = vdisk_write(&disk, pointers[indirect_index], indirect_block);
            if (result != 0) {
                free_block(new_block);
                return result;
            }
        }
        
        return sub_pointers[entry_index];
    }
    
    return E_INVALID_OFFSET; // Offset too large for this file system
}

// Helper function to check if disk is mounted
static int check_mounted() {
    if (!disk_mounted) {
        return E_DISK_NOT_MOUNTED;
    }
    return 0;
}

// Helper function to initialize a block with zeros
static int initialize_block(int block_num) {
    if (!disk_mounted) return E_DISK_NOT_MOUNTED;
    
    uint8_t zeros[BLOCK_SIZE] = {0};
    int result = vdisk_write(&disk, block_num, zeros);
    return result;
}