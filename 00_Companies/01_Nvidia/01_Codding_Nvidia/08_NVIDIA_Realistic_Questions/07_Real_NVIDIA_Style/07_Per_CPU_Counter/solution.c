// Per-CPU counter implementation in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/smp.h>

DEFINE_PER_CPU(int, my_counter);

void inc_my_counter(void) {
    this_cpu_inc(my_counter);
}

int sum_my_counter(void) {
    int sum = 0, cpu;
    for_each_possible_cpu(cpu)
        sum += per_cpu(my_counter, cpu);
    return sum;
}

MODULE_LICENSE("GPL");
