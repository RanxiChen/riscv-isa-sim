// mmovw.x.m rd, ms2, rs1
AME_MMOV_X_M(int32_t,
({
  reg_t result = src.elt<int32_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow);
  WRITE_RD(sext_xlen(result));
}));
