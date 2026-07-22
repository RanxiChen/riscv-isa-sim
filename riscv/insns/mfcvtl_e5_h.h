// mfcvtl.e5.h
AME_MFCVT_NARROW_LOW(e5m2_t, float16_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F16),
  f16_to_e5m2(value, AMU.xmsaten_enabled()),
  { D[i][j] = convert(S[i][j]); });
