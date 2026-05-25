Bitwise Operations
Interview Preparation Guide
Comprehensive Reference: 30 Questions with Detailed Explanations & Code Examples
Senior Linux Kernel / Embedded Systems | Firmware & Register Programming

Key Bitwise Operators
| Operator | Symbol | Description | Common Use |
| AND | & | 1 if both bits are 1 | Masking, clearing bits |
| OR | | | 1 if at least one bit is 1 | Setting bits |
| XOR | ^ | 1 if bits differ | Toggling, swap, parity, finding unique |
| NOT | ~ | Inverts all bits | Creating complement masks |
| Left Shift | << | Shift bits left (×2 per position) | Multiply by powers of 2 |
| Right Shift | >> | Shift bits right (÷2 per position) | Divide by powers of 2, sign check |


| SECTION 1: Fundamental Bitwise Operations (Q1 – Q15) |


Q1: How to SET a particular bit?
Concept: Use OR (|) with a mask that has 1 at the desired bit position. OR with 0 leaves a bit unchanged; OR with 1 always sets the bit to 1.
Formula: number |= (1 << n)   where n = bit position (0-indexed from right)
Walkthrough (Set bit 2 in 0b00001001 = 9):
| number = 0b 0000 1001 (9)mask = 0b 0000 0100 (1 << 2 = 4)result = 0b 0000 1001 | 0b 0000 0100 = 0b 0000 1101 (13) ✓ bit 2 is now set |

| #define SET_BIT(num, pos) ((num) |= (1U << (pos)))// Usage in register programming:void example_set_bit(void) { uint32_t reg = read_register(CTRL_REG); reg |= (1 << ENABLE_BIT); // Set the enable bit write_register(CTRL_REG, reg);} |

Q2: How to CLEAR a particular bit?
Concept: Use AND (&) with a mask that has 0 at the target position and 1 everywhere else. AND with 1 leaves a bit unchanged; AND with 0 always clears it.
Formula: number &= ~(1 << n)
Walkthrough (Clear bit 3 in 0b00001111 = 15):
| number = 0b 0000 1111 (15)1 << 3 = 0b 0000 1000~(1 << 3) = 0b 1111 0111 (inverted mask)result = 0b 0000 1111 & 0b 1111 0111 = 0b 0000 0111 (7) ✓ bit 3 cleared |

| #define CLEAR_BIT(num, pos) ((num) &= ~(1U << (pos)))// Usage: Clear error bit in status registervoid clear_error(void) { uint32_t status = read_register(STATUS_REG); status &= ~(1 << ERROR_BIT); write_register(STATUS_REG, status);} |

Q3: How to TOGGLE / FLIP a particular bit?
Concept: Use XOR (^) with a mask. XOR with 0 leaves the bit unchanged; XOR with 1 flips it — perfectly reversible.
Formula: number ^= (1 << n)
| // Toggle bit 2 in 0b0000_1010 (10)0b0000_1010 ^ 0b0000_0100 = 0b0000_1110 (14) → bit 2: 0→10b0000_1110 ^ 0b0000_0100 = 0b0000_1010 (10) → bit 2: 1→0#define TOGGLE_BIT(num, pos) ((num) ^= (1U << (pos)))// Usage: Toggle LED on GPIO pinvoid toggle_led(void) { gpio_reg ^= (1 << LED_PIN); } |

Q4: How to CHECK if a particular bit is set?
Concept: AND with a mask targeting the bit. A non-zero result means the bit is set.
Formula: number & (1 << n)   or alternatively   (number >> n) & 1
| #define CHECK_BIT(num, pos) ((num) & (1U << (pos)))#define GET_BIT(num, pos) (((num) >> (pos)) & 1)// Check bit 3 in 0b0000_1010 (10):// 0b0000_1010 & 0b0000_1000 = 0b0000_1000 (non-zero → bit 3 IS set)// Check bit 2 in 0b0000_1010 (10):// 0b0000_1010 & 0b0000_0100 = 0b0000_0000 (zero → bit 2 NOT set)void example_check(void) { if (read_register(STATUS_REG) & (1 << TX_FULL_BIT)) wait_for_fifo(); // TX buffer is full} |

