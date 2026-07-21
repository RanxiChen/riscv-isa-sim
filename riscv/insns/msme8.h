// msme8 ms3, (rs1) — whole matrix store e8
AME_MATRIX_WHOLE_LDST(uint8_t,
({
  MMU.store<uint8_t>(
      base + i * rowBytes + j * elementBytes, Rij);
}))
