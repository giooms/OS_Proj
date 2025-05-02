#!/bin/bash

# Utility script to inspect disk image contents

usage() {
    echo "Usage: $0 [OPTIONS] disk_image"
    echo "Options:"
    echo "  -b, --block NUM         Show specific block number (default: 0)"
    echo "  -n, --blocks COUNT      Number of blocks to display (default: 1)"
    echo "  -a, --all               Show entire disk"
    echo "  -s, --superblock        Show superblock (block 0)"
    echo "  -i, --inodes            Show inode blocks"
    echo "  -h, --help              Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 mydisk.img -s                   # Show superblock"
    echo "  $0 mydisk.img -b 5 -n 3            # Show blocks 5-7"
    echo "  $0 mydisk.img -i                   # Show all inode blocks"
    echo "  $0 mydisk.img -a | less            # View entire disk with paging"
}

# Default values
BLOCK=0
BLOCKS=1
SHOW_ALL=0
SHOW_SUPERBLOCK=0
SHOW_INODES=0
DISK_IMAGE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    key="$1"

    case $key in
        -b|--block)
            BLOCK="$2"
            shift 2
            ;;
        -n|--blocks)
            BLOCKS="$2"
            shift 2
            ;;
        -a|--all)
            SHOW_ALL=1
            shift
            ;;
        -s|--superblock)
            SHOW_SUPERBLOCK=1
            shift
            ;;
        -i|--inodes)
            SHOW_INODES=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            DISK_IMAGE="$1"
            shift
            ;;
    esac
done

# Check if disk image was provided
if [ -z "$DISK_IMAGE" ]; then
    echo "Error: No disk image specified"
    usage
    exit 1
fi

# Check if disk image exists
if [ ! -f "$DISK_IMAGE" ]; then
    echo "Error: Disk image file not found: $DISK_IMAGE"
    exit 1
fi

# Get disk size in blocks (assuming 1024-byte blocks)
DISK_SIZE=$(($(stat -c%s "$DISK_IMAGE") / 1024))
echo "Disk image: $DISK_IMAGE ($DISK_SIZE blocks of 1024 bytes each)"

# Function to display blocks
show_blocks() {
    local start=$1
    local count=$2
    local desc=$3

    echo "==================================================="
    echo "$desc"
    echo "==================================================="

    # Check for valid range
    if [ $start -ge $DISK_SIZE ]; then
        echo "Error: Starting block $start is beyond disk size"
        return 1
    fi

    # Adjust count if it would go beyond disk size
    if [ $((start + count)) -gt $DISK_SIZE ]; then
        count=$((DISK_SIZE - start))
        echo "Warning: Adjusted block count to $count to fit disk size"
    fi

    # Calculate byte offsets
    local start_byte=$((start * 1024))
    local size=$((count * 1024))

    # Use xxd to dump hex and ASCII representation
    xxd -g 4 -a -s $start_byte -l $size "$DISK_IMAGE"
    echo ""
}

# Show the requested blocks based on command line options

# Show superblock (block 0)
if [ $SHOW_SUPERBLOCK -eq 1 ]; then
    show_blocks 0 1 "SUPERBLOCK (Block 0)"
fi

# Show inode blocks (typically starting at block 1)
if [ $SHOW_INODES -eq 1 ]; then
    # We don't know exactly how many inode blocks there are
    # Assuming we could parse the superblock, we'd know
    # For now, let's assume there are 5 or fewer inode blocks
    show_blocks 1 5 "INODE BLOCKS (Blocks 1-5)"
fi

# Show the entire disk
if [ $SHOW_ALL -eq 1 ]; then
    show_blocks 0 $DISK_SIZE "ENTIRE DISK (Blocks 0-$((DISK_SIZE-1)))"
fi

# Show specific blocks if not showing all, superblock, or inodes
if [ $SHOW_ALL -eq 0 ] && [ $SHOW_SUPERBLOCK -eq 0 ] && [ $SHOW_INODES -eq 0 ]; then
    show_blocks $BLOCK $BLOCKS "BLOCKS $BLOCK-$((BLOCK+BLOCKS-1))"
fi
