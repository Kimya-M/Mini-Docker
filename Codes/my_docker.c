#define _GNU_SOURCE 
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/mount.h> 
#include <fcntl.h> 
#include <errno.h> 
#include <sched.h> 
#include <string.h> 
#include <dirent.h> 
#include <sys/wait.h> 
#include <pthread.h>
#include <libgen.h>


 
#define STACK_SIZE (1024 * 1024) // Stack size for child process 
#define BASE_DIR "/tmp"          // Base directory for root filesystems 
#define CGROUP_BASE "/sys/fs/cgroup" // Base cgroup directory
#define MAX_FILES 100  // Adjust based on how many files you expect
#define CPU_NUMBERS 4 // number of cpus
#define NUM_THREADS 4  // Adjust based on CPU cores
#define SHARE_MOUNT 1
#define ICP 0

pthread_mutex_t lock;

typedef struct {
    char *object_file_path;
    char *root;
    int cpu_number;
} ThreadArgs;
 
// Function to generate the next unique directory under /tmp 
char *generate_unique_root_dir() { 
    static char dir_name[256]; 
    char suffix = 'a'; 
 
    while (suffix <= 'z') { 
        snprintf(dir_name, sizeof(dir_name), "%s/%c", BASE_DIR, suffix); 
 
        // Check if the directory already exists
        sleep(1);
        struct stat st; 
        if (stat(dir_name, &st) != 0) { // If the directory doesn't exist 
            if (errno == ENOENT) { 
                mkdir(dir_name, 0755); // Create the directory 
                return dir_name; 
            } 
        } 
        suffix++; 
    } 
 
    fprintf(stderr, "Error: No available root directory names under %s\n", BASE_DIR); 
    return NULL; 
} 

// Function to write to cgroup files
int write_to_cgroup(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("Error: Failed to open cgroup file");
        return -1;
    }
    if (write(fd, value, strlen(value)) < 0) {
        perror("Error: Failed to write to cgroup file");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// Function to set up cgroups for resource limitations
int setup_cgroups(pid_t pid, int cpu_limit, int memory_limit, int io_read_limit, int io_write_limit, const char *device) {
    char cgroup_path[256];
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    // Create a unique cgroup
    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/cpu/container_%d", pid);
    if (mkdir(cgroup_path, 0755) < 0 && errno != EEXIST) {
        perror("Error: Failed to create CPU cgroup");
        return -1;
    }
    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/memory/container_%d", pid);
    if (mkdir(cgroup_path, 0755) < 0 && errno != EEXIST) {
        perror("Error: Failed to create memory cgroup");
        return -1;
    }
    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/blkio/container_%d", pid);
    if (mkdir(cgroup_path, 0755) < 0 && errno != EEXIST) {
        perror("Error: Failed to create io cgroup");
        return -1;    
    }

    // Set CPU limit
    if (cpu_limit > 0) {
        snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/cpu/container_%d/cpu.cfs_quota_us", pid);
        char cpu_quota[32];
        snprintf(cpu_quota, sizeof(cpu_quota), "%d", cpu_limit * 1000); // Convert milliseconds to microseconds
        if (write_to_cgroup(cgroup_path, cpu_quota) < 0) return -1;
    }

    // Set memory limit
    if (memory_limit > 0) {
        snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/memory/container_%d/memory.limit_in_bytes", pid);
        char memory_limit_str[32];
        snprintf(memory_limit_str, sizeof(memory_limit_str), "%d", memory_limit);
        if (write_to_cgroup(cgroup_path, memory_limit_str) < 0) return -1;
    }

    // Set I/O limits
    if (io_read_limit > 0 || io_write_limit > 0) {
        snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/blkio/container_%d", pid);
        mkdir(cgroup_path, 0755);

        // Apply read limit
        if (io_read_limit > 0) {
            snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/blkio/container_%d/blkio.throttle.read_bps_device", pid);
            char read_limit_str[64];
            snprintf(read_limit_str, sizeof(read_limit_str), "%s %d", device, io_read_limit);
            write_to_cgroup(cgroup_path, read_limit_str);
        }

        // Apply write limit
        if (io_write_limit > 0) {
            snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/blkio/container_%d/blkio.throttle.write_bps_device", pid);
            char write_limit_str[64];
            snprintf(write_limit_str, sizeof(write_limit_str), "%s %d", device, io_write_limit);
            write_to_cgroup(cgroup_path, write_limit_str);
        }
    }

    // Add the process to the cgroups
    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/cpu/container_%d/cgroup.procs", pid);
    if (write_to_cgroup(cgroup_path, pid_str) < 0) return -1;

    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/memory/container_%d/cgroup.procs", pid);
    if (write_to_cgroup(cgroup_path, pid_str) < 0) return -1;

    snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/blkio/container_%d/cgroup.procs", pid);
    if (write_to_cgroup(cgroup_path, pid_str) < 0) return -1;

    return 0;
}

// Function to recursively delete a directory and its contents 
int delete_directory(const char *path) { 
    struct stat st; 
    struct dirent *entry; 
    DIR *dir = opendir(path); 
 
    if (!dir) { 
        perror("Error: Failed to open directory"); 
        return -1; 
    } 
 
    while ((entry = readdir(dir)) != NULL) { 
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { 
            continue; // Skip current and parent directory 
        } 
 
        char full_path[1024]; 
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name); 
 
        if (stat(full_path, &st) == 0) { 
            if (S_ISDIR(st.st_mode)) { 
                // Recursively delete subdirectories 
                if (delete_directory(full_path) != 0) { 
                    closedir(dir); 
                    return -1; 
                } 
            } else { 
                // Delete files 
                if (unlink(full_path) != 0) { 
                    perror("Error: Failed to delete file"); 
                    closedir(dir); 
                    return -1; 
                } 
            } 
        } 
    } 
 
    closedir(dir); 
 
    // Delete the directory itself 
    if (rmdir(path) != 0) { 
        perror("Error: Failed to delete directory"); 
        return -1; 
    } 
 
    return 0; 
} 
 
