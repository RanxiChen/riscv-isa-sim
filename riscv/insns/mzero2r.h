// mzero2r md
AME_MZERO_LOOP(2,
({
  if (index < 4)
    P.AMU.tile_regs[index].zero();
  else
    P.AMU.acc_regs[index - 4].zero();
}));
