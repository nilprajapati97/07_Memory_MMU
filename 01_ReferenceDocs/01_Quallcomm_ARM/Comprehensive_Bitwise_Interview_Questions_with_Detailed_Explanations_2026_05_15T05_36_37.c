
/*
 * ============================================================================
 * COMPREHENSIVE BITWISE INTERVIEW QUESTIONS - DETAILED EXPLANATIONS
 * ============================================================================
 * Prepared for: Senior Linux Kernel / Embedded Systems Interviews
 * Topics: Bit manipulation, Register programming, Firmware patterns
 * ============================================================================
 */

/* ============================================================================
 * SECTION 1: FUNDAMENTAL BITWISE OPERATIONS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Q1: How to SET a particular bit in a variable?
 * --------------------------------------------------------------------------
 * Concept: Use OR (|) with a mask that has 1 at the desired position.
 * Formula: number |= (1 << n)   where n = bit position (0-indexed from right)
 *
 * Why it works: OR with 0 leaves original bit unchanged; OR with 1 always gives 1.
 *
 * Example: Set bit 2 in number 0b00001001 (decimal 9)
 *   number    = 0b00001001  (9)
 *   mask      = 0b00000100  (1 << 2 = 4)
 *   result    = 0b00001001 | 0b00000100 = 0b00001101  (13)
 */
#define SET_BIT(num, pos)  ((num) |= (1U << (pos)))

// Usage in register programming:
void example_set_bit(void) {
    uint32_t reg = read_register(CTRL_REG);
    reg |= (1 << ENABLE_BIT);  // Set enable bit
    write_register(CTRL_REG, reg);
}


/* --------------------------------------------------------------------------
 * Q2: How to CLEAR a particular bit in a number?
 * --------------------------------------------------------------------------
 * Concept: Use AND (&) with a mask that has 0 at desired position, 1 elsewhere.
 * Formula: number &= ~(1 << n)
 *
 * Why it works: AND with 1 leaves original bit unchanged; AND with 0 always gives 0.
 *
 * Example: Clear bit 3 in number 0b00001111 (decimal 15)
 *   number    = 0b00001111  (15)
 *   1 << 3    = 0b00001000
 *   ~(1 << 3) = 0b11110111  (inverted mask)
 *   result    = 0b00001111 & 0b11110111 = 0b00000111  (7)
 */
#define CLEAR_BIT(num, pos)  ((num) &= ~(1U << (pos)))

// Usage: Clear error bit in status register
void example_clear_bit(void) {
    uint32_t status = read_register(STATUS_REG);
    status &= ~(1 << ERROR_BIT);
    write_register(STATUS_REG, status);
}


/* --------------------------------------------------------------------------
 * Q3: How to TOGGLE/FLIP a particular bit?
 * --------------------------------------------------------------------------
 * Concept: Use XOR (^) with a mask that has 1 at the desired position.
 * Formula: number ^= (1 << n)
 *
 * Why it works: XOR with 0 leaves bit unchanged; XOR with 1 flips the bit.
 *
 * Example: Toggle bit 2 in 0b00001010 (10)
 *   0b00001010 ^ 0b00000100 = 0b00001110 (14) → bit 2: 0→1
 *   0b00001110 ^ 0b00000100 = 0b00001010 (10) → bit 2: 1→0
 */
#define TOGGLE_BIT(num, pos)  ((num) ^= (1U << (pos)))

// Usage: Toggle LED on GPIO pin
void toggle_led(void) {
    gpio_reg ^= (1 << LED_PIN);
}


/* --------------------------------------------------------------------------
 * Q4: How to CHECK if a particular bit is set or not?
 * --------------------------------------------------------------------------
 * Concept: Use AND (&) with a mask. Non-zero result = bit is set.
 * Formula: (number >> n) & 1  OR  number & (1 << n)
 *
 * Example: Check bit 3 in 0b00001010 (10)
 *   0b00001010 & 0b00001000 = 0b00001000 (non-zero → bit IS set)
 * Check bit 2:
 *   0b00001010 & 0b00000100 = 0b00000000 (zero → bit NOT set)
 */
#define CHECK_BIT(num, pos)  ((num) & (1U << (pos)))
#define GET_BIT(num, pos)    (((num) >> (pos)) & 1)

