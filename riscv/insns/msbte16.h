// msbte16 tr, (rs1), rs2
AME_MATRIX_STORE_B_E16_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
