CFLAGS = -Wall -Wextra -g -std=gnu99 -U_FORTIFY_SOURCE
LDFLAGS = -lbsd -Wl,--hash-style=both -static-libgcc
INCLUDE = -Iinclude

SRCS = main.c fs.c error.c vdisk/vdisk.c
OBJS = $(SRCS:.c=.o)
TARGET = fs_test

all: $(OBJS)
	gcc -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean