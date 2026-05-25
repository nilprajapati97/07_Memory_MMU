# Page Table Walk in the Linux Kernel

## Problem
How do you walk the page table for a given virtual address in the Linux kernel? What are the key structures and steps?

## Solution Overview
- Use the process's `mm_struct` to access the page tables.
- Traverse PGD, P4D, PUD, PMD, and PTE levels for the given address.
- Print or inspect the PTE for the address.

## Key Structures
- `pgd_t`, `p4d_t`, `pud_t`, `pmd_t`, `pte_t`
- `mm_struct` (process memory descriptor)

## Interview Notes
- Discuss differences between x86 and ARM page table layouts.
- Be ready to explain large pages, page faults, and kernel/user address spaces.
