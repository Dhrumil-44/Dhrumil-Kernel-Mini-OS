.syntax unified
    .cpu cortex-m4
    .thumb

    .extern tasks
    .extern current_task

    .global os_start
    .type os_start, %function

os_start:
    LDR     R0, =tasks
    @ load stack_ptr of task[0]
    LDR     R0, [R0]
    @ PSP points to saved R4
    MSR     PSP, R0
    ISB

    MRS     R0, CONTROL
    ORR     R0, R0, #2
    MSR     CONTROL, R0
    ISB

    LDR     R0, =0xE000ED04
    MOV     R1, #0x10000000
    STR     R1, [R0]
    ISB

    CPSIE   I
os_start_loop:
    WFI
    B       os_start_loop
