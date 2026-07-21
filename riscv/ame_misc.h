// See LICENSE for license details.

#ifndef _RISCV_AME_MISC_H
#define _RISCV_AME_MISC_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

template<typename T>
static inline std::vector<T>
ame_extract_column(MatrixReg& src, reg_t column)
{
  const reg_t rows = src.get_rownum();
  std::vector<T> result(rows);
  for (reg_t row = 0; row < rows; ++row)
    memcpy(&result[row], &src.elt<T>(row, column), sizeof(T));
  return result;
}

template<typename T>
static inline void
ame_copy_column(MatrixReg& dst, reg_t column, const std::vector<T>& source)
{
  assert(source.size() == dst.get_rownum());
  for (reg_t row = 0; row < dst.get_rownum(); ++row)
    memcpy(&dst.elt<T>(row, column), &source[row], sizeof(T));
}

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

#define AME_MBCE_LOOP(ELEMENT_TYPE, BODY)                               \
  do {                                                                  \
    require_matrix_ms;                                                  \
    require(insn.m_uimm3() == 0);                                      \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t srcIndex = insn.m_ms1();                                \
    require((dstIndex < 4) == (srcIndex < 4));                          \
    MatrixReg& dst = dstIndex < 4                                       \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    MatrixReg& src = srcIndex < 4                                       \
      ? P.AMU.tile_regs[srcIndex]                                       \
      : P.AMU.acc_regs[srcIndex - 4];                                   \
    require(dst.get_rownum() == src.get_rownum());                      \
    require(dst.get_row_bytes() == src.get_row_bytes());                \
    require(dst.get_row_bytes() % sizeof(ELEMENT_TYPE) == 0);           \
    const ELEMENT_TYPE val = src.elt<ELEMENT_TYPE>(0, 0);               \
    const reg_t rows = dst.get_rownum();                                \
    const reg_t cols = dst.get_row_bytes() / sizeof(ELEMENT_TYPE);      \
    for (reg_t i = 0; i < rows; ++i) {                                  \
      for (reg_t j = 0; j < cols; ++j) {                                \
        ELEMENT_TYPE& Cij = dst.elt<ELEMENT_TYPE>(i, j);                \
        BODY;                                                           \
      }                                                                 \
    }                                                                   \
    AME_END;                                                            \
  } while (0)

#define AME_MRBC_LOOP(BODY)                                             \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t srcIndex = insn.m_ms1();                                \
    require((dstIndex < 4) == (srcIndex < 4));                          \
    MatrixReg& dst = dstIndex < 4                                       \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    MatrixReg& src = srcIndex < 4                                       \
      ? P.AMU.tile_regs[srcIndex]                                       \
      : P.AMU.acc_regs[srcIndex - 4];                                   \
    require(dst.get_rownum() == src.get_rownum());                      \
    require(dst.get_row_bytes() == src.get_row_bytes());                \
    const reg_t rows = dst.get_rownum();                                \
    require(rows != 0 && (rows & (rows - 1)) == 0);                     \
    const reg_t srcRowIndex = insn.m_uimm3() & (rows - 1);              \
    const reg_t rowBytes = dst.get_row_bytes();                          \
    const uint8_t* srcRow = src.row_ptr<uint8_t>(srcRowIndex);          \
    for (reg_t i = 0; i < rows; ++i) {                                  \
      uint8_t* dstRow = dst.row_ptr<uint8_t>(i);                        \
      if (dstRow != srcRow)                                             \
        BODY;                                                           \
    }                                                                   \
    AME_END;                                                            \
  } while (0)

#define AME_MCBCE_LOOP(ELEMENT_TYPE, BODY)                              \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t srcIndex = insn.m_ms1();                                \
    require((dstIndex < 4) == (srcIndex < 4));                          \
    MatrixReg& dst = dstIndex < 4                                       \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    MatrixReg& src = srcIndex < 4                                       \
      ? P.AMU.tile_regs[srcIndex]                                       \
      : P.AMU.acc_regs[srcIndex - 4];                                   \
    require(dst.get_rownum() == src.get_rownum());                      \
    require(dst.get_row_bytes() == src.get_row_bytes());                \
    require(dst.get_row_bytes() % sizeof(ELEMENT_TYPE) == 0);           \
    const reg_t cols = dst.get_row_bytes() / sizeof(ELEMENT_TYPE);      \
    require(cols != 0 && (cols & (cols - 1)) == 0);                     \
    const reg_t srcColIndex = insn.m_uimm3() & (cols - 1);              \
    const std::vector<ELEMENT_TYPE> srcColumn =                         \
      ame_extract_column<ELEMENT_TYPE>(src, srcColIndex);               \
    for (reg_t j = 0; j < cols; ++j) {                                  \
      BODY;                                                             \
    }                                                                   \
    AME_END;                                                            \
  } while (0)

#endif
