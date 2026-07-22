// mfcvth.s.bf16
AME_MFCVT_WIDEN_HIGH(float32_t, bfloat16_t, AME_MFEW_FEATURE(XMISA_BIT_MMBF16F32), false,
  bf16_to_f32(value),
  { D[i][j] = convert(S[i][dstCols + j]); });
