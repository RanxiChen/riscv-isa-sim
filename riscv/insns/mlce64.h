// mlce64 tr, (rs1), rs2
AME_MATRIX_LDST(C, E64, {
  Rij = MMU.load<uint64_t>(
      base + i * stride + j * elementBytes);
})
