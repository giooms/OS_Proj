// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "perf_example.h"

struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u32));
} perf_map SEC(".maps");

SEC("tracepoint/syscalls/sys_enter_execve")
int tracepoint_syscalls_sys_enter_execve(struct trace_event_raw_sys_enter* ctx)
{
    pid_t pid;
    struct struct_to_give_to_perf struct_perf = {0};

    struct_perf.pid = bpf_get_current_pid_tgid() >> 32;
    char *message = "New process created";

    bpf_probe_read_str(&struct_perf.message, sizeof(struct_perf.message), message);
    bpf_perf_event_output(ctx, &perf_map, BPF_F_CURRENT_CPU, &struct_perf, sizeof(struct_perf));
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
