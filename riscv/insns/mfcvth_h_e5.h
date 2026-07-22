// mfcvth.h.e5
AME_MFCVT_WIDEN_HIGH(float16_t, e5m2_t, AME_MFEW_FEATURE(XMISA_BIT_MMF8F16), false,
  e5m2_to_f16(value),
  { D[i][j] = convert(S[i][dstCols + j]); });
