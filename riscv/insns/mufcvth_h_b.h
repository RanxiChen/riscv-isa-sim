// mufcvth.h.b
AME_MFCVT_WIDEN_HIGH(float16_t, uint8_t, AME_MFIC_FEATURE(XMISA_BIT_MMF16F16), true,
  ui32_to_f16(static_cast<uint32_t>(value)),
  { D[i][j] = convert(S[i][dstCols + j]); });
