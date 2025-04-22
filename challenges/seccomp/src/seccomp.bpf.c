// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif

#ifndef EPERM
#define EPERM 1
#endif

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("lsm/file_open")
int BPF_PROG(lsm_file_open, struct file *file)
{
    // get curr process name to see if 'malicious'
    char comm[TASK_COMM_LEN];

    // @ https://github.com/torvalds/linux/blob/v6.8/include/uapi/linux/bpf.h#L2012
    int ret = bpf_get_current_comm(comm, TASK_COMM_LEN);

    // check if bpf_get_current_comm failed
    if (ret < 0) {
        return 0;   // hesitation to return the -EPERM as additional safeguard but might block certain legit fopen calls
    }

    // it didn't fail so now, compare with 'malicious' str
    // @ https://github.com/torvalds/linux/blob/v6.8/include/uapi/linux/bpf.h#L5273
    if (bpf_strncmp(comm, sizeof(comm), "malicious") == 0) {
        return -EPERM; // Op not permitted
    }

    return 0;
}