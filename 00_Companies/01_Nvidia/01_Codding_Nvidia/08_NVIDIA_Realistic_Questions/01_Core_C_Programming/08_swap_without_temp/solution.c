// Swap two numbers without temp variable
void swap(int *a, int *b) {
    if (a == b) return;
    *a = *a ^ *b;
    *b = *a ^ *b;
    *a = *a ^ *b;
}
