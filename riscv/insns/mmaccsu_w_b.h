// mmaccsu.w.b md, ms2, ms1
// int8 ms1 x uint8 ms2 -> int32
AME_MMACC_I32_I8_U8
({
  Cij += vecdot(A.row(i), B.row(j));
})
