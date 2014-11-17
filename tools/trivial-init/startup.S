.section    .boot, "ax", @progbits
.global _start
_start:
.global _reset_handler
.type   _reset_handler, @function
_reset_handler:

		xor r0, r0, r0
    wcsr    IE, r0
    wcsr    IM, r0
    mvhi    r1, hi(_reset_handler)
    ori     r1, r1, lo(_reset_handler)
    wcsr    EBA, r1
    xor     r0, r0, r0
    mvhi    sp, hi(_fstack)
    ori     sp, sp, lo(_fstack)
		calli 	main
