/**
 * Author: Federico Vaga <federico.vaga@cern.ch>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "libdevmap.h"

static const char * const libdevmap_version_s = "libdevmap version: "
	__GIT_VER__ ", by " __GIT_USR__ " " __TIME__ " " __DATE__;

#ifdef SUPPORT_CERN_VMEBRIDGE
#include <libvmebus.h>

struct vmebridge_map_args {
	/**
 	 * VME memory map arguments
 	 */
	uint32_t data_width; /**< default register size in bytes */
	uint32_t am; /**< VME address modifier to use */
	uint64_t addr; /**< physical base address */
};
#endif

#ifdef SUPPORT_CERN_VMEBRIDGE
static struct mapping_desc *cern_vmebridge_dev_map(
		struct mapping_args *map_args, int mapping_length)
{
	struct mapping_desc *desc;
	struct vme_mapping *vme_mapping;
	volatile void *ptr_map;
	struct vmebridge_map_args *vme_args =
		(struct vmebridge_map_args *)map_args->vme_extra_args;

	vme_mapping = malloc(sizeof(struct vme_mapping));
	if (!vme_mapping)
		return NULL;

	/* Prepare mmap description */
	memset(vme_mapping, 0, sizeof(struct vme_mapping));
	vme_mapping->am = vme_args->am;
	vme_mapping->data_width = vme_args->data_width;
	vme_mapping->sizel = map_args->offset + mapping_length;
	vme_mapping->vme_addrl = vme_args->addr;

	/* Do mmap */
	ptr_map = vme_map(vme_mapping, 1);
	if (!ptr_map)
		goto out_alloc;

	desc = malloc(sizeof(struct mapping_desc));
	if (!desc)
		goto out_map;
	desc->base = ptr_map + map_args->offset;
	desc->mmap = (void *)vme_mapping;

	return desc;

out_map:
	vme_unmap(vme_mapping, 1);
out_alloc:
	free(vme_mapping);

	return NULL;
}
#endif

/**
 * It maps into memory the given device resource
 * @parma[in] resource_path path to the resource file which can be mmapped
 * @param[in] offset virtual address of the device memory region of interest
 *            within the resource
 *
 * @todo make the open more user friendly
 *
 * @return NULL on error and errno is approproately set.
 *         On success a mapping descriptor.
 */
struct mapping_desc *dev_map(struct mapping_args *map_args, uint32_t map_length)
{
	struct mapping_desc *desc;
	off_t pa_offset /*page aligned offset */;

#ifdef SUPPORT_CERN_VMEBRIDGE
	if (map_args->vme_extra_args) { //map device through CERN VME_BRIDGE
		desc =  cern_vmebridge_dev_map(map_args, map_length);
		if (!desc) {
			return NULL;
		}
		desc->is_be = 1; /* VME Bus is big endian */
		desc->args = map_args;
		return desc;
	}
#endif
	desc = malloc(sizeof(struct mapping_desc));
	if (!desc)
		goto out_alloc;

	desc->fd = open(map_args->resource_file, O_RDWR | O_SYNC);
	if (desc->fd <= 0)
		goto out_open;

	/* offset is page aligned */
	pa_offset = map_args->offset & ~(getpagesize() - 1);
	desc->map_pa_length = map_length + map_args->offset - pa_offset;
	desc->mmap = mmap(NULL, desc->map_pa_length,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED, desc->fd, pa_offset);
	if ((long)desc->mmap == -1)
		goto out_mmap;

	desc->base = desc->mmap + map_args->offset - pa_offset;
 	/*
	 * @todo for future VME devices handled via a resource file,
	 * exposed in standard place (/sys/bus/vme/device/xxx/resource-file)
	 * the bus type (pci or vme) should be checked to set properly
	 * the endianess (could be get from the path's resource)
	 */
	desc->is_be = 0; /* default set to little endian */
	return desc;

out_mmap:
	close(desc->fd);
out_open:
	free(desc);
out_alloc:
	return NULL;
}

/**
 * It releases the resources allocated by dev_map(). The mapping will
 * not be available anymore and the descriptor will be invalidated
 * @param[in,out] descriptor token from dev_map()
 */
void dev_unmap(struct mapping_desc *desc)
{
#ifdef SUPPORT_CERN_VMEBRIDGE
	if (!desc->fd) { /* cern vmebridge resource */
		vme_unmap((struct vme_mapping *)desc->mmap, 1);
		free(desc->args->vme_extra_args);
		free(desc->args);
		free(desc);
		return;
	}
#endif
	munmap(desc->mmap, desc->map_pa_length);
	close(desc->fd);
	free(desc->args);
	free(desc);
}


#ifdef SUPPORT_CERN_VMEBRIDGE

#define CERN_VMEBRIDGE_REQUIRED_ARG_NB 2
#define CERN_VMEBRIDGE 1

