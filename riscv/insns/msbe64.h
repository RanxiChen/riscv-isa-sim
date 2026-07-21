// msbe64 tr, (rs1), rs2
AME_MATRIX_LDST(B, E64, {
  MMU.store<uint64_t>(
      base + i * stride + j * elementBytes, Rij);
})
