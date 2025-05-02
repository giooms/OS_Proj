FLAGS = -Wall -pedantic -std=c99 -Wextra -D_POSIX_SOURCE

# Target executable name
TARGET = fs_test

# Source files
SRCS = main.c fs.c fs_helpers.c vdisk/vdisk.c error.c

# Object files (automatically generated from source files)
OBJS = $(SRCS:.c=.o)

# Libraries needed for linking
LIBS = -lbsd

# Default target
all: $(TARGET)

# Linking rule
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

# Pattern rule for object files
%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
