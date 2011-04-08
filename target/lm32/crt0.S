/*
 * Lattice Mico32 C startup code.
 * Contributed by Jon Beniston <jon@beniston.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

        .section    .boot, "ax", @progbits

        /* Program starts here */
        
        .global     _start  
_start:

        /* Setup stack and global pointer */
        mvhi    sp, hi(_fstack)
        ori     sp, sp, lo(_fstack)
        mvhi    gp, hi(_gp)
        ori     gp, gp, lo(_gp)

        bi _crt0_enter
       .org 0xc0

_interrupt_handler:
 sw (sp+0),ra
 calli _save_all
 mvi r1,2
 calli _irq_entry
 bi _restore_all_and_return
nop
nop
 nop
                
                


_crt0_enter:
                                
        /* Clear BSS */
        mvhi    r1, hi(_fbss)
        ori     r1, r1, lo(_fbss)
        mvi     r2, 0
        mvhi    r3, hi(_ebss)
        ori     r3, r3, lo(_ebss)
        sub     r3, r3, r1

        calli   memset
        
        mvi r1, 0xffffffff
        wcsr im, r1

        mvi r1, 0x1
        wcsr ie, r1
        
                                                  
        /* Call main program */
        mvi     r1, 0
        mvi     r2, 0
        mvi     r3, 0
        calli   main

_save_all:
addi sp,sp,-128
 sw (sp+4),r1
 sw (sp+8),r2
 sw (sp+12),r3
  sw (sp+16),r4
 sw (sp+20),r5
  sw (sp+24),r6
 sw (sp+28),r7
sw (sp+32),r8
sw (sp+36),r9
 sw (sp+40),r10
 sw (sp+44),r11
 sw (sp+48),r12
sw (sp+52),r13
   sw (sp+56),r14
 sw (sp+60),r15
  sw (sp+64),r16
  sw (sp+68),r17
  sw (sp+72),r18
 sw (sp+76),r19
 sw (sp+80),r20
 sw (sp+84),r21
   sw (sp+88),r22
 sw (sp+92),r23
  sw (sp+96),r24
 sw (sp+100),r25
 sw (sp+104),gp
  sw (sp+108),fp
  sw (sp+120),ea
 sw (sp+124),ba
   lw r1,(sp+128)
 sw (sp+116),r1
 mv r1,sp
 addi r1,r1,128
 sw (sp+112),r1
   ret

_restore_all_and_return:
lw r1,(sp+4)
  lw r2,(sp+8)
lw r3,(sp+12)
 lw r4,(sp+16)
 lw r5,(sp+20)
  lw r6,(sp+24)
  lw r7,(sp+28)
  lw r8,(sp+32)
 lw r9,(sp+36)
 lw r10,(sp+40)
lw r11,(sp+44)
 lw r12,(sp+48)
  lw r13,(sp+52)
 lw r14,(sp+56)
  lw r15,(sp+60)
lw r16,(sp+64)
lw r17,(sp+68)
  lw r18,(sp+72)
lw r19,(sp+76)
  lw r20,(sp+80)
 lw r21,(sp+84)
  lw r22,(sp+88)
 lw r23,(sp+92)
  lw r24,(sp+96)
lw r25,(sp+100)
lw gp,(sp+104)
 lw fp,(sp+108)
  lw ra,(sp+116)
lw ea,(sp+120)
 lw ba,(sp+124)
 lw sp,(sp+112)
 eret

        
