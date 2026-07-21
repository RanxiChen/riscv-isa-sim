// msbe16 tr, (rs1), rs2
AME_MATRIX_LDST(B, E16, {
  MMU.store<uint16_t>(
      base + i * stride + j * elementBytes, Rij);
})
