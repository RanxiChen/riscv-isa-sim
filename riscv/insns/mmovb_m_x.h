// mmovb.m.x md, rs2, rs1
AME_MMOV_M_X(uint8_t,
({
  dst.elt<uint8_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow) = static_cast<uint8_t>(scalarValue);
}));
