// msfcvt.s.w
AME_MFCVT_SAME(float32_t, int32_t, AME_MFIC_FEATURE(XMISA_BIT_MMF32F32),
  i32_to_f32(value),
  { D[i][j] = convert(S[i][j]); });
