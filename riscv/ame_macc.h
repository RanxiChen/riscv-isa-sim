// See LICENSE for license details.

#ifndef _RISCV_AME_MACC_H
#define _RISCV_AME_MACC_H

#include "decode.h"

class processor_t;

void execute_mfmacc_d(processor_t* p, insn_t insn);

#endif
