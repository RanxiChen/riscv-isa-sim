// msate16 tr, (rs1), rs2
AME_MATRIX_LDST(A, E16, {
  MMU.store<uint16_t>(
      base + j * stride + i * elementBytes, Rij);
})
