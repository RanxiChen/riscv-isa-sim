// See LICENSE for license details.

#include "ame_macc.h"

#include "processor.h"

void AmeF64AccRef::operator+=(const AmeF64DotExpr& dot)
{
  float64_t value = reg.elt<float64_t>(row, col);

  for (reg_t k = 0; k < dot.lhs.size(); ++k)
    value = f64_mulAdd(dot.lhs.elt(k), dot.rhs.elt(k), value);

  reg.elt<float64_t>(row, col) = value;
}

void ame_set_matrix_fp_exceptions(ameUnit_t& AMU)
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

void execute_mfmacc_d(processor_t* p, insn_t insn)
{
  AME_MFMACC_D_CORE
  ({
    Cij += vecdot(A.row(i), B.row(j));
  });
}
