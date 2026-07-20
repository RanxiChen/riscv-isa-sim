// msce32 tr, (rs1), rs2
AME_MATRIX_STORE_C_E32_NORMAL
({
  Memory[base + i * stride + j * elementBytes] = Rij;
})
