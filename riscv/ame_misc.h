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

template<typename T>
static inline void
ame_zero_column(MatrixReg& dst, reg_t column)
{
  for (reg_t row = 0; row < dst.get_rownum(); ++row)
    memset(&dst.elt<T>(row, column), 0, sizeof(T));
}

using AmeByteColumn = std::vector<uint8_t>;
using AmeByteColumnBatch = std::vector<AmeByteColumn>;

static inline AmeByteColumnBatch
ame_extract_columns(MatrixReg& src, reg_t begin, reg_t end)
{
  assert(begin <= end && end <= src.get_row_bytes());
  AmeByteColumnBatch columns;
  columns.reserve(end - begin);
  for (reg_t column = begin; column < end; ++column)
    columns.push_back(ame_extract_column<uint8_t>(src, column));
  return columns;
}

static inline void
ame_pack_columns(MatrixReg& dst,
                 const AmeByteColumnBatch& firstColumns,
                 const AmeByteColumnBatch& secondColumns)
{
  assert(firstColumns.size() == secondColumns.size());
  assert(firstColumns.size() + secondColumns.size() == dst.get_row_bytes());
  reg_t dstColumn = 0;
  for (const auto& sourceColumn : firstColumns)
    ame_copy_column<uint8_t>(dst, dstColumn++, sourceColumn);
  for (const auto& sourceColumn : secondColumns)
    ame_copy_column<uint8_t>(dst, dstColumn++, sourceColumn);
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

#define AME_MPACK_LOOP(BODY)                                            \
  do {                                                                  \
    require_matrix_ms;                                                  \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t ms1Index = insn.m_ms1();                                \
    const reg_t ms2Index = insn.m_ms2();                                \
    const bool tileType = dstIndex < 4;                                 \
    require(tileType == (ms1Index < 4));                                \
    require(tileType == (ms2Index < 4));                                \
    MatrixReg& dst = tileType                                           \
      ? P.AMU.tile_regs[dstIndex]                                       \
      : P.AMU.acc_regs[dstIndex - 4];                                   \
    MatrixReg& ms1 = tileType                                           \
      ? P.AMU.tile_regs[ms1Index]                                       \
      : P.AMU.acc_regs[ms1Index - 4];                                   \
    MatrixReg& ms2 = tileType                                           \
      ? P.AMU.tile_regs[ms2Index]                                       \
      : P.AMU.acc_regs[ms2Index - 4];                                   \
    require(dst.get_rownum() == ms1.get_rownum());                      \
    require(dst.get_rownum() == ms2.get_rownum());                      \
    require(dst.get_row_bytes() == ms1.get_row_bytes());                \
    require(dst.get_row_bytes() == ms2.get_row_bytes());                \
    const reg_t cols = dst.get_row_bytes();                             \
    require(cols != 0 && cols % 2 == 0);                                \
    const reg_t halfCols = cols / 2;                                    \
    BODY;                                                               \
    AME_END;                                                            \
  } while (0)

#define AME_MRSLIDEDOWN_LOOP(BODY)                                     \
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
    const reg_t shift = insn.m_uimm3() & (rows - 1);                    \
    const reg_t rowBytes = dst.get_row_bytes();                          \
    for (reg_t i = 0; i < rows - shift; ++i) {                          \
      uint8_t* dstRow = dst.row_ptr<uint8_t>(i);                        \
      const uint8_t* srcRow = src.row_ptr<uint8_t>(i + shift);          \
      if (dstRow != srcRow)                                             \
        BODY;                                                           \
    }                                                                   \
    for (reg_t i = rows - shift; i < rows; ++i)                         \
      memset(dst.row_ptr<uint8_t>(i), 0, rowBytes);                     \
    AME_END;                                                            \
  } while (0)

#define AME_MRSLIDEUP_LOOP(BODY)                                       \
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
    const reg_t shift = insn.m_uimm3() & (rows - 1);                    \
    const reg_t rowBytes = dst.get_row_bytes();                          \
    for (reg_t i = rows; i-- > shift; ) {                               \
      uint8_t* dstRow = dst.row_ptr<uint8_t>(i);                        \
      const uint8_t* srcRow = src.row_ptr<uint8_t>(i - shift);          \
      if (dstRow != srcRow)                                             \
        BODY;                                                           \
    }                                                                   \
    for (reg_t i = 0; i < shift; ++i)                                  \
      memset(dst.row_ptr<uint8_t>(i), 0, rowBytes);                     \
    AME_END;                                                            \
  } while (0)

#define AME_MCSLIDEDOWN_LOOP(ELEMENT_TYPE, BODY)                       \
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
    const reg_t shift = insn.m_uimm3() & (cols - 1);                    \
    for (reg_t j = 0; j < cols - shift; ++j) {                          \
      const std::vector<ELEMENT_TYPE> srcColumn =                       \
        ame_extract_column<ELEMENT_TYPE>(src, j + shift);               \
      BODY;                                                             \
    }                                                                   \
    for (reg_t j = cols - shift; j < cols; ++j)                         \
      ame_zero_column<ELEMENT_TYPE>(dst, j);                            \
    AME_END;                                                            \
  } while (0)

