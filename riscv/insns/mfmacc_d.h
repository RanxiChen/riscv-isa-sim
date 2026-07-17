// mfmacc.d md, ms2, ms1
// fp64 × fp64 -> fp64
AME_MFMACC_D
({
  Cij += vecdot(A.row(i), B.row(j));
})
