// mfcvth.e4.h
AME_MFCVT_NARROW_HIGH(e4m3_t, float16_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F16),
  f16_to_e4m3(value, AMU.xmsaten_enabled()),
  { D[i][srcCols + j] = convert(S[i][j]); });
