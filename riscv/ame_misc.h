// See LICENSE for license details.

#ifndef _RISCV_AME_MISC_H
#define _RISCV_AME_MISC_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

template<typename T,
         T (*Add)(T, T), T (*Sub)(T, T), T (*Mul)(T, T),
         T (*Max)(T, T), T (*Min)(T, T)>
class AmeFpOperand
{
public:
  explicit AmeFpOperand(T value) : value(value) {}

  T raw() const { return value; }

  friend T operator+(AmeFpOperand lhs, AmeFpOperand rhs)
  {
    return Add(lhs.value, rhs.value);
  }

  friend T operator-(AmeFpOperand lhs, AmeFpOperand rhs)
  {
    return Sub(lhs.value, rhs.value);
  }

  friend T operator*(AmeFpOperand lhs, AmeFpOperand rhs)
  {
    return Mul(lhs.value, rhs.value);
  }

  friend T max(AmeFpOperand lhs, AmeFpOperand rhs)
  {
    return Max(lhs.value, rhs.value);
  }

  friend T min(AmeFpOperand lhs, AmeFpOperand rhs)
  {
    return Min(lhs.value, rhs.value);
  }

private:
  T value;
};

using AmeFp16Operand = AmeFpOperand<
  float16_t, f16_add, f16_sub, f16_mul, f16_max, f16_min>;
using AmeFp32Operand = AmeFpOperand<
  float32_t, f32_add, f32_sub, f32_mul, f32_max, f32_min>;
using AmeFp64Operand = AmeFpOperand<
  float64_t, f64_add, f64_sub, f64_mul, f64_max, f64_min>;

#define AME_FP_EWISE_TYPE_H float16_t
#define AME_FP_EWISE_TYPE_S float32_t
#define AME_FP_EWISE_TYPE_D float64_t

#define AME_FP_EWISE_OPERAND_H AmeFp16Operand
#define AME_FP_EWISE_OPERAND_S AmeFp32Operand
#define AME_FP_EWISE_OPERAND_D AmeFp64Operand

#define AME_FP_EWISE_SUPPORTED_H(AMU) ((AMU).supports_fp16_element_wise())
#define AME_FP_EWISE_SUPPORTED_S(AMU) ((AMU).supports_fp32_element_wise())
#define AME_FP_EWISE_SUPPORTED_D(AMU) ((AMU).supports_fp64_element_wise())

