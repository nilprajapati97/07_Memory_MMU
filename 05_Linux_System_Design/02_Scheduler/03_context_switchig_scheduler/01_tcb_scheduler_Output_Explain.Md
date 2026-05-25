═══════════════════════════════════════════════════════════════
  TCB-Based Cooperative Scheduler — System Design Implementation
  Task : Print 1 … 20 alternating ODD / EVEN threads
═══════════════════════════════════════════════════════════════
═══════════════════════════════════════════════════════════════
  [SCHEDULER INIT]
  Algorithm   : Cooperative Round-Robin
  Max Threads : 8
  Quantum     : 1000 µs
  Queue       : Circular FIFO array  (head → dequeue, tail → enqueue)
  Context     : sim_PC / sim_SP / sim_LR / sim_R0 / sim_R1
═══════════════════════════════════════════════════════════════

  [MAIN]  Registering threads...

  [REGISTER]  TID=0  ODD_THREAD    Priority=5  Quantum=1000 µs
              TCB allocated at 0x5adffbc892b0  (size=144 bytes)
  [THREAD BORN]  TID=0  ODD_THREAD    pthread=0x7975a63ff6c0  State: NEW → READY
              State: NEW → READY  |  pthread=0x7975a63ff6c0
  [READY QUEUE] size=1 [ TID0:ODD_THREAD ]

  [REGISTER]  TID=1  EVEN_THREAD   Priority=5  Quantum=1000 µs
              TCB allocated at 0x5adffbc89490  (size=144 bytes)
  [THREAD BORN]  TID=1  EVEN_THREAD   pthread=0x7975a5bfe6c0  State: NEW → READY
              State: NEW → READY  |  pthread=0x7975a5bfe6c0
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]

  [MAIN]  Both threads registered — handing control to scheduler

═══════════════════════════════════════════════════════════════
  [SCHEDULER RUN]  Dispatch loop started
