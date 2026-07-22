// mufcvt.s.w
AME_MFCVT_SAME(float32_t, uint32_t, AME_MFIC_FEATURE(XMISA_BIT_MMF32F32),
  ui32_to_f32(value),
  { D[i][j] = convert(S[i][j]); });
