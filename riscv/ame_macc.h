// See LICENSE for license details.

#ifndef _RISCV_AME_MACC_H
#define _RISCV_AME_MACC_H

#include "ame_unit.h"
#include "decode.h"
#include "softfloat/softfloat.h"
#include "trap.h"

class processor_t;

inline void ame_require(bool cond, insn_t insn)
{
  if (!cond)
    throw trap_illegal_instruction(insn.bits());
}

template<typename T>
class AmeRowView
{
public:
  AmeRowView(MatrixReg& reg, reg_t row, reg_t length)
    : reg(reg), row_index(row), length(length) {}

  T elt(reg_t col) const { return reg.elt<T>(row_index, col); }
  reg_t size() const { return length; }

private:
  MatrixReg& reg;
  reg_t row_index;
  reg_t length;
};

template<typename T>
class AmeMatrixView
{
public:
  AmeMatrixView(MatrixReg& reg, reg_t row_length)
    : reg(reg), row_length(row_length) {}

  AmeRowView<T> row(reg_t index) const { return AmeRowView<T>(reg, index, row_length); }

private:
  MatrixReg& reg;
  reg_t row_length;
};

template<typename T>
struct AmeDotExpr
{
  AmeRowView<T> lhs;
  AmeRowView<T> rhs;
};

template<typename T>
inline AmeDotExpr<T> vecdot(AmeRowView<T> lhs, AmeRowView<T> rhs)
{
  return {lhs, rhs};
}

template<typename T>
struct AmeFloatOps;

template<>
struct AmeFloatOps<float16_t>
{
  static float16_t mul_add(float16_t lhs, float16_t rhs, float16_t acc);
};

template<>
struct AmeFloatOps<float32_t>
{
  static float32_t mul_add(float32_t lhs, float32_t rhs, float32_t acc);
};

template<>
struct AmeFloatOps<float64_t>
{
  static float64_t mul_add(float64_t lhs, float64_t rhs, float64_t acc);
};

template<typename T>
class AmeAccRef
{
public:
  AmeAccRef(MatrixReg& reg, reg_t row, reg_t col)
    : reg(reg), row_index(row), col_index(col) {}

  void operator+=(const AmeDotExpr<T>& dot)
  {
    T value = reg.elt<T>(row_index, col_index);
    for (reg_t k = 0; k < dot.lhs.size(); ++k)
      value = AmeFloatOps<T>::mul_add(dot.lhs.elt(k), dot.rhs.elt(k), value);
    reg.elt<T>(row_index, col_index) = value;
  }

private:
  MatrixReg& reg;
  reg_t row_index;
  reg_t col_index;
};

void ame_set_matrix_fp_exceptions(ameUnit_t& AMU);

#define AME_MFMACC_FP_CORE(TYPE, BITS, FEATURE, BODY) \
  do { \
    auto& AMU = p->AMU; \
    const reg_t md_field = insn.m_md(); \
    const reg_t ms1 = insn.m_ms1(); \
    const reg_t ms2 = insn.m_ms2(); \
    ame_require(md_field >= 4 && md_field < 8, insn); \
    ame_require(ms1 < 4, insn); \
    ame_require(ms2 < 4, insn); \
    ame_require(AMU.supports_xmisa_feature(ameUnit_t::xmisa_bit(FEATURE)), insn); \
    ame_require(AMU.get_elen() >= BITS, insn); \
    ame_require(AMU.xmfrm_is_valid(AMU.get_xmfrm()), insn); \
    const reg_t M = AMU.mtilem->read(); \
    const reg_t N = AMU.mtilen->read(); \
    const reg_t K = AMU.mtilek->read(); \
    ame_require(M <= AMU.get_rownum(), insn); \
    ame_require(N <= AMU.get_rownum(), insn); \
    ame_require(K <= AMU.get_trlen() / BITS, insn); \
    softfloat_roundingMode = AMU.get_xmfrm(); \
    softfloat_exceptionFlags = 0; \
    MatrixReg& Areg = AMU.tile_regs[ms1]; \
    MatrixReg& Breg = AMU.tile_regs[ms2]; \
    MatrixReg& Creg = AMU.acc_regs[md_field - 4]; \
    AmeMatrixView<TYPE> A(Areg, K); \
    AmeMatrixView<TYPE> B(Breg, K); \
    for (reg_t i = 0; i < M; ++i) { \
      for (reg_t j = 0; j < N; ++j) { \
        AmeAccRef<TYPE> Cij(Creg, i, j); \
        BODY; \
      } \
    } \
    const TYPE zero = {0}; \
    const reg_t acc_rows = Creg.get_rownum(); \
    const reg_t acc_cols = Creg.get_row_bytes() / sizeof(TYPE); \
    for (reg_t i = 0; i < acc_rows; ++i) { \
      for (reg_t j = 0; j < acc_cols; ++j) { \
        if (i >= M || j >= N) \
          Creg.elt<TYPE>(i, j) = zero; \
      } \
    } \
    ame_set_matrix_fp_exceptions(AMU); \
  } while (0)

#define AME_MFMACC_FP(TYPE, BITS, FEATURE, BODY) \
  do { \
    require_matrix_ms; \
    AME_MFMACC_FP_CORE(TYPE, BITS, FEATURE, BODY); \
    AME_END; \
  } while (0);

#define AME_MFMACC_H(BODY) \
  AME_MFMACC_FP(float16_t, 16, ameUnit_t::XMISA_BIT_MMF16F16, BODY)

#define AME_MFMACC_S(BODY) \
  AME_MFMACC_FP(float32_t, 32, ameUnit_t::XMISA_BIT_MMF32F32, BODY)

#define AME_MFMACC_D(BODY) \
  AME_MFMACC_FP(float64_t, 64, ameUnit_t::XMISA_BIT_MMF64F64, BODY)

void execute_mfmacc_h(processor_t* p, insn_t insn);
void execute_mfmacc_s(processor_t* p, insn_t insn);
void execute_mfmacc_d(processor_t* p, insn_t insn);

#endif
