#include <stdio.h>
#include <stdlib.h>

/* 2D Array */
int** alloc2D(int rows, int cols) {
    int **arr = (int**)malloc(rows * sizeof(int*));
    for (int i = 0; i < rows; i++)
        arr[i] = (int*)malloc(cols * sizeof(int));
    return arr;
}

void free2D(int **arr, int rows) {
    for (int i = 0; i < rows; i++)
        free(arr[i]);
    free(arr);
}

void input2D(int **arr, int rows, int cols) {
    printf("Enter %d elements:\n", rows * cols);
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            scanf("%d", &arr[i][j]);
}

void print2D(int **arr, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%4d", arr[i][j]);
        printf("\n");
    }
}

/* 3D Array */
int*** alloc3D(int x, int y, int z) {
    int ***arr = (int***)malloc(x * sizeof(int**));
    for (int i = 0; i < x; i++) {
        arr[i] = (int**)malloc(y * sizeof(int*));
        for (int j = 0; j < y; j++)
            arr[i][j] = (int*)malloc(z * sizeof(int));
    }
    return arr;
}

void free3D(int ***arr, int x, int y) {
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++)
            free(arr[i][j]);
        free(arr[i]);
    }
    free(arr);
}

void input3D(int ***arr, int x, int y, int z) {
    printf("Enter %d elements:\n", x * y * z);
    for (int i = 0; i < x; i++)
        for (int j = 0; j < y; j++)
            for (int k = 0; k < z; k++)
                scanf("%d", &arr[i][j][k]);
}

void print3D(int ***arr, int x, int y, int z) {
    for (int i = 0; i < x; i++) {
        printf("Layer %d:\n", i);
        for (int j = 0; j < y; j++) {
            for (int k = 0; k < z; k++)
                printf("%4d", arr[i][j][k]);
            printf("\n");
        }
        printf("\n");
    }
}

int main(void) {
    int choice;
    printf("Choose: 1=2D, 2=3D: ");
    scanf("%d", &choice);

    if (choice == 1) {
        int rows, cols;
        printf("Rows Cols: ");
        scanf("%d %d", &rows, &cols);
        
        int **arr = alloc2D(rows, cols);
        input2D(arr, rows, cols);
        print2D(arr, rows, cols);
        free2D(arr, rows);
    } else if (choice == 2) {
        int x, y, z;
        printf("X Y Z: ");
        scanf("%d %d %d", &x, &y, &z);
        
        int ***arr = alloc3D(x, y, z);
        input3D(arr, x, y, z);
        print3D(arr, x, y, z);
        free3D(arr, x, y);
    }

    return 0;
}
