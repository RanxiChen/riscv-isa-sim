// See LICENSE for license details.

#include "config.h"
#include "ame_unit.h"
#include "processor.h"

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
       | (_xmxrm->read());
}

bool ame_mcsr_t::unlogged_write(const reg_t val) noexcept
{
  _xmxrm->write(val & 0x3);
  _xmsat->write((val >> XMSAT_LSB) & 0x1);
  _xmfflags->write((val >> XMFFLAGS_LSB) & 0x1F);
  _xmfrm->write((val >> XMFRM_LSB) & 0x7);
  _xmsaten->write((val >> XMSATEN_LSB) & 0x1);
  // Logging is handled by the individual sub-CSRs.
  return false;
}

// =============================================================================
// ameUnit_t — AME architectural state container
// =============================================================================

void ameUnit_t::reset()
{
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

  // 0xCC0: xmisa — matrix ISA feature bitmap
  // TODO: populate feature bitmap per spec 3.2.
  state->add_csr(0xCC0,
    std::make_shared<matrix_csr_t>(p, 0xCC0, /*init*/ 0));

  // 0xCC1: xtlenb = TLEN / 8
  state->add_csr(0xCC1,
    std::make_shared<matrix_csr_t>(p, 0xCC1, /*init*/ TLEN / 8));

  // 0xCC2: xtrlenb = TRLEN / 8
  state->add_csr(0xCC2,
    std::make_shared<matrix_csr_t>(p, 0xCC2, /*init*/ TRLEN / 8));

  // 0xCC3: xalenb = ROWNUM^2 * ELEN / 8
  state->add_csr(0xCC3,
    std::make_shared<matrix_csr_t>(p, 0xCC3, /*init*/ ROWNUM * ROWNUM * ELEN / 8));
}
