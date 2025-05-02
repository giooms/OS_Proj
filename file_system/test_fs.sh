#!/bin/bash

# Comprehensive testing script for the file system implementation

# ==============================
# Configuration
# ==============================
DISK_IMAGE="test_disk.img"
DISK_SIZE=1024  # Size in blocks (1KB per block)
TEST_LOG="fs_test_results.log"
FS_TEST="./src/fs_test"  # Updated path to run from src directory
VERBOSE=1       # Set to 1 for verbose output, 0 for minimal output

# ==============================
# Utility Functions
# ==============================

# ANSI color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Log a message to the console and log file
log() {
    local level=$1
    local message=$2
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")

    # Only show INFO messages if VERBOSE is enabled
    if [ "$level" != "INFO" ] || [ $VERBOSE -eq 1 ]; then
        # Use colors for terminal output based on message level
        if [ "$level" == "SUCCESS" ]; then
            echo -e "[${GREEN}$level${NC}] $timestamp - $message"
        elif [ "$level" == "ERROR" ]; then
            echo -e "[${RED}$level${NC}] $timestamp - $message"
        else
            echo "[$level] $timestamp - $message"
        fi
    fi

    # Log file doesn't include colors
    echo "[$level] $timestamp - $message" >> $TEST_LOG
}

# Log an error message
error() {
    log "ERROR" "$1"
}

# Log an info message
info() {
    log "INFO" "$1"
}

# Log a success message
success() {
    log "SUCCESS" "$1"
}

# Log a test name
test_name() {
    echo "" >> $TEST_LOG
    echo "===================================" >> $TEST_LOG
    echo "TEST: $1" >> $TEST_LOG
    echo "===================================" >> $TEST_LOG
    echo ""
    echo "====================================="
    echo -e "${GREEN}RUNNING TEST: $1${NC}"
    echo "====================================="
}

# Check if a command succeeded and log the result
check_result() {
    local result=$1
    local success_msg=$2
    local error_msg=$3

    if [ $result -eq 0 ]; then
        success "$success_msg"
        return 0
    else
        error "$error_msg (Error code: $result)"
        return 1
    fi
}

# Run a command and check its result
run_cmd() {
    local cmd=$1
    local success_msg=$2
    local error_msg=$3

    info "Running: $cmd"

    # Run the command and capture output and result
    output=$($cmd 2>&1)
    result=$?

    # Log the output if it exists
    if [ -n "$output" ]; then
        echo "$output" >> $TEST_LOG
        if [ $VERBOSE -eq 1 ]; then
            echo "$output"
        fi
    fi

    # Check the result
    check_result $result "$success_msg" "$error_msg"
    return $?
}

# Examine disk image with xxd
examine_disk() {
    local start_block=$1
    local num_blocks=$2
    local message=$3

    info "$message - Examining disk blocks $start_block to $((start_block + num_blocks - 1))"

    # Calculate byte offsets
    local start_byte=$((start_block * 1024))
    local size=$((num_blocks * 1024))

    # Use xxd to dump hex and ASCII representation
    xxd -g 4 -s $start_byte -l $size $DISK_IMAGE >> $TEST_LOG

    if [ $VERBOSE -eq 1 ]; then
        echo "Disk contents (blocks $start_block-$((start_block + num_blocks - 1))):"
        xxd -g 4 -s $start_byte -l $size $DISK_IMAGE
    fi
}

# Create a file with specific content
create_test_file() {
    local inode_var=$1
    local content=$2

    # Create a file and capture the inode number
    info "Creating test file with content: $content"
    local output=$($FS_TEST create)
    local inode=$(echo $output | grep -o '[0-9]\+')

    # Set the inode variable for the caller
    eval $inode_var=$inode

    if [ -z "$inode" ]; then
        error "Failed to extract inode number from output: $output"
        return 1
    fi

    # Write content to the file
    run_cmd "$FS_TEST write $inode 0 \"$content\"" \
        "Successfully wrote to inode $inode" \
        "Failed to write to inode $inode"

    return $?
}

