/* Approach 2: Dynamic 2D Array - Contiguous Memory */
#include <stdio.h>
#include <stdlib.h>

int **allocate_2d_contiguous(int rows, int cols) {
    // Allocate memory for pointers and data together
    int **arr = malloc(rows * sizeof(int *) + rows * cols * sizeof(int));
    if (!arr) return NULL;
    
    // Set up pointers
    int *data = (int *)(arr + rows);
    for (int i = 0; i < rows; i++) {
        arr[i] = data + i * cols;
    }
    
    return arr;
}

void free_2d_contiguous(int **arr) {
    free(arr);  // Single free for contiguous allocation
}

int main() {
    int rows = 3, cols = 4;
    
    int **matrix = allocate_2d_contiguous(rows, cols);
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
    printf("Contiguous 2D array:\n");
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }
    
    free_2d_contiguous(matrix);
    
    return 0;
}
