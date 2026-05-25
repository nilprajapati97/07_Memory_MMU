/* Approach 1: Dynamic 2D Array - Array of Pointers */
#include <stdio.h>
#include <stdlib.h>

int **allocate_2d_array(int rows, int cols) {
    int **arr = malloc(rows * sizeof(int *));
    if (!arr) return NULL;
    
    for (int i = 0; i < rows; i++) {
        arr[i] = malloc(cols * sizeof(int));
        if (!arr[i]) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                free(arr[j]);
            }
            free(arr);
            return NULL;
        }
    }
    
    return arr;
}

void free_2d_array(int **arr, int rows) {
    for (int i = 0; i < rows; i++) {
        free(arr[i]);
    }
    free(arr);
}

int main() {
    int rows = 3, cols = 4;
    
    int **matrix = allocate_2d_array(rows, cols);
    if (!matrix) {
        printf("Allocation failed\n");
        return 1;
    }
    
    // Initialize
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = i * cols + j;
        }
    }
    
    // Print
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
    
    free_2d_array(matrix, rows);
    
    return 0;
}
