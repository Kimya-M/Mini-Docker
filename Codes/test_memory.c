#include <stdio.h>
#include <stdlib.h>

int main() {
    // Allocate 2 MB of memory
    size_t size = 2 * 1024 * 1024; // 2 MB in bytes
    void *ptr = malloc(size);

    // Check if memory allocation is successful
    if (ptr == NULL) {
        printf("Memory allocation failed!\n");
        return 1; // Return an error code
    }

    printf("Memory successfully allocated: 2 MB\n");

    // Free the allocated memory
    free(ptr);

    return 0;
}
