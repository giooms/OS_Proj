#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void fork_bomb(int depth, int max_depth) {
    printf("Depth %d\n", depth);

    if (depth >= max_depth) {
        return;  // Stop recursion once max depth is reached
    }

    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 == 0) {
        fork_bomb(depth + 1, max_depth);
        exit(0);
    }

    pid2 = fork();
    if (pid2 == 0) {
        fork_bomb(depth + 1, max_depth);
        exit(0);
    }

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

int main() {
    int max_depth = 12;
    fork_bomb(0, max_depth);
    return 0;
}
