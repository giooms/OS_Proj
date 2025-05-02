#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "include/fs.h"
#include "include/vdisk.h"
#include "include/error.h"
#include "include/fs_helpers.h"

#define BLOCK_SIZE 1024
#define INODE_SIZE 32
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define MAGIC_NUMBER "\xf0\x55\x4c\x49\x45\x47\x45\x49\x4e\x46\x4f\x30\x39\x34\x30\x0f"


/*************************/
/* Data structures       */
/*************************/

// Super block structure
typedef struct
{
    uint8_t magic[16];         // Magic # for SSFS
    uint32_t num_blocks;       // Total # of blocks
    uint32_t num_inode_blocks; // Number of inode blocks
    uint32_t block_size;       // Block size in bytes (1024)
} superblock_t;

// File system state
bool disk_mounted = false;
DISK disk; // Defined in vdisk.h
superblock_t superblock;
uint32_t *block_bitmap = NULL; // For tracking free blocks
char *mounted_disk = NULL;


/*************************/
/* Core functions        */
/*************************/

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

    // Open disk image file
    DISK format_disk;
    int result = vdisk_on(disk_name, &format_disk);
    if (result != 0)
    {
        return result;
    }

    // Get required # of inode blocks (ceiling division)
    int num_inode_blocks = (inodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;
    if (num_inode_blocks <= 0)
    {
        num_inode_blocks = 1;
    }

    // Calculate total # of blocks available on disk
    uint32_t total_blocks = format_disk.size_in_sectors;

    // Ensure enough space for at least one data block
    //  +1 to account for the superblock!
    if (num_inode_blocks + 1 >= total_blocks)
    {
        vdisk_off(&format_disk);
        return E_OUT_OF_SPACE; // can't fit inode b + superb + (>=1) one data b
    }

    // Init superblock
    superblock_t sb;
    memcpy(sb.magic, MAGIC_NUMBER, 16);
    sb.num_blocks = total_blocks;
    sb.num_inode_blocks = num_inode_blocks;
    sb.block_size = BLOCK_SIZE;

    // Write superblock to block 0
    uint8_t block_buffer[BLOCK_SIZE] = {0};
    memcpy(block_buffer, &sb, sizeof(superblock_t));
    result = vdisk_write(&format_disk, 0, block_buffer);
    if (result != 0)
    {
        vdisk_off(&format_disk);
        return result;
    }

    // Init inode blocks (starting at block idx 1)
    memset(block_buffer, 0, BLOCK_SIZE); // zero-out buffer
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

    // Close the disk and return success
    vdisk_off(&format_disk);
    return 0;
}