#define AME_FP_EWISE_IMPL(TYPE, USES_ROUNDING, BODY)                     \
  do {                                                                    \
    require_matrix_ms;                                                    \
    auto& AMU = P.AMU;                                                    \
    require(AME_FP_EWISE_SUPPORTED_##TYPE(AMU));                          \
    if (USES_ROUNDING)                                                    \
      require(AMU.xmfrm_is_valid(AMU.get_xmfrm()));                       \
    const reg_t dstIndex = insn.m_md();                                   \
    const reg_t ms1Index = insn.m_ms1();                                  \
    const reg_t ms2Index = insn.m_ms2();                                  \
    require(dstIndex >= 4 && dstIndex < 8);                               \
    require(ms1Index >= 4 && ms1Index < 8);                               \
    require(ms2Index >= 4 && ms2Index < 8);                               \
    MatrixReg& md = AMU.acc_regs[dstIndex - 4];                           \
    MatrixReg& ms1 = AMU.acc_regs[ms1Index - 4];                          \
    MatrixReg& ms2 = AMU.acc_regs[ms2Index - 4];                          \
    require(md.get_rownum() == ms1.get_rownum());                         \
    require(md.get_rownum() == ms2.get_rownum());                         \
    require(md.get_row_bytes() == ms1.get_row_bytes());                   \
    require(md.get_row_bytes() == ms2.get_row_bytes());                   \
    using AmeFpType = AME_FP_EWISE_TYPE_##TYPE;                           \
    using AmeFpValue = AME_FP_EWISE_OPERAND_##TYPE;                       \
    const reg_t rows = md.get_rownum();                                   \
    const reg_t cols = md.get_row_bytes() / sizeof(AmeFpType);            \
    const reg_t M = AMU.mtilem->read();                                   \
    const reg_t N = AMU.mtilen->read();                                   \
    require(M <= rows);                                                   \
    require(N <= cols);                                                   \
    const reg_t ctrl = insn.m_uimm3();                                   \
    const bool matrixMatrix = ctrl == 0b111;                              \
    std::vector<AmeFpType> sourceRow;                                    \
    if (!matrixMatrix) {                                                  \
      require(ctrl < M);                                                  \
      sourceRow.reserve(N);                                               \
      for (reg_t j = 0; j < N; ++j)                                     \
        sourceRow.push_back(ms1.elt<AmeFpType>(ctrl, j));                 \
    }                                                                     \
    if (USES_ROUNDING)                                                    \
      softfloat_roundingMode = AMU.get_xmfrm();                           \
    softfloat_exceptionFlags = 0;                                         \
    for (reg_t i = 0; i < M; ++i) {                                      \
      for (reg_t j = 0; j < N; ++j) {                                    \
        AmeFpType& Cij = md.elt<AmeFpType>(i, j);                         \
        const AmeFpValue Aij(ms2.elt<AmeFpType>(i, j));                   \
        const AmeFpValue Bij(matrixMatrix                                \
          ? ms1.elt<AmeFpType>(i, j)                                     \
          : sourceRow[j]);                                                \
        BODY;                                                              \
      }                                                                   \
    }                                                                     \
    const AmeFpType zero = {0};                                           \
    for (reg_t i = 0; i < rows; ++i)                                     \
      for (reg_t j = 0; j < cols; ++j)                                   \
        if (i >= M || j >= N)                                             \
          md.elt<AmeFpType>(i, j) = zero;                                \
    ame_set_matrix_fp_exceptions(AMU);                                    \
    AME_END;                                                              \
  } while (0)

#define AME_FP_EWISE(TYPE, BODY)                                         \
  AME_FP_EWISE_IMPL(TYPE, true, BODY)

#define AME_FP_EWISE_COMPARE(TYPE, BODY)                                 \
  AME_FP_EWISE_IMPL(TYPE, false, BODY)

#define AME_MFEW_FEATURE(BIT)                                            \
  (AMU.xmisa_mfew_mask() | ameUnit_t::xmisa_bit(ameUnit_t::BIT))
#define AME_MFIC_FEATURE(BIT)                                            \
  (AMU.xmisa_mfic_mask() | ameUnit_t::xmisa_bit(ameUnit_t::BIT))
#define AME_MIEW_FEATURE(BIT)                                            \
  (AMU.xmisa_miew_mask() | ameUnit_t::xmisa_bit(ameUnit_t::BIT))

template<typename T>
class AmeConvertSnapshot
{
public:
  class Row
  {
  public:
    explicit Row(const T* data) : data(data) {}
    const T& operator[](reg_t col) const { return data[col]; }

  private:
    const T* data;
  };

  explicit AmeConvertSnapshot(MatrixReg& src)
    : rows(src.get_rownum()), cols(src.get_row_bytes() / sizeof(T)),
      values(rows * cols)
  {
    assert(src.get_row_bytes() % sizeof(T) == 0);
    for (reg_t i = 0; i < rows; ++i)
      memcpy(values.data() + i * cols, src.row_ptr<T>(i),
             cols * sizeof(T));
  }

  Row operator[](reg_t row) const { return Row(values.data() + row * cols); }
  reg_t row_count() const { return rows; }
  reg_t col_count() const { return cols; }

private:
  reg_t rows;
  reg_t cols;
  std::vector<T> values;
};

template<typename T>
class AmeConvertDest
{
public:
  class Row
  {
  public:
    explicit Row(T* data) : data(data) {}
    T& operator[](reg_t col) const { return data[col]; }

  private:
    T* data;
  };

  explicit AmeConvertDest(MatrixReg& dst) : dst(dst)
  {
    assert(dst.get_row_bytes() % sizeof(T) == 0);
  }

  Row operator[](reg_t row) { return Row(dst.row_ptr<T>(row)); }

private:
  MatrixReg& dst;
};

#define AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, USES_ROUNDING,  \
                         TRACK_FP_FLAGS, BODY)                            \
  do {                                                                    \
    require_matrix_ms;                                                    \
    auto& AMU = P.AMU;                                                    \
    require(AMU.supports_xmisa_feature(FEATURE_MASK));                    \
    if (USES_ROUNDING)                                                    \
      require(AMU.xmfrm_is_valid(AMU.get_xmfrm()));                       \
    const reg_t dstIndex = insn.m_md();                                   \
    const reg_t srcIndex = insn.m_ms1();                                  \
    require(dstIndex >= 4 && dstIndex < 8);                               \
    require(srcIndex >= 4 && srcIndex < 8);                               \
    MatrixReg& md = AMU.acc_regs[dstIndex - 4];                           \
    MatrixReg& ms1 = AMU.acc_regs[srcIndex - 4];                          \
    require(md.get_rownum() == ms1.get_rownum());                         \
    require(md.get_row_bytes() == ms1.get_row_bytes());                   \
    require(md.get_row_bytes() % sizeof(DST_TYPE) == 0);                  \
    require(ms1.get_row_bytes() % sizeof(SRC_TYPE) == 0);                 \
    AmeConvertSnapshot<SRC_TYPE> S(ms1);                                  \
    AmeConvertDest<DST_TYPE> D(md);                                       \
    const reg_t rows = md.get_rownum();                                   \
    const reg_t srcCols = ms1.get_row_bytes() / sizeof(SRC_TYPE);         \
    const reg_t dstCols = md.get_row_bytes() / sizeof(DST_TYPE);          \
    if (USES_ROUNDING)                                                    \
      softfloat_roundingMode = AMU.get_xmfrm();                           \
    if (TRACK_FP_FLAGS)                                                   \
      softfloat_exceptionFlags = 0;                                      \
    BODY;                                                                 \
    if (TRACK_FP_FLAGS)                                                   \
      ame_set_matrix_fp_exceptions(AMU);                                  \
    AME_END;                                                              \
  } while (0)