// Usage: Check if transmit buffer is full
void example_check(void) {
    if (read_register(STATUS_REG) & (1 << TX_FULL_BIT)) {
        // Buffer full, wait
    }
}


/* --------------------------------------------------------------------------
 * Q5: How to COUNT the number of set bits (Hamming Weight)?
 * --------------------------------------------------------------------------
 * Method 1: Brian Kernighan's Algorithm (MOST POPULAR IN INTERVIEWS)
 * Key Insight: n & (n-1) clears the lowest set bit.
 *
 * Example: n = 13 = 0b1101
 *   Iter 1: n=0b1101, n-1=0b1100, n&(n-1)=0b1100  count=1
 *   Iter 2: n=0b1100, n-1=0b1011, n&(n-1)=0b1000  count=2
 *   Iter 3: n=0b1000, n-1=0b0111, n&(n-1)=0b0000  count=3
 *   n=0, STOP. Answer = 3
 *
 * Time Complexity: O(number of set bits) — better than O(32) naive
 */
int countSetBits(unsigned int n) {
    int count = 0;
    while (n) {
        n &= (n - 1);  // Clear lowest set bit
        count++;
    }
    return count;
}

// Method 2: Naive (shift and check each bit) — O(32)
int countSetBits_naive(unsigned int n) {
    int count = 0;
    while (n) {
        count += (n & 1);
        n >>= 1;
    }
    return count;
}

// Method 3: GCC built-in — O(1)
// int count = __builtin_popcount(n);


/* --------------------------------------------------------------------------
 * Q6: How to check if a number is a POWER OF 2?
 * --------------------------------------------------------------------------
 * Key Insight: A power of 2 has exactly ONE set bit.
 *   1=0b0001, 2=0b0010, 4=0b0100, 8=0b1000
 *
 * Formula: n > 0 && (n & (n-1)) == 0
 *
 * Why: n & (n-1) clears the only set bit → result is 0
 *   n=8:  0b1000 & 0b0111 = 0b0000 → IS power of 2
 *   n=6:  0b0110 & 0b0101 = 0b0100 → NOT power of 2
 *
 * Linux Kernel: include/linux/log2.h
 *   #define is_power_of_2(n) ((n) != 0 && (((n) & ((n) - 1)) == 0))
 */
int isPowerOf2(unsigned int n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}


/* --------------------------------------------------------------------------
 * Q7: How to SWAP two numbers without a temporary variable?
 * --------------------------------------------------------------------------
 * Using XOR:
 *   a = a ^ b
 *   b = a ^ b  → b = (a^b)^b = a
 *   a = a ^ b  → a = (a^b)^a = b
 *
 * Example: a=5 (0b0101), b=3 (0b0011)
 *   Step 1: a = 5^3 = 0b0110 (6)
 *   Step 2: b = 6^3 = 0b0101 (5) ← original a!
 *   Step 3: a = 6^5 = 0b0011 (3) ← original b!
 *
 * ⚠️ CAVEAT: Fails if a and b point to SAME memory location (zeroes out)!
 */
void swap(int *a, int *b) {
    if (a != b) {  // Critical check!
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}


/* --------------------------------------------------------------------------
 * Q8: Find the ONLY non-repeating element (all others appear twice)
 * --------------------------------------------------------------------------
 * Concept: XOR all elements. x^x=0 and x^0=x, so duplicates cancel out.
 *
 * Example: arr = [2, 3, 5, 3, 2]
 *   2^3^5^3^2 = (2^2)^(3^3)^5 = 0^0^5 = 5
 *
 * Time: O(n), Space: O(1)
 */
int findUnique(int arr[], int n) {
    int result = 0;
    for (int i = 0; i < n; i++)
        result ^= arr[i];
    return result;
}


/* --------------------------------------------------------------------------
 * Q9: Find number that appears ODD number of times
 * --------------------------------------------------------------------------
 * Same as Q8! XOR cancels pairs (even occurrences), leaving the odd one.
 *
 * Example: arr = [1, 2, 3, 2, 3, 1, 3]
 *   (1^1)^(2^2)^(3^3^3) = 0^0^3 = 3 (appears 3 times)
 */
// Same implementation as findUnique() above


/* --------------------------------------------------------------------------
 * Q10: How to REVERSE BITS of an integer?
 * --------------------------------------------------------------------------
 * Method 1: Bit-by-bit reversal — O(32)
 */
uint32_t reverseBits(uint32_t n) {
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result <<= 1;       // Make room for next bit
        result |= (n & 1);  // Copy LSB of n to result
        n >>= 1;            // Move to next bit of n
    }
    return result;
}

