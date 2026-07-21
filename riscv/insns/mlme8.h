// mlme8 md, (rs1) — whole matrix load e8
AME_MATRIX_WHOLE_LDST(uint8_t,
({
  Rij = MMU.load<uint8_t>(
      base + i * rowBytes + j * elementBytes);
}))
