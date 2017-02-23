/*
 * This header defines structures for dumping other structures from
 * binary files. Every arch has a different endianness and alignment/size,
 * so we can't just use the structures from the host compiler. It used to
 * work for lm32/i386, but it fails with x86-64, so let's change attitude.
 */

#include <stdint.h>

/*
 * To ease copying from header files, allow int, char and other known types.
 * Please add more type as more structures are included here
 */
enum dump_type {
	dump_type_char, /* for zero-terminated strings */
	dump_type_bina, /* for binary stull in MAC format */
	/* normal types follow */
	dump_type_uint8_t,
	dump_type_uint32_t,
	dump_type_uint16_t,
	dump_type_int,
	dump_type_unsigned_long,
	dump_type_unsigned_char,
	dump_type_unsigned_short,
	dump_type_double,
	dump_type_float,
	dump_type_pointer,
	/* and strange ones, from IEEE */
	dump_type_UInteger64,
	dump_type_Integer64,
	dump_type_UInteger32,
	dump_type_Integer32,
	dump_type_UInteger16,
	dump_type_Integer16,
	dump_type_UInteger8,
	dump_type_Integer8,
	dump_type_Enumeration8,
	dump_type_Boolean,
	dump_type_ClockIdentity,
	dump_type_PortIdentity,
	dump_type_ClockQuality,
	/* and this is ours */
	dump_type_pp_time,
	dump_type_ip_address,
};

/* because of the sizeof later on, we need these typedefs */
typedef void *         pointer;
typedef struct pp_time pp_time;
typedef unsigned long  unsigned_long;
typedef unsigned char  unsigned_char;
typedef unsigned short unsigned_short;
typedef struct {unsigned char addr[4];} ip_address;

/*
 * This is generated with the target compiler, and then linked
 * by the host compiler, so size and alignment must be safe. Then, the
 * first structure in each group has the endian flag and the structure name.
 * Following ones have zero in endian flag and field name.
 */
#define DUMP_ENDIAN_FLAG 0x12345678
struct dump_info {
	uint32_t endian_flag;
	uint32_t type;
	uint32_t offset;
	uint32_t size;
	char name[48];
};
extern struct dump_info dump_info[]; /* wrpc-sw/dump-info.c -> bina -> elf */

#define DUMP_HEADER(_struct) {			\
	.endian_flag = DUMP_ENDIAN_FLAG,	\
	.name = _struct,			\
}

/* The macros below rely on DUMP_STRUCT that must be externally defined */
#define DUMP_FIELD(_type, _fname) {		\
	.endian_flag = 0,			\
	.type = dump_type_ ## _type,		\
	.offset = offsetof(DUMP_STRUCT, _fname),\
	.size = sizeof(_type),			\
	.name = #_fname,			\
}
#define DUMP_FIELD_SIZE(_type, _fname, _size) { \
	.endian_flag = 0,			\
	.type = dump_type_ ## _type,		\
	.offset = offsetof(DUMP_STRUCT, _fname),\
	.size = _size,				\
	.name = #_fname,			\
}

