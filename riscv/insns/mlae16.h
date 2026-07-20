// mlae16 tr, (rs1), rs2
AME_MATRIX_LOAD_A_E16_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
