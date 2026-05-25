// Seqlock usage example in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/seqlock.h>

static seqlock_t my_seqlock;
static int shared_data;

void writer(void) {
    write_seqlock(&my_seqlock);
    shared_data++;
    write_sequnlock(&my_seqlock);
}

int reader(void) {
    unsigned seq;
    int val;
    do {
        seq = read_seqbegin(&my_seqlock);
        val = shared_data;
    } while (read_seqretry(&my_seqlock, seq));
    return val;
}

MODULE_LICENSE("GPL");
