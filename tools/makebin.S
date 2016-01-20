/*
 * "objcopy -O binary" they say...
 * how can I say "spit the default ELF object format"?
 */
.data
.global dump_info
dump_info:
.incbin "dump-info-host.bin"

