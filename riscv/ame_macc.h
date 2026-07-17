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

class AmeF64RowView
{
public:
  AmeF64RowView(MatrixReg& reg, reg_t row, reg_t length)
    : reg(reg), row(row), length(length) {}

  float64_t elt(reg_t col) const { return reg.elt<float64_t>(row, col); }
  reg_t size() const { return length; }

private:
  MatrixReg& reg;
  reg_t row;
  reg_t length;
};

class AmeF64MatrixView
{
public:
  AmeF64MatrixView(MatrixReg& reg, reg_t row_length)
    : reg(reg), row_length(row_length) {}

  AmeF64RowView row(reg_t index) const { return AmeF64RowView(reg, index, row_length); }

private:
  MatrixReg& reg;
  reg_t row_length;
};

struct AmeF64DotExpr
{
  AmeF64RowView lhs;
  AmeF64RowView rhs;
};

inline AmeF64DotExpr vecdot(AmeF64RowView lhs, AmeF64RowView rhs)
{
  return {lhs, rhs};
}

class AmeF64AccRef
{
public:
  AmeF64AccRef(MatrixReg& reg, reg_t row, reg_t col)
    : reg(reg), row(row), col(col) {}

  void operator+=(const AmeF64DotExpr& dot);

private:
  MatrixReg& reg;
  reg_t row;
  reg_t col;
};

void ame_set_matrix_fp_exceptions(ameUnit_t& AMU);

#define AME_MFMACC_D_CORE(BODY) \
  do { \
    auto& AMU = p->AMU; \
    const reg_t md_field = insn.m_md(); \
    const reg_t ms1 = insn.m_ms1(); \
    const reg_t ms2 = insn.m_ms2(); \
    ame_require(md_field >= 4 && md_field < 8, insn); \
    ame_require(ms1 < 4, insn); \
    ame_require(ms2 < 4, insn); \
    ame_require(AMU.supports_xmisa_feature(ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF64F64)), insn); \
    ame_require(AMU.get_elen() >= 64, insn); \
    ame_require(AMU.xmfrm_is_valid(AMU.get_xmfrm()), insn); \
    const reg_t M = AMU.mtilem->read(); \
    const reg_t N = AMU.mtilen->read(); \
    const reg_t K = AMU.mtilek->read(); \
    ame_require(M <= AMU.get_rownum(), insn); \
    ame_require(N <= AMU.get_rownum(), insn); \
    ame_require(K <= AMU.get_trlen() / 64, insn); \
    softfloat_roundingMode = AMU.get_xmfrm(); \
    softfloat_exceptionFlags = 0; \
    MatrixReg& Areg = AMU.tile_regs[ms1]; \
    MatrixReg& Breg = AMU.tile_regs[ms2]; \
    MatrixReg& Creg = AMU.acc_regs[md_field - 4]; \
    AmeF64MatrixView A(Areg, K); \
    AmeF64MatrixView B(Breg, K); \
    for (reg_t i = 0; i < M; ++i) { \
      for (reg_t j = 0; j < N; ++j) { \
        AmeF64AccRef Cij(Creg, i, j); \
        BODY; \
      } \
    } \
    const float64_t zero = {0}; \
    const reg_t acc_rows = Creg.get_rownum(); \
    const reg_t acc_cols = Creg.get_row_bytes() / sizeof(float64_t); \
    for (reg_t i = 0; i < acc_rows; ++i) { \
      for (reg_t j = 0; j < acc_cols; ++j) { \
        if (i >= M || j >= N) \
          Creg.elt<float64_t>(i, j) = zero; \
      } \
    } \
    ame_set_matrix_fp_exceptions(AMU); \
  } while (0)

#define AME_MFMACC_D(BODY) \
  do { \
    require_matrix_ms; \
    AME_MFMACC_D_CORE(BODY); \
    AME_END; \
  } while (0);

void execute_mfmacc_d(processor_t* p, insn_t insn);

#endif