#define AME_MCSLIDEUP_LOOP(ELEMENT_TYPE, BODY)                         \
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
    const reg_t shift = insn.m_uimm3() & (cols - 1);                    \
    for (reg_t j = cols; j-- > shift; ) {                               \
      const std::vector<ELEMENT_TYPE> srcColumn =                       \
        ame_extract_column<ELEMENT_TYPE>(src, j - shift);               \
      BODY;                                                             \
    }                                                                   \
    for (reg_t j = 0; j < shift; ++j)                                  \
      ame_zero_column<ELEMENT_TYPE>(dst, j);                            \
    AME_END;                                                            \
  } while (0)

static inline int32_t
ame_int_sat_addsub(int64_t value, ameUnit_t& AMU)
{
  if (AMU.xmsaten_enabled()) {
    if (value > INT32_MAX)
      return INT32_MAX;
    if (value < INT32_MIN)
      return INT32_MIN;
  }
  return static_cast<int32_t>(static_cast<uint32_t>(value));
}

static inline int32_t
ame_int_mul_low(int32_t a, int32_t b)
{
  const int64_t product = static_cast<int64_t>(a) * static_cast<int64_t>(b);
  return static_cast<int32_t>(static_cast<uint32_t>(product));
}

static inline int32_t
ame_int_mul_high(int32_t a, int32_t b)
{
  const int64_t product = static_cast<int64_t>(a) * static_cast<int64_t>(b);
  return static_cast<int32_t>(product >> 32);
}

#define AME_INT_EWISE_SETUP(BODY)                                      \
  do {                                                                  \
    require_matrix_ms;                                                  \
    require(P.AMU.get_xmisa() & P.AMU.xmisa_miew_mask());              \
    require(P.AMU.supports_xmisa_feature(                               \
      ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMI8I32)));             \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t ms1Index = insn.m_ms1();                                \
    const reg_t ms2Index = insn.m_ms2();                                \
    require(dstIndex >= 4 && dstIndex < 8);                             \
    require(ms1Index >= 4 && ms1Index < 8);                             \
    require(ms2Index >= 4 && ms2Index < 8);                             \
    MatrixReg& md = P.AMU.acc_regs[dstIndex - 4];                       \
    MatrixReg& ms1 = P.AMU.acc_regs[ms1Index - 4];                      \
    MatrixReg& ms2 = P.AMU.acc_regs[ms2Index - 4];                      \
    require(md.get_rownum() == ms1.get_rownum());                       \
    require(md.get_rownum() == ms2.get_rownum());                       \
    require(md.get_row_bytes() == ms1.get_row_bytes());                 \
    require(md.get_row_bytes() == ms2.get_row_bytes());                 \
    const reg_t rows = md.get_rownum();                                 \
    const reg_t cols = md.get_row_bytes() / sizeof(int32_t);             \
    require(cols != 0 && (cols & (cols - 1)) == 0);                     \
    for (reg_t i = 0; i < rows; ++i) {                                  \
      for (reg_t j = 0; j < cols; ++j) {                                \
        int32_t& Cij = md.elt<int32_t>(i, j);                            \
        const int32_t Aij = ms2.elt<int32_t>(i, j);                     \
        const int32_t Bij = ms1.elt<int32_t>(i, j);                     \
        BODY;                                                            \
      }                                                                  \
    }                                                                    \
    AME_END;                                                             \
  } while (0)

#define AME_INT_EWISE_MV_SETUP(BODY)                                   \
  do {                                                                  \
    require_matrix_ms;                                                  \
    require(P.AMU.get_xmisa() & P.AMU.xmisa_miew_mask());              \
    require(P.AMU.supports_xmisa_feature(                               \
      ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMI8I32)));             \
    const reg_t dstIndex = insn.m_md();                                 \
    const reg_t ms1Index = insn.m_ms1();                                \
    const reg_t ms2Index = insn.m_ms2();                                \
    require(dstIndex >= 4 && dstIndex < 8);                             \
    require(ms1Index >= 4 && ms1Index < 8);                             \
    require(ms2Index >= 4 && ms2Index < 8);                             \
    MatrixReg& md = P.AMU.acc_regs[dstIndex - 4];                       \
    MatrixReg& ms1 = P.AMU.acc_regs[ms1Index - 4];                      \
    MatrixReg& ms2 = P.AMU.acc_regs[ms2Index - 4];                      \
    require(md.get_rownum() == ms1.get_rownum());                       \
    require(md.get_rownum() == ms2.get_rownum());                       \
    require(md.get_row_bytes() == ms1.get_row_bytes());                 \
    require(md.get_row_bytes() == ms2.get_row_bytes());                 \
    const reg_t rows = md.get_rownum();                                 \
    const reg_t cols = md.get_row_bytes() / sizeof(int32_t);             \
    require(cols != 0 && (cols & (cols - 1)) == 0);                     \
    const reg_t row = insn.m_uimm3();                                   \
    require(row < rows);                                                \
    for (reg_t i = 0; i < rows; ++i) {                                  \
      for (reg_t j = 0; j < cols; ++j) {                                \
        int32_t& Cij = md.elt<int32_t>(i, j);                            \
        const int32_t Aij = ms2.elt<int32_t>(i, j);                     \
        const int32_t Bij = ms1.elt<int32_t>(row, j);                   \
        BODY;                                                            \
      }                                                                  \
    }                                                                    \
    AME_END;                                                             \
  } while (0)

#endif
