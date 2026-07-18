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
template<typename AType, typename BType>
struct AmeDotExpr
{
  AmeRowView<AType> lhs;
  AmeRowView<BType> rhs;
};
template<typename AType, typename BType>
inline AmeDotExpr<AType, BType> vecdot(AmeRowView<AType> lhs, AmeRowView<BType> rhs)
{
  return {lhs, rhs};
}
template<
  typename CType,
  typename AType,
  typename BType,
  CType (*Op)(AType, BType, CType)>
class AmeAccRef
{
public:
  AmeAccRef(MatrixReg& reg, reg_t row, reg_t col)
    : reg(reg), row_index(row), col_index(col) {}
  void operator+=(const AmeDotExpr<AType, BType>& dot)
  {
    CType value = reg.elt<CType>(row_index, col_index);
    for (reg_t k = 0; k < dot.lhs.size(); ++k)
      value = Op(dot.lhs.elt(k), dot.rhs.elt(k), value);
    reg.elt<CType>(row_index, col_index) = value;
  }
private:
  MatrixReg& reg;
  reg_t row_index;
  reg_t col_index;
};
void ame_set_matrix_fp_exceptions(ameUnit_t& AMU);

// --- token-paste type/bits mappings ---
#define AME_MFMACC_TYPE_H    float16_t
#define AME_MFMACC_TYPE_BF16 bfloat16_t
#define AME_MFMACC_TYPE_S    float32_t
#define AME_MFMACC_TYPE_D    float64_t
#define AME_MFMACC_TYPE_E4   e4m3_t
#define AME_MFMACC_TYPE_E5   e5m2_t

#define AME_MFMACC_BITS_H    16
#define AME_MFMACC_BITS_BF16 16
#define AME_MFMACC_BITS_S    32
#define AME_MFMACC_BITS_D    64
#define AME_MFMACC_BITS_E4   8
#define AME_MFMACC_BITS_E5   8

#define AME_MFMACC_OP_RAW(CT, AT, BT) ame_mfmacc_op_##CT##_##AT##_##BT
#define AME_MFMACC_OP(CT, AT, BT) AME_MFMACC_OP_RAW(CT, AT, BT)

// --- 12 operation function declarations ---
#define AME_MFMACC_DECL_OP(CT, AT, BT)   AME_MFMACC_TYPE_##CT ame_mfmacc_op_##CT##_##AT##_##BT(     AME_MFMACC_TYPE_##AT a, AME_MFMACC_TYPE_##BT b, AME_MFMACC_TYPE_##CT c)

AME_MFMACC_DECL_OP(H, H, H);
AME_MFMACC_DECL_OP(S, S, S);
AME_MFMACC_DECL_OP(D, D, D);
AME_MFMACC_DECL_OP(S, H, H);
AME_MFMACC_DECL_OP(S, BF16, BF16);
AME_MFMACC_DECL_OP(D, S, S);
AME_MFMACC_DECL_OP(H, E4, E4);
AME_MFMACC_DECL_OP(H, E5, E5);
AME_MFMACC_DECL_OP(BF16, E4, E4);
AME_MFMACC_DECL_OP(BF16, E5, E5);
AME_MFMACC_DECL_OP(S, E4, E4);
AME_MFMACC_DECL_OP(S, E5, E5);

#undef AME_MFMACC_DECL_OP

