/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#ifndef __LIBDEVMAP_H__
#define __LIBDEVMAP_H__

#include <stdint.h>
#include <arpa/inet.h>

#define iomemr32(is_be, val) ((is_be) ? ntohl(val) : val)
#define iomemw32(is_be, val) ((is_be) ? htonl(val) : val)

struct mapping_args {
	char *resource_file;
	uint64_t offset;
	void *vme_extra_args;
};

/* device resource mapped into memory */
struct mapping_desc {
	int fd;
	void *mmap;
	int map_pa_length; /* mapped length is page aligned */
	volatile void *base; /* address of the concerned memory region */
	int is_be; /* tells the device endianess */
	struct mapping_args *args;
};

/**
 * @defgroup device resource mapping
 *@{
 */
extern struct mapping_desc *dev_map(struct mapping_args *map_args,
				    uint32_t map_length);
extern void dev_unmap(struct mapping_desc *dev);
extern struct mapping_args *dev_parse_mapping_args(int argc, char *argv[]);
extern const char * const dev_mapping_help();
extern const char * const dev_get_version();

/** @}*/

#endif //__LIBDEVMAP_H__
