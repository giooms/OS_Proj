// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "page_fault.h"


char LICENSE[] SEC("license") = "Dual BSD/GPL";


// ****************************************
// Methodo
// ****************************************

/* Need to do 3 things:
 * 1. Probe to kernel-space handle_mm_fault() to detect page faults. When hook gets triggered -> update the `mapping` map defined below.
 * 2. Hook into `mapping` updates to check if a pid has `page_fault_count % log_step == 0`, meaning it is a multiple of `log_step`. If this is the case, go to 3.
 * 3. Trigger a perf buffer output in the console, using the `page_fault_event_out`struct defined below.
 */

/* EDIT:
 * Strategy above is not possible as point 2. is not possible: can't directly hook into BPF map updates.
 * Solution is to integrate this part directly in the hook setup for point 1, which indeed makes more sense as either way we would have wanted to perform the check for all updates. So let's perform the check at the same place as we do the update!
 */


// ****************************************
// Global variable
// ****************************************

const volatile int log_step = 50;


// ****************************************
// Data structures
// ****************************************

// Tuple to act as the values of the HASH map used to track fault counts for each pid
struct fault_tracking {
    unsigned int pg_fault_count;     // Curr fault count
    char comm[TASK_COMM_LEN];      // Process name
};

// The hash map to track the fault_count of a pid
// map[pid]=(pg_fault_count, process name)
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, __u32); // PID as keys
    __type(value, struct fault_tracking);   // custom `fault_tracking`struct as values
   } mapping SEC(".maps");

// The perf buffer to send notifs to userspace
struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __uint(key_size, sizeof(__u32));
    __uint(value_size, sizeof(__u32));
} events SEC(".maps");


// ****************************************
// Custom helpers
// ****************************************

int buffer_out(void *ctx, int pid)
{
    // retrieve data associated to given pid
    struct fault_tracking *ft = bpf_map_lookup_elem(&mapping, &pid);
    if (ft)
    {
        // create the out object following `page_fault_event_out` structure
        struct page_fault_event_out out_this = {0};
        out_this.pid = pid;
        out_this.page_fault_count = ft->pg_fault_count;

        bpf_probe_read_str(&out_this.comm, sizeof(out_this.comm), ft->comm);

        // output this
        bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &out_this, sizeof(out_this));
        return 0;
    }

    // failure
    return -1;
}


// ****************************************
// Hooks
// ****************************************

// @ https://github.com/torvalds/linux/blob/v6.8/mm/memory.c#L5438
SEC("kprobe/handle_mm_fault")
int BPF_KPROBE(handle_mm_fault, struct vm_area_struct *vma, unsigned long address,
               unsigned int flags, struct pt_regs *regs)
{
    // get current pid (shifted by 32 bits to exlude tgid)
    pid_t pid;
    pid = bpf_get_current_pid_tgid() >> 32;

    // find in `mapping` if pid entry already exists, else add it
    struct fault_tracking *ft = bpf_map_lookup_elem(&mapping, &pid);
    if (ft == NULL) {
        // entry does not exist yet in the mapping, so:
        // 1. init new `fault_tracking` tuple for the pid
        struct fault_tracking new_ft = {0};
        new_ft.pg_fault_count = 1;

        // 2. get process name and add it to the `fault_tracking` tuple
        char comm[TASK_COMM_LEN];
        bpf_get_current_comm(&comm, sizeof(comm));
        bpf_probe_read_str(&new_ft.comm, sizeof(new_ft.comm), comm);

        // 3. update the mapping with the new entry
        bpf_map_update_elem(&mapping, &pid, &new_ft, BPF_ANY);

        // then we check if log_step == 1 because if it is, we already need to log in the console
        if (log_step == 1)
        {
            buffer_out(ctx, pid);
        }
    } else {
        // entry already exists -> increment fault count
        ft->pg_fault_count++;

        // check if new pg_fault_count is a multiple of log_step
        if (ft->pg_fault_count % log_step == 0)
        {
            buffer_out(ctx, pid);
        }
    }
    
    return 0;
}