#define AME_MFCVT_WIDEN_LOW(DST_TYPE, SRC_TYPE, FEATURE_MASK,             \
                            USES_ROUNDING, CONVERT_EXPR, BODY)            \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, USES_ROUNDING, true, \
    {                                                                     \
      require(srcCols == 2 * dstCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < dstCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_WIDEN_HIGH(DST_TYPE, SRC_TYPE, FEATURE_MASK,            \
                             USES_ROUNDING, CONVERT_EXPR, BODY)           \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, USES_ROUNDING, true, \
    {                                                                     \
      require(srcCols == 2 * dstCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < dstCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_NARROW_LOW(DST_TYPE, SRC_TYPE, FEATURE_MASK,            \
                             CONVERT_EXPR, BODY)                          \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, true, true,          \
    {                                                                     \
      require(dstCols == 2 * srcCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < srcCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_NARROW_HIGH(DST_TYPE, SRC_TYPE, FEATURE_MASK,           \
                              CONVERT_EXPR, BODY)                         \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, true, true,          \
    {                                                                     \
      require(dstCols == 2 * srcCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < srcCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_QUARTER_LOW(DST_TYPE, SRC_TYPE, FEATURE_MASK,           \
                              CONVERT_EXPR, BODY)                         \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, true, true,          \
    {                                                                     \
      require(dstCols == 4 * srcCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < srcCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_QUARTER_HIGH(DST_TYPE, SRC_TYPE, FEATURE_MASK,          \
                               CONVERT_EXPR, BODY)                        \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, true, true,          \
    {                                                                     \
      require(dstCols == 4 * srcCols);                                   \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < srcCols; ++j)                              \
          BODY;                                                           \
    })

#define AME_MFCVT_SAME(DST_TYPE, SRC_TYPE, FEATURE_MASK,                  \
                       CONVERT_EXPR, BODY)                                \
  AME_CONVERT_CORE(DST_TYPE, SRC_TYPE, FEATURE_MASK, true, true,          \
    {                                                                     \
      require(dstCols == srcCols);                                       \
      auto convert = [&](SRC_TYPE value) -> DST_TYPE { return CONVERT_EXPR; }; \
      for (reg_t i = 0; i < rows; ++i)                                   \
        for (reg_t j = 0; j < srcCols; ++j)                              \
          BODY;                                                           \
    })

static inline int64_t
ame_round_shift_signed(int32_t value, unsigned shift,
                       ameUnit_t::xmxrm_rounding_mode mode)
{
  if (shift == 0)
    return value;
  const uint32_t bits = static_cast<uint32_t>(value);
  const uint32_t roundBit = (bits >> (shift - 1)) & 1;
  const uint32_t lowerMask = (uint32_t(1) << (shift - 1)) - 1;
  const bool lowerNonzero = (bits & lowerMask) != 0;
  const bool discardedNonzero = (bits & ((uint32_t(1) << shift) - 1)) != 0;
  const uint32_t retainedLsb = (bits >> shift) & 1;
  uint32_t increment = 0;
  switch (mode) {
    case ameUnit_t::XMXRM_RNU: increment = roundBit; break;
    case ameUnit_t::XMXRM_RNE:
      increment = roundBit && (lowerNonzero || retainedLsb); break;
    case ameUnit_t::XMXRM_RDN: break;
    case ameUnit_t::XMXRM_ROD:
      increment = !retainedLsb && discardedNonzero; break;
  }
  const int64_t divisor = int64_t(1) << shift;
  const int64_t base = value >= 0
    ? int64_t(value) / divisor
    : -((-int64_t(value) + divisor - 1) / divisor);
  return base + increment;
}

static inline uint64_t
ame_round_shift_unsigned(uint32_t value, unsigned shift,
                         ameUnit_t::xmxrm_rounding_mode mode)
{
  if (shift == 0)
    return value;
  const uint32_t roundBit = (value >> (shift - 1)) & 1;
  const uint32_t lowerMask = (uint32_t(1) << (shift - 1)) - 1;
  const bool lowerNonzero = (value & lowerMask) != 0;
  const bool discardedNonzero =
    (value & ((uint32_t(1) << shift) - 1)) != 0;
  const uint32_t retainedLsb = (value >> shift) & 1;
  uint32_t increment = 0;
  switch (mode) {
    case ameUnit_t::XMXRM_RNU: increment = roundBit; break;
    case ameUnit_t::XMXRM_RNE:
      increment = roundBit && (lowerNonzero || retainedLsb); break;
    case ameUnit_t::XMXRM_RDN: break;
    case ameUnit_t::XMXRM_ROD:
      increment = !retainedLsb && discardedNonzero; break;
  }
  return (uint64_t(value) >> shift) + increment;
}

static inline int8_t
ame_saturate_i8(int64_t value, ameUnit_t& AMU)
{
  if (value > std::numeric_limits<int8_t>::max()) {
    AMU.xmsat->write_raw((AMU.get_xmsat() | 1) & ameUnit_t::XMSAT_MASK);
    return std::numeric_limits<int8_t>::max();
  }
  if (value < std::numeric_limits<int8_t>::min()) {
    AMU.xmsat->write_raw((AMU.get_xmsat() | 1) & ameUnit_t::XMSAT_MASK);
    return std::numeric_limits<int8_t>::min();
  }
  return static_cast<int8_t>(value);
}

static inline uint8_t
ame_saturate_u8(uint64_t value, ameUnit_t& AMU)
{
  if (value > std::numeric_limits<uint8_t>::max()) {
    AMU.xmsat->write_raw((AMU.get_xmsat() | 1) & ameUnit_t::XMSAT_MASK);
    return std::numeric_limits<uint8_t>::max();
  }
  return static_cast<uint8_t>(value);
}

#define AME_MN4CLIP(DST_TYPE, SRC_TYPE, FEATURE_MASK, HIGH_QUARTER,       \
                    ROUND_EXPR, BODY)                                    \
  do {                                                                    \
    require_matrix_ms;                                                    \
    auto& AMU = P.AMU;                                                    \
    require(AMU.supports_xmisa_feature(FEATURE_MASK));                    \
    const reg_t dstIndex = insn.m_md();                                   \
    const reg_t ms1Index = insn.m_ms1();                                  \
    const reg_t ms2Index = insn.m_ms2();                                  \
    require(dstIndex >= 4 && dstIndex < 8);                               \
    require(ms1Index >= 4 && ms1Index < 8);                               \
    require(ms2Index >= 4 && ms2Index < 8);                               \
    MatrixReg& md = AMU.acc_regs[dstIndex - 4];                           \
    MatrixReg& ms1 = AMU.acc_regs[ms1Index - 4];                          \
    MatrixReg& ms2 = AMU.acc_regs[ms2Index - 4];                          \
    require(md.get_rownum() == ms1.get_rownum());                         \
    require(md.get_rownum() == ms2.get_rownum());                         \
    require(md.get_row_bytes() == ms1.get_row_bytes());                   \
    require(md.get_row_bytes() == ms2.get_row_bytes());                   \
    require(md.get_row_bytes() % sizeof(DST_TYPE) == 0);                  \
    require(ms1.get_row_bytes() % sizeof(uint32_t) == 0);                 \
    require(ms2.get_row_bytes() % sizeof(SRC_TYPE) == 0);                 \
    AmeConvertDest<DST_TYPE> D(md);                                       \
    AmeConvertSnapshot<SRC_TYPE> S(ms2);                                  \
    AmeConvertSnapshot<uint32_t> Shift(ms1);                              \
    const reg_t rows = md.get_rownum();                                   \
    const reg_t srcCols = ms2.get_row_bytes() / sizeof(SRC_TYPE);         \
    const reg_t dstCols = md.get_row_bytes() / sizeof(DST_TYPE);          \
    require(dstCols == 4 * srcCols);                                      \
    const reg_t ctrl = insn.m_uimm3();                                   \
    const bool matrixMatrix = ctrl == 0b111;                              \
    if (!matrixMatrix)                                                    \
      require(ctrl < rows);                                               \
    const reg_t dstBase = HIGH_QUARTER ? srcCols : 0;                     \
    const auto roundingMode = AMU.get_xmxrm_mode();                       \
    auto convert = [&](SRC_TYPE value, unsigned shift) -> DST_TYPE {      \
      return ROUND_EXPR;                                                  \
    };                                                                     \
    for (reg_t i = 0; i < rows; ++i)                                     \
      for (reg_t j = 0; j < srcCols; ++j) {                              \
        const unsigned shift = Shift[matrixMatrix ? i : ctrl][j] & 0x1F; \
        BODY;                                                             \
      }                                                                   \
    AME_END;                                                              \
  } while (0)

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
