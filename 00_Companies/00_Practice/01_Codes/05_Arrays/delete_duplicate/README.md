# Delete Duplicates from Array - Complete Learning Guide

## � QUICK RECOMMENDATIONS

### For Embedded Systems (MINIMUM CPU CYCLES):
- **Best Choice: METHOD 4 (Hash Array)** ✅✅✅
  - ~2,200 cycles (1000 elements)
  - Fixed range integers (0-100, 0-4095, etc.)
  - Stack allocation (no malloc)
  - See: `EmbeddedOptimization.c`

- **If Pre-Sorted: METHOD 2 (Sorted Array)** ✅✅
  - ~2,000 cycles (LOWEST!)
  - Best if you can sort offline
  - Absolutely minimal overhead

### For General Use:
- **Best Choice: METHOD 5 (Sort-First)** ✅
  - O(n log n) time
  - O(1) space
  - Works with any array

---

## �📚 Overview

This directory contains **6 different methods** to delete/remove duplicate elements from an array in C.

Each method is implemented in a **separate file** with:
- Detailed explanation in comments
- Full algorithm walkthrough
- Multiple test cases
- Example traces
- Time and space complexity analysis

---

## 📂 Files in This Directory

### Individual Method Files

1. **Method1SimpleShift.c** - Simple Shifting Approach
2. **Method2SortedArray.c** - Sorted Array Approach
3. **Method3ExtraArray.c** - Extra Array Approach
4. **Method4HashArray.c** - Hash Array Approach
5. **Method5SortFirst.c** - Sort-First Approach
6. **Method6OrderPreserve.c** - Order-Preserving Approach

### Supporting Files

- **ComparisonAllMethods.c** - Side-by-side comparison of all 6 methods
- **README.md** - This comprehensive guide (you are here!)

---

## 🎯 Quick Reference

| File | Method | Time | Space | Best For | Cycles |
|------|--------|------|-------|----------|--------|
| method1 | Simple Shift | O(n²) | O(1) | Learning | ❌ Very High |
| method2 | Sorted Array | O(n) | O(1) | Pre-sorted data | ✅ Lowest (if sorted) |
| method3 | Extra Array | O(n²) | O(n) | Order preservation | ❌ Very High |
| method4 | Hash Array | O(n) | O(n) | Small range | ✅ Lowest overall |
| method5 | Sort-First | O(n log n) | O(1) | **General use** ✓ | 🟡 Low-Medium |
| method6 | Order-Preserve | O(n²) | O(n) | Order matters | ❌ Very High |

---

## 📖 Detailed Methods

### METHOD 1: Simple Shifting
**File:** `Method1SimpleShift.c`

**What it does:**
- Compare each element with all following elements
- If duplicate found, shift all elements left to remove it

**Complexity:**
- Time: **O(n²)** - nested loops with shifting
- Space: **O(1)** - in-place modification

**When to use:**
- ✅ Very small arrays (< 10 elements)
- ✅ Learning/educational purposes
- ✅ Memory extremely limited
- ❌ NOT for large arrays (too slow)

**Example:**
```
Original:  [1, 2, 2, 3, 4, 4, 5]
           Shift left when duplicate found
Result:    [1, 2, 3, 4, 5]
```

---

### METHOD 2: Sorted Array
**File:** `Method2SortedArray.c`

**What it does:**
- Uses two-pointer technique on sorted array
- Compare adjacent elements
- Move unique elements forward

**Complexity:**
- Time: **O(n)** - single pass through sorted array
- Space: **O(1)** - in-place

**When to use:**
- ✅ Array is already sorted
- ✅ Need O(n) performance
- ✅ Memory is limited
- ❌ Unsorted data (need sort first: O(n log n))

**Example:**
```
Original (sorted): [1, 1, 2, 2, 3, 3]
                    ↓ Only unique elements forward
Result:            [1, 2, 3]
```

