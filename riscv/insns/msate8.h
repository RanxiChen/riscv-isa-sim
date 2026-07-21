// msate8 tr, (rs1), rs2 — A transposed store e8
AME_MATRIX_LDST(A, E8, {
  MMU.store<uint8_t>(
      base + j * stride + i * elementBytes, Rij);
})
