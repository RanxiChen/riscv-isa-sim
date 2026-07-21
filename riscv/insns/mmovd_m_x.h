// mmovd.m.x md, rs2, rs1
AME_MMOV_M_X(uint64_t,
({
  dst.elt<uint64_t>(
      elementIndex / elementsPerRow,
      elementIndex % elementsPerRow) = static_cast<uint64_t>(scalarValue);
}));
