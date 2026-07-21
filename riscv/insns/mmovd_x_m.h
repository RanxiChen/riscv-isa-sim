// mmovd.x.m rd, ms2, rs1
AME_MMOV_X_M(uint64_t,
({
  reg_t result = src.elt<uint64_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow);
  WRITE_RD(sext_xlen(result));
}));
