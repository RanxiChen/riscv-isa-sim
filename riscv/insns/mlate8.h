// mlate8 tr, (rs1), rs2 — A transposed load e8
AME_MATRIX_LDST(A, E8, {
  Rij = MMU.load<uint8_t>(
      base + j * stride + i * elementBytes);
})
