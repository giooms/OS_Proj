#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "include/fs.h"
#include "include/vdisk.h"
#include "include/error.h"

// ANSI color codes for terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define LOG_FILE "fs_test_results.log"
FILE* log_file = NULL;

// Helper function to read and display file contents
void display_file_contents(int inode_num, int file_size) {
    uint8_t buffer[1024];
    int offset = 0;
    int chunk_size = 1024;

    printf("File contents:\n");
    if (log_file) fprintf(log_file, "File contents:\n");

    while (offset < file_size) {
        if (file_size - offset < chunk_size) {
            chunk_size = file_size - offset;
        }

        int bytes_read = read(inode_num, buffer, chunk_size, offset);
        if (bytes_read <= 0) {
            printf("Error reading file at offset %d\n", offset);
            if (log_file) fprintf(log_file, "Error reading file at offset %d\n", offset);
            break;
        }

        // Print the data (assuming it's text)
        buffer[bytes_read] = '\0'; // Null-terminate for printing
        printf("%s", (char*)buffer);
        if (log_file) fprintf(log_file, "%s", (char*)buffer);

        offset += bytes_read;
    }
    printf("\n");
    if (log_file) fprintf(log_file, "\n");
}

// Test result tracking
typedef struct {
    int total;
    int passed;
    int failed;
} TestResults;

// Utility functions for logging and test management
void log_message(const char* level, const char* message) {
    time_t now;
    struct tm *tm_info;
    char timestamp[26];

    time(&now);
    tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    if (strcmp(level, "SUCCESS") == 0) {
        printf("[%s%s%s] %s - %s\n", ANSI_COLOR_GREEN, level, ANSI_COLOR_RESET, timestamp, message);
    } else if (strcmp(level, "ERROR") == 0) {
        printf("[%s%s%s] %s - %s\n", ANSI_COLOR_RED, level, ANSI_COLOR_RESET, timestamp, message);
    } else if (strcmp(level, "TEST") == 0) {
        printf("\n[%s%s%s] %s - %s\n", ANSI_COLOR_BLUE, level, ANSI_COLOR_RESET, timestamp, message);
    } else {
        printf("[%s] %s - %s\n", level, timestamp, message);
    }

    // Write to log file if open
    if (log_file) {
        fprintf(log_file, "[%s] %s - %s\n", level, timestamp, message);
    }
}

void log_info(const char* message) {
    log_message("INFO", message);
}

void log_success(const char* message) {
    log_message("SUCCESS", message);
}

void log_error(const char* message) {
    log_message("ERROR", message);
}

void log_test(const char* message) {
    log_message("TEST", message);
    if (log_file) {
        fprintf(log_file, "\n====================================\n");
        fprintf(log_file, "TEST: %s\n", message);
        fprintf(log_file, "====================================\n");
    }

    printf("\n====================================\n");
    printf("%sTEST: %s%s\n", ANSI_COLOR_BLUE, message, ANSI_COLOR_RESET);
    printf("====================================\n");
}

void print_test_header(const char* test_name) {
    printf("\n===== TESTING: %s =====\n", test_name);
    if (log_file) fprintf(log_file, "\n===== TESTING: %s =====\n", test_name);
}

