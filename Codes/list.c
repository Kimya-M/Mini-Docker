#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define PROC_PATH "/proc/"
#define STATUS_FILE "/status"
#define MAX_PATH 256
#define MAX_LINE 256

// Function to get the parent PID of a process
pid_t get_parent_pid(pid_t pid) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *file;
    pid_t ppid = -1;
    
    snprintf(path, sizeof(path), "%s%d%s", PROC_PATH, pid, STATUS_FILE);
    file = fopen(path, "r");
    if (!file) return -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "PPid:", 5) == 0) {
            sscanf(line, "PPid:\t%d", &ppid);
            break;
        }
    }
    
    fclose(file);
    return ppid;
}

// Function to get process name and state from status file
int get_process_info(pid_t pid, char *name, size_t name_size, char *state, size_t state_size) {
    char path[MAX_PATH], line[MAX_LINE];
    FILE *file;
    
    snprintf(path, sizeof(path), "%s%d%s", PROC_PATH, pid, STATUS_FILE);
    file = fopen(path, "r");
    if (!file) return -1;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "Name:", 5) == 0) {
            sscanf(line, "Name:\t%255s", name);
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line, "State:\t%255s", state);
        }
    }
    
    fclose(file);
    return 0;
}

// Function to check if a process is a child of the given parent PID
void find_child_processes(pid_t parent_pid) {
    struct dirent *entry;
    DIR *dir = opendir(PROC_PATH);
    if (!dir) {
        perror("opendir");
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;
        
        pid_t pid = atoi(entry->d_name);
        if (get_parent_pid(pid) == parent_pid) {
            char name[MAX_LINE], state[MAX_LINE];
            if (get_process_info(pid, name, sizeof(name), state, sizeof(state)) == 0) {
                printf("PID: %d, Name: %s, State: %s, Parent PID: %d\n", pid, name, state, parent_pid);
            }
        }
    }
    
    closedir(dir);
}

// Function to find PID of a process by name
pid_t get_pid_by_name(const char *proc_name) {
    struct dirent *entry;
    DIR *dir = opendir(PROC_PATH);
    if (!dir) {
        perror("opendir");
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) continue;
        
        pid_t pid = atoi(entry->d_name);
        char name[MAX_LINE], state[MAX_LINE];
        if (get_process_info(pid, name, sizeof(name), state, sizeof(state)) == 0) {
            if (strcmp(name, proc_name) == 0) {
                closedir(dir);
                return pid;
            }
        }
    }
    
    closedir(dir);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <process_name>\n", argv[0]);
        return 1;
    }
    
    pid_t parent_pid = get_pid_by_name(argv[1]);
    if (parent_pid == -1) {
        fprintf(stderr, "Process %s not found.\n", argv[1]);
        return 1;
    }
    
    printf("Children of process %s (PID: %d):\n", argv[1], parent_pid);
    find_child_processes(parent_pid);
    
    return 0;
}
