// mscte16 tr, (rs1), rs2
AME_MATRIX_STORE_C_E16_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
