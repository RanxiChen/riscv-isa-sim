// mcbce32.mv.i md, ms1[uimm3]
AME_MCBCE_LOOP(uint32_t,
({
  ame_copy_column<uint32_t>(dst, j, srcColumn);
}));
