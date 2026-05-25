// Stack overflow detection in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>

void check_stack_overflow(void) {
    unsigned long sp = (unsigned long)&sp;
    unsigned long stack_start = (unsigned long)task_stack_page(current);
    unsigned long stack_end = stack_start + THREAD_SIZE;
    if (sp < stack_start + 256) // 256 bytes margin
        pr_warn("Stack overflow risk: sp near stack start\n");
    if (sp > stack_end - 256)
        pr_warn("Stack overflow risk: sp near stack end\n");
}

MODULE_LICENSE("GPL");
