/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */

/* To avoid many #ifdef and associated mess, all headers are included there */
#include "libsdbfs.h"

static struct sdbfs *sdbfs_list;

/* All fields unused by the caller are expected to be zeroed */
int sdbfs_dev_create(struct sdbfs *fs, int verbose)
{
	unsigned int magic;

	/* First, check we have the magic */
	if (fs->data)
		magic = *(unsigned int *)(fs->data + fs->entrypoint);
	else
		fs->read(fs, fs->entrypoint, &magic, sizeof(magic));
	if (htonl(magic) != SDB_MAGIC)
		return -ENOTDIR;


	if (verbose)
		fs->flags |= SDBFS_F_VERBOSE;

	fs->next = sdbfs_list;
	sdbfs_list = fs;

	return 0;
}

int sdbfs_dev_destroy(struct sdbfs *fs)
{
	struct sdbfs **p;

	for (p = &sdbfs_list; *p && *p != fs; p = &(*p)->next)
		;
	if (!*p)
		return -ENOENT;
	*p = fs->next;
	return 0;
}

struct sdbfs *sdbfs_dev_find(const char *name)
{
	struct sdbfs *l;

	for (l = sdbfs_list; l && strcmp(l->name, name); l = l->next)
		;
	if (!l)
		return NULL;
	return l;
}

/*
 * To open by name or by ID we need to scan the tree. The scan
 * function is also exported in order for "sdb-ls" to use it
 */

static struct sdb_device *sdbfs_readentry(struct sdbfs *fs,
					  unsigned long offset)
{
	/*
	 * This function reads an entry from a known good offset. It
	 * returns the pointer to the entry, which may be stored in
	 * the fs structure itself. Only touches fs->current_record
	 */
	if (fs->data)
		return (struct sdb_device *)(fs->data + offset);
	if (!fs->read)
		return NULL;
	fs->read(fs, offset, &fs->current_record, sizeof(fs->current_record));
	return &fs->current_record;
}

struct sdb_device *sdbfs_scan(struct sdbfs *fs, int newscan)
{
	/*
	 * This returns a pointer to the next sdb record, or a new one.
	 * Subdirectories are not supported. Uses all internal fields
	 */
	struct sdb_device *ret;
	struct sdb_interconnect *i;

	if (newscan) {
		fs->f_offset = fs->entrypoint;
	} else {
		fs->f_offset += sizeof(struct sdb_device);
		if (!fs->nleft)
			return NULL;
	}
	ret = sdbfs_readentry(fs, fs->f_offset);
	if (newscan) {
		i = (typeof(i))ret;
		fs->nleft = ntohs(i->sdb_records) - 1;
	} else {
		fs->nleft--;
	}
	return ret;
}

static void __open(struct sdbfs *fs)
{
	fs->f_offset = htonll(fs->currentp->sdb_component.addr_first);
	fs->f_len = htonll(fs->currentp->sdb_component.addr_last)
		+ 1 - fs->f_offset;
	fs->read_offset = 0;
}

int sdbfs_open_name(struct sdbfs *fs, const char *name)
{
	struct sdb_device *d;
	int len = strlen(name);

	if (len > 19)
		return -ENOENT;
	sdbfs_scan(fs, 1); /* new scan: get the interconnect and igore it */
	while ( (d = sdbfs_scan(fs, 0)) != NULL) {
		if (strncmp(name, d->sdb_component.product.name, len))
			continue;
		if (len < 19 && d->sdb_component.product.name[len] != ' ')
			continue;
		fs->currentp = d;
		__open(fs);
		return 0;
	}
	return -ENOENT;
}

int sdbfs_open_id(struct sdbfs *fs, uint64_t vid, uint32_t did)
{
	struct sdb_device *d;

	sdbfs_scan(fs, 1); /* new scan: get the interconnect and igore it */
	while ( (d = sdbfs_scan(fs, 0)) != NULL) {
		if (vid != d->sdb_component.product.vendor_id)
			continue;
		if (did != d->sdb_component.product.device_id)
			continue;
		fs->currentp = d;
		__open(fs);
		return 0;
	}
	return -ENOENT;
}

int sdbfs_close(struct sdbfs *fs)
{
	fs->currentp = NULL;
	return 0;
}

