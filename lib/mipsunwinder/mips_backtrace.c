/*
**************************************************************************
* Robust call-stack backtracing code for MIPS.
*
* Copyright (C) 2011-2012 by Enea Software AB.
* Released under the GPL as of Sep. 18, 2012.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
**************************************************************************
*/

/* Algorithm:
      Scan forward from the supplied PC until return from function found.
      Check for stack frame restore and return address restore, if found then
       save frame size and return address frame offset.
      Unconditional branches and jumps are followed.
      Conditional branches are followed both ways (taken and not taken).
   Limitations: 
      Does not support floating point conditional branches.
      Does not support functions using frame pointer (where the stack pointer
       is restored from the FP before restoring the RA).
      Does not follow jump register instructions. Possibly this breaks position
       independent code (PIC) as well.
      Does not handle potential forever loops (tracing aborted). I think this
       could be done a little bit smarter, i.e. by re-starting back-tracing at
       some earlier conditional branch.
      Understands end-of-stack for OSE but other OS's will probably need some
       other heuristic for this.
   Tested with n32 ABI, did once work with o32 and might work with n64 barring
    some printf format issues.

   Idea and coding by Ola Liljedahl (ola.liljedahl@gmail.com).
   Thanks Gunnar Eklund and Magnus Gille for comments and enhancements.
*/

#include <stdint.h>
#include <stdbool.h>

#define MAX_STACK_FRAMES 8
#define MAXBRANCHES 128
#define MAXJUMPS 128
#define MAXINSN 2048

typedef unsigned long address_t;

//#define PRINTF(fmt, ...) printf((fmt), __VA_ARGS__)
#define PRINTF(fmt, ...) lcd_putsf(0, (*line)++, (fmt), __VA_ARGS__); lcd_update()
#if 0
#define DBG_PRINTF(...) \
   printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

/* FIXME */
/* Safe read memory, should not crash */
#define READMEM(_addr, _buf, _size) \
        memcpy((void *)(_buf), (void *)(_addr), (_size))

extern unsigned long stackbegin[];
extern unsigned long irqstackend[];