/* Method 2: Divide and Conquer — O(log n), used in production */
uint32_t reverseBits_fast(uint32_t n) {
    n = ((n >> 16) | (n << 16));                            // Swap 16-bit halves
    n = ((n & 0xFF00FF00) >> 8) | ((n & 0x00FF00FF) << 8); // Swap bytes
    n = ((n & 0xF0F0F0F0) >> 4) | ((n & 0x0F0F0F0F) << 4); // Swap nibbles
    n = ((n & 0xCCCCCCCC) >> 2) | ((n & 0x33333333) << 2); // Swap pairs
    n = ((n & 0xAAAAAAAA) >> 1) | ((n & 0x55555555) << 1); // Swap bits
    return n;
}


/* --------------------------------------------------------------------------
 * Q11: Find the position of the RIGHTMOST SET BIT
 * --------------------------------------------------------------------------
 * Key Insight: n & (-n) isolates the rightmost set bit.
 * In two's complement: -n = ~n + 1
 *
 * Example: n = 12 = 0b1100
 *   n    = 0b00001100
 *   -n   = 0b11110100
 *   n&-n = 0b00000100 → position 3 (1-indexed)
 */
int rightmostSetBit(unsigned int n) {
    if (n == 0) return -1;
    int pos = 1;
    unsigned int isolated = n & (-n);
    while (isolated >>= 1) pos++;
    return pos;
}
// GCC built-in: __builtin_ffs(n) — returns 1-indexed, 0 if n=0


/* --------------------------------------------------------------------------
 * Q12: How to TURN OFF the rightmost set bit?
 * --------------------------------------------------------------------------
 * Formula: n & (n - 1)
 *
 * Why: Subtracting 1 flips all bits from rightmost set bit to end.
 *   n   = ...1 0 0 0  (rightmost 1 + trailing zeros)
 *   n-1 = ...0 1 1 1  (that bit→0, zeros→1)
 *   AND = ...0 0 0 0  (rightmost set bit cleared!)
 *
 * Example: n=12=0b1100, n-1=0b1011, n&(n-1)=0b1000
 */
#define TURN_OFF_RIGHTMOST_BIT(n)  ((n) & ((n) - 1))


/* --------------------------------------------------------------------------
 * Q13: Determine the SIGN of an integer using bitwise operators
 * --------------------------------------------------------------------------
 * Concept: MSB is the sign bit in two's complement.
 * Formula: (n >> 31) & 1 for 32-bit int (1=negative, 0=positive/zero)
 *
 * ⚠️ Right-shifting signed integers is implementation-defined!
 */
int getSign(int n) {
    return (n >> (sizeof(int) * 8 - 1)) & 1;  // 1 if negative
}


/* --------------------------------------------------------------------------
 * Q14: Compute ABSOLUTE VALUE without branching
 * --------------------------------------------------------------------------
 * Formula: (n ^ mask) - mask   where mask = n >> 31
 *
 * How it works:
 *   If n >= 0: mask=0x00000000 → (n^0)-0 = n (unchanged)
 *   If n < 0:  mask=0xFFFFFFFF → (n^0xFFFFFFFF)-(-1) = (~n)+1 = -n
 *
 * Example: n=-5 (0xFFFFFFFB)
 *   mask = 0xFFFFFFFF
 *   n^mask = 0x00000004 = 4
 *   4 - (-1) = 5 ✓
 */
int abs_val(int n) {
    int mask = n >> 31;
    return (n ^ mask) - mask;
}


/* --------------------------------------------------------------------------
 * Q15: MULTIPLY/DIVIDE by powers of 2 using shifts
 * --------------------------------------------------------------------------
 * Left Shift (<<) = Multiply by 2^n:
 *   5<<1=10, 5<<2=20, 5<<3=40, 3<<4=48
 *
 * Right Shift (>>) = Divide by 2^n:
 *   20>>1=10, 20>>2=5, 15>>1=7 (truncated)
 *
 * Multiply by arbitrary constants:
 */