# Setup test environment
setup() {
    test_name "Setup Test Environment"

    # Make sure we're in the right directory
    info "Ensuring we're in the correct directory"
    if [ ! -d "./src" ]; then
        error "Please run this script from the root directory of the file system project"
        exit 1
    fi

    # Create an empty log file
    > $TEST_LOG

    # Remove any existing disk image
    if [ -f "$DISK_IMAGE" ]; then
        info "Removing existing disk image"
        rm $DISK_IMAGE
    fi

    # Create a new disk image
    run_cmd "dd if=/dev/zero of=$DISK_IMAGE bs=1024 count=$DISK_SIZE status=none" \
        "Created blank disk image of $DISK_SIZE blocks" \
        "Failed to create disk image"

    # Examine the empty disk
    examine_disk 0 2 "Empty disk image"
}

# Cleanup test environment
cleanup() {
    test_name "Cleanup Test Environment"

    # Ensure disk is unmounted
    $FS_TEST unmount 2>/dev/null

    # Keep the disk image and logs for inspection
    info "Test completed. Log file: $TEST_LOG"
    info "Disk image preserved at: $DISK_IMAGE"
    info "You can use 'xxd -g 4 $DISK_IMAGE | less' to examine the disk"
    info "Or 'xxd -g 4 -s <offset> -l <length> $DISK_IMAGE' to examine specific blocks"
}

# ==============================
# Basic Tests
# ==============================

# Test formatting the disk
test_format() {
    test_name "Format Disk"

    run_cmd "$FS_TEST format $DISK_IMAGE 100" \
        "Disk formatted successfully" \
        "Failed to format disk"

    examine_disk 0 3 "Superblock and inode blocks after formatting"
}

# Test mounting the disk
test_mount() {
    test_name "Mount Disk"

    run_cmd "$FS_TEST mount $DISK_IMAGE" \
        "Disk mounted successfully" \
        "Failed to mount disk"
}

# Test creating a file
test_create() {
    test_name "Create File"

    local output=$($FS_TEST create)
    local inode=$(echo $output | grep -o '[0-9]\+')

    if [ -z "$inode" ]; then
        error "Failed to extract inode number from output: $output"
        return 1
    fi

    success "Created file with inode $inode"
    info "Raw output: $output"

    # Examine the inode block (block 1 contains inodes starting at 0)
    examine_disk 1 1 "Inode block after file creation"

    # Return the inode number for use by other tests
    echo $inode
}

# Test writing to a file
test_write() {
    local inode=$1
    local offset=$2
    local content=$3

    test_name "Write to File (inode=$inode, offset=$offset)"

    run_cmd "$FS_TEST write $inode $offset \"$content\"" \
        "Successfully wrote to inode $inode" \
        "Failed to write to inode $inode"

    # Examine the inode block and data blocks
    examine_disk 1 1 "Inode block after write"
    examine_disk 5 5 "First few data blocks after write"
}

# Test reading from a file
test_read() {
    local inode=$1
    local offset=$2
    local length=$3
    local expected=$4

    test_name "Read from File (inode=$inode, offset=$offset, length=$length)"

    local output=$($FS_TEST read $inode $offset $length)

    # Check if output contains the expected data
    if [[ "$output" == *"$expected"* ]]; then
        success "Successfully read data from inode $inode"
        success "Read data matches expected: \"$expected\""
    else
        error "Read data does not match expected"
        error "Expected: \"$expected\""
        error "Got: \"$output\""
        return 1
    fi

    info "Raw output: $output"
    return 0
}

# Test getting file stats
test_stat() {
    local inode=$1
    local expected_size=$2

    test_name "File Stat (inode=$inode)"

    local output=$($FS_TEST stat $inode)
    local size=$(echo $output | grep -o '[0-9]\+')

    if [ "$size" == "$expected_size" ]; then
        success "File size is correct: $size bytes"
    else
        error "File size is incorrect"
        error "Expected: $expected_size bytes"
        error "Got: $size bytes"
        return 1
    fi

    info "Raw output: $output"
    return 0
}

