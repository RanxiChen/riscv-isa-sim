// mlce16 tr, (rs1), rs2
AME_MATRIX_LDST(C, E16, {
  Rij = MMU.load<uint16_t>(
      base + i * stride + j * elementBytes);
})
