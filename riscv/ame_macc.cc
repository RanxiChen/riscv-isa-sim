// See LICENSE for license details.

#include "ame_macc.h"

#include "processor.h"
#include "softfloat.h"
#include "trap.h"

namespace {

static void require_ame(bool cond, insn_t insn)
{
  if (!cond)
    throw trap_illegal_instruction(insn.bits());
}

static reg_t m_md(insn_t insn)
{
  return insn.m_md();
}

static reg_t m_ms1(insn_t insn)
{
  return insn.m_ms1();
}

static reg_t m_ms2(insn_t insn)
{
  return insn.m_ms2();
}

static void set_matrix_fp_exceptions(ameUnit_t& AMU)
{
  // AME xmfflags uses NX/UF/OF/reserved/NV. SoftFloat's divide-by-zero
  // flag occupies the reserved bit position, and mfmacc.d cannot generate it.
  const reg_t matrix_flags = softfloat_exceptionFlags &
    (softfloat_flag_inexact | softfloat_flag_underflow |
     softfloat_flag_overflow | softfloat_flag_invalid);

  if (matrix_flags)
    AMU.xmfflags->write_raw((AMU.get_xmfflags() | matrix_flags) & AMU.XMFFLAGS_MASK);

  softfloat_exceptionFlags = 0;
}

} // namespace

void execute_mfmacc_d(processor_t* p, insn_t insn)
{
  auto& AMU = p->AMU;

  const reg_t md_field = m_md(insn);
  const reg_t ms1 = m_ms1(insn);
  const reg_t ms2 = m_ms2(insn);

  require_ame(md_field >= 4 && md_field < 8, insn);
  require_ame(ms1 < 4, insn);
  require_ame(ms2 < 4, insn);

  require_ame(AMU.supports_xmisa_feature(ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF64F64)), insn);
  require_ame(AMU.get_elen() >= 64, insn);
  require_ame(AMU.xmfrm_is_valid(AMU.get_xmfrm()), insn);

  const reg_t M = AMU.mtilem->read();
  const reg_t N = AMU.mtilen->read();
  const reg_t K = AMU.mtilek->read();

  require_ame(M <= AMU.get_rownum(), insn);
  require_ame(N <= AMU.get_rownum(), insn);
  require_ame(K <= AMU.get_trlen() / 64, insn);

  softfloat_roundingMode = AMU.get_xmfrm();
  softfloat_exceptionFlags = 0;

  MatrixReg& A = AMU.tile_regs[ms1];
  MatrixReg& B = AMU.tile_regs[ms2];
  MatrixReg& C = AMU.acc_regs[md_field - 4];

  for (reg_t i = 0; i < M; ++i) {
    for (reg_t j = 0; j < N; ++j) {
      float64_t Cij = C.elt<float64_t>(i, j);

      for (reg_t k = 0; k < K; ++k) {
        const float64_t Aik = A.elt<float64_t>(i, k);
        const float64_t Bjk = B.elt<float64_t>(j, k);
        Cij = f64_mulAdd(Aik, Bjk, Cij);
      }

      C.elt<float64_t>(i, j) = Cij;
    }
  }

  const float64_t zero = {0};
  const reg_t acc_rows = C.get_rownum();
  const reg_t acc_cols = C.get_row_bytes() / sizeof(float64_t);
  for (reg_t i = 0; i < acc_rows; ++i) {
    for (reg_t j = 0; j < acc_cols; ++j) {
      if (i >= M || j >= N)
        C.elt<float64_t>(i, j) = zero;
    }
  }

  set_matrix_fp_exceptions(AMU);
}
