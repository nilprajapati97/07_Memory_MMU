# Q48: Trie for Kernel Module Parameter Namespace

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** trie, prefix tree, module parameters, string lookup, O(L) search, `module_param`, namespace management

---

## Question

Implement a trie for GPU driver module parameter namespace lookup.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/string.h>

/* ─── Trie Node ───────────────────────────────────────────────────────────
 * Supports printable ASCII characters (0x20 to 0x7E, index 0–94).
 * Using full 128-char table for simplicity (indices 0–127).
 */
#define TRIE_ALPHA 128  /* ASCII character set size */

struct trie_node {
    struct trie_node *children[TRIE_ALPHA];
    void             *value;       /* non-NULL = this node is a key endpoint */
    bool              is_terminal; /* true if a complete key ends here        */
};

/* ─── Trie (module parameter store) ──────────────────────────────────────*/
struct trie {
    struct trie_node *root;
    int               count; /* number of stored keys */
};

/* ─── Helper: allocate a trie node ───────────────────────────────────────*/
static struct trie_node *trie_node_alloc(void)
{
    return kzalloc(sizeof(struct trie_node), GFP_KERNEL);
    /* kzalloc zeros all children pointers and is_terminal */
}

/* ─── Initialize trie ─────────────────────────────────────────────────────*/
int trie_init(struct trie *t)
{
    t->root  = trie_node_alloc();
    t->count = 0;
    return t->root ? 0 : -ENOMEM;
}

/* ─── Insert key with associated value ──────────────────────────────────
 * Time: O(L) where L = key length
 * Space: O(L × TRIE_ALPHA) worst case per key (no sharing)
 */
int trie_insert(struct trie *t, const char *key, void *value)
{
    struct trie_node *cur = t->root;
    unsigned char c;

    if (!key || !*key)
        return -EINVAL;

    while ((c = (unsigned char)*key++) != '\0') {
        if (c >= TRIE_ALPHA) {
            pr_err("Trie: invalid character %c (0x%x)\n", c, c);
            return -EINVAL;
        }

        if (!cur->children[c]) {
            cur->children[c] = trie_node_alloc();
            if (!cur->children[c])
                return -ENOMEM;
        }
        cur = cur->children[c];
    }

    /* Mark this node as a terminal (complete key) */
    if (!cur->is_terminal)
        t->count++;

    cur->is_terminal = true;
    cur->value       = value;
    return 0;
}

/* ─── Look up a key ──────────────────────────────────────────────────────
 * Returns the value if key exists, NULL if not found.
 * Time: O(L)
 */
void *trie_lookup(struct trie *t, const char *key)
{
    struct trie_node *cur = t->root;
    unsigned char c;

    if (!key || !*key)
        return NULL;

    while ((c = (unsigned char)*key++) != '\0') {
        if (c >= TRIE_ALPHA || !cur->children[c])
            return NULL; /* key not in trie */
        cur = cur->children[c];
    }

    return cur->is_terminal ? cur->value : NULL;
}

/* ─── Prefix search: find all keys with given prefix ─────────────────────
 * Useful for tab-completion of module parameters.
 * Calls fn(key, value, priv) for each matching key.
 */
static void trie_prefix_walk(struct trie_node *node,
                               char *buf, int depth,
                               void (*fn)(const char *, void *, void *),
                               void *priv)
{
    int c;

    if (node->is_terminal)
        fn(buf, node->value, priv);

    for (c = 0; c < TRIE_ALPHA; c++) {
        if (node->children[c]) {
            buf[depth]     = (char)c;
            buf[depth + 1] = '\0';
            trie_prefix_walk(node->children[c], buf, depth + 1, fn, priv);
        }
    }
}

