// mfcvtl.s.d
AME_MFCVT_NARROW_LOW(float32_t, float64_t, AME_MFEW_FEATURE(XMISA_BIT_MMF32F64),
  f64_to_f32(value),
  { D[i][j] = convert(S[i][j]); });
