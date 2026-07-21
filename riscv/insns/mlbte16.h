// mlbte16 tr, (rs1), rs2
AME_MATRIX_LDST(B, E16, {
  Rij = MMU.load<uint16_t>(
      base + j * stride + i * elementBytes);
})
