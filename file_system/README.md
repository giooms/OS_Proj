# File System Testing Suite

This repository contains a testing suite for the file system implementation. The testing suite is designed to thoroughly test all aspects of the file system, including edge cases and error conditions.

## Files

- `fs_test`: Your file system test program (executable)
- `test_fs.sh`: The main comprehensive test script
- `inspect_disk.sh`: A utility script to examine disk images
- `README.md`: This documentation file

## Getting Started

### Basic Usage

To run a complete test suite on your file system implementation:

```bash
chmod +x test_fs.sh
./test_fs.sh
```

The test script will:

1. Create a test disk image
2. Run a series of tests on all file system operations
3. Generate a detailed log file `fs_test_results.log`

### Inspecting Disk Images

To examine the contents of a disk image for debugging:

```bash
chmod +x inspect_disk.sh
./inspect_disk.sh mydisk.img
```

This will show the first block of the disk image. For more options:

```bash
./inspect_disk.sh --help
```

## Testing Workflow

The test script performs these tests in sequence:

1. **Basic Operations**
   - Format a disk
   - Mount the disk
   - Create a file
   - Write to a file
   - Read from a file
   - Get file stats
   - Delete a file
   - Unmount the disk

2. **Advanced Tests**
   - Large file operations (files > 50KB)
   - Sparse file operations (writing at non-contiguous offsets)
   - File overwrite operations
   - Multiple files test
   - Error handling test
   - Mount/unmount cycle test
   - Disk full test

## Manual Testing

For quick manual testing, you can use this workflow:

```bash
# Create a disk image
dd if=/dev/zero of=mydisk.img bs=1024 count=100

# Format the disk
./fs_test format mydisk.img 10

# Mount the disk
./fs_test mount mydisk.img

# Create a file and note the inode number
./fs_test create

# Write to the file
./fs_test write 0 0 "Hello, world!"

# Read from the file
./fs_test read 0 0 13

# Check file size
./fs_test stat 0

# Examine the disk using hexdump
xxd -g 4 mydisk.img | less

# Unmount when done
./fs_test unmount
```

## Understanding Disk Layout

The file system has a structure similar to:

- **Block 0**: Superblock (contains file system metadata)
- **Blocks 1-n**: Inode blocks (each containing multiple inodes)
- **Remaining blocks**: Data blocks

You can examine specific regions with:

```bash
# Show superblock
./inspect_disk.sh mydisk.img --superblock

# Show inode blocks
./inspect_disk.sh mydisk.img --inodes

# Show specific data blocks (e.g., 10-12)
./inspect_disk.sh mydisk.img --block 10 --blocks 3
```

## Troubleshooting

If tests are failing, check these common issues:

1. **Permission denied**: Make sure test scripts are executable (`chmod +x *.sh`)
2. **Disk not mounted**: Some operations require a mounted disk
3. **Out of space**: If writing large files fails
4. **Invalid inode**: If accessing a non-existent or deleted file

For detailed logging, check the `fs_test_results.log` file created during testing.