int mul7(int n)  { return (n << 3) - n; }       // n*8 - n
int mul10(int n) { return (n << 3) + (n << 1); } // n*8 + n*2
int mul15(int n) { return (n << 4) - n; }       // n*16 - n

/* ⚠️ Interview notes:
 * - Left shift can overflow
 * - Right shift of negative numbers is implementation-defined
 * - Compiler already optimizes multiplication by constants to shifts
 */


/* ============================================================================
 * SECTION 2: ADVANCED / FIRMWARE-SPECIFIC QUESTIONS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Q16: Write to a Control/Status Register to clear error bits
 * --------------------------------------------------------------------------
 * Scenario: 32-bit status register, bits [7:4] are error flags.
 *
 * Key Interview Point: Many HW registers are W1C (Write-1-to-Clear)!
 */
#define STATUS_REG_ADDR  0x40001000
#define ERROR_MASK       0x000000F0  // Bits [7:4]

void clear_errors(void) {
    volatile uint32_t *reg = (volatile uint32_t *)STATUS_REG_ADDR;

    // Method 1: Read-Modify-Write (for normal registers)
    uint32_t val = *reg;
    val &= ~ERROR_MASK;  // Clear bits [7:4]
    *reg = val;

    // Method 2: Write-1-to-clear (W1C) registers
    // Writing 1 to a bit CLEARS it (common in hardware!)
    *reg = ERROR_MASK;
}


/* --------------------------------------------------------------------------
 * Q17: Combine and write parameters to a configuration register
 * --------------------------------------------------------------------------
 * Scenario: UART config register:
 *   Bits [1:0] = Parity (00=None, 01=Odd, 10=Even)
 *   Bits [3:2] = Stop bits (00=1, 01=1.5, 10=2)
 *   Bits [5:4] = Data bits (00=5, 01=6, 10=7, 11=8)
 *   Bit  [6]   = Enable
 */
#define PARITY_SHIFT  0
#define PARITY_MASK   (0x3 << PARITY_SHIFT)
#define STOP_SHIFT    2
#define STOP_MASK     (0x3 << STOP_SHIFT)
#define DATA_SHIFT    4
#define DATA_MASK     (0x3 << DATA_SHIFT)
#define ENABLE_BIT    6

void configure_uart(uint8_t parity, uint8_t stop, uint8_t data_bits, uint8_t enable) {
    uint32_t reg = 0;

    reg &= ~PARITY_MASK;
    reg |= ((parity & 0x3) << PARITY_SHIFT);

    reg &= ~STOP_MASK;
    reg |= ((stop & 0x3) << STOP_SHIFT);

    reg &= ~DATA_MASK;
    reg |= ((data_bits & 0x3) << DATA_SHIFT);

    if (enable)
        reg |= (1 << ENABLE_BIT);

    write_register(UART_CTRL_REG, reg);
}


/* --------------------------------------------------------------------------
 * Q18: Check if transmit register is busy
 * --------------------------------------------------------------------------
 * Scenario: Poll UART status register until TX is not busy.
 */
#define UART_STATUS_REG   0x40001004
#define TX_BUSY_BIT       5
#define TX_FIFO_FULL_BIT  3

int is_tx_busy(void) {
    volatile uint32_t *status = (volatile uint32_t *)UART_STATUS_REG;
    return (*status & (1 << TX_BUSY_BIT)) != 0;
}

void uart_send_byte(uint8_t data) {
    volatile uint32_t *status = (volatile uint32_t *)UART_STATUS_REG;
    volatile uint32_t *tx_reg = (volatile uint32_t *)UART_TX_REG;

    // Wait until TX FIFO is not full
    while (*status & (1 << TX_FIFO_FULL_BIT))
        ;  // Busy wait

    *tx_reg = data;
}


/* --------------------------------------------------------------------------
 * Q19: Return error status using bitwise operators (multiple error codes)
 * --------------------------------------------------------------------------
 * Concept: Each bit = one error flag. Single return encodes MULTIPLE errors.
 * Used in: Linux ioctl(), poll() event masks, errno patterns
 */
