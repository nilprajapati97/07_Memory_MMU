# Periodic Kernel Timer (hrtimer)

## Problem
Implement a kernel timer (hrtimer) that fires periodically and schedules deferred work on a workqueue.

## Solution Overview
- Uses hrtimer for high-resolution periodic events.
- Schedules work on a workqueue.

## Key Points
- Useful for periodic tasks in kernel modules.
