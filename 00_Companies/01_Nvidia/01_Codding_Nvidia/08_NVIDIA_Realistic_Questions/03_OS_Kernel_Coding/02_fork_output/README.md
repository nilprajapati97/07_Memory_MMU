# Explain output of fork-based code

## In-depth Explanation (Nvidia Interview Style)

- fork() duplicates the process; both parent and child execute the next instruction.
- Output order is non-deterministic due to scheduling.
- Each process has its own address space.

### Interview Tips
- Be ready to analyze code and predict possible outputs.
- Discuss process IDs and parent/child relationships.