# Test deleting a file
test_delete() {
    local inode=$1

    test_name "Delete File (inode=$inode)"

    run_cmd "$FS_TEST delete $inode" \
        "Successfully deleted inode $inode" \
        "Failed to delete inode $inode"

    # Examine the inode block after deletion
    examine_disk 1 1 "Inode block after deletion"

    # Try to read from the deleted file - should fail
    local output=$($FS_TEST read $inode 0 10 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Reading from deleted file correctly fails"
    else
        error "Reading from deleted file should fail but succeeded"
        error "Output: $output"
        return 1
    fi
}

# Test unmounting the disk
test_unmount() {
    test_name "Unmount Disk"

    run_cmd "$FS_TEST unmount" \
        "Disk unmounted successfully" \
        "Failed to unmount disk"
}

# ==============================
# Advanced Tests
# ==============================

# Test large file operations
test_large_file() {
    test_name "Large File Operations"

    # Create a file
    local inode=$(test_create)

    # Generate a large content string (50KB)
    local large_content=$(head -c 51200 /dev/urandom | base64 | head -c 51200)

    # Write the large content
    test_write $inode 0 "$large_content"

    # Check file size
    test_stat $inode 51200

    # Read back and check a sample of the content
    local sample_size=100
    local sample_start=1000
    local expected_sample=${large_content:$sample_start:$sample_size}

    # Read a portion to verify
    local output=$($FS_TEST read $inode $sample_start $sample_size)
    local read_sample=$(echo "$output" | grep -o "Data:.*" | cut -d':' -f2- | xargs)

    if [ "$read_sample" == "$expected_sample" ]; then
        success "Sample data read correctly from large file"
    else
        error "Sample data does not match expected"
        error "Expected: ${expected_sample:0:20}..."
        error "Got: ${read_sample:0:20}..."
        return 1
    fi

    # Clean up
    test_delete $inode
}

# Test writing beyond file size
test_sparse_file() {
    test_name "Sparse File Operations (Writing with Gaps)"

    # Create a file
    local inode=$(test_create)

    # Write a small string at the beginning
    test_write $inode 0 "Start of file"

    # Write another string at offset 10000
    test_write $inode 10000 "Middle of file"

    # Write a third string at offset 20000
    test_write $inode 20000 "End of file"

    # Check file size
    test_stat $inode 20012  # "End of file" is 12 bytes

    # Read from different parts
    test_read $inode 0 13 "Start of file"
    test_read $inode 10000 13 "Middle of file"
    test_read $inode 20000 12 "End of file"

    # Read from a gap (should return zeros)
    local output=$($FS_TEST read $inode 100 10)
    if [[ "$output" == *"$'\0'"* ]] || [[ "$output" == *"\\x00"* ]] || [[ "$output" == *" "* ]]; then
        success "Gap correctly contains zeros or empty space"
    else
        info "Gap contains: $output (may be displayed as empty space)"
    fi

    # Clean up
    test_delete $inode
}

# Test file overwrite
test_file_overwrite() {
    test_name "File Overwrite Operations"

    # Create a file
    local inode=$(test_create)

    # Write initial content
    test_write $inode 0 "Initial content that will be partially overwritten"

    # Overwrite part of the content
    test_write $inode 8 "content which overwrites"

    # Read and verify
    test_read $inode 0 47 "Initial content which overwrites be partially overwritten"

    # Clean up
    test_delete $inode
}

# Test multiple files
test_multiple_files() {
    test_name "Multiple Files Test"

    # Create test data
    local files=()
    local contents=()

    # Create multiple files
    for i in {1..5}; do
        local content="This is file $i with some unique content: $(date +%s.%N)"
        contents+=("$content")

        local inode=""
        create_test_file inode "$content"
        files+=($inode)

        info "Created file $i with inode $inode"
    done

    # Verify all files
    for i in {0..4}; do
        local inode=${files[$i]}
        local content=${contents[$i]}
        local content_length=${#content}

        test_stat $inode $content_length
        test_read $inode 0 $content_length "$content"
    done

    # Delete some files
    test_delete ${files[1]}
    test_delete ${files[3]}

    # Verify remaining files
    for i in 0 2 4; do
        local inode=${files[$i]}
        local content=${contents[$i]}
        local content_length=${#content}

        test_stat $inode $content_length
        test_read $inode 0 $content_length "$content"
    done

    # Create more files to fill in deleted inodes
    local new_files=()
    local new_contents=()

    for i in {1..3}; do
        local content="New file $i with different content: $(date +%s.%N)"
        new_contents+=("$content")

        local inode=""
        create_test_file inode "$content"
        new_files+=($inode)

        info "Created new file $i with inode $inode"
    done

    # Verify all files still work
    for i in 0 2 4; do
        local inode=${files[$i]}
        local content=${contents[$i]}
        local content_length=${#content}

        test_stat $inode $content_length
        test_read $inode 0 $content_length "$content"
    done

    # Verify new files
    for i in {0..2}; do
        local inode=${new_files[$i]}
        local content=${new_contents[$i]}
        local content_length=${#content}

        test_stat $inode $content_length
        test_read $inode 0 $content_length "$content"
    done
}

# Test error conditions
test_error_conditions() {
    test_name "Error Handling Test"

    # Try to stat a non-existent inode
    local output=$($FS_TEST stat 999 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Stat on non-existent inode correctly fails"
    else
        error "Stat on non-existent inode should fail but succeeded"
        error "Output: $output"
    fi

    # Try to read from a non-existent inode
    output=$($FS_TEST read 999 0 10 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Read from non-existent inode correctly fails"
    else
        error "Read from non-existent inode should fail but succeeded"
        error "Output: $output"
    fi

    # Try to delete a non-existent inode
    output=$($FS_TEST delete 999 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Delete of non-existent inode correctly fails"
    else
        error "Delete of non-existent inode should fail but succeeded"
        error "Output: $output"
    fi

    # Try to write to a non-existent inode
    output=$($FS_TEST write 999 0 "test content" 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Write to non-existent inode correctly fails"
    else
        error "Write to non-existent inode should fail but succeeded"
        error "Output: $output"
    fi

    # Try to mount already mounted disk
    output=$($FS_TEST mount $DISK_IMAGE 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Mount of already mounted disk correctly fails"
    else
        error "Mount of already mounted disk should fail but succeeded"
        error "Output: $output"
    fi

    # Create a valid file for negative offset test
    local inode=$(test_create)

    # Try to read with negative offset
    output=$($FS_TEST read $inode -10 10 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Read with negative offset correctly fails"
    else
        error "Read with negative offset should fail but succeeded"
        error "Output: $output"
    fi

    # Try to write with negative offset
    output=$($FS_TEST write $inode -10 "test content" 2>&1)
    if [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        success "Write with negative offset correctly fails"
    else
        error "Write with negative offset should fail but succeeded"
        error "Output: $output"
    fi

    # Clean up the test file
    test_delete $inode
}

# Test mount/unmount cycle
test_mount_cycle() {
    test_name "Mount/Unmount Cycle Test"

    # Create files before unmounting
    local inode1=""
    local inode2=""
    create_test_file inode1 "Content for file 1"
    create_test_file inode2 "Content for file 2 which is longer"

    info "Created files with inodes $inode1 and $inode2"

    # Unmount
    test_unmount

    # Examine disk after unmounting
    examine_disk 0 10 "Disk after unmounting"

    # Mount again
    test_mount

    # Verify files still exist and have correct content
    test_stat $inode1 16  # "Content for file 1" is 16 bytes
    test_read $inode1 0 16 "Content for file 1"

    test_stat $inode2 31  # "Content for file 2 which is longer" is 31 bytes
    test_read $inode2 0 31 "Content for file 2 which is longer"

    # Make changes to the files
    test_write $inode1 0 "Updated content 1"
    test_write $inode2 0 "Updated content 2"

    # Unmount again
    test_unmount

    # Mount once more
    test_mount

    # Verify changes persisted
    test_read $inode1 0 16 "Updated content 1"
    test_read $inode2 0 16 "Updated content 2"

    # Clean up
    test_delete $inode1
    test_delete $inode2
}

# Test disk full conditions
test_disk_full() {
    test_name "Disk Full Test"

    # Create a small test disk for this test (just 10 blocks)
    local small_disk="small_disk.img"
    run_cmd "dd if=/dev/zero of=$small_disk bs=1024 count=10 status=none" \
        "Created small disk image of 10 blocks" \
        "Failed to create small disk image"

    # Format with minimal inodes
    run_cmd "$FS_TEST format $small_disk 2" \
        "Small disk formatted successfully" \
        "Failed to format small disk"

    # Mount the small disk
    run_cmd "$FS_TEST mount $small_disk" \
        "Small disk mounted successfully" \
        "Failed to mount small disk"

    # Create a file
    local output=$($FS_TEST create)
    local inode=$(echo $output | grep -o '[0-9]\+')

    if [ -z "$inode" ]; then
        error "Failed to extract inode number from output: $output"
        return 1
    else
        success "Created file with inode $inode"
    fi

    # Try to write a large file to fill the disk
    # With 10 blocks, 1 for superblock, 1 for inodes, we have ~8 blocks for data
    # That's about 8KB, so try to write 9KB
    local large_content=$(head -c 9216 /dev/urandom | base64 | head -c 9216)

    output=$($FS_TEST write $inode 0 "$large_content" 2>&1)
    if [[ "$output" == *"bytes written"* ]]; then
        local bytes=$(echo $output | grep -o '[0-9]\+ bytes written' | cut -d' ' -f1)
        info "Wrote $bytes bytes before disk became full"
        if [ $bytes -lt 9216 ]; then
            success "Disk full condition detected correctly"
        else
            error "Wrote full content despite small disk size"
        fi
    elif [[ "$output" == *"failed"* ]] || [[ "$output" == *"error"* ]]; then
        info "Write operation failed with message: $output"
        if [[ "$output" == *"space"* ]]; then
            success "Disk full error detected correctly"
        else
            error "Failed with unexpected error: $output"
        fi
    else
        error "Unexpected output: $output"
    fi

    # Unmount the small disk
    run_cmd "$FS_TEST unmount" \
        "Small disk unmounted successfully" \
        "Failed to unmount small disk"

    # Clean up
    if [ -f "$small_disk" ]; then
        rm $small_disk
    fi

    # Remount the main test disk
    test_mount
}

# ==============================
# Run all tests
# ==============================

run_all_tests() {
    setup

    # Basic operations
    test_format
    test_mount

    # File creation
    local inode=$(test_create)

    # Write, read, stat
    test_write $inode 0 "Hello, world!"
    test_read $inode 0 13 "Hello, world!"
    test_stat $inode 13

    # Second file
    local inode2=$(test_create)
    test_write $inode2 0 "Second file content"

    # Delete the first file
    test_delete $inode

    # Verify second file still works
    test_stat $inode2 19  # "Second file content" is 19 bytes
    test_read $inode2 0 19 "Second file content"

    # Clean up second file
    test_delete $inode2

    # Advanced tests
    test_large_file
    test_sparse_file
    test_file_overwrite
    test_multiple_files
    test_error_conditions
    test_mount_cycle
    test_disk_full

    # Final unmount
    test_unmount

    cleanup

    # Final summary
    echo ""
    echo "============================================="
    echo -e "${GREEN}TESTING COMPLETE - Check $TEST_LOG for details${NC}"
    echo "============================================="
}

# Run all tests
run_all_tests
