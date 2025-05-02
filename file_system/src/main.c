#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "include/fs.h"
#include "include/vdisk.h"
#include "include/error.h"

// Your main.c

void print_usage(const char* program_name) {
    printf("Usage: %s <command> [arguments]\n", program_name);
    printf("Commands:\n");
    printf("  format <disk_image> <inodes>      - Format a disk with SSFS\n");
    printf("  mount <disk_image>                - Mount a SSFS disk\n");
    printf("  unmount                           - Unmount the current SSFS disk\n");
    printf("  create                            - Create a new file and print its inode number\n");
    printf("  delete <inode_num>                - Delete a file\n");
    printf("  stat <inode_num>                  - Print file size\n");
    printf("  read <inode_num> <offset> <length> - Read data from a file\n");
    printf("  write <inode_num> <offset> <data>  - Write data to a file\n");
}

// Helper function to read and display file contents
void display_file_contents(int inode_num, int file_size) {
    uint8_t buffer[1024];
    int offset = 0;
    int chunk_size = 1024;

    printf("File contents:\n");

    while (offset < file_size) {
        if (file_size - offset < chunk_size) {
            chunk_size = file_size - offset;
        }

        int bytes_read = read(inode_num, buffer, chunk_size, offset);
        if (bytes_read <= 0) {
            printf("Error reading file at offset %d\n", offset);
            break;
        }

        // Print the data (assuming it's text)
        buffer[bytes_read] = '\0'; // Null-terminate for printing
        printf("%s", (char*)buffer);

        offset += bytes_read;
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* command = argv[1];
    int result;

    if (strcmp(command, "format") == 0) {
        if (argc < 4) {
            printf("Error: format command requires disk_image and inodes arguments\n");
            return 1;
        }

        const char* disk_name = argv[2];
        int inodes = atoi(argv[3]);

        result = format((char *)disk_name, inodes);
        if (result == 0) {
            printf("Disk '%s' formatted successfully with %d inodes\n", disk_name, inodes);
        } else {
            printf("Format failed with error code: %d\n", result);
        }
    }
    else if (strcmp(command, "mount") == 0) {
        if (argc < 3) {
            printf("Error: mount command requires disk_image argument\n");
            return 1;
        }

        const char* disk_name = argv[2];
        result = mount((char *)disk_name);
        if (result == 0) {
            printf("Disk '%s' mounted successfully\n", disk_name);
        } else {
            printf("Mount failed with error code: %d\n", result);
        }
    }
    else if (strcmp(command, "unmount") == 0) {
        result = unmount();
        if (result == 0) {
            printf("Disk unmounted successfully\n");
        } else {
            printf("Unmount failed with error code: %d\n", result);
        }
    }
    else if (strcmp(command, "create") == 0) {
        int inode_num = create();
        if (inode_num >= 0) {
            printf("File created successfully with inode number: %d\n", inode_num);
        } else {
            printf("File creation failed with error code: %d\n", inode_num);
        }
    }
    else if (strcmp(command, "delete") == 0) {
        if (argc < 3) {
            printf("Error: delete command requires inode_num argument\n");
            return 1;
        }

        int inode_num = atoi(argv[2]);
        result = delete(inode_num);
        if (result == 0) {
            printf("File with inode %d deleted successfully\n", inode_num);
        } else {
            printf("File deletion failed with error code: %d\n", result);
        }
    }
    else if (strcmp(command, "stat") == 0) {
        if (argc < 3) {
            printf("Error: stat command requires inode_num argument\n");
            return 1;
        }

        int inode_num = atoi(argv[2]);
        int file_size = stat(inode_num);
        if (file_size >= 0) {
            printf("File with inode %d has size: %d bytes\n", inode_num, file_size);
        } else {
            printf("Stat failed with error code: %d\n", file_size);
        }
    }
    else if (strcmp(command, "read") == 0) {
        if (argc < 5) {
            printf("Error: read command requires inode_num, offset, and length arguments\n");
            return 1;
        }

        int inode_num = atoi(argv[2]);
        int offset = atoi(argv[3]);
        int length = atoi(argv[4]);

        uint8_t* buffer = (uint8_t*)malloc(length + 1);
        if (!buffer) {
            printf("Memory allocation failed\n");
            return 1;
        }

        int bytes_read = read(inode_num, buffer, length, offset);
        if (bytes_read >= 0) {
            printf("Read %d bytes from inode %d at offset %d\n", bytes_read, inode_num, offset);
            buffer[bytes_read] = '\0'; // Null-terminate for printing
            printf("Data: %s\n", buffer);
        } else {
            printf("Read failed with error code: %d\n", bytes_read);
        }

        free(buffer);
    }
    else if (strcmp(command, "write") == 0) {
        if (argc < 5) {
            printf("Error: write command requires inode_num, offset, and data arguments\n");
            return 1;
        }

        int inode_num = atoi(argv[2]);
        int offset = atoi(argv[3]);
        const char* data = argv[4];
        int data_len = strlen(data);

        int bytes_written = write(inode_num, (uint8_t*)data, data_len, offset);
        if (bytes_written >= 0) {
            printf("Wrote %d bytes to inode %d at offset %d\n", bytes_written, inode_num, offset);
            // Show the file contents after writing
            int file_size = stat(inode_num);
            if (file_size >= 0) {
                display_file_contents(inode_num, file_size);
            }
        } else {
            printf("Write failed with error code: %d\n", bytes_written);
        }
    }
    else {
        printf("Unknown command: %s\n", command);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
