# Architecture Notes

This document goes one level deeper than the main README, focused on how the context switch actually works, the part of a kernel that's easiest to get wrong.

## Task Control Block

Each task is a static entry in a `TCB tasks[OS_MAX_TASKS]` array (`Core/OS/Inc/os.h`). It holds the task's entry-point function pointer, its own private stack, a saved stack pointer, its current `TaskState`, a `wake_tick` for sleeping tasks, and a name string used for introspection. Nothing here is heap-allocated; the array and every task's stack exist for the lifetime of the program.

## Bootstrapping a Task's Stack

Before a task ever runs, `os_task_stack_init()` pre-fills its stack with a synthetic exception frame so that the very first `PendSV` return can "resume" it as if it had already been interrupted once:

- The hardware-stacked frame (`xPSR`, `PC`, `LR`, `R12`, `R3–R0`) is built with `PC` pointing at the task's entry function and the Thumb bit set in `xPSR`.
- The software-stacked frame (`R11–R4`) is filled with recognisable placeholder values, useful for spotting stack corruption in a debugger.

This means task creation and the first context switch use the exact same restore path as every subsequent switch; there's no special-cased "first run".

## The Context Switch: `PendSV_Handler`

Context switches happen in `Core/Src/pendsv_handler.s`, triggered by setting `PENDSVSET` in the `SCB->ICSR` register (see `os_yield()`, `os_sleep_ms()`, `os_task_block()`). `PendSV` is deliberately assigned the lowest interrupt priority (`os_init()` sets it to `0xFF`), so it always runs after any other pending interrupt completes. This is the standard ARM pattern for avoiding a context switch mid-ISR.

On entry:

1. **Skip save on the very first switch** checked via the `os_started` flag, since there's no "previous task" to save yet.
2. **Save the outgoing task's context** read `PSP`, push `R4–R11` onto it, and store the resulting stack pointer into `tasks[current_task].stack_ptr`. (`R0–R3`, `R12`, `LR`, `PC`, `xPSR` are already saved automatically by the CPU on exception entry.)
3. **Call `os_schedule()`** in C, which picks the next `READY` task and updates the `current_task` index.
4. **Restore the incoming task's context** load its saved stack pointer, pop `R4–R11`, and write the result back into `PSP`.
5. **Return via `EXC_RETURN`** with the "use PSP" bit set, handing control back to the newly selected task.

The task-index-to-TCB-offset math (`MOV R4, #532; MUL R2, R2, R4`) is the size of a `TCB` struct in bytes, a reminder that this handler is tightly coupled to the layout of `TCB` in `os.h`, and any change there needs a matching change here.

## Scheduling Policy

`os_schedule()` itself is intentionally simple: starting one slot after the current task, it scans the TCB array for the next task in `READY` state and switches to it. There's no priority, no time-slicing beyond what a task requests via `os_sleep_ms()`, and no forced preemption. A task keeps the CPU until it calls `os_yield()`, `os_sleep_ms()`, or blocks on `os_event_wait()`.

`os_tick()`, called from `SysTick_Handler` every 1 ms, is the only place `SLEEPING → READY` transitions happen. It walks the task table and wakes anything whose `wake_tick` has arrived.

## Interrupt Safety

`os_queue` and `os_event` both wrap their state mutations in `__disable_irq()` / `__enable_irq()` critical sections, since both are designed to be called from ISR context (e.g. a UART RX callback pushing into a queue) as well as task context. These critical sections are kept short and non-blocking by design, neither primitive spins nor sleeps while interrupts are masked.

## Idle Behaviour

When every task is `SLEEPING` or `BLOCKED`, the scheduler has nothing to run. Rather than busy-loop, the kernel executes `__WFI` (Wait For Interrupt), putting the core into a low-power sleep state until the next `SysTick` (or any other) interrupt occurs.
