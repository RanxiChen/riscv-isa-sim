// mlce32 tr, (rs1), rs2
AME_MATRIX_LOAD_C_E32_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
