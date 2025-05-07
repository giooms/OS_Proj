#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "include/helpers.h"
#include "include/vdisk.h"
#include "include/error.h"

// File system state (global variables)
bool disk_mounted = false;
DISK disk;
superblock_t superblock;
uint32_t *block_bitmap = NULL; // For tracking free blocks
char *mounted_disk = NULL;

// Helper function to read an inode from disk
// bypass_mount_check: if true, skip the mounted disk check (used only during mount operation)
int read_inode(int inode_num, inode_t *inode, bool bypass_mount_check)
{
    if (!disk_mounted && !bypass_mount_check)
    {
        return E_DISK_NOT_MOUNTED;
    }

    if (inode_num < 0 || (uint32_t)inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // Calculate block # and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;

    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0)
    {
        return result;
    }

    // Copy inode data
    memcpy(inode, block + offset, INODE_SIZE);

    return 0;
}

// Helper function to write an inode to disk
int write_inode(int inode_num, inode_t *inode)
{
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    if (inode_num < 0 || (uint32_t)inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // Calculate block # and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;

    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0)
    {
        return result;
    }

    // Update inode data in the block
    memcpy(block + offset, inode, INODE_SIZE);

    // Write the block back to disk
    result = vdisk_write(&disk, block_num, block);
    if (result != 0)
    {
        return result;
    }

    return 0;
}

// Helper function to find a free block
int find_free_block()
{
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // Start from the first data block (after superblock and inode blocks)
    int first_data_block = 1 + superblock.num_inode_blocks;

    // Search for the first available block using first-available strategy
    for (uint32_t i = (uint32_t)first_data_block; i < superblock.num_blocks; i++)
    {
        if (block_bitmap[i] == 0)
        {
            // Mark the block as used
            block_bitmap[i] = 1;
            return i;
        }
    }

    return E_OUT_OF_SPACE; // No free blocks available
}

// Helper function to mark a block as free
void free_block(int block_num)
{
    if (disk_mounted && block_num > 0 && (uint32_t)block_num < superblock.num_blocks)
    {
        // Mark the block as free in the bitmap
        block_bitmap[block_num] = 0;
    }
}

// Helper function to get block # for a specific file offset
int get_block_for_offset(inode_t *inode, int offset, bool allocate)
{
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }
    if (offset < 0)
    {
        return E_INVALID_OFFSET;
    }

    // Calculate which block this offset falls into
    int block_index = offset / BLOCK_SIZE;

    // Direct blocks (0-3)
    if (block_index < 4)
    {
        if (inode->direct_blocks[block_index] == 0 && allocate)
        {
            // Need to allocate a new block
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block; // Error finding free block
            }

            // Init the new block with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            inode->direct_blocks[block_index] = new_block;
        }
        return inode->direct_blocks[block_index];
    }

    // Indirect blocks (4-259)
    block_index -= 4;
    if ((uint32_t)block_index < POINTERS_PER_BLOCK)
    {
        // Check if we have an indirect block
        if (inode->indirect_block == 0)
        {
            if (!allocate)
            {
                return 0; // No block and not allocating
            }

            // Allocate new indirect block
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block;
            }

            // Init with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            inode->indirect_block = new_block;
        }

        // Read the indirect block
        uint8_t indirect_block[BLOCK_SIZE];
        int result = vdisk_read(&disk, inode->indirect_block, indirect_block);
        if (result != 0)
        {
            return result;
        }

        uint32_t *pointers = (uint32_t *)indirect_block;

        // Check if we need to allocate a new data block
        if (pointers[block_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block;
            }

            // Init with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            pointers[block_index] = new_block;

            // Write the updated indirect block back
            result = vdisk_write(&disk, inode->indirect_block, indirect_block);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }
        }

        return pointers[block_index];
    }

    // Double indirect blocks (260+)
    block_index -= POINTERS_PER_BLOCK;
    if ((uint32_t)block_index < (uint32_t)POINTERS_PER_BLOCK * POINTERS_PER_BLOCK)
    {
        // Check if we have a double indirect block
        if (inode->double_indirect_block == 0)
        {
            if (!allocate)
            {
                return 0; // No block and not allocating
            }

            // Allocate new double indirect block
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block;
            }

            // Init with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            inode->double_indirect_block = new_block;
        }

        // Read the double indirect block
        uint8_t double_indirect_block[BLOCK_SIZE];
        int result = vdisk_read(&disk, inode->double_indirect_block, double_indirect_block);
        if (result != 0)
        {
            return result;
        }

        uint32_t *pointers = (uint32_t *)double_indirect_block;

        // Calculate which indirect block and entry within that block
        int indirect_index = block_index / POINTERS_PER_BLOCK;
        int entry_index = block_index % POINTERS_PER_BLOCK;

        // Check if we need to allocate a new indirect block
        if (pointers[indirect_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block;
            }

            // Init with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            int result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            pointers[indirect_index] = new_block;

            // Write the updated double indirect block back
            result = vdisk_write(&disk, inode->double_indirect_block, double_indirect_block);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }
        }
        else if (pointers[indirect_index] == 0)
        {
            return 0; // No block and not allocating
        }

        // Read the indirect block
        uint8_t indirect_block[BLOCK_SIZE];
        result = vdisk_read(&disk, pointers[indirect_index], indirect_block);
        if (result != 0)
        {
            return result;
        }

        uint32_t *sub_pointers = (uint32_t *)indirect_block;

        // Check if we need to allocate a new data block
        if (sub_pointers[entry_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
            {
                return new_block;
            }

            // Init with 0s
            uint8_t zeros[BLOCK_SIZE] = {0};
            result = vdisk_write(&disk, new_block, zeros);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }

            sub_pointers[entry_index] = new_block;

            // Write the updated indirect block back
            result = vdisk_write(&disk, pointers[indirect_index], indirect_block);
            if (result != 0)
            {
                free_block(new_block);
                return result;
            }
        }

        return sub_pointers[entry_index];
    }

    return E_INVALID_OFFSET; // Offset too large for this file system
}
