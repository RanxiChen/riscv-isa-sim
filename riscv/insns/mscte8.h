// mscte8 tr, (rs1), rs2
AME_MATRIX_LDST(C, E8, {
  MMU.store<uint8_t>(
      base + j * stride + i * elementBytes, Rij);
})
