// mfucvt.w.s
AME_MFCVT_SAME(uint32_t, float32_t, AME_MFIC_FEATURE(XMISA_BIT_MMF32F32),
  static_cast<uint32_t>(f32_to_ui32(value, AMU.get_xmfrm(), true)),
  { D[i][j] = convert(S[i][j]); });
