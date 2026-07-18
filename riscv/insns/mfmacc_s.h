// mfmacc.s md, ms2, ms1
// fp32 × fp32 -> fp32
AME_MFMACC_S_S_S
({
  Cij += vecdot(A.row(i), B.row(j));
})
