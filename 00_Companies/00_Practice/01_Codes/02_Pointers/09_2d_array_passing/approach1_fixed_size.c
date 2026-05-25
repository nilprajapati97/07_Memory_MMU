/* Approach 1: Pass 2D Array - Fixed Size */
#include <stdio.h>

#define ROWS 3
#define COLS 4

// Method 1: Fixed size array
void print_array_v1(int arr[ROWS][COLS]) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%d ", arr[i][j]);
        }
        printf("\n");
    }
}

// Method 2: Pointer to array
void print_array_v2(int (*arr)[COLS], int rows) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%d ", arr[i][j]);
        }
        printf("\n");
    }
}

int main() {
    int matrix[ROWS][COLS] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12}
    };
    
    printf("Method 1:\n");
    print_array_v1(matrix);
    
    printf("\nMethod 2:\n");
    print_array_v2(matrix, ROWS);
    
    return 0;
}
