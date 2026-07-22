// mn4clipl.w.mm / .mv.i
AME_MN4CLIP(int8_t, int32_t, AME_MIEW_FEATURE(XMISA_BIT_MMI8I32),
  false, ame_saturate_i8(ame_round_shift_signed(value, shift, roundingMode), AMU),
  { D[i][dstBase + j] = convert(S[i][j], shift); });
