// msme32 ms3, (rs1) — whole matrix store e32
AME_MATRIX_WHOLE_LDST(uint32_t,
({
  MMU.store<uint32_t>(
      base + i * rowBytes + j * elementBytes, Rij);
}))
