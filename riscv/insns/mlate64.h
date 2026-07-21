// mlate64 tr, (rs1), rs2
AME_MATRIX_LDST(A, E64, {
  Rij = MMU.load<uint64_t>(
      base + j * stride + i * elementBytes);
})
