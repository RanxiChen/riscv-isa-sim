// msfcvtl.h.b
AME_MFCVT_WIDEN_LOW(float16_t, int8_t, AME_MFIC_FEATURE(XMISA_BIT_MMF16F16), true,
  i32_to_f16(static_cast<int32_t>(value)),
  { D[i][j] = convert(S[i][j]); });
