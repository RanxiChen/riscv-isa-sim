// See LICENSE for license details.

#ifndef _RISCV_AME_MISC_H
#define _RISCV_AME_MISC_H

#include <algorithm>
#include <cstdint>

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

#define AME_MMOV_MM_LOOP(BODY)                                          \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t srcIndex = insn.m_ms1();                                \
    MatrixReg& dst = dstIndex < 4                                       \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    MatrixReg& src = srcIndex < 4                                       \
      ? P.AMU.tile_regs[srcIndex]                                       \
      : P.AMU.acc_regs[srcIndex - 4];                                   \
    require(dst.get_rownum() == src.get_rownum());                      \
    const reg_t rownum = dst.get_rownum();                              \
    const reg_t copyBytes = std::min(dst.get_row_bytes(),               \
                                     src.get_row_bytes());              \
    if (dstIndex != srcIndex) {                                         \
      for (reg_t row = 0; row < rownum; ++row) {                        \
        uint8_t* dstRow = dst.row_ptr<uint8_t>(row);                    \
        const uint8_t* srcRow = src.row_ptr<uint8_t>(row);              \
        BODY;                                                           \
      }                                                                 \
    }                                                                   \
    AME_END;                                                            \
  } while (0)

#define AME_MMOV_X_M(ELEMENT_TYPE, BODY)                                \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t srcIndex = insn.m_ms2();                                \
    MatrixReg& src = srcIndex < 4                                       \
      ? P.AMU.tile_regs[srcIndex]                                       \
      : P.AMU.acc_regs[srcIndex - 4];                                   \
    const reg_t elementsPerRow =                                        \
      src.get_row_bytes() / sizeof(ELEMENT_TYPE);                       \
    const reg_t totalElements =                                         \
      src.get_total_bytes() / sizeof(ELEMENT_TYPE);                     \
    const reg_t elementIndex = RS1 & (totalElements - 1);               \
    BODY;                                                               \
    AME_END;                                                            \
  } while (0)

#define AME_MMOV_M_X(ELEMENT_TYPE, BODY)                                \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t dstIndex = insn.m_md();                                 \
    MatrixReg& dst = dstIndex < 4                                       \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    const reg_t elementsPerRow =                                        \
      dst.get_row_bytes() / sizeof(ELEMENT_TYPE);                       \
    const reg_t totalElements =                                         \
      dst.get_total_bytes() / sizeof(ELEMENT_TYPE);                     \
    const reg_t elementIndex = RS1 & (totalElements - 1);               \
    const reg_t scalarValue = sext_xlen(RS2);                           \
    BODY;                                                               \
    AME_END;                                                            \
  } while (0)

#endif
