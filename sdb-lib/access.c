/*
 * Copyright (C) 2012,2013 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */

/* To avoid many #ifdef and associated mess, all headers are included there */
#include "libsdbfs.h"

int sdbfs_fstat(struct sdbfs *fs, struct sdb_device *record_return)
{
	if (!fs->currentp)
		return -ENOENT;
	memcpy(record_return, fs->currentp, sizeof(*record_return));
	return 0;
}

int sdbfs_fread(struct sdbfs *fs, int offset, void *buf, int count)
{
	int ret = count;

	if (!fs->currentp)
		return -ENOENT;
	if (offset < 0)
		offset = fs->read_offset;
	if (offset + count > fs->f_len)
		count = fs->f_len - offset;
	if (fs->data)
		memcpy(buf, fs->data + fs->f_offset + offset, count);
	else
		ret = fs->read(fs, fs->f_offset + offset, buf, count);
	if (ret > 0)
		fs->read_offset = offset + ret;
	return ret;
}

int sdbfs_fwrite(struct sdbfs *fs, int offset, void *buf, int count)
{
	int ret = count;

	if (!fs->currentp)
		return -ENOENT;
	if (offset < 0)
		offset = fs->read_offset;
	if (offset + count > fs->f_len)
		count = fs->f_len - offset;
	if (fs->data)
		memcpy(buf, fs->data + fs->f_offset + offset, count);
	else
		ret = fs->write(fs, fs->f_offset + offset, buf, count);
	if (ret > 0)
		fs->read_offset = offset + ret;
	return ret;
}
