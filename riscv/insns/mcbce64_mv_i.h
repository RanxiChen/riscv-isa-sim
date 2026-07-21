// mcbce64.mv.i md, ms1[uimm3]
AME_MCBCE_LOOP(uint64_t,
({
  ame_copy_column<uint64_t>(dst, j, srcColumn);
}));