Q5: How to COUNT the number of set bits (Hamming Weight / popcount)?
Brian Kernighan's Algorithm — the most popular interview answer. Key insight: n & (n-1) clears the lowest set bit every iteration.
Time Complexity: O(k) where k = number of set bits — better than naive O(32).
| // Example trace: n = 13 = 0b1101// Iter 1: n=0b1101, n-1=0b1100, n&(n-1)=0b1100 count=1// Iter 2: n=0b1100, n-1=0b1011, n&(n-1)=0b1000 count=2// Iter 3: n=0b1000, n-1=0b0111, n&(n-1)=0b0000 count=3 ← STOPint countSetBits(unsigned int n) { int count = 0; while (n) { n &= (n - 1); // Clear the lowest set bit count++; } return count;}// Method 2: Naive O(32) — shift and check each bitint countSetBits_naive(unsigned int n) { int count = 0; while (n) { count += (n & 1); n >>= 1; } return count;}// Method 3: GCC built-in — O(1)// int count = __builtin_popcount(n); |

Q6: How to check if a number is a POWER OF 2?
Key Insight: A power of 2 has exactly ONE set bit. Therefore n & (n-1) clears that only bit, producing 0.
Formula: n > 0 && (n & (n-1)) == 0
| // Examples:// n=8: 0b1000 & 0b0111 = 0b0000 → IS power of 2// n=6: 0b0110 & 0b0101 = 0b0100 → NOT power of 2int isPowerOf2(unsigned int n) { return (n > 0) && ((n & (n - 1)) == 0);}// Linux Kernel — include/linux/log2.h:// #define is_power_of_2(n) ((n) != 0 && (((n) & ((n)-1)) == 0)) |

Q7: How to SWAP two numbers without a temporary variable?
Technique: Use XOR — the trick leverages the self-inverse property: x ^ x = 0 and x ^ 0 = x.
| // a=5 (0b0101), b=3 (0b0011)// Step 1: a = 5^3 = 0b0110 (6)// Step 2: b = 6^3 = 0b0101 (5) ← original a!// Step 3: a = 6^5 = 0b0011 (3) ← original b!void swap(int *a, int *b) { if (a != b) { // Critical: must check pointer identity! *a ^= *b; *b ^= *a; *a ^= *b; }} |

| ⚠ CAVEAT: Fails if a and b point to the SAME memory location — both would be zeroed out. The pointer check (a != b) is mandatory. |

Q8: Find the ONLY non-repeating element (all others appear exactly twice)
Insight: XOR all elements. Since x ^ x = 0 (pairs cancel) and x ^ 0 = x, only the unique element survives.
Time: O(n)   Space: O(1) — optimal.
| // arr = [2, 3, 5, 3, 2]// 2^3^5^3^2 = (2^2)^(3^3)^5 = 0^0^5 = 5 ✓int findUnique(int arr[], int n) { int result = 0; for (int i = 0; i < n; i++) result ^= arr[i]; return result;} |

Q9: Find the element that appears an ODD number of times
Same algorithm as Q8. XOR cancels all pairs (even occurrences), leaving only the element with odd occurrences.
| // arr = [1, 2, 3, 2, 3, 1, 3]// (1^1)^(2^2)^(3^3^3) = 0^0^3 = 3 (appears 3 times) ✓// Same implementation as findUnique() in Q8int findOddOccurrence(int arr[], int n) { int result = 0; for (int i = 0; i < n; i++) result ^= arr[i]; return result;} |

