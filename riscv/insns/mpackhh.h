// mpackhh md, ms1, ms2: high(ms1) + high(ms2)
AME_MPACK_LOOP
({
  const auto firstColumns = ame_extract_columns(ms1, halfCols, cols);
  const auto secondColumns = ame_extract_columns(ms2, halfCols, cols);
  ame_pack_columns(dst, firstColumns, secondColumns);
});
