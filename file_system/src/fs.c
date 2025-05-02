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
typedef struct
{
    uint8_t magic[16];         // Magic number for SSFS
    uint32_t num_blocks;       // Total number of blocks
    uint32_t num_inode_blocks; // Number of inode blocks
    uint32_t block_size;       // Block size in bytes (1024)
} superblock_t;

// Inode structure (32 bytes)
typedef struct
{
    uint8_t valid;                  // 0 if free, 1 if allocated
    uint32_t size;                  // File size in bytes
    uint32_t direct_blocks[4];      // Direct block pointers
    uint32_t indirect_block;        // Single indirect block pointer
    uint32_t double_indirect_block; // Double indirect block pointer
} inode_t;

// File system state
static bool disk_mounted = false;
static DISK disk; // Defined in vdisk.h
static superblock_t superblock;
static uint32_t *block_bitmap = NULL; // For tracking free blocks
static char *mounted_disk = NULL;

/* Implementation of core functions */

int format(char *disk_name, int inodes)
{
    // Precondition: Check if disk already mounted
    if (disk_mounted)
    {
        return E_DISK_ALREADY_MOUNTED;
    }

    // Precondition: Adjust inodes to be at least 1
    if (inodes <= 0)
    {
        inodes = 1;
    }

    // Open the disk image file
    DISK format_disk;
    int result = vdisk_on(disk_name, &format_disk);
    if (result != 0)
    {
        return result; // Return error from vdisk_on
    }

    // Get required nb of inode blocks (ceiling division)
    int num_inode_blocks = (inodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    if (num_inode_blocks <= 0)
    {
        num_inode_blocks = 1;
    }

    // Calculate total number of blocks available on the disk
    uint32_t total_blocks = format_disk.size_in_sectors;

    // Ensure enough space for at least one data block
    //  +1 to account for the superblock!
    if (num_inode_blocks + 1 >= total_blocks)
    {
        vdisk_off(&format_disk);
        return E_OUT_OF_SPACE; // can't fit inode b + superb + (>=1) one data b
    }

    // Initialize superblock
    superblock_t sb;
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks = total_blocks;
    sb.num_inode_blocks = num_inode_blocks;
    sb.block_size = BLOCK_SIZE;

    // Write superblock to Block 0
    uint8_t block_buffer[BLOCK_SIZE] = {0};
    memcpy(block_buffer, &sb, sizeof(superblock_t));
    result = vdisk_write(&format_disk, 0, block_buffer);
    if (result != 0)
    {
        vdisk_off(&format_disk);
        return result;
    }

    // Initialize inode blocks (starting at block idx 1)
    memset(block_buffer, 0, BLOCK_SIZE); // Zero out the buffer
    for (int i = 1; i <= num_inode_blocks; i++)
    {
        result = vdisk_write(&format_disk, i, block_buffer);
        if (result != 0)
        {
            vdisk_off(&format_disk);
            return result;
        }
    }

    // Sync to ensure all changes are written to disk
    result = vdisk_sync(&format_disk);
    if (result != 0)
    {
        vdisk_off(&format_disk);
        return result;
    }

    // Close the disk
    vdisk_off(&format_disk);

    return 0; // Success
}

int mount(char *disk_name)
{
    // 1. Check if disk is already mounted
    if (disk_mounted)
    {
        return E_DISK_ALREADY_MOUNTED;
    }

    // 2. Open the disk image file
    int result = vdisk_on(disk_name, &disk);
    if (result != 0)
    {
        return result; // Return error from vdisk_on
    }

    // 3. Read the superblock (Block 0)
    uint8_t block_buffer[BLOCK_SIZE];
    result = vdisk_read(&disk, 0, block_buffer);
    if (result != 0)
    {
        vdisk_off(&disk);
        return result;
    }

    // Copy superblock data
    memcpy(&superblock, block_buffer, sizeof(superblock_t));

    // 4. Verify the magic number
    if (memcmp(superblock.magic, MAGIC_NUMBER, 16) != 0)
    {
        vdisk_off(&disk);
        return E_CORRUPT_DISK;
    }

    // 5. Allocate memory for the block bitmap
    block_bitmap = (uint32_t *)calloc(superblock.num_blocks, sizeof(uint32_t));
    if (block_bitmap == NULL)
    {
        vdisk_off(&disk);
        return E_OUT_OF_SPACE; // Using this error code for memory allocation failure
    }

    // 6. Initialize block bitmap - mark superblock and inode blocks as used
    block_bitmap[0] = 1; // Superblock (Block 0)

    // Mark all inode blocks as used (from Block 1 up to num_inode_blocks)
    for (uint32_t i = 1; i <= superblock.num_inode_blocks; i++)
    {
        block_bitmap[i] = 1;
    }

    // Scan all inodes to mark data blocks as used if allocated
    for (int i = 0; i < superblock.num_inode_blocks * INODES_PER_BLOCK; i++)
    {
        inode_t inode;
        int result = read_inode(i, &inode);
        if (result != 0)
        {
            free(block_bitmap);
            block_bitmap = NULL;
            vdisk_off(&disk);
            return result; // Return error immediately if read_inode fails
        }

        // Only process valid inodes
        if (inode.valid)
        {
            // Mark direct blocks
            for (int j = 0; j < 4; j++)
            {
                if (inode.direct_blocks[j] != 0)
                {
                    block_bitmap[inode.direct_blocks[j]] = 1;
                }
            }

            // Process indirect block if present
            if (inode.indirect_block != 0)
            {
                block_bitmap[inode.indirect_block] = 1;

                // Read the indirect block
                uint8_t indirect_block[BLOCK_SIZE];
                result = vdisk_read(&disk, inode.indirect_block, indirect_block);
                if (result != 0)
                {
                    free(block_bitmap);
                    block_bitmap = NULL;
                    vdisk_off(&disk);
                    return result;
                }

                // Mark all non-zero entries in the indirect block
                uint32_t *pointers = (uint32_t *)indirect_block;
                for (int k = 0; k < POINTERS_PER_BLOCK; k++)
                {
                    if (pointers[k] != 0)
                    {
                        block_bitmap[pointers[k]] = 1;
                    }
                }
            }

            // Process double indirect block if present
            if (inode.double_indirect_block != 0)
            {
                block_bitmap[inode.double_indirect_block] = 1;

                // Read the double indirect block
                uint8_t double_indirect_block[BLOCK_SIZE];
                result = vdisk_read(&disk, inode.double_indirect_block, double_indirect_block);
                if (result != 0)
                {
                    free(block_bitmap);
                    block_bitmap = NULL;
                    vdisk_off(&disk);
                    return result;
                }

                // Process each indirect block pointer in the double indirect block
                uint32_t *indirect_pointers = (uint32_t *)double_indirect_block;
                for (int j = 0; j < POINTERS_PER_BLOCK; j++)
                {
                    if (indirect_pointers[j] != 0)
                    {
                        // Mark the indirect block as used
                        block_bitmap[indirect_pointers[j]] = 1;

                        // Read this indirect block
                        uint8_t curr_indirect_block[BLOCK_SIZE];
                        result = vdisk_read(&disk, indirect_pointers[j], curr_indirect_block);
                        if (result != 0)
                        {
                            free(block_bitmap);
                            block_bitmap = NULL;
                            vdisk_off(&disk);
                            return result;
                        }

                        // Mark all non-zero entries in this indirect block
                        uint32_t *data_pointers = (uint32_t *)curr_indirect_block;
                        for (int k = 0; k < POINTERS_PER_BLOCK; k++)
                        {
                            if (data_pointers[k] != 0)
                            {
                                block_bitmap[data_pointers[k]] = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    // 7. Store the disk name
    int name_length = strlen(disk_name) + 1;
    mounted_disk = (char *)malloc(name_length);
    if (mounted_disk == NULL)
    {
        free(block_bitmap);
        block_bitmap = NULL;
        vdisk_off(&disk);
        return E_OUT_OF_SPACE; // Using this error code for memory allocation failure
    }
    strcpy(mounted_disk, disk_name);

    // 8. Set disk_mounted flag
    disk_mounted = true;

    return 0; // Success
}

int unmount(void)
{
    // 1. Check if disk is currently mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Sync any pending changes to disk
    int result = vdisk_sync(&disk);
    // don't check result here, as we want to clean up even if sync fails
    // we check for success in the final return

    // 3. Free memory allocated for block bitmap
    if (block_bitmap != NULL)
    {
        free(block_bitmap);
        block_bitmap = NULL;
    }

    // 4. Free memory allocated for mounted disk name
    if (mounted_disk != NULL)
    {
        free(mounted_disk);
        mounted_disk = NULL;
    }

    // 5. Close the virtual disk
    vdisk_off(&disk);

    // 6. Reset the disk_mounted flag
    disk_mounted = false;

    // Return 0 for success, or the error code from vdisk_sync if it failed
    return (result == 0) ? 0 : result;
}

int create(void)
{
    // 1. Check if disk is mounted
    // 2. Find first available inode
    // 3. Initialize inode (valid=1, size=0)
    // 4. Write inode to disk
    // 5. Return inode number
}

int delete(int inode_num)
{
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Free all allocated blocks (direct, indirect, double-indirect)
    // 4. Mark inode as invalid (valid=0)
    // 5. Write inode to disk
}

int stat(int inode_num)
{
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Return inode.size
}

int read(int inode_num, uint8_t *data, int len, int offset)
{
    // 1. Check if disk is mounted
    // 2. Read inode
    // 3. Adjust length if needed (don't read past file end)
    // 4. For each block needed:
    //    - Calculate which block contains this offset
    //    - Read block
    //    - Copy appropriate portion to user buffer
    // 5. Return total bytes read
}

int write(int inode_num, uint8_t *data, int len, int offset)
{
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
static int read_inode(int inode_num, inode_t *inode)
{
    if (!disk_mounted)
        return E_DISK_NOT_MOUNTED;
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
        return E_INVALID_INODE;

    // Calculate block number and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;

    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0)
        return result;

    // Copy inode data
    memcpy(inode, block + offset, INODE_SIZE);

    return 0;
}

// Helper function to write an inode to disk
static int write_inode(int inode_num, inode_t *inode)
{
    if (!disk_mounted)
        return E_DISK_NOT_MOUNTED;
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
        return E_INVALID_INODE;

    // Calculate block number and offset for the inode
    int block_num = 1 + (inode_num / INODES_PER_BLOCK); // +1 because block 0 is superblock
    int offset = (inode_num % INODES_PER_BLOCK) * INODE_SIZE;

    // Read the block containing the inode
    uint8_t block[BLOCK_SIZE];
    int result = vdisk_read(&disk, block_num, block);
    if (result != 0)
        return result;

    // Update inode data in the block
    memcpy(block + offset, inode, INODE_SIZE);

    // Write the block back to disk
    result = vdisk_write(&disk, block_num, block);
    if (result != 0)
        return result;

    return 0;
}

// Helper function to find a free block
static int find_free_block()
{
    if (!disk_mounted)
        return E_DISK_NOT_MOUNTED;

    // Start from the first data block (after superblock and inode blocks)
    int first_data_block = 1 + superblock.num_inode_blocks;

    // Search for the first available block using first-available strategy
    for (int i = first_data_block; i < superblock.num_blocks; i++)
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
static void free_block(int block_num)
{
    if (disk_mounted && block_num > 0 && block_num < superblock.num_blocks)
    {
        // Mark the block as free in the bitmap
        block_bitmap[block_num] = 0;
    }
}

// Helper function to get block number for a specific file offset
static int get_block_for_offset(inode_t *inode, int offset, bool allocate)
{
    if (!disk_mounted)
        return E_DISK_NOT_MOUNTED;
    if (offset < 0)
        return E_INVALID_OFFSET;

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
                return new_block; // Error finding free block

            // Initialize the new block with zeros
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
    if (block_index < POINTERS_PER_BLOCK)
    {
        // Check if we have an indirect block
        if (inode->indirect_block == 0)
        {
            if (!allocate)
                return 0; // No block and not allocating

            // Allocate new indirect block
            int new_block = find_free_block();
            if (new_block < 0)
                return new_block;

            // Initialize with zeros
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
            return result;

        uint32_t *pointers = (uint32_t *)indirect_block;

        // Check if we need to allocate a new data block
        if (pointers[block_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
                return new_block;

            // Initialize with zeros
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
    if (block_index < POINTERS_PER_BLOCK * POINTERS_PER_BLOCK)
    {
        // Check if we have a double indirect block
        if (inode->double_indirect_block == 0)
        {
            if (!allocate)
                return 0; // No block and not allocating

            // Allocate new double indirect block
            int new_block = find_free_block();
            if (new_block < 0)
                return new_block;

            // Initialize with zeros
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
            return result;

        uint32_t *pointers = (uint32_t *)double_indirect_block;

        // Calculate which indirect block and entry within that block
        int indirect_index = block_index / POINTERS_PER_BLOCK;
        int entry_index = block_index % POINTERS_PER_BLOCK;

        // Check if we need to allocate a new indirect block
        if (pointers[indirect_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
                return new_block;

            // Initialize with zeros
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
            return result;

        uint32_t *sub_pointers = (uint32_t *)indirect_block;

        // Check if we need to allocate a new data block
        if (sub_pointers[entry_index] == 0 && allocate)
        {
            int new_block = find_free_block();
            if (new_block < 0)
                return new_block;

            // Initialize with zeros
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

// Helper function to check if disk is mounted
static int check_mounted()
{
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }
    return 0;
}

// Helper function to initialize a block with zeros
static int initialize_block(int block_num)
{
    if (!disk_mounted)
        return E_DISK_NOT_MOUNTED;

    uint8_t zeros[BLOCK_SIZE] = {0};
    int result = vdisk_write(&disk, block_num, zeros);
    return result;
}