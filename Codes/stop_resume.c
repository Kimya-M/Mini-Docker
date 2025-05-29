#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void usage() {
    printf("Usage: ./process_control -p <pid>  -> To stop the process\n");
    printf("       ./process_control -r <pid>  -> To resume the process\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage();
        return 1;
    }

    int pid = atoi(argv[2]);
    if (pid <= 0) {
        printf("Invalid PID\n");
        return 1;
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (kill(pid, SIGSTOP) == -1) {
            perror("Failed to stop the process");
            return 1;
        }
        printf("Process with PID %d has been stopped.\n", pid);
    }
    else if (strcmp(argv[1], "-r") == 0) {
        if (kill(pid, SIGCONT) == -1) {
            perror("Failed to resume the process");
            return 1;
        }
        printf("Process with PID %d has been resumed.\n", pid);
    }
    else {
        usage();
        return 1;
    }

    return 0;
}
