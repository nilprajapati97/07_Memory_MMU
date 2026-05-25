// Check if a number is power of 2
int is_power_of_2(unsigned int n) {
    return n && !(n & (n - 1));
}
