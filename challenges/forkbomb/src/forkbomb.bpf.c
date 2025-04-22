#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

// Event structure for outputting to userspace
struct event {
    u32 pid;
    u32 ppid;
    char comm[16];
    char ancestor_comm[16];
    u64 time_delta_ns;
    bool killed;
};

// Global variables that can be configured from command line
const volatile int ancestor_separations = 3;
const volatile long time_separation_ns = 1000000000;

// Map to store process creation times
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, u32);
    __type(value, u64);
} process_start_times SEC(".maps");

// Helper function to get process name
static inline void get_comm(struct task_struct *task, char *comm) {
    bpf_probe_read_kernel_str(comm, 16, task->comm);
}

// Function to find ancestor at specified generation depth
static struct task_struct *find_ancestor(struct task_struct *task, int depth) {
    struct task_struct *current_task = task;
    struct task_struct *parent;

    // Limit loop iterations for BPF verifier
    #pragma unroll
    for (int i = 0; i < 10; i++) {
        if (i >= depth)
            break;
        
        // Get parent task
        bpf_probe_read_kernel(&parent, sizeof(parent), &current_task->real_parent);
        if (!parent)
            return NULL;
        
        current_task = parent;
    }
    
    return current_task;
}

// Check if current process has same name as its ancestor
static bool has_same_name_as_ancestor(struct task_struct *task, int depth) {
    char current_comm[16] = {};
    char ancestor_comm[16] = {};
    struct task_struct *ancestor;
    
    // Get current process name
    get_comm(task, current_comm);
    
    // Find ancestor
    ancestor = find_ancestor(task, depth);
    if (!ancestor)
        return false;
    
    // Get ancestor process name
    get_comm(ancestor, ancestor_comm);
    
    // Compare names
    #pragma unroll
    for (int i = 0; i < 16; i++) {
        if (current_comm[i] != ancestor_comm[i])
            return false;
        if (current_comm[i] == '\0')
            break;
    }
    
    return true;
}

// Hook clone system call entry
SEC("tracepoint/syscalls/sys_enter_clone")
int trace_clone_enter(struct trace_event_raw_sys_enter *ctx) {
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    u64 current_time = bpf_ktime_get_ns();
    struct task_struct *current_task;
    struct task_struct *ancestor;
    u32 ancestor_pid;
    u64 *ancestor_start_time_ptr;
    u64 time_delta;
    bool killed = false;
    struct event e = {};
    
    // Store current time for this process
    bpf_map_update_elem(&process_start_times, &pid, &current_time, BPF_ANY);
    
    // Get current process information
    current_task = (struct task_struct *)bpf_get_current_task();
    if (!current_task)
        return 0;
    
    // Check if process has same name as ancestor
    if (!has_same_name_as_ancestor(current_task, ancestor_separations))
        return 0;
    
    // Find the ancestor process
    ancestor = find_ancestor(current_task, ancestor_separations);
    if (!ancestor)
        return 0;
    
    // Get ancestor's pid
    ancestor_pid = BPF_CORE_READ(ancestor, tgid);
    
    // Get ancestor's start time
    ancestor_start_time_ptr = bpf_map_lookup_elem(&process_start_times, &ancestor_pid);
    if (!ancestor_start_time_ptr)
        return 0;
    
    // Calculate time difference between process creation times
    if (current_time >= *ancestor_start_time_ptr) {
        time_delta = current_time - *ancestor_start_time_ptr;
    } else {
        time_delta = *ancestor_start_time_ptr - current_time;
    }
    
    // Kill process if it's spawning too quickly after its ancestor with same name
    if (time_delta < time_separation_ns) {
        bpf_send_signal(9); // SIGKILL
        killed = true;
        
        // Log something basic when we kill a process
        bpf_printk("Killed fork bomb process: pid=%d name=%s", pid, e.comm);
    }
    
    // Prepare event for user space
    e.pid = pid;
    e.ppid = BPF_CORE_READ(current_task, real_parent, tgid);
    get_comm(current_task, e.comm);
    get_comm(ancestor, e.ancestor_comm);
    e.time_delta_ns = time_delta;
    e.killed = killed;
    
    // Log event details
    bpf_printk("Process: pid=%d name=%s, ancestor=%s, time_delta=%llu, killed=%d", 
               pid, e.comm, e.ancestor_comm, time_delta, killed);
    
    return 0;
}

// Hook clone system call exit
SEC("tracepoint/syscalls/sys_exit_clone")
int trace_clone_exit(struct trace_event_raw_sys_exit *ctx) {
    return 0;
}