// mlbe32 tr, (rs1), rs2
AME_MATRIX_LOAD_B_E32_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
