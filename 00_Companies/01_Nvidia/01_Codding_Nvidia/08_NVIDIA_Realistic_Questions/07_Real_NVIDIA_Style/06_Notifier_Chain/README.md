# Notifier Chains in the Linux Kernel

## Problem
How do you implement and use a notifier chain in the Linux kernel? What are the types and use cases?

## Solution Overview
- Use `BLOCKING_NOTIFIER_HEAD` to declare a blocking notifier chain.
- Register/unregister notifier blocks with the chain.
- Call the chain to notify all registered listeners.

## Types of Notifier Chains
- Atomic, blocking, raw, SRCU
- Each has different locking and context requirements

## Interview Notes
- Discuss use cases: reboot, network events, memory hotplug, etc.
- Be ready to explain locking, priorities, and error handling in notifier chains.
