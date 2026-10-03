    .section .text.init, "ax", @progbits
    .globl _start
_start:
    # Sum 1 through 100. The backward branch gives timing models a workload.
    li t0, 0
    li t1, 1
    li t2, 101
1:
    add t0, t0, t1
    addi t1, t1, 1
    bne t1, t2, 1b

    # Check a data store and load before reporting the result.
    la t3, result
    sd t0, 0(t3)
    ld t4, 0(t3)
    li t5, 5050
    bne t4, t5, fail

    li t0, 1                  # HTIF pass: exit code 0
    j finish
fail:
    li t0, 3                  # HTIF fail: exit code 1
finish:
    fence rw, rw
    la t1, tohost
    sd t0, 0(t1)
2:
    j 2b                      # Spike observes tohost and exits.

    .section .bss, "aw", @nobits
    .align 3
result:
    .dword 0

    .section .tohost, "aw", @progbits
    .align 6
    .globl tohost
tohost:
    .dword 0
    .align 6
    .globl fromhost
fromhost:
    .dword 0