#define ERR_NONE     0x00
#define ERR_TIMEOUT  (1 << 0)  // 0x01
#define ERR_OVERFLOW (1 << 1)  // 0x02
#define ERR_PARITY   (1 << 2)  // 0x04
#define ERR_FRAMING  (1 << 3)  // 0x08
#define ERR_DMA      (1 << 4)  // 0x10

uint32_t check_uart_errors(void) {
    uint32_t errors = ERR_NONE;
    uint32_t status = read_register(UART_STATUS_REG);

    if (status & (1 << TIMEOUT_FLAG))   errors |= ERR_TIMEOUT;
    if (status & (1 << OVERFLOW_FLAG))  errors |= ERR_OVERFLOW;
    if (status & (1 << PARITY_FLAG))    errors |= ERR_PARITY;

    return errors;  // Can contain multiple errors!
}

// Caller:
// uint32_t err = check_uart_errors();
// if (err & ERR_TIMEOUT)  handle_timeout();
// if (err & ERR_PARITY)   handle_parity_error();
// if (err == ERR_NONE)    // All good


/* --------------------------------------------------------------------------
 * Q20: Implement BIT MASKING for hardware register access
 * --------------------------------------------------------------------------
 * Concept: Read-Modify-Write pattern for specific bit fields.
 */
#define REG_SET_FIELD(reg, mask, shift, value) \
    do { \
        uint32_t tmp = read_register(reg); \
        tmp &= ~(mask);                         /* Clear field */ \
        tmp |= ((value) << (shift)) & (mask);   /* Set new value */ \
        write_register(reg, tmp); \
    } while(0)

#define REG_GET_FIELD(reg, mask, shift) \
    ((read_register(reg) & (mask)) >> (shift))

// Example: Set clock divider (bits [11:8]) to value 5
// #define CLK_DIV_MASK   0x00000F00
// #define CLK_DIV_SHIFT  8
// REG_SET_FIELD(CLK_CTRL_REG, CLK_DIV_MASK, CLK_DIV_SHIFT, 5);


/* --------------------------------------------------------------------------
 * Q21: ENDIANNESS conversion (Big-Endian ↔ Little-Endian)
 * --------------------------------------------------------------------------
 * Little-Endian: LSB at lowest address (x86, ARM default)
 * Big-Endian:    MSB at lowest address (Network byte order)
 *
 * 32-bit swap: 0xAABBCCDD → 0xDDCCBBAA
 */
uint32_t swap_endian_32(uint32_t n) {
    return ((n >> 24) & 0x000000FF) |  // Byte 3 → Byte 0
           ((n >> 8)  & 0x0000FF00) |  // Byte 2 → Byte 1
           ((n << 8)  & 0x00FF0000) |  // Byte 1 → Byte 2
           ((n << 24) & 0xFF000000);   // Byte 0 → Byte 3
}

uint16_t swap_endian_16(uint16_t n) {
    return (n >> 8) | (n << 8);
}

/* Linux Kernel functions:
 * cpu_to_be32(x), be32_to_cpu(x)  — CPU ↔ Big-Endian
 * cpu_to_le32(x), le32_to_cpu(x)  — CPU ↔ Little-Endian
 * htonl(x), ntohl(x)              — Host ↔ Network (Big-Endian)
 * htons(x), ntohs(x)              — 16-bit versions
 */


/* --------------------------------------------------------------------------
 * Q22: Circular buffer with power-of-2 size using bitwise AND
 * --------------------------------------------------------------------------
 * Concept: AND with (size-1) instead of modulo (%) — MUCH faster!
 * Requirement: Buffer size MUST be power of 2.
 *
 * Why: index % 256 → division (expensive on ARM w/o HW divider)
 *      index & 0xFF → single AND instruction (1 cycle)
 */
#define BUFFER_SIZE  256              // Must be power of 2
#define BUFFER_MASK  (BUFFER_SIZE - 1)  // 0xFF

typedef struct {
    uint8_t data[BUFFER_SIZE];
    uint32_t head;  // Write index
    uint32_t tail;  // Read index
} circular_buf_t;

void buf_write(circular_buf_t *buf, uint8_t byte) {
    buf->data[buf->head & BUFFER_MASK] = byte;
    buf->head++;
}

