#include "asm.h"
#include "atom_if.h"
#include <pico/platform/sections.h>

#include "asm/test.h"

void asm_init(void)
{
  eb_set_perm(0x0A00, EB_PERM_READ_ONLY, 0xFE);
  eb_set_perm(0x0AFE, EB_PERM_READ_WRITE, 2);
  eb_set_chars(0x0A00, _6502_prog, _6502_prog_len);
}
