// mcbce8.mv.i md, ms1[uimm3]
AME_MCBCE_LOOP(uint8_t,
({
  ame_copy_column<uint8_t>(dst, j, srcColumn);
}));