static void __attribute__((used))
dump_callstack_mips (address_t pc, address_t sp, unsigned *line)
{
    unsigned i, j, k;
    address_t ra = 0;

    PRINTF ("no: PC SP (fsz)", 0);
    PRINTF ("0: 0x%lx 0x%lx (?)", pc, sp);


    for (i = 1; i <= MAX_STACK_FRAMES; i++)
    {
        /* Find size of stack frame and position of return address within it */
        bool branch_jump = false;

        int16_t stk_size = 0;   /* Default no stack frame */
        /* npc because of MIPS delayed branches */
        address_t npc = pc + 4;
        /* nnpc because of simulator updating pc/npc late */
        address_t nnpc = pc + 8;

        /* Conditional branch history table */
        address_t braddr[MAXBRANCHES];
        uint32_t numbr = 0;

        /* Unconditional branch/jump history table */
        address_t jpaddr[MAXJUMPS];
        uint32_t numjp = 0;

        for (j = 0; j < MAXINSN; j++)
        {
            branch_jump = false;
            uint32_t insn, ninsn;
            uint32_t opcode, subcode;

            READMEM (pc, &insn, sizeof insn);
            /* Read potential branch/jump delay slot */
            READMEM (npc, &ninsn, sizeof ninsn);
            opcode = (insn >> 26) & 0x3F;
            subcode = (insn >> 16) & 0x1F;

            /* 1: check for unconditional branch */
            if ((opcode == 4 /* beq */  || opcode == 20 /* beql */ ) &&
                ((insn >> 21) & 0x1F) /* s-reg */  == subcode /* t-reg */ )
            {
                int32_t offset = 4 * (int16_t) (insn & 0xffff);
                nnpc = npc + offset;
                /* mark that we need to check branch delay slot */
                branch_jump = true;
            }
            else if ((insn & 0xfc000000) == 0x08000000) /* j target */
            {
                uint32_t offset = (insn & 0x03ffffff) << 2;
                nnpc = (pc & 0xfc000000) | offset;
                /* mark that we need to check branch delay slot */
                branch_jump = true;
            }

            /* 2: check for conditional branch */
            else if ((opcode == 1 && subcode < 4) ||    /* bltz/bgez/bltzl/bgezl */
                     (opcode >= 4 && opcode < 8) ||     /* beq/bne/blez/bgtz */
                     (opcode >= 20 && opcode < 24))     /* beql/bnel/blezl/bgtzl */
            {
                unsigned k;
                int32_t broffset = 4 * (int16_t) (insn & 0xffff);

                DBG_PRINTF ("0x%lx: CondBranch (%d)", pc, broffset);
                for (k = 0; k < numbr; k++)
                {
                    if ((braddr[k] & ~1) == pc)
                    {
                        if ((braddr[k] & 1) == 0)       /* Branch not taken, take it */
                        {
                            nnpc = npc + broffset;
                            braddr[k] |= 1;
                            DBG_PRINTF ("Taking branch");
                        }
                        else    /* Branch already taken */
                        {
                            DBG_PRINTF ("All branch alternatives tested, aborting...");
                            PRINTF ("Potential infinite loop detected", 0);
                            while(1);
                            return;
                        }
                        break;
                    }
                }               /* for branch table entry */

                if (k == numbr) /* Branch not found in table */
                {
                    /* Save PC of branch in branch history table */
                    if (numbr < MAXBRANCHES)
                    {
                        /* OK, save PC in branch history table */
                        braddr[numbr] = pc;     /* LSB = 0 */
                        numbr++;
                        /* FIXME: Skip delay insn for not taken branch likely? */

                        DBG_PRINTF ("Not taking branch");
                    }
                    else
                    {
                        DBG_PRINTF ("Too many CondBranches, aborting...");
                        while(1);
                        return;
                    }
                }
                /* Else branch already present in table */
            }

            /* 3: check for stack frame restore */
            else if ((insn & 0xffff8000) == 0x27bd0000) /* addiu sp,sp,+offset */
            {
                /* Save size of stack frame */
                stk_size = (int16_t) (insn & 0xffff);
                DBG_PRINTF ("0x%lx: Restore SP (fsz %d)", pc, stk_size);
            }

            /* 4: check for stack frame allocate */
            else if ((insn & 0xffff8000) == 0x27bd8000) /* addiu sp,sp,-offset */
            {
                /* Don't save stk_size! */
                DBG_PRINTF ("0x%lx: Save SP (fsz %d)", pc, (int16_t) (insn & 0xffff));
                if (!branch_jump)
                {
                    DBG_PRINTF ("Leaf function");
                    break;
                }
            }

            /* 5: check for restore RA (32-bit) */
            else if ((insn & 0xffff8000) == 0x8fbf0000) /* lw ra,+offset(sp) */
            {
                int16_t ra_offset = (int16_t) (insn & 0xffff);
                READMEM (sp + ra_offset, &ra, sizeof (ra));
                DBG_PRINTF ("0x%lx: Restore RA (offset %d)", pc, ra_offset);
            }

            /* 6: check for return */
            else if (insn == 0x03e00008)        /* jr ra */
            {
                DBG_PRINTF ("0x%lx: Function return", pc);

                /* 6b: Must check delay slot, may contain stack restore insn */
                if ((ninsn & 0xffff8000) == 0x27bd0000) /* addiu sp,sp,+offset */
                {
                    /* Save size of stack frame */
                    stk_size = (int16_t) (ninsn & 0xffff);
                    DBG_PRINTF ("0x%lx: Restore SP (fsz %d)", npc, stk_size);
                }
                break;
            }
            else if ((insn & ~0x03d00000) == 0x00000008)        /* jr s */
            {
                DBG_PRINTF ("0x%lx: Jump register!!!", pc);
                break;
            }

            /* In case of unconditional branch or jump
             * check delay slot, may contain stack restore insn
             */
            if (branch_jump)
            {
                for (k = 0; k < numjp; k++)
                {
                    if (jpaddr[k] == pc)
                    {
                        DBG_PRINTF ("0x%lx: Found PC in jump history", pc);
                        DBG_PRINTF ("Potential infinite loop detected", 0);
                        while(1);
                        return;
                    }
                }

                if (k == numjp) /* Jump not found in table */
                {
                    /* Save PC of jump in jump history table */
                    if (numjp < MAXJUMPS)
                    {
                        jpaddr[k] = pc;
                        numjp++;
                        DBG_PRINTF("0x%lx: Jump/branch", pc);
                    }
                    else
                    {
                        DBG_PRINTF ("Too many jumps, aborting...");
                        while(1);
                        return;
                    }
                }

#if 0
                /* check instruction in branch delay slot of jump/branch */
                if ((ninsn & 0xffff8000) == 0x27bd0000) /* addiu sp,sp,+offset */
                {
                    /* Save size of stack frame */
                    stk_size = (int16_t) (ninsn & 0xffff);
                    DBG_PRINTF ("0x%lx: Restore SP (fsz %d)", npc, stk_size);
                    break;
                }
#endif
            } /* if (check_ninsn) */

            /* Update architecture's pc/npc registers */
            pc = npc;
            npc = nnpc;

            /* Update simulator's nnpc register */
            nnpc = nnpc + 4;
        }                       /* for instruction */

        if (j == MAXINSN)
        {
            PRINTF ("Failed to find end of function", 0);
            while(1);
            return;
        }

        if (ra == 0)
        {
            PRINTF("Invalid return address", 0);
            while(1);
            return;
        }


        /* Unwind to next stack frame */
        sp += stk_size;
        pc = ra;
        ra = 0;

        PRINTF ("%d: 0x%08lx 0x%08lx (%d)", i, pc, sp, stk_size);

        if ((pc & 3) != 0)
        {
            PRINTF ("Found invalid return address", 0);
            while(1);
            return;
        }

        if (sp < (address_t)stackbegin || sp > (address_t)irqstackend)
        {
            DBG_PRINTF("SP out of bounds");
            while(1);
            return;
        }

    }
}

void dump_callstack(unsigned *line)
{
    asm volatile (
                  ".set noreorder\n"
                  "move $a2, $a0\n"
                  "move $a0, $ra\n"
                  "j dump_callstack_mips\n"
                  "move $a1, $sp\n"
                  ".set reorder\n"
    );
}

#if 0
/*
** NOTE! the sp argument is ignored. To make a backtrace, we need not only
** sp, but also a matching pc and return address. Therefore, current sp
** is fetched below.
*/
void
dump_callstack (void)
{
    address_t current_sp = (address_t) __builtin_frame_address (0);

my_current_pc:
    {
        void *current_pc = &&my_current_pc;
        dump_callstack_mips ((address_t) current_pc, current_sp,
                             (address_t) __builtin_return_address (0));
    }

    while (1);
}
#endif