**Note:** Most efficient IF data is pre-sorted!

---

### METHOD 3: Extra Array
**File:** `Method3ExtraArray.c`

**What it does:**
- Use separate result array for unique elements
- For each element, check if already in result
- Add if not found (first occurrence)

**Complexity:**
- Time: **O(n²)** - checking against all previous
- Space: **O(n)** - extra result array

**When to use:**
- ✅ Must preserve original order
- ✅ Unsorted array
- ✅ Order of first appearance matters
- ❌ Memory very limited
- ❌ Large arrays with speed requirement

**Example:**
```
Original:  [5, 3, 5, 1, 3, 2]
           Check if in result → if not, add
Result:    [5, 3, 1, 2] (order preserved)
```

---

### METHOD 4: Hash Array
**File:** `Method4HashArray.c`

**What it does:**
- Create frequency array for each possible value
- Mark which numbers appear
- Collect elements with frequency > 0

**Complexity:**
- Time: **O(n + maxVal)** - effectively O(n) if maxVal small
- Space: **O(maxVal)** - frequency array size

**When to use:**
- ✅ Integer range is small (0-100, 0-1000)
- ✅ Need fastest performance for this range
- ✅ Sorted result acceptable (automatic)
- ❌ Large integer range (0-1,000,000,000)
- ❌ Floating point numbers
- ❌ Negative numbers (need offset)

**Example:**
```
Original:  [5, 2, 8, 1, 5, 2]
           Mark frequencies: freq[1]=1, freq[2]=2, freq[5]=2, freq[8]=1
Result:    [1, 2, 5, 8] (automatically sorted!)
```

**⚡ FASTEST METHOD for small ranges!**

---

### METHOD 5: Sort-First
**File:** `Method5SortFirst.c`

**What it does:**
- Sort array first using Quick Sort
- Then apply efficient O(n) duplicate removal
- Best balance of time and space

**Complexity:**
- Time: **O(n log n)** - sorting dominates
- Space: **O(1)** - in-place (except O(log n) recursion)

**When to use:**
- ✅ Unsorted array
- ✅ Need good balance of time/space
- ✅ Memory not extremely limited
- ✅ **GENERAL-PURPOSE / RECOMMENDED** ✓
- ✅ Production code
- ❌ Must preserve original order
- ❌ Data already sorted (use Method 2)

**Example:**
```
Original:  [7, 3, 7, 1, 9]
Step 1:    Sort → [1, 3, 7, 7, 9]
Step 2:    Remove duplicates → [1, 3, 7, 9]
Result:    [1, 3, 7, 9] (sorted, unique)
```

**✅ BEST FOR MOST REAL-WORLD SCENARIOS!**

---

### METHOD 6: Order-Preserving
**File:** `Method6OrderPreserve.c`

**What it does:**
- Track which elements have been seen
- Only add element on first appearance
- Maintain original sequence

**Complexity:**
- Time: **O(n²)** - checking against all previous
- Space: **O(n)** - result + flag array

**When to use:**
- ✅ Order of first appearance matters
- ✅ Processing logs or events
- ✅ Time-series data
- ✅ Unsorted input
- ❌ Speed critical for large arrays
- ❌ Memory very limited

**Example:**
```
Original:  [3, 1, 3, 2, 1, 4, 2]
First 3:   Add (first time) → [3]
First 1:   Add (first time) → [3, 1]
Dup 3:     Skip
First 2:   Add (first time) → [3, 1, 2]
Dup 1:     Skip
First 4:   Add (first time) → [3, 1, 2, 4]
Dup 2:     Skip

Result:    [3, 1, 2, 4] (order preserved)
```

---

## 🔍 How to Compile and Run

### Compile individual method:
```bash
gcc -o Method1 Method1SimpleShift.c
./Method1

gcc -o Method2 Method2SortedArray.c
./Method2

gcc -o Method3 Method3ExtraArray.c
./Method3

gcc -o Method4 Method4HashArray.c
./Method4

gcc -o Method5 Method5SortFirst.c
./Method5

gcc -o Method6 Method6OrderPreserve.c
./Method6
```