void print_test_result(const char* test_name, bool success, int result_code) {
    if (success) {
        printf("%s✓ PASS:%s %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET, test_name);
        if (log_file) fprintf(log_file, "✓ PASS: %s\n", test_name);
    } else {
        printf("%s✗ FAIL:%s %s (Error code: %d)\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, test_name, result_code);
        if (log_file) fprintf(log_file, "✗ FAIL: %s (Error code: %d)\n", test_name, result_code);
    }
}

void print_test_summary(TestResults results) {
    printf("\n===== TEST SUMMARY =====\n");
    printf("Total tests: %d\n", results.total);
    printf("Passed: %s%d%s\n", ANSI_COLOR_GREEN, results.passed, ANSI_COLOR_RESET);
    printf("Failed: %s%d%s\n", ANSI_COLOR_RED, results.failed, ANSI_COLOR_RESET);
    printf("Success rate: %s%.1f%%%s\n",
           (results.passed == results.total) ? ANSI_COLOR_GREEN : ANSI_COLOR_YELLOW,
           (results.passed * 100.0) / results.total,
           ANSI_COLOR_RESET);

    if (log_file) {
        fprintf(log_file, "\n===== TEST SUMMARY =====\n");
        fprintf(log_file, "Total tests: %d\n", results.total);
        fprintf(log_file, "Passed: %d\n", results.passed);
        fprintf(log_file, "Failed: %d\n", results.failed);
        fprintf(log_file, "Success rate: %.1f%%\n", (results.passed * 100.0) / results.total);
    }
}

// Run basic tests (original workflow)
TestResults run_basic_tests() {
    TestResults results = {0, 0, 0};
    const char* disk_name = "test_disk.img";
    int num_inodes = 10;
    int result;
    int test_inode = -1;
    int test_inode2 = -1;
    int file_size = -1;
    const char* test_data = "Hello, File System World!";
    const char* additional_data = " This is additional data.";
    uint8_t read_buffer[1024];

    log_test("Basic File System Tests");
    printf("Starting Basic File System Testing Suite\n");
    printf("----------------------------------\n");
    if (log_file) {
        fprintf(log_file, "Starting Basic File System Testing Suite\n");
        fprintf(log_file, "----------------------------------\n");
    }

    // Test 1: Format disk
    print_test_header("Format");
    results.total++;
    result = format((char*)disk_name, num_inodes);
    if (result == 0) {
        printf("Disk '%s' formatted successfully with %d inodes\n", disk_name, num_inodes);
        if (log_file) fprintf(log_file, "Disk '%s' formatted successfully with %d inodes\n", disk_name, num_inodes);
        results.passed++;
    } else {
        printf("Format failed with error code: %d\n", result);
        if (log_file) fprintf(log_file, "Format failed with error code: %d\n", result);
        results.failed++;
        // Fatal error - can't continue without formatting
        print_test_summary(results);
        return results;
    }
    print_test_result("Format disk", result == 0, result);

    // Test 2: Mount disk
    print_test_header("Mount");
    results.total++;
    result = mount((char*)disk_name);
    if (result == 0) {
        printf("Disk '%s' mounted successfully\n", disk_name);
        if (log_file) fprintf(log_file, "Disk '%s' mounted successfully\n", disk_name);
        results.passed++;
    } else {
        printf("Mount failed with error code: %d\n", result);
        if (log_file) fprintf(log_file, "Mount failed with error code: %d\n", result);
        results.failed++;
        // Fatal error - can't continue without mounting
        print_test_summary(results);
        return results;
    }
    print_test_result("Mount disk", result == 0, result);

    // Test 3: Create a file
    print_test_header("Create file");
    results.total++;
    test_inode = create();
    if (test_inode >= 0) {
        printf("File created successfully with inode number: %d\n", test_inode);
        if (log_file) fprintf(log_file, "File created successfully with inode number: %d\n", test_inode);
        results.passed++;
    } else {
        printf("File creation failed with error code: %d\n", test_inode);
        if (log_file) fprintf(log_file, "File creation failed with error code: %d\n", test_inode);
        results.failed++;
    }
    print_test_result("Create file", test_inode >= 0, test_inode);

    // Test 4: Create a second file
    print_test_header("Create second file");
    results.total++;
    test_inode2 = create();
    if (test_inode2 >= 0) {
        printf("Second file created successfully with inode number: %d\n", test_inode2);
        if (log_file) fprintf(log_file, "Second file created successfully with inode number: %d\n", test_inode2);
        results.passed++;
    } else {
        printf("Second file creation failed with error code: %d\n", test_inode2);
        if (log_file) fprintf(log_file, "Second file creation failed with error code: %d\n", test_inode2);
        results.failed++;
    }
    print_test_result("Create second file", test_inode2 >= 0, test_inode2);

    // Test 5: Write to file
    if (test_inode >= 0) {
        print_test_header("Write to file");
        results.total++;
        int data_len = strlen(test_data);
        int bytes_written = write(test_inode, (uint8_t*)test_data, data_len, 0);
        if (bytes_written == data_len) {
            printf("Wrote %d bytes to inode %d\n", bytes_written, test_inode);
            if (log_file) fprintf(log_file, "Wrote %d bytes to inode %d\n", bytes_written, test_inode);
            results.passed++;
        } else {
            printf("Write failed or incomplete: wrote %d of %d bytes, error code: %d\n",
                   bytes_written > 0 ? bytes_written : 0, data_len, bytes_written);
            if (log_file) fprintf(log_file, "Write failed or incomplete: wrote %d of %d bytes, error code: %d\n",
                   bytes_written > 0 ? bytes_written : 0, data_len, bytes_written);
            results.failed++;
        }
        print_test_result("Write to file", bytes_written == data_len, bytes_written);
    }

    // Test 6: Stat file
    if (test_inode >= 0) {
        print_test_header("Stat file");
        results.total++;
        file_size = stat(test_inode);
        if (file_size >= 0) {
            printf("File with inode %d has size: %d bytes\n", test_inode, file_size);
            if (log_file) fprintf(log_file, "File with inode %d has size: %d bytes\n", test_inode, file_size);
            results.passed++;
        } else {
            printf("Stat failed with error code: %d\n", file_size);
            if (log_file) fprintf(log_file, "Stat failed with error code: %d\n", file_size);
            results.failed++;
        }
        print_test_result("Stat file", file_size >= 0, file_size);
    }

    // Test 7: Read from file
    if (test_inode >= 0 && file_size > 0) {
        print_test_header("Read from file");
        results.total++;
        int bytes_read = read(test_inode, read_buffer, file_size, 0);
        if (bytes_read == file_size) {
            read_buffer[bytes_read] = '\0';
            printf("Read %d bytes from inode %d: '%s'\n", bytes_read, test_inode, read_buffer);
            if (log_file) fprintf(log_file, "Read %d bytes from inode %d: '%s'\n", bytes_read, test_inode, read_buffer);
            if (memcmp(read_buffer, test_data, bytes_read) == 0) {
                printf("Data verification successful\n");
                if (log_file) fprintf(log_file, "Data verification successful\n");
                results.passed++;
            } else {
                printf("Data verification failed: got '%s', expected '%s'\n",
                       read_buffer, test_data);
                if (log_file) fprintf(log_file, "Data verification failed: got '%s', expected '%s'\n",
                       read_buffer, test_data);
                results.failed++;
            }
        } else {
            printf("Read failed with error code: %d\n", bytes_read);
            if (log_file) fprintf(log_file, "Read failed with error code: %d\n", bytes_read);
            results.failed++;
        }
        print_test_result("Read from file", bytes_read == file_size &&
                          memcmp(read_buffer, test_data, bytes_read) == 0, bytes_read);
    }

    // Test 8: Append to file
    if (test_inode >= 0 && file_size > 0) {
        print_test_header("Append to file");
        results.total++;
        int data_len = strlen(additional_data);
        int bytes_written = write(test_inode, (uint8_t*)additional_data, data_len, file_size);
        if (bytes_written == data_len) {
            printf("Appended %d bytes to inode %d\n", bytes_written, test_inode);
            if (log_file) fprintf(log_file, "Appended %d bytes to inode %d\n", bytes_written, test_inode);
            results.passed++;

            // Update file size and verify full contents
            file_size = stat(test_inode);
            if (file_size > 0) {
                display_file_contents(test_inode, file_size);
            }
        } else {
            printf("Append failed with error code: %d\n", bytes_written);
            if (log_file) fprintf(log_file, "Append failed with error code: %d\n", bytes_written);
            results.failed++;
        }
        print_test_result("Append to file", bytes_written == data_len, bytes_written);
    }

    // Test 9: Delete the second file
    if (test_inode2 >= 0) {
        print_test_header("Delete file");
        results.total++;
        result = delete(test_inode2);
        if (result == 0) {
            printf("File with inode %d deleted successfully\n", test_inode2);
            if (log_file) fprintf(log_file, "File with inode %d deleted successfully\n", test_inode2);
            results.passed++;
        } else {
            printf("File deletion failed with error code: %d\n", result);
            if (log_file) fprintf(log_file, "File deletion failed with error code: %d\n", result);
            results.failed++;
        }
        print_test_result("Delete file", result == 0, result);
    }

    // Test 10: Create a file after deletion (should reuse the deleted inode)
    print_test_header("Create file after deletion");
    results.total++;
    int recycled_inode = create();
    if (recycled_inode >= 0) {
        printf("New file created with inode number: %d\n", recycled_inode);
        if (log_file) fprintf(log_file, "New file created with inode number: %d\n", recycled_inode);
        if (recycled_inode == test_inode2) {
            printf("Successfully recycled the deleted inode\n");
            if (log_file) fprintf(log_file, "Successfully recycled the deleted inode\n");
        } else {
            printf("Created new inode instead of recycling\n");
            if (log_file) fprintf(log_file, "Created new inode instead of recycling\n");
        }
        results.passed++;
    } else {
        printf("File creation failed with error code: %d\n", recycled_inode);
        if (log_file) fprintf(log_file, "File creation failed with error code: %d\n", recycled_inode);
        results.failed++;
    }
    print_test_result("Create file after deletion", recycled_inode >= 0, recycled_inode);

    // Test 11: Unmount
    print_test_header("Unmount");
    results.total++;
    result = unmount();
    if (result == 0) {
        printf("Disk unmounted successfully\n");
        if (log_file) fprintf(log_file, "Disk unmounted successfully\n");
        results.passed++;
    } else {
        printf("Unmount failed with error code: %d\n", result);
        if (log_file) fprintf(log_file, "Unmount failed with error code: %d\n", result);
        results.failed++;
    }
    print_test_result("Unmount disk", result == 0, result);

    // Test 12: Remount and verify file persistence
    print_test_header("Remount and verify persistence");
    results.total++;
    result = mount((char*)disk_name);
    if (result == 0) {
        printf("Disk '%s' remounted successfully\n", disk_name);
        if (log_file) fprintf(log_file, "Disk '%s' remounted successfully\n", disk_name);

        if (test_inode >= 0) {
            file_size = stat(test_inode);
            if (file_size > 0) {
                printf("File with inode %d still exists with size: %d bytes\n", test_inode, file_size);
                if (log_file) fprintf(log_file, "File with inode %d still exists with size: %d bytes\n", test_inode, file_size);
                display_file_contents(test_inode, file_size);
                results.passed++;
            } else {
                printf("File data persistence test failed: stat returned %d\n", file_size);
                if (log_file) fprintf(log_file, "File data persistence test failed: stat returned %d\n", file_size);
                results.failed++;
            }
        }
    } else {
        printf("Remount failed with error code: %d\n", result);
        if (log_file) fprintf(log_file, "Remount failed with error code: %d\n", result);
        results.failed++;
    }
    print_test_result("Remount and verify persistence", result == 0 && file_size > 0, result);

    // Final unmount
    unmount();

    return results;
}

int main(void) {
    // Open log file
    log_file = fopen(LOG_FILE, "w");
    if (!log_file) {
        printf("Warning: Could not open log file %s. Continuing without logging.\n", LOG_FILE);
    } else {
        printf("Logging test results to %s\n", LOG_FILE);
    }

    printf("%sFile System Testing Suite%s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    printf("=======================\n\n");

    TestResults basic_results = run_basic_tests();

    // Print final summary
    printf("\n\n%s==== FINAL TEST SUMMARY ====%s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET);
    printf("Basic Tests: %d/%d passed (%.1f%%)\n",
           basic_results.passed, basic_results.total,
           (basic_results.passed * 100.0) / basic_results.total);
    print_test_summary(basic_results);

    if (log_file) {
        fprintf(log_file, "\n\n==== FINAL TEST SUMMARY ====\n");
        fprintf(log_file, "Basic Tests: %d/%d passed (%.1f%%)\n",
               basic_results.passed, basic_results.total,
               (basic_results.passed * 100.0) / basic_results.total);

        fclose(log_file);
        printf("\nTest results saved to %s\n", LOG_FILE);
    }

    return basic_results.failed > 0 ? 1 : 0;
}