static struct option long_options[] = {
	{"cern-vmebridge", no_argument, 0, CERN_VMEBRIDGE},
	{"address", required_argument, 0, 'a'},
	{"offset", required_argument, 0, 'o'},
	{"data-width", required_argument, 0, 'w'},
	{"am", required_argument, 0, 'm'},
	{0, 0, 0, 0}
};

/*
 * looks for vme-compat option
 * returns 1 if vme-compat is matched, otherwhise 0.
 */
static int cern_vmebridge_match(int argc, char *argv[])
{
	int i;

	for (i = 0; i < argc; ++i) {
		if (!strcmp(argv[i], "--cern-vmebridge")) {
			return 1;
		}
	}
	return 0;
}

/*
 * Parse mandatory arguments to mmap VME physical address space
 * return 0 on sucess, -1 in case of error
 */
static int cern_vmebridge_parse_args(int argc, char *argv[],
			struct mapping_args *map_args)
{
	struct vmebridge_map_args *vme_args;
	int ret, arg_count = 0, c, option_index = 0;

	vme_args = calloc(1, sizeof(struct vmebridge_map_args));
	if (!vme_args)
		return -1;
	map_args->vme_extra_args = vme_args;

	/* set default values in case they are not provided*/
	vme_args->data_width = 32;
	vme_args->am = 0x39;

	while ((c = getopt_long(argc, argv, "w:o:m:a:CERN_VMEBRIDGE", long_options,
				&option_index)) != -1) {
		switch(c) {
		case CERN_VMEBRIDGE:
			// nothing to do
			break;
		case 'w': /* optional arg */
			ret = sscanf(optarg, "%u",
				     &vme_args->data_width);
			if (ret != 1)
				return -1;
			if ( !(vme_args->data_width == 8 ||
			       vme_args->data_width == 16 ||
			       vme_args->data_width == 32 ||
			       vme_args->data_width == 64) )
				return -1;
			break;
		case 'o': /* mandatory arg */
			ret = sscanf(optarg, "0x%"SCNx64, &map_args->offset);
			if (ret != 1)
				return -1;
			++arg_count;
			break;
		case 'm': /* optional arg */
			ret = sscanf(optarg, "0x%x",
				     &vme_args->am);
			if (ret != 1)
				return -1;
			break;
		case 'a': /* mandatory arg */
			ret = sscanf(optarg, "0x%"SCNx64,
				     &vme_args->addr);
			if (ret != 1)
				return -1;
			++arg_count;
			break;
		case '?':
			/* ignore unknown arguments */
			break;
		}
	}
	return (arg_count == CERN_VMEBRIDGE_REQUIRED_ARG_NB) ? 0 : -1;
}
#endif

#define REQUIRED_ARG_NB 2
struct mapping_args *dev_parse_mapping_args(int argc, char *argv[])
{
	struct mapping_args *map_args;
	char c;
	int ret, arg_count = 0;

	map_args = calloc(1, sizeof(struct mapping_args));
	if (!map_args)
		return NULL;

	/*
	 * getopt variable: cancel error message printing because we look for
	 * only mapping options among other options
	 */
	opterr = 0;
#ifdef SUPPORT_CERN_VMEBRIDGE
	if (cern_vmebridge_match(argc, argv)) {
		ret = cern_vmebridge_parse_args(argc, argv, map_args);
		if (ret < 0) {
			goto out;
		}
		/*
	 	 * getopts variable: reset argument index in case application
		 * needs to parse arguments lokkink for specific args.
	 	 */
		optind = 1;
		return map_args;
	}
#endif
	while ((c = getopt (argc, argv, "o:f:")) != -1)
	{
		switch (c)
		{
		case 'o':
			ret = sscanf(optarg, "0x%x",
				     (unsigned int *)&map_args->offset);
			if (ret != 1) {
				goto out;
			}
			++arg_count;
			break;
		case 'f':
			map_args->resource_file = optarg;
			++arg_count;
			break;
		case '?':
			/* ignore unknown arguments */
			break;
		}
	}

	if (arg_count != REQUIRED_ARG_NB) {
		goto out;
	}

	/*
	 * getopts variable: reset argument index in case application needs to
	 * parse arguments lokkink for specific args.
	 */
	optind = 1;
	return map_args;

out:
	free(map_args);
	return NULL;
}

const char * const dev_mapping_help()
{
	static char help_msg[] =
		"Device mapping options: \n"
		"\t-f <file resource path> -o 0x<address offset> \n"
#ifdef SUPPORT_CERN_VMEBRIDGE
		"Device mapping options for CERN vmebus driver: \n"
		"\t--cern-vmebridge -a 0x<VME base address> \n"
		"\t-o 0x<address offset> [-w <data-width[8,16,32] default=32>\n"
		"\t-m 0x<address modifier default=0x39>]\n"
#endif
		;

	return help_msg;
}

const char * const dev_get_version()
{
	return libdevmap_version_s;
}