### Compile comparison:
```bash
gcc -o Comparison ComparisonAllMethods.c
./comparison
```

### Run all methods:
```bash
# Windows (PowerShell)
Method1.exe; Method2.exe; Method3.exe; Method4.exe; Method5.exe; Method6.exe

# Linux/Mac
./Method1 && ./Method2 && ./Method3 && ./Method4 && ./Method5 && ./Method6
```

---

## 📊 Performance Comparison

### For 1000 elements:

```
Method 1 (Simple):     ~500,000 operations (TOO SLOW)
Method 2 (Sorted):     ~1,000 operations (FAST if pre-sorted)
Method 3 (Extra):      ~500,000 operations (SLOW)
Method 4 (Hash):       ~1,050 operations (FASTEST for range 0-50)
Method 5 (Sort):       ~10,000 operations (GOOD BALANCE) ✓
Method 6 (Order):      ~500,000 operations (SLOW)
```

### Recommended for different sizes:

| Array Size | Method | Reason |
|-----------|--------|---------|
| < 50 | Any | All fast enough |
| 50-1000 | 5 (Sort) | Good balance |
| 1000-10000 | 4 (Hash) if range small<br>5 (Sort) otherwise | Hash fastest, Sort practical |
| > 10000 | 4 (Hash) or 5 (Sort) | Avoid O(n²) methods |

---

## 🎓 Learning Path

### Beginner (Start Here)
1. Read Method 1 explanation
2. Trace through the algorithm manually
3. Compile and run Method1SimpleShift.c
4. Modify test cases and experiment
5. Understand O(n²) complexity

### Intermediate
1. Study Method 2 for comparison
2. Understand why sorted data matters
3. Learn Method 5 (Sort-First)
4. Compare time complexity with Method 1
5. Run ComparisonAllMethods.c

### Advanced
1. Study Method 4 (Hash Array)
2. Learn Method 3 & 6 (Order-Preserving)
3. Understand trade-offs between methods
4. Practice choosing right method for scenario
5. Optimize for different constraints

### Expert
1. Master all 6 methods
2. Understand edge cases
3. Know when to use each
4. Can optimize for specific needs
5. Can explain complexity analysis

---

## 💡 Decision Tree

```
Choose a method based on:

1. Is array SORTED?
   YES → Use Method 2 [O(n)]
   NO → Next question

2. Is memory EXTREMELY LIMITED?
   YES → Use Method 5 [O(n log n), O(1)]
   NO → Next question

3. Integer RANGE SMALL (< 1000)?
   YES → Use Method 4 [O(n)]
   NO → Next question

4. Must ORDER BE PRESERVED?
   YES → Use Method 3 or 6 [O(n²)]
   NO → Use Method 5 [O(n log n)] ← RECOMMENDED
```

---

## 📝 Algorithm Characteristics

### Stability
- Stable (preserves order): Methods 3, 6
- Not stable: Methods 1, 2, 4, 5

### In-Place
- In-place: Methods 1, 2, 5
- Extra space: Methods 3, 4, 6

### Output Order
- Preserves input order: Methods 1, 3, 6
- Sorted output: Methods 2, 4, 5

### Input Requirement
- Requires sorted: Method 2
- Works with any: Methods 1, 3, 4, 5, 6

---

## 🐛 Common Issues and Solutions

### Issue: Array not modified
**Solution:** Some methods return size but don't modify array. Check if you're using result array.

### Issue: Time Limit Exceeded
**Solution:** You're using O(n²) method on large array. Use Method 4 or 5.

### Issue: Memory Limit Exceeded
**Solution:** You're using Method 3 or 4 with extra space. Use Method 5 instead.

