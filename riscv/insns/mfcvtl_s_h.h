// mfcvtl.s.h
AME_MFCVT_WIDEN_LOW(float32_t, float16_t, AME_MFEW_FEATURE(XMISA_BIT_MMF16F32), false,
  f16_to_f32(value),
  { D[i][j] = convert(S[i][j]); });
