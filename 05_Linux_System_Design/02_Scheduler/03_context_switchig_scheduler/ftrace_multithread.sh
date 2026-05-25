#!/bin/bash
# ftrace script for 00_multithreading.c
# Traces: function calls, scheduler events, context switches, syscalls

TRACEFS=/sys/kernel/debug/tracing
BINARY="./multithreading_trace"
OUTPUT="ftrace_output.txt"

echo "[*] Setting up ftrace..."

# Reset ftrace
echo 0 > $TRACEFS/tracing_on
echo > $TRACEFS/trace
echo nop > $TRACEFS/current_tracer

# Enable function_graph tracer (shows call graph with timing)
echo function_graph > $TRACEFS/current_tracer

# Filter only relevant kernel functions for threading
echo "pthread*
futex*
do_futex
__schedule
schedule
wake_up*
try_to_wake_up
context_switch
finish_task_switch
copy_process
clone*
sys_clone
kernel_clone" > $TRACEFS/set_ftrace_filter 2>/dev/null || true

# Enable scheduler and syscall events
echo 1 > $TRACEFS/events/sched/sched_switch/enable
echo 1 > $TRACEFS/events/sched/sched_wakeup/enable
echo 1 > $TRACEFS/events/syscalls/sys_enter_futex/enable
echo 1 > $TRACEFS/events/syscalls/sys_exit_futex/enable
echo 1 > $TRACEFS/events/syscalls/sys_enter_clone3/enable

# Set larger buffer
echo 4096 > $TRACEFS/buffer_size_kb

# Start tracing
echo 1 > $TRACEFS/tracing_on
echo "[*] Tracing ON — running $BINARY ..."

# Run the binary and capture its PID
$BINARY &
PID=$!
wait $PID

# Stop tracing
echo 0 > $TRACEFS/tracing_on
echo "[*] Tracing OFF"

# Save output
cat $TRACEFS/trace > $OUTPUT
echo "[*] ftrace output saved to $OUTPUT"
echo "[*] Total lines captured: $(wc -l < $OUTPUT)"

# Cleanup
echo > $TRACEFS/trace
echo nop > $TRACEFS/current_tracer
echo 0 > $TRACEFS/events/sched/sched_switch/enable
echo 0 > $TRACEFS/events/sched/sched_wakeup/enable
echo 0 > $TRACEFS/events/syscalls/sys_enter_futex/enable
echo 0 > $TRACEFS/events/syscalls/sys_exit_futex/enable
echo 0 > $TRACEFS/events/syscalls/sys_enter_clone3/enable