uint8_t buf_read(circular_buf_t *buf) {
    uint8_t byte = buf->data[buf->tail & BUFFER_MASK];
    buf->tail++;
    return byte;
}

int buf_count(circular_buf_t *buf) {
    return (buf->head - buf->tail) & BUFFER_MASK;
}


/* ============================================================================
 * SECTION 3: PROBLEM-SOLVING QUESTIONS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Q23: HAMMING DISTANCE between two integers
 * --------------------------------------------------------------------------
 * Definition: Number of positions where corresponding bits differ.
 * Approach: XOR (gives 1 where bits differ) → count set bits.
 *
 * Example: x=1(0b001), y=4(0b100)
 *   XOR = 0b101, count = 2 → Hamming Distance = 2
 */
int hammingDistance(int x, int y) {
    int xor_val = x ^ y;
    int count = 0;
    while (xor_val) {
        xor_val &= (xor_val - 1);
        count++;
    }
    return count;
}


/* --------------------------------------------------------------------------
 * Q24: Find TWO non-repeating elements (all others appear twice)
 * --------------------------------------------------------------------------
 * Approach:
 *   1. XOR all → gives a^b (XOR of two unique numbers)
 *   2. Find any set bit in a^b (this bit differs between a and b)
 *   3. Divide elements into two groups based on that bit
 *   4. XOR each group → get a and b separately
 *
 * Example: arr = [2, 4, 7, 9, 2, 4]
 *   XOR all = 7^9 = 0b1110
 *   Rightmost set bit = 0b0010 (bit 1)
 *   Group 1 (bit 1 set): 2,7,2 → XOR = 7
 *   Group 2 (bit 1 not set): 4,9,4 → XOR = 9
 */
void findTwoUnique(int arr[], int n, int *x, int *y) {
    int xor_all = 0;
    for (int i = 0; i < n; i++)
        xor_all ^= arr[i];

    int set_bit = xor_all & (-xor_all);  // Isolate rightmost set bit

    *x = 0; *y = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i] & set_bit)
            *x ^= arr[i];
        else
            *y ^= arr[i];
    }
}


/* --------------------------------------------------------------------------
 * Q25: Detect if two integers have OPPOSITE SIGNS
 * --------------------------------------------------------------------------
 * Formula: (x ^ y) < 0
 * Why: XOR of MSBs → 1 if signs differ → result is negative
 *
 *   +5 ^ -3: MSB(0) XOR MSB(1) = 1 → negative → opposite signs!
 *   +5 ^ +3: MSB(0) XOR MSB(0) = 0 → positive → same signs
 */
int oppositeSign(int x, int y) {
    return (x ^ y) < 0;
}


/* --------------------------------------------------------------------------
 * Q26: ADD two numbers without using + operator
 * --------------------------------------------------------------------------
 * Concept:
 *   XOR  → sum without carry
 *   AND  → carry bits
 *   Shift carry left, repeat until no carry
 *
 * Example: a=5(0b0101), b=3(0b0011)
 *   Iter 1: carry=0b0001, a=0b0110, b=0b0010
 *   Iter 2: carry=0b0010, a=0b0100, b=0b0100
 *   Iter 3: carry=0b0100, a=0b0000, b=0b1000
 *   Iter 4: carry=0b0000, a=0b1000, b=0b0000 → STOP, a=8 ✓
 */
int add(int a, int b) {
    while (b != 0) {
        int carry = a & b;
        a = a ^ b;
        b = carry << 1;
    }
    return a;
}


/* --------------------------------------------------------------------------
 * Q27: Find the MISSING NUMBER in array of 1 to N
 * --------------------------------------------------------------------------
 * XOR approach (no overflow risk unlike sum method):
 *   XOR(1..N) ^ XOR(array elements) = missing number
 *
 * Example: arr=[1,2,4,5], N=5
 *   (1^2^3^4^5) ^ (1^2^4^5) = 3
 */
int findMissing(int arr[], int n) {
    int xor_all = 0, xor_arr = 0;
    for (int i = 1; i <= n; i++)
        xor_all ^= i;
    for (int i = 0; i < n - 1; i++)
        xor_arr ^= arr[i];
    return xor_all ^ xor_arr;
}


