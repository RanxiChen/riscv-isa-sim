// mlcte8 tr, (rs1), rs2
AME_MATRIX_LOAD_C_E8_TRANSPOSED
({
  Rij = Memory[base + j * stride + i * elementBytes];
})
