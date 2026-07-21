// msme16 ms3, (rs1) — whole matrix store e16
AME_MATRIX_WHOLE_LDST(uint16_t,
({
  MMU.store<uint16_t>(
      base + i * rowBytes + j * elementBytes, Rij);
}))
