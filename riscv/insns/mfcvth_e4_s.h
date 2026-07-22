// mfcvth.e4.s
AME_MFCVT_QUARTER_HIGH(e4m3_t, float32_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F32),
  f32_to_e4m3(value, AMU.xmsaten_enabled()),
  { D[i][srcCols + j] = convert(S[i][j]); });
