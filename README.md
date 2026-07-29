#Kernel Mini OS for STM32F4

A small, cooperative, statically-allocated real-time kernel written from scratch for the STM32 Nucleo-F446RE (Cortex-M4). It implements task scheduling, context switching via `PendSV`, a lock-free-style message queue, and a binary event flag - all without any dynamic memory allocation.

Built as a firmware assignment for Bytebeam, and used as a hands-on exercise in kernel internals: TCB design, stack frame construction for `PendSV` context switches, ISR-safe primitives, and cooperative scheduling trade-offs.

```
[  1000 ms] heartbeat
[  2000 ms] queue: sending hello
[  2001 ms] event: task unblocked!
[  2014 ms] queue: received hello
[  3000 ms] heartbeat

>>> tasks
[287915 ms] --- task list ---
[287915 ms]   [0] LED          SLEEPING
[287915 ms]   [1] UART         SLEEPING
[287915 ms]   [2] SENDER       SLEEPING
[287915 ms]   [3] RECEIVER     SLEEPING
[287915 ms]   [4] WORKER       BLOCKED
[287915 ms]   [5] CMD          RUNNING
[287915 ms] -----------------
```

## Contents

- [Hardware & Toolchain](#hardware--toolchain)
- [Building & Flashing](#building--flashing)
- [Architecture](#architecture)
- [Scheduler Model](#scheduler-model)
- [Kernel Primitives](#kernel-primitives)
- [Public API](#public-api)
- [Demo Application](#demo-application)
- [Repository Layout](#repository-layout)
- [Known Limitations](#known-limitations)
- [AI Usage & Validation](#ai-usage--validation)
- [License](#license)

## Hardware & Toolchain

| | |
|---|---|
| Board | STM32 Nucleo-F446RE |
| MCU | STM32F446RET6 (Cortex-M4, 180 MHz) |
| IDE | STM32CubeIDE 2.1.0 |
| Toolchain | arm-none-eabi-gcc 14.3.1 |
| HAL | STM32F4xx HAL Driver (CubeMX-generated) |
| Debug probe | ST-Link V2 (onboard) |

## Building & Flashing

1. Open **STM32CubeIDE**.
2. `File → Import → Existing Projects into Workspace` and select this repo's root folder.
3. `Project → Clean`, then `Ctrl+B` to build.
4. `F11` to flash and start a debug session.

To observe serial output on a host machine:

```bash
python3 monitor.py
```

| Setting | Value |
|---|---|
| Baud rate | 115200, 8N1, no flow control |
| Port | `/dev/ttyACM0` (Linux) or `COMx` (Windows) |

## Architecture

The OS is structured in three layers:

1. **Hardware layer** - the `SysTick` timer fires every 1 ms and calls `os_tick()`, which increments the OS tick counter and wakes any sleeping tasks whose wake time has arrived.
2. **Kernel layer** - the scheduler and two primitives (message queue and event flag) live in `Core/OS/`. All kernel state is statically allocated; no dynamic memory is used anywhere in the OS.
3. **Application layer** - six demo tasks in `main.c` exercise the scheduler and both primitives.

## Scheduler Model

This is a **cooperative round-robin scheduler**. Tasks are stored in a static Task Control Block (TCB) array with a maximum of 8 slots. The scheduler loop finds the next `READY` task and context-switches into it via `PendSV`. Tasks voluntarily give up the CPU by calling `os_sleep_ms()` or `os_yield()` - no task is ever forcibly preempted.

When all tasks are sleeping or blocked, the CPU executes `__WFI` (Wait For Interrupt) to save power until the next `SysTick` fires.

**Task states**

| State | Meaning |
|---|---|
| `READY` | Runnable, waiting for its turn |
| `RUNNING` | Currently executing |
| `SLEEPING` | Called `os_sleep_ms()`, has a `wake_tick` |
| `BLOCKED` | Called `os_event_wait()`, waiting for a signal |

## Kernel Primitives

### Message Queue - `os_queue.h` / `os_queue.c`

A fixed-size ring buffer holding up to 8 messages of 16 bytes each. Safe to call from both task and ISR context using `__disable_irq()` / `__enable_irq()` critical sections.

| Function | Description |
|---|---|
| `os_queue_init(q)` | Initialize queue to empty |
| `os_queue_send(q, item, size)` | Write an item; returns `false` if full |
| `os_queue_recv(q, item, size)` | Read an item; returns `false` if empty |
| `os_queue_is_empty(q)` | Check if empty |
| `os_queue_is_full(q)` | Check if full |

Both send and receive are non-blocking, and IRQs are disabled during the critical section on each.

### Event Flag - `os_event.h` / `os_event.c`

A binary event primitive that lets a task block until signalled by another task or an ISR.

| Function | Description |
|---|---|
| `os_event_init(e)` | Initialize event to unsignaled |
| `os_event_signal(e)` | Signal the event; wakes the waiter |
| `os_event_wait(e)` | Block until signaled |

If a signal arrives before a wait, the flag is latched and consumed on the next `os_event_wait()` call. Only one task may wait on a given event at a time.

## Public API

**Scheduler**

```
os_init()                    initialise kernel state
os_task_create(func, name)   registers a task by function pointer
os_start()                   start scheduler, never returns
os_yield()                   gives up the CPU voluntarily
os_sleep_ms(ms)               blocks the current task for N milliseconds
os_now_ms()                   returns the monotonic millisecond counter
os_tick()                     called from SysTick_Handler every 1ms
```

**Introspection** (used by the UART command task)

```
os_task_count()               return number of registered tasks
os_task_get_state(index)      return state of task at index
os_task_get_name(index)       return name string of task at index
```

**Internal** (used by primitives)

```
os_task_block()                mark current task as BLOCKED and yield
os_task_unblock(index)         mark task at index as READY
os_current_task()              return index of currently running task
```

## Demo Application

Six tasks run concurrently to demonstrate the OS:

| Task | Behavior |
|---|---|
| `LED` | Toggles PA5 (green LED) every 500 ms |
| `UART` | Prints a timestamped heartbeat every 1 s |
| `SENDER` | Sends a queue message and signals an event every 2 s |
| `RECEIVER` | Polls the queue every 20 ms, prints on receive |
| `WORKER` | Blocks on the event flag, prints when unblocked |
| `CMD` | Accepts UART commands, prints OS state |

**UART commands** (type over serial at 115200 baud):

| Command | Description |
|---|---|
| `help` | List all available commands |
| `status` | Show OS tick time and task count |
| `tasks` | Show all task names and current states |
| `latest` | Describe last queue and event activity |
| _(other)_ | Prints an unknown-command error |

Commands respond within 50 ms of being typed; the green LED blinks continuously, and heartbeat/queue/event activity print on their own cadences as shown in the sample output above.

## Repository Layout

```
Core/
├── Inc/                  HAL and board headers
├── Src/                  main.c, HAL init, SysTick handler
├── OS/
│   ├── Inc/              os.h, os_queue.h, os_event.h
│   └── Src/              os.c, os_queue.c, os_event.c
└── Startup/               startup_stm32f446retx.s
Drivers/                   STM32F4xx HAL (CubeMX-generated)
STM32F446RETX_FLASH.ld     Linker script (flash target)
STM32F446RETX_RAM.ld       Linker script (RAM target)
Dhrumil_Kernel_Mini_OS.ioc CubeMX configuration file
docs/ARCHITECTURE.md       Deeper notes on scheduler & context-switch internals
```

## Known Limitations

- **Cooperative only.** A task that never calls `os_sleep_ms()` or `os_yield()` will starve all other tasks.
- **No stack isolation.** All tasks share the MSP stack; a stack overflow in one task corrupts the entire system.
- **No priority.** Round-robin only - all tasks are treated equally.
- **Single event waiter.** Each `OSEvent` supports only one waiting task at a time.
- **Non-blocking queue.** `os_queue_send()` silently drops the message if the queue is full.
- **No dynamic allocation.** All task slots and queue buffers are statically allocated at compile time.

## AI Usage & Validation

This project was built with AI assistance (Claude, Anthropic), used for:

- Designing the TCB structure and scheduler loop architecture
- Writing initial implementations of `os.c`, `os_queue.c`, `os_event.c`
- Debugging scheduler issues, including recursive-yield stack overflow, task starvation, and an event flag that wasn't clearing correctly
- Writing the UART command handler and RX interrupt callback
- Explaining CMSIS intrinsics such as `__disable_irq` and `__WFI`

**Validation performed:**

| Check | Method |
|---|---|
| Scheduler timing | Verified heartbeat prints every 1000 ms using Python timestamps over serial |
| SysTick accuracy | Compared `os_now_ms()` against wall clock over 300s of continuous operation |
| Queue correctness | Observed send/receive pairing in serial logs every 2s with no data loss |
| Event flag | Verified `WORKER` shows `BLOCKED` in the `tasks` command and unblocks correctly on signal |
| Interrupt safety | Confirmed no data corruption during concurrent queue and event use |
| UART commands | Tested all 5 commands and verified correct and error responses |
| HAL compatibility | Checked the USART2 IRQ handler in `stm32f4xx_it.c` matches the CubeMX config |
| Build warnings | Resolved all compiler warnings before final submission |

## License

MIT - see [LICENSE](LICENSE).
