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
    ram : ORIGIN = 0x00000000, LENGTH = 0x7000
    mbox : ORIGIN = 0x7000, LENGTH = 0x1000
}

SECTIONS
{
	.boot : { *(.boot) } > ram

	.text : { *(.text .text.*) } > ram =0

	.rodata : { *(.rodata .rodata.*) } > ram

	.data : {
		*(.data .data.*)
		_gp = ALIGN(16) + 0x7ff0; /* FIXME: what is this? */
	} > ram

	.bss : {
		_fbss = .;
		*(.bss .bss.*)
		*(COMMON)
		_ebss = .;
	} > ram

	.mbox : {
		. = ALIGN(4);
		_fmbox = .;
		*(.mbox)
	} > mbox

	/* First location in stack is highest address in RAM */
	PROVIDE(_fstack = ORIGIN(ram) + LENGTH(ram) - 4);
}

/* We need to provide mprintf to ptp-noposix object files, if missing */
PROVIDE(mprintf = pp_printf);
