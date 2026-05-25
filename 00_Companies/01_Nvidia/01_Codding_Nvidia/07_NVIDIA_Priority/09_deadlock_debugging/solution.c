// Deadlock debugging example (pseudo-code)
#include <linux/mutex.h>
#include <linux/printk.h>

void debug_deadlock(struct mutex *a, struct mutex *b) {
    mutex_lock(a);
    printk("Locked A\n");
    mutex_lock(b);
    printk("Locked B\n");
    // ... critical section ...
    mutex_unlock(b);
    mutex_unlock(a);
}
// Use lockdep in kernel for real deadlock detection.