Q10: How to REVERSE BITS of a 32-bit integer?
Method 1: Bit-by-bit loop — O(32). Straightforward for interviews.
| uint32_t reverseBits(uint32_t n) { uint32_t result = 0; for (int i = 0; i < 32; i++) { result <<= 1; // Make room for next bit result |= (n & 1); // Copy LSB of n → result n >>= 1; // Advance to next bit } return result;} |

Method 2: Divide & Conquer — O(log 32) = O(1), used in production firmware.
| uint32_t reverseBits_fast(uint32_t n) { n = ((n >> 16) | (n << 16)); // Swap 16-bit halves n = ((n & 0xFF00FF00) >> 8) | ((n & 0x00FF00FF) << 8); // Swap bytes n = ((n & 0xF0F0F0F0) >> 4) | ((n & 0x0F0F0F0F) << 4); // Swap nibbles n = ((n & 0xCCCCCCCC) >> 2) | ((n & 0x33333333) << 2); // Swap pairs n = ((n & 0xAAAAAAAA) >> 1) | ((n & 0x55555555) << 1); // Swap bits return n;} |

Q11: Find the position of the RIGHTMOST SET BIT
Key Insight: n & (-n) isolates the rightmost set bit using two's complement arithmetic: -n = ~n + 1.
| // n = 12 = 0b0000_1100// -n = 0b1111_0100 (two's complement)// n & -n = 0b0000_0100 → position 3 (1-indexed)int rightmostSetBit(unsigned int n) { if (n == 0) return -1; int pos = 1; unsigned int isolated = n & (-n); while (isolated >>= 1) pos++; return pos;}// GCC built-in: __builtin_ffs(n) — 1-indexed, returns 0 if n=0 |

Q12: How to TURN OFF the rightmost set bit?
Formula: n & (n - 1)   — subtracting 1 flips all bits from the rightmost 1 down to bit 0, so ANDing clears exactly that bit.
| // n=12: 0b1100, n-1=0b1011, n&(n-1)=0b1000 ✓// n=10: 0b1010, n-1=0b1001, n&(n-1)=0b1000 ✓#define TURN_OFF_RIGHTMOST_BIT(n) ((n) & ((n) - 1))// Also used to count set bits (Brian Kernighan's, Q5)// and to test if a number is power of 2 (Q6) |

Q13: How to determine the SIGN of an integer using bitwise operators?
Concept: In two's complement, the MSB (most significant bit) is the sign bit: 1 = negative, 0 = non-negative.
| int getSign(int n) { return (n >> (sizeof(int) * 8 - 1)) & 1; // 1 = negative, 0 = positive/zero}// For 32-bit int: n >> 31 gives 0 (positive) or -1 / 0xFFFFFFFF (negative)// ⚠ Right-shifting signed integers is implementation-defined — use only on// platforms where arithmetic shift is guaranteed (Linux kernel targets). |

Q14: Compute ABSOLUTE VALUE without branching
Formula: (n ^ mask) - mask   where mask = n >> 31. Branchless code avoids branch misprediction penalties.
| // If n >= 0: mask = 0x00000000 → (n ^ 0) - 0 = n (unchanged)// If n < 0: mask = 0xFFFFFFFF → (n ^ ~0) - (-1) = (~n) + 1 = -n// Example: n = -5 (0xFFFFFFFB)// mask = 0xFFFFFFFF// n^mask = 0x00000004 = 4// 4 - (-1) = 5 ✓int abs_val(int n) { int mask = n >> 31; return (n ^ mask) - mask;} |

Q15: MULTIPLY / DIVIDE by powers of 2 using shift operators
Left Shift (<<): Multiplies by 2 per position. Right Shift (>>): Divides by 2 per position (truncates toward negative infinity).
| // Multiply: 5<<1=10, 5<<2=20, 5<<3=40// Divide: 20>>1=10, 20>>2=5, 15>>1=7 (truncated)// Multiply by arbitrary constants using shifts:int mul7(int n) { return (n << 3) - n; } // n*8 - n = n*7int mul10(int n) { return (n << 3) + (n << 1); } // n*8 + n*2 = n*10int mul15(int n) { return (n << 4) - n; } // n*16 - n = n*15 |

| ⚠ Interview Notes:• Left shift can overflow — check bounds in production code.• Right shift of negative signed integers is implementation-defined.• Modern compilers already auto-optimize multiplication by constants to shifts; prefer clarity unless in a tight hot loop. |


| SECTION 2: Advanced & Firmware-Specific Questions (Q16 – Q22) |


Q16: Write to a Control/Status Register to clear error bits
Scenario: 32-bit status register where bits [7:4] are error flags.
| 🔑 Critical Interview Point: Many hardware registers are W1C (Write-1-to-Clear).Writing a 1 to the bit CLEARS it — the opposite of normal register semantics.Always ask: "Is this register R/W, R/O, W1C, or W1S?" |

| #define STATUS_REG_ADDR 0x40001000#define ERROR_MASK 0x000000F0 // Bits [7:4]void clear_errors(void) { volatile uint32_t *reg = (volatile uint32_t *)STATUS_REG_ADDR; // Method 1: Read-Modify-Write (for normal R/W registers) uint32_t val = *reg; val &= ~ERROR_MASK; // Clear bits [7:4] only *reg = val; // Method 2: Write-1-to-Clear (W1C) registers // Write 1 to the error bits — hardware clears them *reg = ERROR_MASK; // Writes 0x000000F0 — clears bits 4–7} |

Q17: Combine and write parameters to a configuration register (UART example)
Scenario: Pack multiple fields into a 32-bit UART config register using bit fields and masks.
Register Layout:
| Bits | Field | Values | Meaning |
| [1:0] | Parity | 00=None, 01=Odd, 10=Even | Parity mode selection |
| [3:2] | Stop bits | 00=1, 01=1.5, 10=2 | Stop bit count |
| [5:4] | Data bits | 00=5, 01=6, 10=7, 11=8 | Data word length |
| [6] | Enable | 0=Disable, 1=Enable | UART enable/disable |


| #define PARITY_SHIFT 0#define PARITY_MASK (0x3 << PARITY_SHIFT) // 0x03#define STOP_SHIFT 2#define STOP_MASK (0x3 << STOP_SHIFT) // 0x0C#define DATA_SHIFT 4#define DATA_MASK (0x3 << DATA_SHIFT) // 0x30#define ENABLE_BIT 6void configure_uart(uint8_t parity, uint8_t stop, uint8_t data_bits, uint8_t enable) { uint32_t reg = 0; reg &= ~PARITY_MASK; reg |= ((parity & 0x3) << PARITY_SHIFT); reg &= ~STOP_MASK; reg |= ((stop & 0x3) << STOP_SHIFT); reg &= ~DATA_MASK; reg |= ((data_bits & 0x3) << DATA_SHIFT); if (enable) reg |= (1 << ENABLE_BIT); write_register(UART_CTRL_REG, reg);} |

Q18: Assess the status of a transmit register — check if TX is busy
Scenario: Poll the UART status register before sending data to avoid dropping bytes.
| #define UART_STATUS_REG 0x40001004#define TX_BUSY_BIT 5#define TX_FIFO_FULL_BIT 3int is_tx_busy(void) { volatile uint32_t *status = (volatile uint32_t *)UART_STATUS_REG; return (*status & (1 << TX_BUSY_BIT)) != 0;}void uart_send_byte(uint8_t data) { volatile uint32_t *status = (volatile uint32_t *)UART_STATUS_REG; volatile uint32_t *tx_reg = (volatile uint32_t *)UART_TX_REG; // Poll until TX FIFO is not full while (*status & (1 << TX_FIFO_FULL_BIT)) ; // Busy-wait (consider timeout in production!) *tx_reg = data;} |

| ⚠ Production note: Use interrupt-driven or DMA TX wherever possible to avoid CPU stalls.Add a watchdog/timeout counter to the busy-wait loop to prevent infinite loops on hardware fault. |

Q19: Return error status using bitwise operators (multiple error codes)
Concept: Assign one bit per error type. A single integer return value encodes all active errors simultaneously — the caller then tests individual bits.
This pattern is used in: Linux ioctl(), poll() event masks, network protocol flags.
| #define ERR_NONE 0x00#define ERR_TIMEOUT (1 << 0) // 0x01#define ERR_OVERFLOW (1 << 1) // 0x02#define ERR_PARITY (1 << 2) // 0x04#define ERR_FRAMING (1 << 3) // 0x08#define ERR_DMA (1 << 4) // 0x10uint32_t check_uart_errors(void) { uint32_t errors = ERR_NONE; uint32_t status = read_register(UART_STATUS_REG); if (status & (1 << TIMEOUT_FLAG)) errors |= ERR_TIMEOUT; if (status & (1 << OVERFLOW_FLAG)) errors |= ERR_OVERFLOW; if (status & (1 << PARITY_FLAG)) errors |= ERR_PARITY; return errors; // May have multiple bits set!}// Caller side — test each error independently:uint32_t err = check_uart_errors();if (err & ERR_TIMEOUT) handle_timeout();if (err & ERR_PARITY) handle_parity_error();if (err == ERR_NONE) /* All good */; |

Q20: Implement BIT MASKING for hardware register access (generic macros)
Pattern: Read-Modify-Write using generic REG_SET_FIELD / REG_GET_FIELD macros. Reusable across any register.
| #define REG_SET_FIELD(reg, mask, shift, value) \ do { \ uint32_t tmp = read_register(reg); \ tmp &= ~(mask); /* Clear field */ \ tmp |= ((value) << (shift)) & (mask); /* Set new */ \ write_register(reg, tmp); \ } while(0)#define REG_GET_FIELD(reg, mask, shift) \ ((read_register(reg) & (mask)) >> (shift))// Example: Set clock divider (bits [11:8]) to value 5#define CLK_DIV_MASK 0x00000F00#define CLK_DIV_SHIFT 8REG_SET_FIELD(CLK_CTRL_REG, CLK_DIV_MASK, CLK_DIV_SHIFT, 5);// Read the current divider value:uint32_t div = REG_GET_FIELD(CLK_CTRL_REG, CLK_DIV_MASK, CLK_DIV_SHIFT); |

| 💡 The do { ... } while(0) idiom makes macros safe to use after if/else statements.Always use volatile pointers for MMIO registers to prevent compiler optimization. |

Q21: ENDIANNESS conversion — Big-Endian ↔ Little-Endian
Context: Little-Endian (x86, ARM default) stores LSB at the lowest address. Big-Endian (network byte order) stores MSB first.
| // 32-bit swap: 0xAABBCCDD → 0xDDCCBBAAuint32_t swap_endian_32(uint32_t n) { return ((n >> 24) & 0x000000FF) | // Byte 3 → Byte 0 ((n >> 8) & 0x0000FF00) | // Byte 2 → Byte 1 ((n << 8) & 0x00FF0000) | // Byte 1 → Byte 2 ((n << 24) & 0xFF000000); // Byte 0 → Byte 3}uint16_t swap_endian_16(uint16_t n) { return (n >> 8) | (n << 8);}// Linux Kernel equivalents:// cpu_to_be32(x), be32_to_cpu(x) — CPU ↔ Big-Endian// cpu_to_le32(x), le32_to_cpu(x) — CPU ↔ Little-Endian// htonl(x), ntohl(x) — Host ↔ Network (= Big-Endian)// htons(x), ntohs(x) — 16-bit versions |

Q22: Circular buffer with power-of-2 size using bitwise AND
Optimization: Replace modulo (%) with bitwise AND (&) for wrapping — requires buffer size to be a power of 2.
Why faster: index % 256 → division instruction (expensive on ARM w/o HW divider)   vs.   index & 0xFF → single AND (1 cycle)
| #define BUFFER_SIZE 256 // MUST be a power of 2#define BUFFER_MASK (BUFFER_SIZE - 1) // 0xFF — AND mask for wrappingtypedef struct { uint8_t data[BUFFER_SIZE]; uint32_t head; // Write index (producer) uint32_t tail; // Read index (consumer)} circular_buf_t;void buf_write(circular_buf_t *buf, uint8_t byte) { buf->data[buf->head & BUFFER_MASK] = byte; buf->head++;}uint8_t buf_read(circular_buf_t *buf) { uint8_t byte = buf->data[buf->tail & BUFFER_MASK]; buf->tail++; return byte;}int buf_count(circular_buf_t *buf) { return (buf->head - buf->tail) & BUFFER_MASK;} |

| 💡 The & BUFFER_MASK trick is used extensively in the Linux kernel kfifo implementation.⚠ For multi-threaded/ISR access: use memory barriers (smp_mb()) between the data write and the head/tail update to prevent reordering. |


| SECTION 3: Problem-Solving Questions (Q23 – Q30) |


Q23: HAMMING DISTANCE between two integers
Definition: Number of bit positions where two integers differ.
Approach: XOR the two numbers (1 where bits differ) → count the set bits using Brian Kernighan's algorithm.
| // x=1 (0b001), y=4 (0b100)// XOR = 0b101 → 2 set bits → Hamming Distance = 2int hammingDistance(int x, int y) { int xor_val = x ^ y; int count = 0; while (xor_val) { xor_val &= (xor_val - 1); // Clear lowest set bit count++; } return count;}// One-liner with built-in:// return __builtin_popcount(x ^ y); |

Q24: Find TWO non-repeating elements (all others appear exactly twice)
Algorithm (3 steps):
- Step 1: XOR all elements → result = a ^ b (XOR of the two unique numbers).
- Step 2: Find any set bit in a^b — this bit position differs between a and b.
- Step 3: Partition all elements into two groups by that bit; XOR each group separately to get a and b.
| // arr = [2, 4, 7, 9, 2, 4]// Step 1: XOR all = 7^9 = 0b0111 ^ 0b1001 = 0b1110// Step 2: Rightmost set bit of 0b1110 = 0b0010 (bit 1)// Step 3:// Group 1 (bit 1 set): 2(10), 7(111), 2(10) → XOR = 7// Group 2 (bit 1 not set): 4(100), 9(1001), 4(100) → XOR = 9// Answer: 7 and 9 ✓void findTwoUnique(int arr[], int n, int *x, int *y) { int xor_all = 0; for (int i = 0; i < n; i++) xor_all ^= arr[i]; int set_bit = xor_all & (-xor_all); // Isolate rightmost set bit *x = 0; *y = 0; for (int i = 0; i < n; i++) { if (arr[i] & set_bit) *x ^= arr[i]; else *y ^= arr[i]; }} |

Q25: Detect if two integers have OPPOSITE SIGNS
Formula: (x ^ y) < 0
Why it works: XOR of the sign bits produces 1 if they differ (opposite signs) → the result is negative.
| // +5 ^ -3: MSB(0) XOR MSB(1) = 1 → result is negative → opposite signs!// +5 ^ +3: MSB(0) XOR MSB(0) = 0 → result is positive → same signsint oppositeSign(int x, int y) { return (x ^ y) < 0; // true if signs differ}// Check same sign:int sameSign(int x, int y) { return (x ^ y) >= 0; } |

Q26: ADD two numbers without using the + operator
Concept: XOR gives the sum without carry; AND gives the carry bits. Shift carry left and repeat until no carry remains.
| // a=5 (0b0101), b=3 (0b0011)// Iter 1: carry = 0b0001, a = 0b0110, b = 0b0010// Iter 2: carry = 0b0010, a = 0b0100, b = 0b0100// Iter 3: carry = 0b0100, a = 0b0000, b = 0b1000// Iter 4: carry = 0b0000, a = 0b1000, b = 0b0000 → STOP, return 8 ✓int add(int a, int b) { while (b != 0) { int carry = a & b; // AND gives carry bits a = a ^ b; // XOR gives sum without carry b = carry << 1; // Shift carry to add at next position } return a;} |

Q27: Find the MISSING NUMBER in an array of 1 to N
XOR Approach (overflow-safe, unlike the sum approach):
XOR all numbers 1..N, then XOR with all array elements. Paired numbers cancel; only the missing number remains.
| // arr = [1, 2, 4, 5], N = 5// XOR(1..5) = 1^2^3^4^5// XOR(arr) = 1^2^4^5// XOR of both = 3 ✓ (the 3s cancel everything else)int findMissing(int arr[], int n) { int xor_all = 0, xor_arr = 0; for (int i = 1; i <= n; i++) xor_all ^= i; for (int i = 0; i < n - 1; i++) xor_arr ^= arr[i]; return xor_all ^ xor_arr;} |

Q28: Compute PARITY of a number
Definition: Parity = 1 if an odd number of bits are set; 0 if even. Used in error detection (ECC, CRC, serial comms).
| // Method 1: Toggle parity bit for each set bit — O(k)int parity(unsigned int n) { int p = 0; while (n) { p ^= 1; n &= (n - 1); // Clear lowest set bit } return p;}// Method 2: Divide and conquer — O(log n) = O(5 ops for 32-bit)// Folds parity: 32 bits → 16 → 8 → 4 → 2 → 1 bitint parity_fast(uint32_t n) { n ^= (n >> 16); n ^= (n >> 8); n ^= (n >> 4); n ^= (n >> 2); n ^= (n >> 1); return n & 1;}// GCC built-in: __builtin_parity(n) — returns 0 (even) or 1 (odd) |

Q29: ISOLATE the lowest set bit
Formula: n & (-n)  (equivalently: n & (~n + 1))
Why it works: In two's complement, negating n flips all bits then adds 1. The carry propagates exactly to the rightmost set bit, so AND isolates only that bit.
| // n = ...a 1 0 0 0 (rightmost 1 with trailing zeros)// ~n = ...ā 0 1 1 1// ~n+1 = ...ā 1 0 0 0 (carry propagates to rightmost 1)// n&-n = 0...0 1 0 0 0 (only that bit survives!)// Example: n=12=0b1100 → n&(-n)=0b0100 ✓unsigned int isolateLowestBit(unsigned int n) { return n & (-n);}// Use cases:// • Fenwick Tree (Binary Indexed Tree) navigation// • Finding rightmost set bit position (Q11)// • Splitting elements into groups (Q24) |

Q30: Generate ALL SUBSETS of a set using bit masking
Concept: For n elements, there are 2^n subsets. Represent each subset as an n-bit integer: bit i = 1 means element i is included.
| // arr=[a,b,c], n=3 → 8 subsets (2^3)// mask 000 → {} mask 001 → {a} mask 010 → {b}// mask 011 → {a,b} mask 100 → {c} mask 101 → {a,c}// mask 110 → {b,c} mask 111 → {a,b,c}void generateSubsets(int arr[], int n) { int total = 1 << n; // 2^n subsets for (int mask = 0; mask < total; mask++) { printf("{ "); for (int i = 0; i < n; i++) { if (mask & (1 << i)) printf("%d ", arr[i]); } printf("}\n"); }} |

Applications: Subset sum problems, Travelling Salesman (bitmask DP), combinatorial optimization, power set generation.

| SECTION 4: Quick Reference — Interview Cheat Sheet |


Fundamental Operations (Q1–Q15)
| # | Question | Key Formula | Key Insight | Time |
| 1 | Set a bit | n |= (1 << pos) | OR with 1 always sets | O(1) |
| 2 | Clear a bit | n &= ~(1 << pos) | AND with 0 always clears | O(1) |
| 3 | Toggle a bit | n ^= (1 << pos) | XOR with 1 flips | O(1) |
| 4 | Check a bit | n & (1 << pos) | Non-zero = bit set | O(1) |
| 5 | Count set bits (popcount) | n &= (n-1) | Kernighan: clears lowest set bit | O(k) |
| 6 | Is power of 2 | n && !(n&(n-1)) | Power of 2 → single set bit | O(1) |
| 7 | Swap without temp var | a^=b;b^=a;a^=b | XOR self-inverse; check a!=b | O(1) |
| 8 | Single unique element | XOR all elements | x^x=0; pairs cancel | O(n) |
| 9 | Odd-occurrence element | XOR all elements | Same as Q8 | O(n) |
| 10 | Reverse bits (32-bit) | D&C or shift loop | Swap halves → bytes → nibbles | O(1) |
| 11 | Rightmost set bit position | n & (-n) | Two's complement isolates it | O(1) |
| 12 | Turn off rightmost set bit | n & (n-1) | n-1 flips bits from rightmost 1 | O(1) |
| 13 | Determine sign of integer | (n >> 31) & 1 | MSB = sign bit in 2's complement | O(1) |
| 14 | Branchless absolute value | (n^mask)-mask | mask = n>>31; avoids branches | O(1) |
| 15 | Multiply/divide by 2^n | x<<n / x>>n | Left=×2, Right=÷2 per shift | O(1) |


Firmware & Register Questions (Q16–Q22)
| # | Topic | Pattern | Key Interview Points |
| 16 | Clear error bits | &= ~mask / W1C | Ask: is register W1C or R/W? Use volatile pointer. |
| 17 | Configure register | &= ~mask; |= val | Use shift macros for each field; mask off overflow. |
| 18 | Poll TX busy | while(reg & bit) | Always volatile; add timeout counter in production. |
| 19 | Multi-error bitmask | errors |= ERR_X | One bit per error; test with & (not ==); matches Linux ioctl. |
| 20 | Generic reg macros | REG_SET/GET_FIELD | Reusable RMW; do-while(0) macro safety; volatile pointer. |
| 21 | Endian conversion | cpu_to_be32() | Know Linux kernel helpers: cpu_to_be32, htonl, etc. |
| 22 | Circular buffer | idx & (SIZE-1) | Size must be power of 2; AND replaces %; used in kfifo. |


Problem-Solving Questions (Q23–Q30)
| # | Question | Key Formula | Algorithm Insight |
| 23 | Hamming Distance | popcount(x ^ y) | XOR gives 1 where bits differ; count set bits |
| 24 | Two unique elements | XOR+split+XOR | XOR all → find set bit → partition into 2 groups |
| 25 | Opposite signs | (x ^ y) < 0 | XOR of sign bits = 1 if different; result is negative |
| 26 | Add without + | XOR(sum) + AND(carry) | Iterate: sum=a^b, carry=(a&b)<<1 until carry=0 |
| 27 | Missing number 1..N | XOR(1..N)^XOR(arr) | Paired numbers cancel; missing one survives; no overflow |
| 28 | Parity | fold XOR right | Fold 32→16→8→4→2→1 bit; final &1 gives parity |
| 29 | Isolate lowest set bit | n & (-n) | Two's complement: -n = ~n+1; carry propagates to rightmost 1 |
| 30 | Generate all subsets | mask 0..(2^n - 1) | Each bit = one element inclusion; 2^n total subsets |


Bonus — One-Liner Tricks Quick Reference
| Trick | Formula | Example / Notes |
| Set bit n | x | (1<<n) | Set bit 3: x | 8 |
| Clear bit n | x & ~(1<<n) | Clear bit 3: x & 0xF7 |
| Toggle bit n | x ^ (1<<n) | Toggle bit 3: x ^ 8 |
| Turn off rightmost 1 | x & (x-1) | 0b1100 → 0b1000 |
| Isolate rightmost 1 | x & (-x) | 0b1100 → 0b0100 |
| Right-propagate rightmost 1 | x | (x-1) | 0b1100 → 0b1111 |
| Is power of 2 | x && !(x&(x-1)) | 8 → true; 6 → false |
| Modulo 2^n (fast) | x & (n-1) | x % 8 = x & 7 (n must be power of 2) |
| Multiply by 2^n | x << n | x * 8 = x << 3 |
| Divide by 2^n | x >> n | x / 8 = x >> 3 |
| Swap without temp | a^=b;b^=a;a^=b | Fails if a==b (same address); check first |
| Branchless abs value | (x^m)-m, m=x>>31 | No branch misprediction penalty |
| Min of two | y^((x^y)&-(x<y)) | Branchless; avoids conditional jump |
| Max of two | x^((x^y)&-(x<y)) | Branchless; avoids conditional jump |
| Same sign check | (x^y) >= 0 | XOR of sign bits = 0 when same sign |
| Round up to next power of 2 | n-- then n|=n>>k | k=1,2,4,8,16 then n++; Linux roundup_pow_of_two() |


| 💡 Interview Tips• Always state your approach before coding — explain the formula and why it works.• For register questions: ask about register type (R/W, R/O, W1C), width (8/16/32-bit), and access alignment.• Mention volatile keyword for MMIO — shows hardware awareness.• Reference GCC built-ins (__builtin_popcount, __builtin_ffs, __builtin_parity) for production vs. hand-rolled for interviews.• Know Linux kernel wrappers: is_power_of_2(), cpu_to_be32(), le32_to_cpu(), htonl() — demonstrates kernel-level familiarity. |

