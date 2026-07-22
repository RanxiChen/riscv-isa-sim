// mfcvth.bf16.s
AME_MFCVT_NARROW_HIGH(bfloat16_t, float32_t, AME_MFEW_FEATURE(XMISA_BIT_MMBF16F32),
  f32_to_bf16(value),
  { D[i][srcCols + j] = convert(S[i][j]); });
