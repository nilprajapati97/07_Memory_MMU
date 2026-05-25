# Check Endianness

## Approaches

### Approach 1: Pointer Cast (Most Common)
- Cast int pointer to char pointer
- Check first byte
- Interview favorite

### Approach 2: Union (Clean)
- Use union to overlay memory
- More readable
- Same principle

### Approach 3: Compile-time Macro
- Use GCC predefined macros
- Zero runtime cost
- Best for production

## Little vs Big Endian
**Little Endian**: LSB at lowest address (x86, ARM)
**Big Endian**: MSB at lowest address (Network byte order)

Example: 0x01020304
- Little: 04 03 02 01
- Big: 01 02 03 04
