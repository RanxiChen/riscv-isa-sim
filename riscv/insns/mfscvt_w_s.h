// mfscvt.w.s
AME_MFCVT_SAME(int32_t, float32_t, AME_MFIC_FEATURE(XMISA_BIT_MMF32F32),
  static_cast<int32_t>(f32_to_i32(value, AMU.get_xmfrm(), true)),
  { D[i][j] = convert(S[i][j]); });
