// mlcte16 tr, (rs1), rs2
AME_MATRIX_LOAD_C_E16_TRANSPOSED
({
  Rij = Memory[base + j * stride + i * elementBytes];
})
