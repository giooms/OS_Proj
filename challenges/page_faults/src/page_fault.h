#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#define TASK_COMM_LEN 20


// Struct sent to the perf buffer for printing
struct page_fault_event_out {
    pid_t pid;
    char comm[TASK_COMM_LEN];
    int page_fault_count;
};


#endif // PAGE_FAULT_H