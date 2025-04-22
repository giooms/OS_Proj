#ifndef __PERF_EXAMPLE_H
#define __PERF_EXAMPLE_H

#define TASK_COMM_LEN 20

struct struct_to_give_to_perf {
    int pid;
    char message[TASK_COMM_LEN];
};

#endif /* __PERF_EXAMPLE_H */
