// Walk the page table for a given virtual address in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/mm_types.h>

static void walk_page_table(struct task_struct *task, unsigned long vaddr) {
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    struct mm_struct *mm = task->mm;

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return;
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return;
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) return;
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) return;
    pte = pte_offset_map(pmd, vaddr);
    if (!pte) return;
    pr_info("PTE for 0x%lx: 0x%llx\n", vaddr, (unsigned long long)pte_val(*pte));
    pte_unmap(pte);
}

MODULE_LICENSE("GPL");
