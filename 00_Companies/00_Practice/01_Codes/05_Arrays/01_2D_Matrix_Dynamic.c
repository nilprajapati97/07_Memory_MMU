#include <stdio.h>
#include <stdlib.h>

/* Allocate rows x cols matrix */
int** allocMatrix(int rows, int cols) {
    int **mat = (int**)malloc(rows * sizeof(int*));
    for (int i = 0; i < rows; i++)
        mat[i] = (int*)malloc(cols * sizeof(int));
    return mat;
}

/* Free allocated matrix */
void freeMatrix(int **mat, int rows) {
    for (int i = 0; i < rows; i++)
        free(mat[i]);
    free(mat);
}

/* Print matrix */
void printMatrix(int **mat, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%4d", mat[i][j]);
        printf("\n");
    }
}

int main(void) {
    int rows = 3, cols = 4;

    int **mat = allocMatrix(rows, cols);

    /* Fill with row*cols + col */
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            mat[i][j] = i * cols + j;

    printMatrix(mat, rows, cols);
    freeMatrix(mat, rows);
    return 0;
}