### Issue: Wrong output order
**Solution:** Check if you need order preserved. Use Method 3 or 6 if needed.

---

## 🎯 Recommended Approach for Most Cases

**For Production Code:**
```c
Use Method 5 (Sort-First)
├─ O(n log n) time (acceptable)
├─ O(1) space (very good)
├─ Works with any array
└─ Predictable performance
```

**For Performance-Critical:**
```c
Use Method 4 (Hash) if range < 100,000
Otherwise use Method 5
```

**For Order Preservation:**
```c
Use Method 3 or 6 (Order-Preserving)
Or use hash table for O(n)
```

**For Educational Purposes:**
```c
Start with Method 1 → Understand concept
Learn Method 2 → See benefit of sorting
Study Methods 4 & 5 → Understand optimization
Master Methods 3 & 6 → Edge cases
```

---

## 🖥️ EMBEDDED SYSTEMS - MINIMUM CPU CYCLES

### **Best Choice for Embedded: METHOD 4 (Hash Array)**

**⚡ LOWEST CPU CYCLES:** Method 4 is optimal for embedded systems!

**Why Method 4 for Embedded Systems:**
```
Single linear pass through data
├─ No nested loops → Predictable cycle count
├─ Fast frequency marking (simple array access)
├─ No complex comparisons
├─ Cache-friendly linear access
└─ Deterministic execution time
```

### CPU Cycles Comparison (for 1000 elements):

```
Method 1 (Simple):     ~1,000,000 cycles  ❌ TOO HIGH
Method 2 (Sorted):     ~2,000 cycles      ✅ IF PRE-SORTED
Method 3 (Extra):      ~1,000,000 cycles  ❌ TOO HIGH
Method 4 (Hash):       ~2,100 cycles      ✅✅ BEST OVERALL
Method 5 (Sort-First): ~20,000 cycles     🟡 ACCEPTABLE
Method 6 (Order):      ~1,000,000 cycles  ❌ TOO HIGH
```

### Embedded System Decision Tree:

```
1. Is integer range FIXED and SMALL (< 1000)?
   YES → Use Method 4 [O(n), ~2000 cycles]
   NO  → Next question

2. Can data be PRE-SORTED?
   YES → Use Method 2 [O(n), ~2000 cycles] 
   NO  → Next question

3. Is memory VERY CONSTRAINED?
   YES → Use Method 5 [O(n log n), ~20k cycles, O(1)]
   NO  → Use Method 4 if possible [O(n), ~2000 cycles, O(range)]
```

### Embedded System Recommendations by Use Case:

**IoT Sensor Data (fixed range 0-100):**
```
✅ Use Method 4 (Hash Array)
   - Sensor values have known range
   - Needs minimal CPU cycles
   - Deterministic performance
   - Real-time friendly
```

**Motor Control (limited range):**
```
✅ Use Method 4 (Hash Array) or Method 2 (Sorted)
   - If data can be pre-sorted → Method 2 (1 pass)
   - If data unsorted → Method 4 (1 pass + setup)
   - Both have predictable cycle count
```

**Real-Time Signal Processing:**
```
✅ Use Method 2 (Sorted Array)
   - Best if you can sort data offline
   - Single O(n) pass
   - Minimal interruption
```

**Memory-Constrained (e.g., 8-bit MCU):**
```
✅ Use Method 5 (Sort-First)
   - O(1) space requirement
   - ~20,000 cycles for small arrays acceptable
   - In-place operation saves RAM
```

### Cycle Count Analysis by Method:

**Method 2 (Sorted) - WINNER IF PRE-SORTED**
```c
Assumptions: Array pre-sorted, n=1000, comparing integers
Per element: 
  - 1 comparison
  - 0-1 assignment
  - 1 increment
Total: ~2,000 cycles
Characteristics:
  ✅ Absolutely minimal
  ✅ Fully predictable
  ✅ Cache-friendly
  ✅ No dynamic allocation
```

