// mscte16 tr, (rs1), rs2
AME_MATRIX_LDST(C, E16, {
  MMU.store<uint16_t>(
      base + j * stride + i * elementBytes, Rij);
})
