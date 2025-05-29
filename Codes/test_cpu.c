#include <stdio.h>
// #include <math.h>

int main() {
    // Print a message to show the program is running
    printf("High CPU usage program started...\n");

    // Infinite loop to keep the CPU busy
    while (1) {
        // Perform a CPU-intensive operation (calculating square roots)
        volatile double result = 0;
        for (int i = 0; i < 1000000; i++) {
            result += i * i;
        }

        // Optional: Print the result after a number of iterations (to keep it visible)
        if (result > 1000000) {
            printf("Result: %f\n", result);
        }
    }

    return 0;
}
