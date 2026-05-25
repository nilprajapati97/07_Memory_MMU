#!/usr/bin/env python3

from enum import Enum
from typing import List

class ProcessState(Enum):
    READY = "READY"
    RUNNING = "RUNNING"
    WAITING = "WAITING"
    TERMINATED = "TERMINATED"

class Process:
    def __init__(self, pid: int, name: str, priority: int, burst_time: int):
        self.pid = pid
        self.name = name
        self.priority = priority
        self.burst_time = burst_time
        self.remaining_time = burst_time
        self.vruntime = 0
        self.state = ProcessState.READY
    
    def __repr__(self):
        return f"Process({self.pid}, {self.name}, vruntime={self.vruntime})"

class Scheduler:
    def __init__(self, time_quantum: int = 4):
        self.run_queue: List[Process] = []
        self.time_quantum = time_quantum
        self.current_time = 0
    
    def add_process(self, process: Process):
        self.run_queue.append(process)
        print(f"Added {process.name} to run queue")
    
    def pick_next_task(self) -> Process:
        """CFS: Pick process with minimum vruntime"""
        if not self.run_queue:
            return None
        return min(self.run_queue, key=lambda p: p.vruntime)
    
    def run(self):
        print("\n=== Linux Scheduler Simulation ===\n")
        
        while self.run_queue:
            current = self.pick_next_task()
            if not current:
                break
            
            current.state = ProcessState.RUNNING
            exec_time = min(current.remaining_time, self.time_quantum)
            
            print(f"Time {self.current_time:3d}: Running [PID:{current.pid} {current.name:12s}] "
                  f"(Priority:{current.priority:2d}, Remaining:{current.remaining_time:2d}, "
                  f"VRuntime:{current.vruntime:3d})")
            
            current.remaining_time -= exec_time
            current.vruntime += exec_time * (20 - current.priority)
            self.current_time += exec_time
            
            if current.remaining_time == 0:
                current.state = ProcessState.TERMINATED
                print(f"             -> Process {current.name} TERMINATED\n")
                self.run_queue.remove(current)
            else:
                current.state = ProcessState.READY
                print(f"             -> Context Switch (Time quantum expired)\n")
        
        print(f"All processes completed at time {self.current_time}")

def main():
    scheduler = Scheduler(time_quantum=4)
    
    scheduler.add_process(Process(1, "WebBrowser", 15, 12))
    scheduler.add_process(Process(2, "VideoPlayer", 10, 8))
    scheduler.add_process(Process(3, "TextEditor", 18, 6))
    scheduler.add_process(Process(4, "Compiler", 12, 10))
    
    scheduler.run()

if __name__ == "__main__":
    main()
