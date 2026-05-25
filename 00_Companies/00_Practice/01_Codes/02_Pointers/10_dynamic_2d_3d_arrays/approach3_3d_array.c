/* Approach 3: Dynamic 3D Array */
#include <stdio.h>
#include <stdlib.h>

int ***allocate_3d_array(int x, int y, int z) {
    int ***arr = malloc(x * sizeof(int **));
    if (!arr) return NULL;
    
    for (int i = 0; i < x; i++) {
        arr[i] = malloc(y * sizeof(int *));
        if (!arr[i]) {
            for (int k = 0; k < i; k++) {
                for (int j = 0; j < y; j++) {
                    free(arr[k][j]);
                }
                free(arr[k]);
            }
            free(arr);
            return NULL;
        }
        
        for (int j = 0; j < y; j++) {
            arr[i][j] = malloc(z * sizeof(int));
            if (!arr[i][j]) {
                for (int k = 0; k < j; k++) {
                    free(arr[i][k]);
                }
                for (int k = 0; k < i; k++) {
                    for (int l = 0; l < y; l++) {
                        free(arr[k][l]);
                    }
                    free(arr[k]);
                }
                free(arr[i]);
                free(arr);
                return NULL;
            }
        }
    }
    
    return arr;
}

void free_3d_array(int ***arr, int x, int y) {
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            free(arr[i][j]);
        }
        free(arr[i]);
    }
    free(arr);
}

int main() {
    int x = 2, y = 3, z = 4;
    
    int ***cube = allocate_3d_array(x, y, z);
    if (!cube) {
        printf("Allocation failed\n");
        return 1;
    }
    
    // Initialize
    int val = 0;
    for (int i = 0; i < x; i++) {
        for (int j = 0; j < y; j++) {
            for (int k = 0; k < z; k++) {
                cube[i][j][k] = val++;
            }
        }
    }
    
    // Print
    printf("3D array:\n");
    for (int i = 0; i < x; i++) {
        printf("Layer %d:\n", i);
        for (int j = 0; j < y; j++) {
            for (int k = 0; k < z; k++) {
                printf("%2d ", cube[i][j][k]);
            }
            printf("\n");
        }
        printf("\n");
    }
    
    free_3d_array(cube, x, y);
    
    return 0;
}
