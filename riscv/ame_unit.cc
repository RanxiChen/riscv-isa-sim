// See LICENSE for license details.

#include "config.h"
#include "ame_unit.h"
#include "processor.h"

#include <cstdlib>
#include <cstring>

// =============================================================================
// MatrixReg — one AME architectural matrix register
// =============================================================================

MatrixReg::~MatrixReg()
{
  free(data);
  data = nullptr;
}

void MatrixReg::reset(reg_t new_total_bits, reg_t new_row_bits)
{
  assert(new_total_bits != 0);
  assert(new_row_bits != 0);
  assert(new_total_bits % new_row_bits == 0);
  assert(new_total_bits % 8 == 0);
  assert(new_row_bits % 8 == 0);

  free(data);

  total_bits = new_total_bits;
  row_bits = new_row_bits;
  total_bytes = total_bits / 8;
  row_bytes = row_bits / 8;
  rownum = total_bits / row_bits;

  data = malloc(total_bytes);
  assert(data != nullptr);
  memset(data, 0, total_bytes);
}

// =============================================================================
// ame_mcsr_t implementation — composite CSR for xmcsr (0x802)
// =============================================================================
ame_mcsr_t::ame_mcsr_t(processor_t* const proc, const reg_t addr,
                       matrix_csr_t_p xmxrm, matrix_csr_t_p xmsat,
                       matrix_csr_t_p xmfflags, matrix_csr_t_p xmfrm,
                       matrix_csr_t_p xmsaten)
  : csr_t(proc, addr),
    _xmxrm(xmxrm),
    _xmsat(xmsat),
    _xmfflags(xmfflags),
    _xmfrm(xmfrm),
    _xmsaten(xmsaten)
  {}

const unsigned ame_mcsr_t::XMSAT_LSB;
const unsigned ame_mcsr_t::XMFFLAGS_LSB;
const unsigned ame_mcsr_t::XMFRM_LSB;
const unsigned ame_mcsr_t::XMSATEN_LSB;

void ame_mcsr_t::verify_permissions(insn_t insn, bool write) const
{
  // All sub-CSRs have the same privilege (URW via address encoding).
  // TODO: require mstatus.MS != Off when MS field is defined (spec 3.10).
  (void)write; // TODO: use when MS check is added
  _xmxrm->verify_permissions(insn, write);
}

reg_t ame_mcsr_t::read() const noexcept
{
  return (_xmsaten->read() << XMSATEN_LSB)
       | (_xmfrm->read()   << XMFRM_LSB)
       | (_xmfflags->read() << XMFFLAGS_LSB)
       | (_xmsat->read()   << XMSAT_LSB)
       | (_xmxrm->read()   << XMXRM_LSB);
}

bool ame_mcsr_t::unlogged_write(const reg_t val) noexcept
{
  _xmxrm->write((val >> XMXRM_LSB) & XMXRM_FIELD_MASK);
  _xmsat->write((val >> XMSAT_LSB) & XMSAT_FIELD_MASK);
  _xmfflags->write((val >> XMFFLAGS_LSB) & XMFFLAGS_FIELD_MASK);
  _xmfrm->write((val >> XMFRM_LSB) & XMFRM_FIELD_MASK);
  _xmsaten->write((val >> XMSATEN_LSB) & XMSATEN_FIELD_MASK);
  // Logging is handled by the individual sub-CSRs.
  return false;
}

// =============================================================================
// ameUnit_t — AME architectural state container
// =============================================================================

reg_t ameUnit_t::xmisa_mfic_mask() const
{
  return reg_t(1) << (p->get_xlen() - 3);
}

reg_t ameUnit_t::xmisa_mfew_mask() const
{
  return reg_t(1) << (p->get_xlen() - 2);
}

reg_t ameUnit_t::xmisa_miew_mask() const
{
  return reg_t(1) << (p->get_xlen() - 1);
}

void ameUnit_t::set_mtilem(reg_t value)
{
  mtilem->write_raw(value);
}

void ameUnit_t::reset()
{
  for (auto& reg : tile_regs)
    reg.reset(TLEN, TRLEN);

  for (auto& reg : acc_regs)
    reg.reset(ALEN, ARLEN);

  auto state = p->get_state();

  // --- URW CSRs: register to global CSR map and keep local shared_ptr ---

  // 0x803-0x805: mtilem/mtilen/mtilek — runtime tile shape
  state->add_csr(0x803, mtilem =
    std::make_shared<matrix_csr_t>(p, 0x803, /*init*/ 0));
  state->add_csr(0x804, mtilen =
    std::make_shared<matrix_csr_t>(p, 0x804, /*init*/ 0));
  state->add_csr(0x805, mtilek =
    std::make_shared<matrix_csr_t>(p, 0x805, /*init*/ 0));

  // 0x806: xmxrm — fixed-point rounding mode (2-bit)
  state->add_csr(0x806, xmxrm =
    std::make_shared<matrix_csr_t>(p, 0x806, /*init*/ 0));

  // 0x807: xmsat — fixed-point saturation flag (1-bit)
  state->add_csr(0x807, xmsat =
    std::make_shared<matrix_csr_t>(p, 0x807, /*init*/ 0));

  // 0x808: xmfflags — FP exception flags (5-bit)
  state->add_csr(0x808, xmfflags =
    std::make_shared<matrix_csr_t>(p, 0x808, /*init*/ 0));

  // 0x809: xmfrm — FP rounding mode (3-bit)
  state->add_csr(0x809, xmfrm =
    std::make_shared<matrix_csr_t>(p, 0x809, /*init*/ 0));

  // 0x80A: xmsaten — saturation enable (1-bit)
  state->add_csr(0x80A, xmsaten =
    std::make_shared<matrix_csr_t>(p, 0x80A, /*init*/ 0));

  // 0x802: xmcsr — composite of the 5 sub-CSRs above
  state->add_csr(0x802,
    std::make_shared<ame_mcsr_t>(p, 0x802, xmxrm, xmsat, xmfflags, xmfrm, xmsaten));

  // --- URO CSRs: read-only hardware capacity registers ---

  // 0xCC0: xmisa — matrix ISA feature bitmap.  Keep all bits clear until
  // matching instruction families are implemented and guarded by feature checks.
  state->add_csr(0xCC0, xmisa =
    std::make_shared<matrix_csr_t>(p, 0xCC0, /*init*/ get_xmisa()));

  // 0xCC1: xtlenb = TLEN / 8
  state->add_csr(0xCC1,
    std::make_shared<matrix_csr_t>(p, 0xCC1, /*init*/ TLENB));

  // 0xCC2: xtrlenb = TRLEN / 8
  state->add_csr(0xCC2,
    std::make_shared<matrix_csr_t>(p, 0xCC2, /*init*/ TRLENB));

  // 0xCC3: xalenb = ROWNUM^2 * ELEN / 8
  state->add_csr(0xCC3,
    std::make_shared<matrix_csr_t>(p, 0xCC3, /*init*/ ALENB));
}