// Function executed in the new namespaces 
int containerized_process(void *arg) { 
    ThreadArgs* args = (ThreadArgs*)arg;
    const char *object_file_path = args->object_file_path;
    const char *new_root = args->root; 
    int cpu_number = args->cpu_number % 4;

    // Generate a unique root directory for this container
    // pthread_mutex_lock(&lock);
    // char *new_root = generate_unique_root_dir(); 
    // printf("root %s\n", new_root);
    // if (!new_root) { 
    //     pthread_exit(NULL);
    // }
    // pthread_mutex_unlock(&lock);

    // Create necessary subdirectories in the new root 
    // char proc_path[256]; 
    // snprintf(proc_path, sizeof(proc_path), "%s/proc", new_root); 
    // if (mkdir(proc_path, 0755) < 0 && errno != EEXIST) { 
    //     perror("Error: Failed to create /proc in the new root"); 
    //     pthread_exit(NULL);
    // } 

    // Copy the object file to the new root directory 
    // char new_object_path[256]; 
    // char command[1024];
    // snprintf(new_object_path, sizeof(new_object_path), "%s/%s", new_root, basename(object_file_path));
    // snprintf(command, sizeof(command), "sudo cp %s %s", object_file_path, new_object_path);
    // system(command);

    // // Independencies
    // snprintf(command, sizeof(command), "sudo mkdir -p %s/usr/bin", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "sudo cp /usr/bin/stress %s/usr/bin/", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "mkdir -p %s/lib/x86_64-linux-gnu", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "mkdir -p %s/lib64", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "sudo cp /lib/x86_64-linux-gnu/libc.so.6 %s/lib/x86_64-linux-gnu", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "sudo cp /lib/x86_64-linux-gnu/libm.so.6 %s/lib/x86_64-linux-gnu", new_root);
    // system(command);
    // snprintf(command, sizeof(command), "sudo cp /lib64/ld-linux-x86-64.so.2 %s/lib64", new_root);
    // system(command);

    // Set the NEW_ROOT environment variable for the child process 
    if (setenv("NEW_ROOT", new_root, 1) < 0) { 
        perror("Error: Failed to set NEW_ROOT environment variable"); 
        pthread_exit(NULL);
    } 
 
    // Get the unique root directory from the parent process 
    const char *root_dir = getenv("NEW_ROOT"); 
    if (!root_dir) { 
        fprintf(stderr, "Error: NEW_ROOT environment variable not set\n"); 
        return EXIT_FAILURE; 
    } 
 
    // Change the root to the new filesystem 
    if (chroot(root_dir) != 0) { 
        perror("Error: Failed to chroot to the new root"); 
        return EXIT_FAILURE; 
    } 
 
    // Change the working directory to the new root 
    if (chdir("/") != 0) { 
        perror("Error: Failed to change directory to new root"); 
        return EXIT_FAILURE; 
    } 
 
    // Mount a new proc filesystem
    if (mount("proc", "/proc", "proc", 0, NULL) < 0) { 
        perror("Error: Failed to mount /proc in new root"); 
        return EXIT_FAILURE; 
    }
     
    if (SHARE_MOUNT) {
        if (args->cpu_number == 0) {
            if (mount(NULL, "/proc", NULL, MS_SHARED, NULL) == -1) {
                perror("mount --make-shared");
                exit(EXIT_FAILURE);
            }
        }
        else {
            if (mount("/proc", "/proc", NULL, MS_BIND, NULL) == -1) {
                perror("mount --bind");
                exit(EXIT_FAILURE);
            }
        }
    }
 
    // Set a unique hostname for the UTS namespace 
    if (sethostname("container", strlen("container")) < 0) { 
        perror("Error: Failed to set hostname"); 
        return EXIT_FAILURE; 
    } 
 
    // Execute the object file 
    printf("Running object file in an isolated container...\n"); 
    
    if (access(object_file_path, F_OK) != 0) { 
        printf("tooy container Error: Object file does not exist, %s\n", object_file_path); 
        return EXIT_FAILURE; 
    }

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_number, &cpu_set);
    pid_t pid = getpid();
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) == -1) {
        perror("sched_setaffinity");
        return EXIT_FAILURE;
    }

    sleep(2);
    execl(object_file_path, object_file_path, (char *)NULL);      
    // If execl returns, it means execution failed 
    perror("Error: Execution failed"); 
    return EXIT_FAILURE; 
}

