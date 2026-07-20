// msae64 tr, (rs1), rs2
AME_MATRIX_STORE_A_E64_NORMAL
({
  Memory[base + i * stride + j * elementBytes] = Rij;
})
