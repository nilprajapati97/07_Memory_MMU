// Simple spinlock implementation
#include <stdatomic.h>
typedef struct { atomic_flag flag; } spinlock_t;

void spinlock_init(spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
}

void spin_lock(spinlock_t *lock) {
    while (atomic_flag_test_and_set(&lock->flag)) {}
}

void spin_unlock(spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
}