═══════════════════════════════════════════════════════════════
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #1  ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING            ║
  ║  sim_PC     = 0x0000000000000000   (resume here)              ║
  ║  sim_SP     = 0x0000000000000000   (stack restored)           ║
  ║  sim_LR     = 0x0000000000000000   (return address)           ║
  ║  sim_R0     = 0          (counter at last save)               ║
  ║  sim_R1     = 0          (odd_turn at last save)              ║
  ║  Priority   = 5       Quantum = 1000   µs                     ║
  ║  Loads=1    Saves=0    CPU total = 0      µs                  ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:   1

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #1)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY              ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)           ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)                ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)           ║
  ║  sim_R0     = 2          (counter at save)                    ║
  ║  sim_R1     = 0          (odd_turn at save)                   ║
  ║  CPU slice  = 12     µs  Saves=1    Loads=1    Yields=1       ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #2  ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING            ║
  ║  sim_PC     = 0x0000000000000000   (resume here)              ║
  ║  sim_SP     = 0x0000000000000000   (stack restored)           ║
  ║  sim_LR     = 0x0000000000000000   (return address)           ║
  ║  sim_R0     = 0          (counter at last save)               ║ 
  ║  sim_R1     = 0          (odd_turn at last save)              ║
  ║  Priority   = 5       Quantum = 1000   µs                     ║
  ║  Loads=1    Saves=0    CPU total = 0      µs                  ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:   2

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #1)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY              ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)           ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)                ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)           ║
  ║  sim_R0     = 3          (counter at save)                    ║
  ║  sim_R1     = 1          (odd_turn at save)                   ║
  ║  CPU slice  = 58     µs  Saves=1    Loads=1    Yields=1       ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #3  ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING            ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)              ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)           ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)           ║
  ║  sim_R0     = 2          (counter at last save)               ║
  ║  sim_R1     = 0          (odd_turn at last save)              ║
  ║  Priority   = 5       Quantum = 1000   µs                     ║
  ║  Loads=2    Saves=1    CPU total = 12     µs                  ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:   3

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #2)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY              ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)           ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)                ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)           ║
  ║  sim_R0     = 4          (counter at save)                    ║
  ║  sim_R1     = 0          (odd_turn at save)                   ║
  ║  CPU slice  = 14     µs  Saves=2    Loads=2    Yields=2       ║
  ╚═══════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #4  ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 3          (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=2    Saves=1    CPU total = 58     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:   4

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #2)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 5          (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 87     µs  Saves=2    Loads=2    Yields=2   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #5  ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 4          (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=3    Saves=2    CPU total = 26     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:   5

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #3)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 6          (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 10     µs  Saves=3    Loads=3    Yields=3   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #6  ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 5          (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=3    Saves=2    CPU total = 145    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:   6

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #3)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 7          (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 10     µs  Saves=3    Loads=3    Yields=3   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #7  ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 6          (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=4    Saves=3    CPU total = 36     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:   7

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #4)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 8          (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 10     µs  Saves=4    Loads=4    Yields=4   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #8  ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 7          (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=4    Saves=3    CPU total = 155    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:   8

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #4)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 9          (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 18     µs  Saves=4    Loads=4    Yields=4   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #9  ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 8          (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=5    Saves=4    CPU total = 46     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:   9

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #5)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 10         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 9      µs  Saves=5    Loads=5    Yields=5   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #10 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 9          (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=5    Saves=4    CPU total = 173    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  10

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #5)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 11         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 8      µs  Saves=5    Loads=5    Yields=5   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #11 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 10         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=6    Saves=5    CPU total = 55     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:  11

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #6)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 12         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 7      µs  Saves=6    Loads=6    Yields=6   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #12 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 11         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=6    Saves=5    CPU total = 181    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  12

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #6)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 13         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 23     µs  Saves=6    Loads=6    Yields=6   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #13 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 12         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=7    Saves=6    CPU total = 62     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:  13

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #7)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 14         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 6      µs  Saves=7    Loads=7    Yields=7   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #14 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 13         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=7    Saves=6    CPU total = 204    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  14

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #7)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 15         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 8      µs  Saves=7    Loads=7    Yields=7   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #15 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 14         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=8    Saves=7    CPU total = 68     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:  15

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #8)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 16         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 16     µs  Saves=8    Loads=8    Yields=8   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #16 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 15         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=8    Saves=7    CPU total = 212    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  16

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #8)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 17         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 14     µs  Saves=8    Loads=8    Yields=8   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #17 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 16         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=9    Saves=8    CPU total = 84     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:  17

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #9)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 18         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 5      µs  Saves=9    Loads=9    Yields=9   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #18 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 17         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=9    Saves=8    CPU total = 226    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  18

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #9)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 19         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 6      µs  Saves=9    Loads=9    Yields=9   ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #19 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 18         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=10   Saves=9    CPU total = 89     µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [ODD_THREAD ] prints:  19

  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #10)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 20         (counter at save)                ║
  ║  sim_R1     = 0          (odd_turn at save)               ║
  ║  CPU slice  = 15     µs  Saves=10   Loads=10   Yields=10  ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #20 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 19         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=10   Saves=9    CPU total = 232    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────

  >>>  [EVEN_THREAD] prints:  20

  [YIELD]  EVEN_THREAD   giving up CPU voluntarily  (yield #10)
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► READY          ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 21         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 12     µs  Saves=10   Loads=10   Yields=10  ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [READY QUEUE] size=2 [ TID0:ODD_THREAD TID1:EVEN_THREAD ]
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #21 ]  Dispatching TID=0  ODD_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 20         (counter at last save)           ║
  ║  sim_R1     = 0          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=11   Saves=10   CPU total = 104    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : ODD_THREAD   ══════════════════════════════════╗
  ║  TID        = 0      State : RUNNING   ──► TERMINATED      ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a63fedb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 21         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 7      µs  Saves=11   Loads=11   Yields=10  ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [TERMINATE] ODD_THREAD    Final stats:
    Saves=11   Loads=11   Yields=10   Blocks=0    CPU=111 µs
═══════════════════════════════════════════════════════════════
  [SCHEDULER CYCLE #22 ]  Dispatching TID=1  EVEN_THREAD
═══════════════════════════════════════════════════════════════
  ─────────────────────────────────────────────────────────────
  ╔══ TCB LOAD   : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : READY     ──► RUNNING         ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume here)       ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack restored)    ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 21         (counter at last save)           ║
  ║  sim_R1     = 1          (odd_turn at last save)          ║
  ║  Priority   = 5       Quantum = 1000   µs                ║
  ║  Loads=11   Saves=10   CPU total = 244    µs            ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  ─────────────────────────────────────────────────────────────
  ╔══ TCB UNLOAD : EVEN_THREAD  ══════════════════════════════════╗
  ║  TID        = 1      State : RUNNING   ──► TERMINATED      ║
  ║  sim_PC     = 0x00005adfd031b6a2   (resume address)    ║
  ║  sim_SP     = 0x00007975a5bfddb0   (stack top)         ║
  ║  sim_LR     = 0x00005adfd031b69a   (return address)    ║
  ║  sim_R0     = 21         (counter at save)                ║
  ║  sim_R1     = 1          (odd_turn at save)               ║
  ║  CPU slice  = 9      µs  Saves=11   Loads=11   Yields=10  ║
  ╚════════════════════════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────
  [TERMINATE] EVEN_THREAD   Final stats:
    Saves=11   Loads=11   Yields=10   Blocks=0    CPU=253 µs
═══════════════════════════════════════════════════════════════
  [FINAL STATS]
═══════════════════════════════════════════════════════════════
  Scheduler cycles       : 22
  Total context switches : 22
  Wall-clock time        : 5044 µs

  Thread ODD_THREAD    TID=0
    Final state      : TERMINATED
    Context saves    : 11  (TCB UNLOAD count)
    Context loads    : 11  (TCB LOAD count)
    Yields           : 10
    Blocks           : 0
    CPU time         : 111 µs

  Thread EVEN_THREAD   TID=1
    Final state      : TERMINATED
    Context saves    : 11  (TCB UNLOAD count)
    Context loads    : 11  (TCB LOAD count)
    Yields           : 10
    Blocks           : 0
    CPU time         : 253 µs

═══════════════════════════════════════════════════════════════