void trie_prefix_search(struct trie *t, const char *prefix,
                          void (*fn)(const char *, void *, void *), void *priv)
{
    struct trie_node *cur = t->root;
    char buf[256];
    int  depth = 0;
    unsigned char c;
    const char *p = prefix;

    /* Navigate to the prefix node */
    while ((c = (unsigned char)*p++) != '\0') {
        if (c >= TRIE_ALPHA || !cur->children[c])
            return; /* prefix not found */
        cur = cur->children[c];
        depth++;
    }

    /* Copy prefix into buf */
    strncpy(buf, prefix, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Walk all keys under this prefix */
    trie_prefix_walk(cur, buf, depth, fn, priv);
}

/* ─── Delete a key ────────────────────────────────────────────────────────
 * Mark terminal as false; optionally prune leaf nodes.
 */
bool trie_delete(struct trie *t, const char *key)
{
    struct trie_node *cur = t->root;
    unsigned char c;

    while ((c = (unsigned char)*key++) != '\0') {
        if (c >= TRIE_ALPHA || !cur->children[c])
            return false; /* key not found */
        cur = cur->children[c];
    }

    if (!cur->is_terminal)
        return false; /* key not in trie */

    cur->is_terminal = false;
    cur->value       = NULL;
    t->count--;
    return true;
}

/* ─── GPU Module Parameter Namespace ─────────────────────────────────────
 * Module parameters: "NVreg_MemoryPoolSize", "NVreg_EnableGpuFirmware", etc.
 * Trie lookup: O(L) where L = parameter name length ≈ 30 chars.
 * Much faster than linear scan for large parameter sets.
 */

struct gpu_param {
    const char *name;
    int         type;   /* PARAM_INT, PARAM_BOOL, PARAM_STRING */
    void       *value_ptr;
};

static struct trie gpu_param_trie;

int gpu_params_init(struct gpu_param *params, int count)
{
    int i, ret;

    ret = trie_init(&gpu_param_trie);
    if (ret)
        return ret;

    for (i = 0; i < count; i++) {
        ret = trie_insert(&gpu_param_trie, params[i].name, &params[i]);
        if (ret) {
            pr_err("GPU params: failed to insert '%s'\n", params[i].name);
            return ret;
        }
    }

    pr_info("GPU params: loaded %d parameters into trie\n", count);
    return 0;
}

struct gpu_param *gpu_param_find(const char *name)
{
    return (struct gpu_param *)trie_lookup(&gpu_param_trie, name);
}
```

---

## Explanation

### Core Concept

```
Trie storing: "NVreg_Mem", "NVreg_Ena", "NVreg_Enable"

root
└── 'N'
    └── 'V'
        └── 'r'
            └── 'e'
                └── 'g'
                    └── '_'
                        ├── 'M'
                        │   └── 'e'
                        │       └── 'm' [terminal: → param_mem]
                        └── 'E'
                            └── 'n'
                                └── 'a' [terminal: → param_ena]
                                    └── 'b'
                                        └── 'l'
                                            └── 'e' [terminal: → param_enable]

Lookup "NVreg_Enable": traverse 11 nodes = O(11) = O(L)
Linear scan alternative: O(N) comparisons where N = number of params
```

### Key APIs / Macros Used

| Concept | Purpose |
|---------|---------|
| `children[128]` | One child pointer per ASCII character |
| `is_terminal` | Marks end of a complete key |
| `value` | Associated data at terminal node |
| `kzalloc` | Allocate and zero-initialize node |
| `(unsigned char)*key++` | Safe character extraction (avoids sign extension) |

### Trade-offs & Pitfalls

- **Memory overhead.** Each node holds 128 pointers × 8 bytes = 1KB per node. A 30-character key creates 30 nodes = 30KB worst case. For large alphabets (UTF-8): use a sorted array of children or a hash map instead of a fixed array.
- **No bounds check on `depth` in `buf`** in the prefix walk. Production code must add a `depth >= sizeof(buf)-1` guard.

### NVIDIA / GPU Context

NVIDIA's driver uses `module_param()` macros for ~200 NVreg parameters. A trie over these parameter names allows O(1) average lookup vs O(200) linear scan. The `/proc/driver/nvidia/params` file enumerates all parameters — trie prefix search generates this list efficiently.

---

## Cross Questions & Answers

**CQ1: What is the time complexity of trie insert and lookup vs a hash table?**
> Trie insert/lookup: O(L) where L = key length. Hash table insert/lookup: O(L) to compute hash + O(1) average for table operation = O(L) overall. Same asymptotic complexity. But trie advantages: (1) O(L) worst case (no hash collisions), (2) prefix search O(L + K) where K = number of results (hash tables cannot do prefix search without scanning all buckets), (3) sorted iteration (in-order trie traversal gives keys in alphabetical order). Hash table advantages: lower constant factor, simpler implementation, better cache behavior.

**CQ2: What is a compressed trie (Patricia trie) and when is it used?**
> Compressed trie: merge chains of nodes with only one child into a single edge labeled with a string. Example: "NVreg_" prefix (6 chars) would normally create 6 nodes — in a Patricia trie, it becomes one node with edge label "NVreg_". Reduces space from O(L×ALPHA) to O(N×ALPHA) where N = number of keys (not total characters). Used when: (1) keys share long common prefixes (like module parameters all starting with "NVreg_"), (2) memory is constrained, (3) the key alphabet is large. Trade-off: slightly more complex string comparison at each node.

**CQ3: How would you make the trie thread-safe for concurrent module parameter access?**
> Two approaches: (1) **RWLock**: `read_lock` for lookups (most common), `write_lock` for insert/delete during module load/unload. Works well since parameter table is set once at module load and read-only thereafter. (2) **RCU**: protect the trie with RCU for read-mostly access. Writers create a new subtree and `rcu_assign_pointer` the parent's child pointer. Readers use `rcu_read_lock`. RCU is more complex to implement for a trie but allows truly lock-free reads. For GPU module parameters (set at load, read-only at runtime): a simple RWLock is sufficient.

**CQ4: How does the Linux kernel's `module_param` system work internally?**
> `module_param(name, type, perm)` macro: (1) creates a `struct kernel_param` with name, type ops, and a pointer to the variable, (2) stores it in the `__param` ELF section of the module, (3) the kernel's parameter parsing code at boot/module-load scans kernel command line / `modprobe` arguments for matching names, (4) calls the type-specific `set` function (e.g., `param_set_int`) to parse the string value and write to the variable. The `__param` section is essentially a flat array — NVIDIA's trie replaces linear scan with O(L) lookup for their custom parameter handling code.

**CQ5: What is an Aho-Corasick automaton and how does it extend the trie concept?**
> Aho-Corasick: a trie with failure links. Failure link of node `n` points to the longest proper suffix of `n`'s string that exists in the trie. Enables multi-pattern string search in O(N + M + Z) time where N = text length, M = total pattern length, Z = number of matches. Use cases in GPU driver: searching kernel oops output for multiple known error patterns simultaneously (ECC error, PCIe error, firmware error). The failure links transform the trie into a DFA (Deterministic Finite Automaton) — each character of input text causes exactly one state transition.
