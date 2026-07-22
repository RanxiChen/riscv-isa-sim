// mfscvth.b.h
AME_MFCVT_NARROW_HIGH(int8_t, float16_t, AME_MFIC_FEATURE(XMISA_BIT_MMF16F16),
  static_cast<int8_t>(f16_to_i8(value, AMU.get_xmfrm(), true)),
  { D[i][srcCols + j] = convert(S[i][j]); });
