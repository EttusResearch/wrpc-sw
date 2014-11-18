/*
 * Copyright (C) 2012,2014 CERN (www.cern.ch)
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
int sdbfs_dev_create(struct sdbfs *fs)
{
	unsigned int magic;

	/* First, check we have the magic */
	if (fs->data || (fs->flags & SDBFS_F_ZEROBASED))
		magic = *(unsigned int *)(fs->data + fs->entrypoint);
	else
		fs->read(fs, fs->entrypoint, &magic, sizeof(magic));
	if (magic == SDB_MAGIC) {
		/* Uh! If we are little-endian, we must convert */
		if (ntohl(1) != 1)
			fs->flags |= SDBFS_F_CONVERT32;
	} else if (htonl(magic) == SDB_MAGIC) {
		/* ok, don't convert */
	} else {
		return -ENOTDIR;
	}

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
	 * the fs structure itself. Only touches fs->current_record.
	 */
	if (fs->data || (fs->flags & SDBFS_F_ZEROBASED)) {
		if (!(fs->flags & SDBFS_F_CONVERT32))
			return (struct sdb_device *)(fs->data + offset);
		/* copy to local storage for conversion */
		memcpy(&fs->current_record, fs->data + offset,
		       sizeof(fs->current_record));
	} else {
		if (!fs->read)
			return NULL;
		fs->read(fs, offset, &fs->current_record,
			 sizeof(fs->current_record));
	}

	if (fs->flags & SDBFS_F_CONVERT32) {
		uint32_t *p = (void *)&fs->current_record;
		int i;

		for (i = 0; i < sizeof(fs->current_record) / sizeof(*p); i++)
			p[i] = ntohl(p[i]);
	}

	return &fs->current_record;
}

/* Helper for scanning: we enter a new directory, and we must validate */
static struct sdb_device *scan_newdir(struct sdbfs *fs, int depth)
{
	struct sdb_device *dev;
	struct sdb_interconnect *intercon;

	dev = fs->currentp = sdbfs_readentry(fs, fs->this[depth]);
	if (dev->sdb_component.product.record_type != sdb_type_interconnect)
		return NULL;

	intercon = (typeof(intercon))dev;
	if (ntohl(intercon->sdb_magic) != SDB_MAGIC)
		return NULL;

	fs->nleft[depth] = ntohs(intercon->sdb_records) - 1;
	fs->this[depth] += sizeof(*intercon);
	fs->depth = depth;
	return dev;
}

struct sdb_device *sdbfs_scan(struct sdbfs *fs, int newscan)
{
	/*
	 * This returns a pointer to the next sdb record, or the first one.
	 * Subdirectories (bridges) are returned before their contents.
	 * It only uses internal fields.
	 */
	struct sdb_device *dev;
	struct sdb_bridge *bridge;
	int depth, type, newdir = 0; /* check there's the magic */

	if (newscan) {
		fs->base[0] = 0;
		fs->this[0] = fs->entrypoint;
		depth = fs->depth = 0;
		newdir = 1;
		goto scan;
	}

	/* If we already returned a bridge, go inside it (check type) */
	depth = fs->depth;
	type = fs->currentp->sdb_component.product.record_type;

	if (type == sdb_type_bridge && depth + 1 < SDBFS_DEPTH) {
		bridge = (typeof(bridge))fs->currentp;
		fs->this[depth + 1] = fs->base[depth]
			+ ntohll(bridge->sdb_child);
		fs->base[depth + 1] = fs->base[depth]
			+ ntohll(bridge->sdb_component.addr_first);
		depth++;
		newdir++;
	}

scan:
	/* If entering a new directory, verify magic and set nleft */
	if (newdir) {
		dev = scan_newdir(fs, depth);
		if (dev)
			goto out;
		/* Otherwise the directory is not there: no intercon */
		if (!depth)
			return NULL; /* no entries at all */
		depth--;
	}

	while (fs->nleft[depth] == 0) {
		/* No more at this level, "cd .." if possible */
		if (!depth)
			return NULL;
		fs->depth = --depth;
	}

	/* so, read the next entry */
	dev = fs->currentp = sdbfs_readentry(fs, fs->this[depth]);
	fs->this[depth] += sizeof(*dev);
	fs->nleft[depth]--;
out:
	fs->f_offset = fs->base[fs->depth]
		+ htonll(fs->currentp->sdb_component.addr_first);
	return dev;
}

static void __open(struct sdbfs *fs)
{
	fs->f_offset = fs->base[fs->depth]
		+ htonll(fs->currentp->sdb_component.addr_first);
	fs->f_len = htonll(fs->currentp->sdb_component.addr_last)
		+ 1 - htonll(fs->currentp->sdb_component.addr_first);
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

/* to "find" a device, open it, get the current offset, then close */
unsigned long sdbfs_find_name(struct sdbfs *fs, const char *name)
{
	unsigned long offset;
	int ret;

	ret = sdbfs_open_name(fs, name);
	if (ret < 0)
		return (unsigned long)ret;

	offset = fs->f_offset;
	sdbfs_close(fs);
	return offset;
}

unsigned long sdbfs_find_id(struct sdbfs *fs, uint64_t vid, uint32_t did)
{
	unsigned long offset;
	int ret;

	ret = sdbfs_open_id(fs, vid, did);
	if (ret < 0)
		return (unsigned long)ret;

	offset = fs->f_offset;
	sdbfs_close(fs);
	return offset;
}
