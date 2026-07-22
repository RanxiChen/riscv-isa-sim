// mn4cliphu.w.mm / .mv.i
AME_MN4CLIP(uint8_t, uint32_t, AME_MIEW_FEATURE(XMISA_BIT_MMI8I32),
  true, ame_saturate_u8(ame_round_shift_unsigned(value, shift, roundingMode), AMU),
  { D[i][dstBase + j] = convert(S[i][j], shift); });
