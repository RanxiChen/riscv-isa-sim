// msce32 tr, (rs1), rs2
AME_MATRIX_LDST(C, E32, {
  MMU.store<uint32_t>(
      base + i * stride + j * elementBytes, Rij);
})
