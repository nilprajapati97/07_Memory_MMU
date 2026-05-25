# Function Pointers

## Declaration Syntax

### Basic Function Pointer
```c
int (*func_ptr)(int, int);  // Pointer to function taking 2 ints, returning int
```

### Array of Function Pointers
```c
int (*operations[4])(int, int);  // Array of 4 function pointers
```

### Function Returning Function Pointer
```c
int (*get_func(char))(int, int);  // Function returning function pointer
```

### Using typedef (Recommended)
```c
typedef int (*operation_t)(int, int);
operation_t get_func(char op);
```

## Reading Complex Declarations

**Clockwise/Spiral Rule:**
1. Start at identifier
2. Go right when you can
3. Go left when you must
4. Go up/down when blocked

Example: `int (*func_ptr)(int, int)`
- Start at `func_ptr`
- Go right: blocked by `)`
- Go left: `*` → pointer to
- Go right: `(int, int)` → function taking 2 ints
- Go left: `int` → returning int

## Common Use Cases

### 1. Callbacks
```c
void process(int data, void (*callback)(int));
```

### 2. Strategy Pattern
```c
int (*operations[])(int, int) = {add, sub, mul, div};
```

### 3. State Machines
```c
typedef void (*state_t)(void);
state_t current_state = idle_state;
```

### 4. Plugin Systems
```c
void (*plugin_init)(void);
plugin_init = dlsym(handle, "init");
```

## Interview Tips

1. **Syntax**: Use typedef for readability
2. **NULL check**: Always check before calling
3. **Use cases**: Callbacks, state machines, dispatch tables
4. **Comparison**: Can compare function pointers for equality

## Common Patterns

### Dispatch Table
```c
void (*handlers[256])(void);
handlers[event_id]();
```

### Command Pattern
```c
typedef void (*command_t)(void);
command_t undo_stack[100];
```

### Observer Pattern
```c
typedef void (*observer_t)(int);
observer_t observers[10];
```
