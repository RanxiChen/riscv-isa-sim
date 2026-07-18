// msae8 tr, (rs1), rs2 — A normal store e8
AME_MATRIX_STORE_A_E8_NORMAL
({
  Memory[base + i * stride + j * elementBytes] = Rij;
})
