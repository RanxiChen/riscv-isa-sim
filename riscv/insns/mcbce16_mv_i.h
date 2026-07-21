// mcbce16.mv.i md, ms1[uimm3]
AME_MCBCE_LOOP(uint16_t,
({
  ame_copy_column<uint16_t>(dst, j, srcColumn);
}));
