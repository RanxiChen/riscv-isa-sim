// mmovh.m.x md, rs2, rs1
AME_MMOV_M_X(uint16_t,
({
  dst.elt<uint16_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow) = static_cast<uint16_t>(scalarValue);
}));
