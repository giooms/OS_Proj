# ==============================================================================
# Makefile for SSFS Project
# ==============================================================================

# Compiler to use
CC = gcc

# Compiler flags:
# -Wall -Wextra: Enable common and extra compiler warnings
# -g: Include debugging information
CFLAGS = -Wall -Wextra -g

# Linker flags:
# -lbsd: Link with libbsd for functions like strlcpy used in vdisk.c
LDFLAGS = -lbsd

# Include directory for header files
INCLUDE_DIR = include

# Source files
# Assumes Makefile is in the 'src' directory
SRCS = main.c fs.c error.c vdisk/vdisk.c

# Object files: automatically generated from source files (.o replace .c)
OBJS = $(SRCS:.c=.o)

# Name of the final executable
TARGET = fs_test

# Phony targets: targets that do not represent actual files
.PHONY: all clean

# Default target: build the executable
all: $(TARGET)

# Rule to link object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

# Generic rule for compiling .c files in the current directory (src/)
# $<: the prerequisite file (the .c file)
# $@: the target file (the .o file)
%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Rule for compiling .c files in the vdisk/ subdirectory
vdisk/%.o: vdisk/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Clean target: remove generated object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)

# End of Makefile
# ==============================================================================