// msate32 tr, (rs1), rs2
AME_MATRIX_LDST(A, E32, {
  MMU.store<uint32_t>(
      base + j * stride + i * elementBytes, Rij);
})