int main(int argc, char *argv[]) { 
    if (argc < 2) {  // Ensure at least one file name is provided
        printf("Usage: %s <file1> <file2> ... [-c <cpu_limit> <memory_limit> <io_read_limit> <io_write_limit>]\n", argv[0]);
        return 1;
    }

    // File names array
    char *files[MAX_FILES];
    int num_files = 0;

    // Resource limits, default to -1
    int cpu_limit = -1;
    int memory_limit = -1;
    int io_read_limit = -1;
    int io_write_limit = -1;

    // Parse command-line arguments
    int i = 1;
    while (i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == 'c') {  // If we encounter the '-c' flag
            // Ensure we have enough arguments left for the limits
            if (i + 4 >= argc) {
                printf("Error: Missing resource limits after '-c' flag.\n");
                return 1;
            }
            // Set resource limits from the command-line arguments
            cpu_limit = atoi(argv[i + 1]);
            memory_limit = atoi(argv[i + 2]);
            io_read_limit = atoi(argv[i + 3]);
            io_write_limit = atoi(argv[i + 4]);

            i += 5;  // Skip the '-c' and its limits
        } else {
            // Add file names to the array
            files[num_files] = argv[i];
            num_files++;
            i++;
        }
    }

    pthread_t threads[num_files];
    ThreadArgs args[num_files];
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("Mutex initialization failed\n");
        return 1;
    }

    pid_t childs[100];
    char *roots[100];
    char *stacks[100];

    for (int j = 0; j < num_files; j++) {
        char *object_file_path = files[j];
        const char *device = "8:0";         // Example device (adjust based on actual system) 

        char command[1024];
        // Check if the object file exists and is executable 
        if (access(object_file_path, F_OK) != 0) { 
            perror("Error: Object file does not exist"); 
            return EXIT_FAILURE; 
        } 
        if (access(object_file_path, X_OK) != 0) { 
            perror("Error: Object file is not executable"); 
            return EXIT_FAILURE; 
        } 
    
        // Generate a unique root directory for this container 
        pthread_mutex_lock(&lock);
        char *new_root = generate_unique_root_dir(); 
        if (!new_root) { 
            return EXIT_FAILURE; 
        } 
        pthread_mutex_unlock(&lock);
        roots[j] = new_root;
    
        // Create necessary subdirectories in the new root 
        char proc_path[256]; 
        snprintf(proc_path, sizeof(proc_path), "%s/proc", new_root); 
        if (mkdir(proc_path, 0755) < 0 && errno != EEXIST) { 
            perror("Error: Failed to create /proc in the new root"); 
            return EXIT_FAILURE; 
        } 
    
        // Copy the object file to the new root directory 
        char new_object_path[256]; 
        snprintf(new_object_path, sizeof(new_object_path), "%s/%s", new_root, basename(object_file_path));
        snprintf(command, sizeof(command), "sudo cp %s %s", object_file_path, new_object_path);
        system(command);

        // independencies
        snprintf(command, sizeof(command), "sudo mkdir -p %s/usr/bin", new_root);
        system(command);
        snprintf(command, sizeof(command), "sudo cp /usr/bin/stress %s/usr/bin/", new_root);
        system(command);
        snprintf(command, sizeof(command), "mkdir %s/lib", new_root);
        system(command);
        snprintf(command, sizeof(command), "mkdir %s/lib/x86_64-linux-gnu", new_root);
        system(command);
        snprintf(command, sizeof(command), "mkdir %s/lib64", new_root);
        system(command);
        snprintf(command, sizeof(command), "sudo cp /lib/x86_64-linux-gnu/libc.so.6 %s/lib/x86_64-linux-gnu", new_root);
        system(command);
        snprintf(command, sizeof(command), "sudo cp /lib/x86_64-linux-gnu/libm.so.6 %s/lib/x86_64-linux-gnu", new_root);
        system(command);
        snprintf(command, sizeof(command), "sudo cp /lib64/ld-linux-x86-64.so.2 %s/lib64", new_root);
        system(command);
    
        // Set the NEW_ROOT environment variable for the child process 
        // if (setenv("NEW_ROOT", new_root, 1) < 0) { 
        //     perror("Error: Failed to set NEW_ROOT environment variable"); 
        //     return EXIT_FAILURE; 
        // } 
    
        // Allocate stack for the child process 
        char *stack = malloc(STACK_SIZE); 
        if (!stack) { 
            perror("Error: Failed to allocate memory for stack"); 
            return EXIT_FAILURE; 
        }
        char *stack_top = stack + STACK_SIZE; 
        stacks[j] = stack;
    
        // Create a new process in isolated namespaces 
        int clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD; 
        if (ICP) {
            clone_flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD;
        }
        args[j].root = new_root;
        args[j].object_file_path = object_file_path;
        args[j].cpu_number = j;
        pid_t child_pid = clone(containerized_process, stack_top, clone_flags, &args[j]);
        childs[j] = child_pid;

        // cpu_set_t cpu_set;
        // CPU_ZERO(&cpu_set);
        // CPU_SET(j % CPU_NUMBERS, &cpu_set);
        // if (sched_setaffinity(child_pid, sizeof(cpu_set_t), &cpu_set) == -1) {
        //     perror("sched_setaffinity");
        //     return EXIT_FAILURE;
        // }
    
        if (child_pid < 0) { 
            perror("Error: Failed to create a new namespace with clone"); 
            free(stack); 
            return EXIT_FAILURE; 
        }

        printf("PID of child: %d\n", child_pid);

        if (setup_cgroups(child_pid, cpu_limit, memory_limit, io_read_limit, io_write_limit, device) != 0) {
            fprintf(stderr, "Error: Failed to set up cgroups\n");
            free(stack);
            return EXIT_FAILURE;
        }

    
        // Wait for the child process to finish 
        // if (waitpid(child_pid, NULL, 0) < 0) { 
        //     perror("Error: Failed to wait for child process"); 
        //     free(stack); 
        //     return EXIT_FAILURE; 
        // } 
    
        // printf("Process in isolated namespace completed.\n"); 
    
        // // Clean up the root directory 
        // printf("Cleaning up root directory: %s\n", new_root); 
        // if (delete_directory(new_root) != 0) { 
        //     fprintf(stderr, "Error: Failed to clean up root directory: %s\n", new_root); 
        //     free(stack); 
        //     return EXIT_FAILURE; 
        // } 
    
        // printf("Cleaned up root directory: %s\n", new_root);

        // // remove cgroup directories
        // char cgroup_path[256];
        // snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/cpu/container_%d", child_pid);
        // snprintf(command, sizeof(command), "sudo rmdir %s", cgroup_path);
        // system(command);

        // snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/memory/container_%d", child_pid);
        // snprintf(command, sizeof(command), "sudo rmdir %s", cgroup_path);
        // system(command);

    
        // // Clean up 
        // free(stack); 

    }
    pid_t child_pid, wpid;
    int status = 0;
    char command[1024];
    while ((wpid = wait(&status)) > 0);

    // for (int j = 0; j < num_files; j++) {
    //     printf("Process %d in isolated namespace completed.\n", childs[j]); 
    //     char *new_root = roots[j];
    //     child_pid = childs[j];
    //     char *stack = stacks[j];

    //     // Clean up the root directory 
    //     printf("Cleaning up root directory: %s\n", new_root); 
    //     if (delete_directory(new_root) != 0) { 
    //         fprintf(stderr, "Error: Failed to clean up root directory: %s\n", new_root); 
    //         free(stack); 
    //         return EXIT_FAILURE; 
    //     } 
        
    //     printf("Cleaned up root directory: %s\n", new_root);

    //     // remove cgroup directories
    //     char cgroup_path[256];
    //     snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/cpu/container_%d", child_pid);
    //     snprintf(command, sizeof(command), "sudo rmdir %s", cgroup_path);
    //     system(command);

    //     snprintf(cgroup_path, sizeof(cgroup_path), CGROUP_BASE "/memory/container_%d", child_pid);
    //     snprintf(command, sizeof(command), "sudo rmdir %s", cgroup_path);
    //     system(command);
    // }
}