# Lock-Free Stack & ABA Problem

## In-depth Explanation (Nvidia Interview Style)
- Lock-free stacks use atomic CAS for push/pop.
- ABA problem: pointer value reused, can cause subtle bugs.

### Interview Tips
- Discuss hazard pointers, tagged pointers, and kernel lock-free APIs.
