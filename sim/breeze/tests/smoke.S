.section .text
.global _start
_start:
  li t0, 100
1:
  addi t0, t0, -1
  bnez t0, 1b
  la t1, data_word
  ld t2, 0(t1)
  addi t2, t2, 1
  sd t2, 0(t1)
  la t3, tohost
  li t4, 1
  sd t4, 0(t3)
2:
  j 2b

.section .data
.align 3
data_word:
  .dword 0

.section .tohost,"aw",@progbits
.align 6
.global tohost
tohost:
  .dword 0
.align 6
.global fromhost
fromhost:
  .dword 0
