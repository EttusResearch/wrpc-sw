/*
 * Link script for Lattice Mico32. Very loosely based on
 * code contributed by Jon Beniston <jon@beniston.com>
 *
 * Jon's license (BSD-style):
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

OUTPUT_FORMAT("elf32-lm32")
ENTRY(_start)

MEMORY
{
    ram   : ORIGIN = 0x0000, LENGTH = 0x6800
    stats : ORIGIN = 0x6800, LENGTH = 0x0800
    mbox  : ORIGIN = 0x7000, LENGTH = 0x1000
    stack : ORIGIN = 0x8000, LENGTH = 0x8000
}

SECTIONS
{
	.boot : { *(.boot) } > ram

	.text : { *(.text .text.*) } > ram =0

	.rodata : { *(.rodata .rodata.*) } > ram

	.data : {
		*(.data .data.*)
	} > ram

	.bss : {
		_fbss = .;
		*(.bss .bss.*)
		*(COMMON)
		_ebss = .;
	} > ram

	/*
	 * Currently (2014-11) the above goes up to around 0x5a00,
	 * so we can "safely" place statistics at a known address.
	 * If we need to move them later, we have a magic number
	 * in there.
	 */
	.stats : {
		*(.stats)
	} > stats

	/*
	 * The rt IPC buffer is placed at 0x7000. The value is hardwired
	 * in the RPC initialization calls (minipc_server_create() and
	 * wr-switch-sw::wrsw_hal in minipc_client_create()). However,
	 * we'd better have an ELF section here, so that the compier
	 * will complain if we overflow the stats structure above.
	 * We'll be able to move this address ahead when stats move,
	 * by using the magic in stats (there's no magic in minipc)
	 */
	.mbox : {
		*(.mbox)
	} > mbox

	/* End of RAM for checking stack overflows */
	PROVIDE(_endram = ORIGIN(stack));

	/* First location in stack is highest address in RAM (stack area) */
	PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack) - 4);

	/* We have no ppi_static nor fifo_log in wrs builds */
	PROVIDE(ppi_static = 0);
	PROVIDE(fifo_log = 0);
}