**Method 4 (Hash) - WINNER FOR UNSORTED**
```c
Assumptions: Array unsorted, n=1000, range=100
Setup: calloc(101) → ~100 cycles
Mark frequencies:
  - 1000 array accesses @ ~1 cycle each → 1000 cycles
  - 1000 increments → 1000 cycles
Collect results:
  - 100 comparisons → 100 cycles
  - Variable assignments → ~100 cycles
Total: ~2,200 cycles (+ malloc overhead)
Characteristics:
  ✅ Linear with data size
  ✅ Independent of duplicates count
  ✅ Predictable timing
  ✅ Best for real-time
```

**Method 5 (Sort-First)**
```c
Assumptions: n=1000 array
Quick Sort: O(n log n)
  - Best case: ~10,000 cycles
  - Worst case: ~50,000 cycles
Remove duplicates: O(n)
  - ~2,000 cycles
Total: 12,000-52,000 cycles (unpredictable!)
Characteristics:
  🟡 Unpredictable timing (randomized pivot)
  🟡 Worst case could spike
  ✅ Good for soft real-time
```

### Cycle-Optimized Code Recommendations:

```c
// FOR EMBEDDED - Method 4 Optimized
int removeDuplicates_HashOptimized(
    int arr[], int n, int result[], int maxVal) {
    
    // Stack-allocated freq array (faster than heap)
    int freq[101] = {0};  // If maxVal ≤ 100
    
    // Mark frequencies (tight loop, cache-friendly)
    for (int i = 0; i < n; i++) {
        freq[arr[i]]++;
    }
    
    // Collect results (predictable)
    int count = 0;
    for (int i = 0; i <= maxVal; i++) {
        if (freq[i] > 0) {
            result[count++] = i;
        }
    }
    
    return count;
}
```

**Cycle Savings: ~500 cycles** (no malloc/free overhead)

### Power Consumption Impact:

For battery-powered embedded devices:

```
Lower cycles = Lower power consumption

Method 4 (Hash): 
  - 2,200 cycles × 0.5mW per cycle = 1.1µW average
  - Good for battery-powered sensors

Method 5 (Sort-First):
  - 20,000 cycles × 0.5mW per cycle = 10µW average
  - 9x more power consumption!
  
Method 1/3/6:
  - 1,000,000 cycles × 0.5mW per cycle = 500µW
  - Battery drains 450x faster!
```

### Embedded System Performance Summary:

```
┌─────────┬──────────┬──────────────┬────────┐
│ Method  │ Cycles   │ Predictable? │ Best   │
├─────────┼──────────┼──────────────┼────────┤
│ 1 Shift │ 1M cycles│ ✅ (but high)│ NO     │
│ 2 Sorted│ 2K cycles│ ✅✅ YES     │ ✅ if  │
│ 3 Extra │ 1M cycles│ ✅ (but high)│ NO     │
│ 4 Hash  │ 2.2K cyc │ ✅✅ YES     │ ✅✅✅ │
│ 5 Sort  │ 12-52K   │ ⚠️  Variable│ 🟡     │
│ 6 Order │ 1M cycles│ ✅ (but high)│ NO     │
└─────────┴──────────┴──────────────┴────────┘

✅✅✅ = Best choice
✅✅   = Good choice  
✅     = Acceptable
🟡     = Use only if needed
NO     = Avoid
```

### Real-World Embedded Example:

**Scenario:** STM32 microcontroller reading 10 temperature sensors every 100ms
```
Data: 10 values, range 0-100°C (stored as 0-100)

Using Method 4 (Hash):
├─ Cycle cost: ~200 cycles per read
├─ Time: 0.2µs on 1MHz MCU
├─ Power: Negligible
└─ Best choice ✅

Using Method 5 (Sort-First):
├─ Cycle cost: ~2000 cycles per read  
├─ Time: 2µs on 1MHz MCU
├─ Power: 10x higher
└─ Overkill for this task
```

