// mfcvtl.d.s
AME_MFCVT_WIDEN_LOW(float64_t, float32_t, AME_MFEW_FEATURE(XMISA_BIT_MMF32F64), false,
  f32_to_f64(value),
  { D[i][j] = convert(S[i][j]); });
