// Atomic reference counter
#include <stdatomic.h>
typedef struct { atomic_int ref; } atomic_ref_t;

void atomic_ref_init(atomic_ref_t *r, int v) { atomic_init(&r->ref, v); }
void atomic_ref_inc(atomic_ref_t *r) { atomic_fetch_add(&r->ref, 1); }
int atomic_ref_dec(atomic_ref_t *r) { return atomic_fetch_sub(&r->ref, 1) == 1; }
