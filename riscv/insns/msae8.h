// msae8 tr, (rs1), rs2 — A normal store e8
AME_MATRIX_LDST(A, E8, {
  MMU.store<uint8_t>(
      base + i * stride + j * elementBytes, Rij);
})
