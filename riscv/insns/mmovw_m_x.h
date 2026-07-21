// mmovw.m.x md, rs2, rs1
AME_MMOV_M_X(uint32_t,
({
  dst.elt<uint32_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow) = static_cast<uint32_t>(scalarValue);
}));
