#include "mips.h"

.global dump_callstack
.extern dump_callstack_mips

.set noreorder
/* void dump_callstack(unsigned *line) */
dump_callstack:
    move a2, a0
    move a0, ra
    j dump_callstack_mips
    move a1, sp
.set reorder
