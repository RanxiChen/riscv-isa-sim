// mfucvtl.b.h
AME_MFCVT_NARROW_LOW(uint8_t, float16_t, AME_MFIC_FEATURE(XMISA_BIT_MMF16F16),
  static_cast<uint8_t>(f16_to_ui8(value, AMU.get_xmfrm(), true)),
  { D[i][j] = convert(S[i][j]); });
