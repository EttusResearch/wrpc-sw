/*
 * This is used to generate wrc.o from all objects. We need to use
 * --gc-sections because sockitowm include a lot of stuff we don't run,
 * but at the same time we need to preserve all commands. So use KEEP()
 */
SECTIONS
{
	.cmd : {
		__cmd_begin = .;
		KEEP(*(.cmd))
		__cmd_end = .;
	}
}
