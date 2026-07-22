// mfcvtl.e5.s
AME_MFCVT_QUARTER_LOW(e5m2_t, float32_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F32),
  f32_to_e5m2(value, AMU.xmsaten_enabled()),
  { D[i][j] = convert(S[i][j]); });
