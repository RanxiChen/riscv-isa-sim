// mlce8 tr, (rs1), rs2
AME_MATRIX_LDST(C, E8, {
  Rij = MMU.load<uint8_t>(
      base + i * stride + j * elementBytes);
})
