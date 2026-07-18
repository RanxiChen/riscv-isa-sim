// See LICENSE for license details.
#include "ame_macc.h"
#include "processor.h"

float16_t ame_mfmacc_op_H_H_H(float16_t a, float16_t b, float16_t c)
{
  return f16_mulAdd(a, b, c);
}

float32_t ame_mfmacc_op_S_S_S(float32_t a, float32_t b, float32_t c)
{
  return f32_mulAdd(a, b, c);
}

float64_t ame_mfmacc_op_D_D_D(float64_t a, float64_t b, float64_t c)
{
  return f64_mulAdd(a, b, c);
}

float32_t ame_mfmacc_op_S_H_H(float16_t a, float16_t b, float32_t c)
{
  return f32_mulAdd(f16_to_f32(a), f16_to_f32(b), c);
}

float32_t ame_mfmacc_op_S_BF16_BF16(bfloat16_t a, bfloat16_t b, float32_t c)
{
  return f32_mulAdd(bf16_to_f32(a), bf16_to_f32(b), c);
}

float64_t ame_mfmacc_op_D_S_S(float32_t a, float32_t b, float64_t c)
{
  return f64_mulAdd(f32_to_f64(a), f32_to_f64(b), c);
}

float16_t ame_mfmacc_op_H_E4_E4(e4m3_t a, e4m3_t b, float16_t c)
{
  return f16_mulAdd(e4m3_to_f16(a), e4m3_to_f16(b), c);
}

float16_t ame_mfmacc_op_H_E5_E5(e5m2_t a, e5m2_t b, float16_t c)
{
  return f16_mulAdd(e5m2_to_f16(a), e5m2_to_f16(b), c);
}

bfloat16_t ame_mfmacc_op_BF16_E4_E4(e4m3_t a, e4m3_t b, bfloat16_t c)
{
  return bf16_mulAdd(e4m3_to_bf16(a), e4m3_to_bf16(b), c);
}

bfloat16_t ame_mfmacc_op_BF16_E5_E5(e5m2_t a, e5m2_t b, bfloat16_t c)
{
  return bf16_mulAdd(e5m2_to_bf16(a), e5m2_to_bf16(b), c);
}

float32_t ame_mfmacc_op_S_E4_E4(e4m3_t a, e4m3_t b, float32_t c)
{
  return f32_mulAdd(f16_to_f32(e4m3_to_f16(a)), f16_to_f32(e4m3_to_f16(b)), c);
}

float32_t ame_mfmacc_op_S_E5_E5(e5m2_t a, e5m2_t b, float32_t c)
{
  return f32_mulAdd(f16_to_f32(e5m2_to_f16(a)), f16_to_f32(e5m2_to_f16(b)), c);
}

void ame_set_matrix_fp_exceptions(ameUnit_t& AMU)
{
  const reg_t matrix_flags = softfloat_exceptionFlags &
    (softfloat_flag_inexact | softfloat_flag_underflow |
     softfloat_flag_overflow | softfloat_flag_invalid);
  if (matrix_flags)
    AMU.xmfflags->write_raw((AMU.get_xmfflags() | matrix_flags) & AMU.XMFFLAGS_MASK);
  softfloat_exceptionFlags = 0;
}

void execute_mfmacc_h(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(H, H, H, XMISA_BIT_MMF16F16, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(S, S, S, XMISA_BIT_MMF32F32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_d(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(D, D, D, XMISA_BIT_MMF64F64, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s_h(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(S, H, H, XMISA_BIT_MMF16F32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s_bf16(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(S, BF16, BF16, XMISA_BIT_MMBF16F32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_d_s(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(D, S, S, XMISA_BIT_MMF32F64, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_h_e4(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(H, E4, E4, XMISA_BIT_MMF8F16, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_h_e5(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(H, E5, E5, XMISA_BIT_MMF8F16, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_bf16_e4(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(BF16, E4, E4, XMISA_BIT_MMF8BF16, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_bf16_e5(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(BF16, E5, E5, XMISA_BIT_MMF8BF16, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s_e4(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(S, E4, E4, XMISA_BIT_MMF8F32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mfmacc_s_e5(processor_t* p, insn_t insn)
{
  AME_MFMACC_IMPL(S, E5, E5, XMISA_BIT_MMF8F32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mmacc_w_b(processor_t* p, insn_t insn)
{
  AME_MMACC_IMPL(I32, I8, I8, XMISA_BIT_MMI8I32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mmaccu_w_b(processor_t* p, insn_t insn)
{
  AME_MMACC_IMPL(I32, U8, U8, XMISA_BIT_MMI8I32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mmaccus_w_b(processor_t* p, insn_t insn)
{
  AME_MMACC_IMPL(I32, U8, I8, XMISA_BIT_MMI8I32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}

void execute_mmaccsu_w_b(processor_t* p, insn_t insn)
{
  AME_MMACC_IMPL(I32, I8, U8, XMISA_BIT_MMI8I32, ({
    Cij += vecdot(A.row(i), B.row(j));
  }));
}
