// mfcvth.h.s
AME_MFCVT_NARROW_HIGH(float16_t, float32_t, AME_MFEW_FEATURE(XMISA_BIT_MMF16F32),
  f32_to_f16(value),
  { D[i][srcCols + j] = convert(S[i][j]); });
