// See LICENSE for license details.

#include "ame_macc.h"

#include "processor.h"

float16_t AmeFloatOps<float16_t>::mul_add(float16_t lhs, float16_t rhs, float16_t acc)
{
  return f16_mulAdd(lhs, rhs, acc);
}

float32_t AmeFloatOps<float32_t>::mul_add(float32_t lhs, float32_t rhs, float32_t acc)
{
  return f32_mulAdd(lhs, rhs, acc);
}

float64_t AmeFloatOps<float64_t>::mul_add(float64_t lhs, float64_t rhs, float64_t acc)
{
  return f64_mulAdd(lhs, rhs, acc);
}

void ame_set_matrix_fp_exceptions(ameUnit_t& AMU)
{
  // AME xmfflags uses NX/UF/OF/reserved/NV. SoftFloat's divide-by-zero
  // flag occupies the reserved bit position, and mfmacc cannot generate it.
  const reg_t matrix_flags = softfloat_exceptionFlags &
    (softfloat_flag_inexact | softfloat_flag_underflow |
     softfloat_flag_overflow | softfloat_flag_invalid);

  if (matrix_flags)
    AMU.xmfflags->write_raw((AMU.get_xmfflags() | matrix_flags) & AMU.XMFFLAGS_MASK);

  softfloat_exceptionFlags = 0;
}

void execute_mfmacc_h(processor_t* p, insn_t insn)
{
  AME_MFMACC_FP_CORE(float16_t, 16, ameUnit_t::XMISA_BIT_MMF16F16,
  ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s(processor_t* p, insn_t insn)
{
  AME_MFMACC_FP_CORE(float32_t, 32, ameUnit_t::XMISA_BIT_MMF32F32,
  ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_d(processor_t* p, insn_t insn)
{
  AME_MFMACC_FP_CORE(float64_t, 64, ameUnit_t::XMISA_BIT_MMF64F64,
  ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}