/* --------------------------------------------------------------------------
 * Q28: Compute PARITY of a number
 * --------------------------------------------------------------------------
 * Parity = 1 if odd number of set bits, 0 if even.
 *
 * Method 1: Toggle parity for each set bit
 */
int parity(unsigned int n) {
    int p = 0;
    while (n) {
        p ^= 1;
        n &= (n - 1);
    }
    return p;
}

/* Method 2: Divide and conquer — O(log n)
 * Folds parity from 32 bits → 16 → 8 → 4 → 2 → 1 bit
 */
int parity_fast(uint32_t n) {
    n ^= (n >> 16);
    n ^= (n >> 8);
    n ^= (n >> 4);
    n ^= (n >> 2);
    n ^= (n >> 1);
    return n & 1;
}


/* --------------------------------------------------------------------------
 * Q29: ISOLATE the lowest set bit
 * --------------------------------------------------------------------------
 * Formula: n & (-n)  or  n & (~n + 1)
 *
 * Why it works:
 *   n    = ...a 1 0 0 0  (rightmost 1 + trailing zeros)
 *   ~n   = ...ā 0 1 1 1
 *   ~n+1 = ...ā 1 0 0 0  (carry propagates to rightmost 1)
 *   n&-n = 0...0 1 0 0 0  (only that bit survives!)
 *
 * Example: n=12=0b1100 → n&(-n) = 0b0100
 *
 * Use cases: Fenwick Tree, finding rightmost set bit position
 */
unsigned int isolateLowestBit(unsigned int n) {
    return n & (-n);
}


/* --------------------------------------------------------------------------
 * Q30: Generate ALL SUBSETS using bit masking
 * --------------------------------------------------------------------------
 * For n elements → 2^n subsets.
 * Each subset = n-bit number where bit i = element i included.
 *
 * Example: arr=[a,b,c], n=3 → 8 subsets
 *   000→{}, 001→{a}, 010→{b}, 011→{a,b}
 *   100→{c}, 101→{a,c}, 110→{b,c}, 111→{a,b,c}
 *
 * Applications: Subset sum, TSP (bitmask DP), combinatorial optimization
 */
void generateSubsets(int arr[], int n) {
    int total = 1 << n;  // 2^n subsets

    for (int mask = 0; mask < total; mask++) {
        printf("{ ");
        for (int i = 0; i < n; i++) {
            if (mask & (1 << i))
                printf("%d ", arr[i]);
        }
        printf("}
");
    }
}


/* ============================================================================
 * SECTION 4: BONUS — QUICK REFERENCE TRICKS
 * ============================================================================
 *
 * | Trick                      | Formula              | Example              |
 * |----------------------------|----------------------|----------------------|
 * | Set bit n                  | x | (1<<n)           | Set bit 3: x|8      |
 * | Clear bit n                | x & ~(1<<n)          | Clear bit 3: x&0xF7 |
 * | Toggle bit n               | x ^ (1<<n)           | Toggle bit 3: x^8   |
 * | Check bit n                | x & (1<<n)           | Check bit 3: x&8    |
 * | Turn off rightmost 1       | x & (x-1)            | 0b1100 → 0b1000     |
 * | Isolate rightmost 1        | x & (-x)             | 0b1100 → 0b0100     |
 * | Right propagate rightmost  | x | (x-1)            | 0b1100 → 0b1111     |
 * | Is power of 2              | x && !(x&(x-1))      | 8 → true            |
 * | Modulo 2^n                 | x & (n-1)            | x%8 = x&7           |
 * | Multiply by 2^n            | x << n               | x*8 = x<<3          |
 * | Divide by 2^n              | x >> n               | x/8 = x>>3          |
 * | Swap without temp          | a^=b; b^=a; a^=b     | —                    |
 * | Absolute value             | (x^mask)-mask        | mask=x>>31           |
 * | Min of two                 | y^((x^y)&-(x<y))     | —                    |
 * | Max of two                 | x^((x^y)&-(x<y))     | —                    |
 * | Check same sign            | (x^y) >= 0           | —                    |
 */

/* Round up to next power of 2 — Linux: roundup_pow_of_two() */
uint32_t next_power_of_2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}
// Examples: 5→8, 17→32, 32→32

