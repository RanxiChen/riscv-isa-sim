// msbte64 tr, (rs1), rs2
AME_MATRIX_STORE_B_E64_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