### Embedded Best Practices:

```
1. For FIXED RANGE data:
   Use Method 4 (Hash) with stack allocation
   Example: Sensor values 0-4095 (12-bit ADC)
   Stack freq array: int freq[4096]

2. For PRE-SORTED data:
   Use Method 2 (Sorted Array)
   Best if you can sort during initialization

3. For CRITICAL TIMING:
   Use Method 2 (Sorted) → Most predictable
   Avoid Method 5 (Sort-First) → Variable timing

4. For MEMORY-CRITICAL (< 2KB):
   Use Method 5 (Sort-First) → O(1) space
   Avoid Method 4 (Hash) → Large frequency array

5. For POWER-CRITICAL (battery):
   Use Method 4 (Hash) → Least cycles
   Method 4 wins in all scenarios
```

---

## 📚 Key Concepts to Learn

1. **Time Complexity Analysis**
   - O(n) vs O(n²) vs O(n log n)
   - How nested loops affect complexity
   - How sorting impacts total time

2. **Space Complexity Trade-offs**
   - In-place vs extra space
   - When extra space is worth it
   - Stack space in recursion

3. **Algorithm Optimization**
   - Same problem, multiple solutions
   - Trade-offs between time/space
   - Practical considerations vs theory

4. **Data Structure Choices**
   - Arrays vs sorted arrays
   - Hash tables vs arrays
   - When each is appropriate

---

## 🔗 Related Topics

- Sorting algorithms (Quick Sort, Merge Sort)
- Hash tables and hashing
- Two-pointer technique
- Time complexity analysis
- Space complexity analysis

---

## ✅ Summary Table

All methods at a glance:

```
┌────┬──────────────┬────────┬────────┬─────────┬──────────────┬─────────────────────┐
│ No │ Method       │ Time   │ Space  │ Sorted? │ Cycles       │ Best For            │
├────┼──────────────┼────────┼────────┼─────────┼──────────────┼─────────────────────┤
│ 1  │ Simple       │ O(n²)  │ O(1)   │ No      │ ~1M (HIGH)   │ Learning            │
│ 2  │ Sorted Arr   │ O(n)   │ O(1)   │ YES     │ ~2K (LOWEST) │ Pre-sorted, Embedded│
│ 3  │ Extra Arr    │ O(n²)  │ O(n)   │ No      │ ~1M (HIGH)   │ Order preserved     │
│ 4  │ Hash         │ O(n)   │ O(n)   │ No      │ ~2.2K (LOW)  │ Embedded ✓✓✓ BEST   │
│ 5  │ Sort-First   │ O(nlogn)│ O(1)   │ No      │ ~20K (MED)   │ General use ✓       │
│ 6  │ Order Pres   │ O(n²)  │ O(n)   │ No      │ ~1M (HIGH)   │ Order matters       │
└────┴──────────────┴────────┴────────┴─────────┴──────────────┴─────────────────────┘

NOTE: Cycle counts are approximate for 1000 elements on typical 32-bit MCU
      Embedded: Use Method 4 (Hash) or Method 2 (Sorted) for minimum cycles
```

---

## 📞 Practice Exercises

1. **Beginner:**
   - Modify test cases in method1
   - Trace algorithm on paper
   - Calculate complexity for different inputs

2. **Intermediate:**
   - Compare output of different methods
   - Time methods on large arrays
   - Modify methods to add features

3. **Advanced:**
   - Combine methods (sort + hash)
   - Handle edge cases
   - Optimize for specific scenarios
   - Implement variant algorithms

---

## 🎉 Conclusion

You now have **6 complete, well-documented methods** to remove duplicates from arrays!

**Start with** Method1SimpleShift.c for learning.
**Use in production** Method5SortFirst.c for most cases.
**Optimize specifically** with Method4HashArray.c when needed.

Happy learning! 🚀
