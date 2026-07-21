// msme64 ms3, (rs1) — whole matrix store e64
AME_MATRIX_WHOLE_LDST(uint64_t,
({
  MMU.store<uint64_t>(
      base + i * rowBytes + j * elementBytes, Rij);
}))
