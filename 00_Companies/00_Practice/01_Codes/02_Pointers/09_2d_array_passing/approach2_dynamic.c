/* Approach 2: Pass 2D Array - Pointer to Pointer */
#include <stdio.h>
#include <stdlib.h>

// Method 3: Pointer to pointer (for dynamically allocated)
void print_array_v3(int **arr, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", arr[i][j]);
        }
        printf("\n");
    }
}

// Method 4: Single pointer with manual indexing
void print_array_v4(int *arr, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", arr[i * cols + j]);
        }
        printf("\n");
    }
}

int main() {
    int rows = 3, cols = 4;
    
    // Allocate 2D array (array of pointers)
    int **matrix = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        matrix[i] = malloc(cols * sizeof(int));
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = i * cols + j + 1;
        }
    }
    
    printf("Method 3 (pointer to pointer):\n");
    print_array_v3(matrix, rows, cols);
    
    // Allocate contiguous 2D array
    int *flat = malloc(rows * cols * sizeof(int));
    for (int i = 0; i < rows * cols; i++) {
        flat[i] = i + 1;
    }
    
    printf("\nMethod 4 (single pointer):\n");
    print_array_v4(flat, rows, cols);
    
    // Free memory
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
    free(flat);
    
    return 0;
}
