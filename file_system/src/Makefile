# ==============================================================================
# Makefile for SSFS Project
# ==============================================================================

# Compiler to use
CC = gcc

# Compiler flags:
CFLAGS = -Wall -Wextra -g

# Linker flags:
LDFLAGS = -lbsd

# Include directory for header files
INCLUDE_DIR = include

# Source files
SRCS = main.c fs.c error.c vdisk/vdisk.c

# Object files
OBJS = $(SRCS:.c=.o)

# Name of the final executable
TARGET = fs_test

# Phony targets
.PHONY: all clean

# Default target
all: $(TARGET)

# Rule to link object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

# Rule for compiling .c files in the src/ dir
%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Rule for compiling .c files in the vdisk/ subdir
vdisk/%.o: vdisk/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJS) $(TARGET)

# End of Makefile
# ==============================================================================