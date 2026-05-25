/* Approach 3: Callback Functions */
#include <stdio.h>

// Callback function type
typedef void (*callback_t)(int);

// Functions that will be used as callbacks
void on_success(int value) {
    printf("✓ Success: %d\n", value);
}

void on_error(int code) {
    printf("✗ Error: %d\n", code);
}

// Function that accepts callback
void process_data(int data, callback_t callback) {
    printf("Processing data: %d\n", data);
    
    if (data > 0) {
        callback(data);
    } else {
        on_error(-1);
    }
}

// Generic array processor
void for_each(int *arr, int size, void (*func)(int)) {
    for (int i = 0; i < size; i++) {
        func(arr[i]);
    }
}

void print_element(int x) {
    printf("%d ", x);
}

int main() {
    // Using callbacks
    process_data(42, on_success);
    process_data(-1, on_error);
    
    // Array processing with callback
    printf("\nArray elements: ");
    int arr[] = {1, 2, 3, 4, 5};
    for_each(arr, 5, print_element);
    printf("\n");
    
    return 0;
}
