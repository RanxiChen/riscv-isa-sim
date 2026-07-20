// mmaccu.w.b md, ms2, ms1
// uint8 x uint8 -> int32
AME_MMACC_I32_U8_U8
({
  Cij += vecdot(A.row(i), B.row(j));
})
