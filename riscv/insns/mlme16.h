// mlme16 md, (rs1) — whole matrix load e16
AME_MATRIX_WHOLE_LDST(uint16_t,
({
  Rij = MMU.load<uint16_t>(
      base + i * rowBytes + j * elementBytes);
}))