// --- core execution macro (no require_matrix_ms / AME_END) ---
// Parameters: CT/AT/BT are type tokens (H, S, D, BF16, E4, E5)
// FEATURE is the xmisa bit enum name (e.g. XMISA_BIT_MMF16F16)
// BODY is the instruction body block
#define AME_MFMACC_IMPL(CT, AT, BT, FEATURE, BODY)   do {     auto& AMU = p->AMU;     const reg_t md_field = insn.m_md();     const reg_t ms1 = insn.m_ms1();     const reg_t ms2 = insn.m_ms2();     ame_require(md_field >= 4 && md_field < 8, insn);     ame_require(ms1 < 4, insn);     ame_require(ms2 < 4, insn);     ame_require(AMU.supports_xmisa_feature(ameUnit_t::xmisa_bit(ameUnit_t::FEATURE)), insn);     ame_require(AMU.get_elen() >= AME_MFMACC_BITS_##CT, insn);     ame_require(AMU.xmfrm_is_valid(AMU.get_xmfrm()), insn);     const reg_t M = AMU.mtilem->read();     const reg_t N = AMU.mtilen->read();     const reg_t K = AMU.mtilek->read();     ame_require(M <= AMU.get_rownum(), insn);     ame_require(N <= AMU.get_rownum(), insn);     ame_require(K <= AMU.get_trlen() / AME_MFMACC_BITS_##AT, insn);     softfloat_roundingMode = AMU.get_xmfrm();     softfloat_exceptionFlags = 0;     MatrixReg& Areg = AMU.tile_regs[ms1];     MatrixReg& Breg = AMU.tile_regs[ms2];     MatrixReg& Creg = AMU.acc_regs[md_field - 4];     AmeMatrixView<AME_MFMACC_TYPE_##AT> A(Areg, K);     AmeMatrixView<AME_MFMACC_TYPE_##BT> B(Breg, K);     for (reg_t i = 0; i < M; ++i) {       for (reg_t j = 0; j < N; ++j) {         AmeAccRef<AME_MFMACC_TYPE_##CT, AME_MFMACC_TYPE_##AT, AME_MFMACC_TYPE_##BT, AME_MFMACC_OP(CT, AT, BT)> Cij(Creg, i, j);         BODY;       }     }     const AME_MFMACC_TYPE_##CT zero = {0};     const reg_t acc_rows = Creg.get_rownum();     const reg_t acc_cols = Creg.get_row_bytes() / sizeof(AME_MFMACC_TYPE_##CT);     for (reg_t i = 0; i < acc_rows; ++i) {       for (reg_t j = 0; j < acc_cols; ++j) {         if (i >= M || j >= N)           Creg.elt<AME_MFMACC_TYPE_##CT>(i, j) = zero;       }     }     ame_set_matrix_fp_exceptions(AMU);   } while (0);

// --- outer wrapper: require_matrix_ms + core + AME_END ---
#define AME_MFMACC_FP(CT, AT, BT, FEATURE, BODY)   do {     require_matrix_ms;     AME_MFMACC_IMPL(CT, AT, BT, FEATURE, BODY);     AME_END;   } while (0);

// --- per-instruction macros ---
#define AME_MFMACC_H_H_H(BODY)   AME_MFMACC_FP(H, H, H, XMISA_BIT_MMF16F16, BODY)
#define AME_MFMACC_S_S_S(BODY)   AME_MFMACC_FP(S, S, S, XMISA_BIT_MMF32F32, BODY)
#define AME_MFMACC_D_D_D(BODY)   AME_MFMACC_FP(D, D, D, XMISA_BIT_MMF64F64, BODY)
#define AME_MFMACC_S_H_H(BODY)   AME_MFMACC_FP(S, H, H, XMISA_BIT_MMF16F32, BODY)
#define AME_MFMACC_S_BF16_BF16(BODY)   AME_MFMACC_FP(S, BF16, BF16, XMISA_BIT_MMBF16F32, BODY)
#define AME_MFMACC_D_S_S(BODY)   AME_MFMACC_FP(D, S, S, XMISA_BIT_MMF32F64, BODY)
#define AME_MFMACC_H_E4_E4(BODY)   AME_MFMACC_FP(H, E4, E4, XMISA_BIT_MMF8F16, BODY)
#define AME_MFMACC_H_E5_E5(BODY)   AME_MFMACC_FP(H, E5, E5, XMISA_BIT_MMF8F16, BODY)
#define AME_MFMACC_BF16_E4_E4(BODY)   AME_MFMACC_FP(BF16, E4, E4, XMISA_BIT_MMF8BF16, BODY)
#define AME_MFMACC_BF16_E5_E5(BODY)   AME_MFMACC_FP(BF16, E5, E5, XMISA_BIT_MMF8BF16, BODY)
#define AME_MFMACC_S_E4_E4(BODY)   AME_MFMACC_FP(S, E4, E4, XMISA_BIT_MMF8F32, BODY)
#define AME_MFMACC_S_E5_E5(BODY)   AME_MFMACC_FP(S, E5, E5, XMISA_BIT_MMF8F32, BODY)

// --- execute functions for internal tests ---
void execute_mfmacc_h(processor_t* p, insn_t insn);
void execute_mfmacc_s(processor_t* p, insn_t insn);
void execute_mfmacc_d(processor_t* p, insn_t insn);
void execute_mfmacc_s_h(processor_t* p, insn_t insn);
void execute_mfmacc_s_bf16(processor_t* p, insn_t insn);
void execute_mfmacc_d_s(processor_t* p, insn_t insn);
void execute_mfmacc_h_e4(processor_t* p, insn_t insn);
void execute_mfmacc_h_e5(processor_t* p, insn_t insn);
void execute_mfmacc_bf16_e4(processor_t* p, insn_t insn);
void execute_mfmacc_bf16_e5(processor_t* p, insn_t insn);
void execute_mfmacc_s_e4(processor_t* p, insn_t insn);
void execute_mfmacc_s_e5(processor_t* p, insn_t insn);

#endif
