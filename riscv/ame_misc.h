// See LICENSE for license details.

#ifndef _RISCV_AME_MISC_H
#define _RISCV_AME_MISC_H

#define AME_MZERO_LOOP(NUM_REGS, BODY)                                  \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t start = insn.m_md();                                    \
    require_align(start, NUM_REGS);                                     \
    require(start + NUM_REGS <= 8);                                     \
    for (reg_t offset = 0; offset < NUM_REGS; ++offset) {               \
      const reg_t index = start + offset;                               \
      BODY;                                                             \
    }                                                                   \
    AME_END;                                                            \
  } while (0)

#endif