int mount(char *disk_name)
{
    // 1. Check if disk already mounted
    if (disk_mounted)
    {
        return E_DISK_ALREADY_MOUNTED;
    }

    // 2. Open disk image file
    int result = vdisk_on(disk_name, &disk);
    if (result != 0)
    {
        return result;
    }

    // 3. Read superblock (Block 0) and copy its data
    uint8_t block_buffer[BLOCK_SIZE];
    result = vdisk_read(&disk, 0, block_buffer);
    if (result != 0)
    {
        vdisk_off(&disk);
        return result;
    }

    memcpy(&superblock, block_buffer, sizeof(superblock_t));

    // 4. Verify the magic #
    if (memcmp(superblock.magic, MAGIC_NUMBER, 16) != 0)
    {
        vdisk_off(&disk);
        return E_CORRUPT_DISK;
    }

    // 5. Allocate mem for the block bitmap
    block_bitmap = (uint32_t *)calloc(superblock.num_blocks, sizeof(uint32_t));
    if (block_bitmap == NULL)
    {
        vdisk_off(&disk);
        return E_OUT_OF_SPACE;  // see error.h
    }

    // 6. Init block bitmap - mark superblock and inode blocks as used
    block_bitmap[0] = 1;
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
            return result;
        }

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

            // Mark indirect block
            if (inode.indirect_block != 0)
            {
                block_bitmap[inode.indirect_block] = 1;

                uint8_t indirect_block[BLOCK_SIZE];
                result = vdisk_read(&disk, inode.indirect_block, indirect_block);
                if (result != 0)
                {
                    free(block_bitmap);
                    block_bitmap = NULL;
                    vdisk_off(&disk);
                    return result;
                }

                // Set non-zero entries in indirect block as used
                uint32_t *pointers = (uint32_t *)indirect_block;
                for (int k = 0; k < POINTERS_PER_BLOCK; k++)
                {
                    if (pointers[k] != 0)
                    {
                        block_bitmap[pointers[k]] = 1;
                    }
                }
            }

            // Mark double indirect block
            if (inode.double_indirect_block != 0)
            {
                block_bitmap[inode.double_indirect_block] = 1;

                uint8_t double_indirect_block[BLOCK_SIZE];
                result = vdisk_read(&disk, inode.double_indirect_block, double_indirect_block);
                if (result != 0)
                {
                    free(block_bitmap);
                    block_bitmap = NULL;
                    vdisk_off(&disk);
                    return result;
                }

                // Process pointer in the double indirect block
                uint32_t *indirect_pointers = (uint32_t *)double_indirect_block;
                for (int j = 0; j < POINTERS_PER_BLOCK; j++)
                {
                    if (indirect_pointers[j] != 0)
                    {
                        // Mark indir block as used
                        block_bitmap[indirect_pointers[j]] = 1;

                        uint8_t curr_indirect_block[BLOCK_SIZE];
                        result = vdisk_read(&disk, indirect_pointers[j], curr_indirect_block);
                        if (result != 0)
                        {
                            free(block_bitmap);
                            block_bitmap = NULL;
                            vdisk_off(&disk);
                            return result;
                        }

                        // Set non-zero entries in this indirect block
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

    // 7. Store disk name
    int name_length = strlen(disk_name) + 1;
    mounted_disk = (char *)malloc(name_length);
    if (mounted_disk == NULL)
    {
        free(block_bitmap);
        block_bitmap = NULL;
        vdisk_off(&disk);
        return E_OUT_OF_SPACE; // see error.h
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
    // we actually don't check the result here
    // because we want to clean up even if sync fails
    // -> will check in the final return

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

    // 5. Close virtual disk and reset flag
    vdisk_off(&disk);
    disk_mounted = false;

    // Return 0 for success or err code from vdisk_sync if it failed
    return (result == 0) ? 0 : result;
}

int create(void)
{
    // 1. Check for disk mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Iterate through all inodes
    int max_inodes = superblock.num_inode_blocks * INODES_PER_BLOCK;
    for (int inode_num = 0; inode_num < max_inodes; inode_num++)
    {
        // 3. Get curr inode
        inode_t inode;
        int result = read_inode(inode_num, &inode);
        if (result != 0)
        {
            return result;
        }

        // 4. Check if inode is free (valid = 0)
        if (inode.valid == 0)
        {
            // Init inode fields
            inode.valid = 1; // mark as allocated
            inode.size = 0;  // empty file

            // Init all block pointers to 0
            for (int i = 0; i < 4; i++)
            {
                inode.direct_blocks[i] = 0;
            }
            inode.indirect_block = 0;
            inode.double_indirect_block = 0;

            // 5. Write inode back to disk
            result = write_inode(inode_num, &inode);
            if (result != 0)
            {
                return result;
            }

            return inode_num;
        }
    }

    // 7. If we get here -> no free inodes found
    return E_OUT_OF_INODES;
}

int delete(int inode_num)
{
    // 1. Check for disk mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Check if inode # is valid
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // 3. Read inode
    inode_t inode;
    int result = read_inode(inode_num, &inode);
    if (result != 0)
    {
        return result;
    }

    // 4. Check if inode is allocated
    if (inode.valid == 0)
    {
        return E_INVALID_INODE; // inode already free
    }

    // 5. Free direct blocks
    for (int i = 0; i < 4; i++)
    {
        if (inode.direct_blocks[i] != 0)
        {
            free_block(inode.direct_blocks[i]);
            inode.direct_blocks[i] = 0;
        }
    }

    // 6. Free indirect block and all blocks it points to
    if (inode.indirect_block != 0)
    {
        // Read the indirect block
        uint8_t indirect_block[BLOCK_SIZE];
        result = vdisk_read(&disk, inode.indirect_block, indirect_block);
        if (result != 0)
        {
            return result;
        }

        // Free all referenced data blocks
        uint32_t *pointers = (uint32_t *)indirect_block;
        for (int i = 0; i < POINTERS_PER_BLOCK; i++)
        {
            if (pointers[i] != 0)
            {
                free_block(pointers[i]);
            }
        }

        // Free indirect block itself
        free_block(inode.indirect_block);
        inode.indirect_block = 0;
    }

    // 7. Free double indirect block and all blocks it points to
    if (inode.double_indirect_block != 0)
    {
        // Read the double indirect block
        uint8_t double_indirect_block[BLOCK_SIZE];
        result = vdisk_read(&disk, inode.double_indirect_block, double_indirect_block);
        if (result != 0)
        {
            return result;
        }

        // Process pointer in the double indirect block
        uint32_t *indirect_pointers = (uint32_t *)double_indirect_block;
        for (int i = 0; i < POINTERS_PER_BLOCK; i++)
        {
            if (indirect_pointers[i] != 0)
            {
                // Read this indirect block
                uint8_t indirect_block[BLOCK_SIZE];
                result = vdisk_read(&disk, indirect_pointers[i], indirect_block);
                if (result != 0)
                {
                    return result;
                }

                // Free all referenced data blocks
                uint32_t *data_pointers = (uint32_t *)indirect_block;
                for (int j = 0; j < POINTERS_PER_BLOCK; j++)
                {
                    if (data_pointers[j] != 0)
                    {
                        free_block(data_pointers[j]);
                    }
                }

                // Free indirect block itself
                free_block(indirect_pointers[i]);
            }
        }

        // Free double indirect block itself
        free_block(inode.double_indirect_block);
        inode.double_indirect_block = 0;
    }

    // 8. Mark inode as free
    inode.valid = 0;
    inode.size = 0;

    // 9. Write back to disk
    result = write_inode(inode_num, &inode);
    if (result != 0)
    {
        return result;
    }

    return 0;
}

int stat(int inode_num)
{
    // 1. Check for disk mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Check if inode # is valid
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // 3. Read inode
    inode_t inode;
    int result = read_inode(inode_num, &inode);
    if (result != 0)
    {
        return result;
    }

    // 4. Check if inode is allocated/valid
    if (inode.valid == 0)
    {
        return E_INVALID_INODE;
    }

    return inode.size;
}

int read(int inode_num, uint8_t *data, int len, int offset)
{
    // 1. Check for disk  mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Check if inode # is valid
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // 3. Read inode
    inode_t inode;
    int result = read_inode(inode_num, &inode);
    if (result != 0)
    {
        return result;
    }

    // 4. Check if inode is allocated/valid)
    if (inode.valid == 0)
    {
        return E_INVALID_INODE;
    }

    // 5. Determine actual # of bytes to read
    int bytes_to_read = 0;
    if (offset < inode.size)
    {
        bytes_to_read = inode.size - offset;
        if (bytes_to_read > len)
        {
            bytes_to_read = len;
        }
    }

    // 6. If no bytes to read, return 0
    if (bytes_to_read <= 0)
    {
        return 0;
    }

    // 7. Init counter for total bytes read
    int bytes_read = 0;
    int current_offset = offset;

    // 8. Read block by block
    while (bytes_read < bytes_to_read)
    {
        // Get curr block idx and offset w/in the block
        // Then get physical block # for curr offset
        int block_offset = current_offset % BLOCK_SIZE;
        int block_num = get_block_for_offset(&inode, current_offset, false);

        // If <=0, that means null pointer or error
        if (block_num <= 0)
        {
            break;
        }

        // Read the block into temp buffer
        uint8_t block[BLOCK_SIZE];
        result = vdisk_read(&disk, block_num, block);
        if (result != 0)
        {
            // but if some data has already been read, return the count
            // else, return the error
            return (bytes_read > 0) ? bytes_read : result;
        }

        // Calculate how many bytes to copy from this block
        int bytes_to_copy = BLOCK_SIZE - block_offset;
        if (bytes_to_copy > (bytes_to_read - bytes_read))
        {
            bytes_to_copy = bytes_to_read - bytes_read;
        }

        // Copy data from block to user buffer
        memcpy(data + bytes_read, block + block_offset, bytes_to_copy);

        // Update counters
        bytes_read += bytes_to_copy;
        current_offset += bytes_to_copy;
    }

    return bytes_read;  // # of bytes actually read
}

int write(int inode_num, uint8_t *data, int len, int offset)
{
    // 1. Check for disk mounted
    if (!disk_mounted)
    {
        return E_DISK_NOT_MOUNTED;
    }

    // 2. Check if inode # is valid
    if (inode_num < 0 || inode_num >= superblock.num_inode_blocks * INODES_PER_BLOCK)
    {
        return E_INVALID_INODE;
    }

    // 3. Read the inode
    inode_t inode;
    int result = read_inode(inode_num, &inode);
    if (result != 0)
    {
        return result;
    }

    // 4. Check if inode is allocated/valid
    if (inode.valid == 0)
    {
        return E_INVALID_INODE;
    }

    // 5. If offset beyond curr file size, fill the gap with 0s
    if (offset > inode.size)
    {
        int zero_fill_start = inode.size;
        int zero_fill_end = offset;
        uint8_t zeros[BLOCK_SIZE] = {0};

        for (int curr_offset = zero_fill_start; curr_offset < zero_fill_end; )
        {
            int block_offset = curr_offset % BLOCK_SIZE;
            int block_num = get_block_for_offset(&inode, curr_offset, true);

            if (block_num <= 0)
            {
                // Error allocating block
                // but potentially we've already modified the file
                // <=> update inode size to reflect changes so far
                inode.size = (curr_offset > inode.size) ? curr_offset : inode.size;
                write_inode(inode_num, &inode);
                return block_num; // err code
            }

            // Get how many bytes to fill in this block
            int bytes_to_fill = BLOCK_SIZE - block_offset;
            if (bytes_to_fill > (zero_fill_end - curr_offset))
            {
                bytes_to_fill = zero_fill_end - curr_offset;
            }

            // If block not empty / we're not writing a full block,
            // we need to read the existing block
            uint8_t block[BLOCK_SIZE];
            if (block_offset > 0 || bytes_to_fill < BLOCK_SIZE)
            {
                result = vdisk_read(&disk, block_num, block);
                if (result != 0)
                {
                    // If read fails -> update inode and return the error
                    inode.size = (curr_offset > inode.size) ? curr_offset : inode.size;
                    write_inode(inode_num, &inode);
                    return result;
                }
            }
            else
            {
                // If writing a full block, just use our 0-buffer
                memcpy(block, zeros, BLOCK_SIZE);
            }

            // Fill the appropriate portion of the block with 0s
            memset(block + block_offset, 0, bytes_to_fill);

            // Write block back to disk
            result = vdisk_write(&disk, block_num, block);
            if (result != 0)
            {
                // If write fails -> update inode and return the error
                inode.size = (curr_offset > inode.size) ? curr_offset : inode.size;
                write_inode(inode_num, &inode);
                return result;
            }

            // Update offset
            curr_offset += bytes_to_fill;
        }

        // Update inode size to new offset
        inode.size = offset;
    }

    // 6. Write data from user buffer
    int bytes_written = 0;
    int current_offset = offset;

    while (bytes_written < len)
    {
        // Get block idx and offset w/in the block
        int block_offset = current_offset % BLOCK_SIZE;
        int block_num = get_block_for_offset(&inode, current_offset, true); // pass allocate=true for potential new block

        // If error getting/allocating the block
        if (block_num <= 0)
        {
            // Update inode size to reflect changes so far
            if (current_offset > inode.size)
            {
                inode.size = current_offset;
                write_inode(inode_num, &inode);
            }
            return (bytes_written > 0) ? bytes_written : block_num;
        }

        // Get how many bytes to write to this block
        int bytes_to_write = BLOCK_SIZE - block_offset;
        if (bytes_to_write > (len - bytes_written))
        {
            bytes_to_write = len - bytes_written;
        }

        // If not writing a full block or starting from the beginning of a block,
        // => then need to read the existing block to preserve data
        uint8_t block[BLOCK_SIZE];
        if (block_offset > 0 || bytes_to_write < BLOCK_SIZE)
        {
            result = vdisk_read(&disk, block_num, block);
            if (result != 0)
            {
                // If some data was already written, update size and rtn count
                if (bytes_written > 0)
                {
                    if (current_offset > inode.size)
                    {
                        inode.size = current_offset;
                        write_inode(inode_num, &inode);
                    }
                    return bytes_written;
                }
                return result;
            }
        }

        // Copy data from user buffer to block
        memcpy(block + block_offset, data + bytes_written, bytes_to_write);

        // Write block back to disk
        result = vdisk_write(&disk, block_num, block);
        if (result != 0)
        {
            // If some data was already written, update size and rtn count
            if (bytes_written > 0)
            {
                if (current_offset > inode.size)
                {
                    inode.size = current_offset;
                    write_inode(inode_num, &inode);
                }
                return bytes_written;
            }
            return result;
        }

        // Update counters
        bytes_written += bytes_to_write;
        current_offset += bytes_to_write;
    }

    // 7. Update inode size if the write extended the file
    if (current_offset > inode.size)
    {
        inode.size = current_offset;
        result = write_inode(inode_num, &inode);
        if (result != 0)
        {
            // Even writing inode fails, we have written data,
            // so return count of bytes written so far
            return bytes_written;
        }
    }

    return bytes_written;
}