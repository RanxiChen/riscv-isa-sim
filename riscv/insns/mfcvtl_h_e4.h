// mfcvtl.h.e4
AME_MFCVT_WIDEN_LOW(float16_t, e4m3_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F16), false,
  e4m3_to_f16(value),
  { D[i][j] = convert(S[i][j]); });
