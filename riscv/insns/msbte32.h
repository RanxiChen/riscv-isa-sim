// msbte32 tr, (rs1), rs2
AME_MATRIX_LDST(B, E32, {
  MMU.store<uint32_t>(
      base + j * stride + i * elementBytes, Rij);
})
