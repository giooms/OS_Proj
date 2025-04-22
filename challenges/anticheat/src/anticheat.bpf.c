// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

// Hook the read_guesses function with a uprobe
SEC("uretprobe/hangman/hangman:read_guesses")
int BPF_KRETPROBE(uretprobe_read_guesses, int ret)
{
    // Check if the number of guesses is different from 6
    if (ret != 6) {
        // Kill the process with SIGKILL (signal 9)
        bpf_send_signal(9);
    }

    return 0;
}
