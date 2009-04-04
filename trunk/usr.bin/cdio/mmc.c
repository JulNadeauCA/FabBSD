/* $OpenBSD: mmc.c,v 1.23 2008/07/23 21:33:32 av Exp $ */

/*
 * Copyright (c) 2006 Michael Coulter <mjc@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/limits.h>
#include <sys/types.h>
#include <sys/scsiio.h>
#include <scsi/cd.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "extern.h"

extern int fd;
extern int mediacap;
extern char *cdname;

#define SCSI_GET_CONFIGURATION		0x46
#define SCSI_SET_SPEED			0xbb

#define MMC_FEATURE_CDRW_CAV		0x27
#define MMC_FEATURE_CD_TAO		0x2d
#define MMC_FEATURE_CDRW_WRITE		0x37

int
get_media_capabilities(int *cap)
{
	scsireq_t scr;
	u_char buf[4096];
	int error;
	u_int32_t i, dsz;
	u_int16_t feature;

	*cap = 0;
	memset(buf, 0, sizeof(buf));
	memset(&scr, 0, sizeof(scr));

	scr.cmd[0] = SCSI_GET_CONFIGURATION;
	scr.cmd[1] = 1;	/* enumerate only "current" features */
	*(u_int16_t *)(scr.cmd + 7) = betoh16(sizeof(buf));

	scr.flags = SCCMD_ESCAPE | SCCMD_READ;
	scr.databuf = buf;
	scr.datalen = sizeof(buf);
	scr.cmdlen = 10;
	scr.timeout = 120000;
	scr.senselen = SENSEBUFLEN;

	error = ioctl(fd, SCIOCCOMMAND, &scr);
	if (error == -1 || scr.retsts != 0)
		return (-1);
	if (scr.datalen_used < 8)
		return (-1);	/* can't get header */

	dsz = betoh32(*(u_int32_t *)buf);
	if (dsz > scr.datalen_used - 4)
		dsz = scr.datalen_used - 4;

	dsz += 4;	/* total size of bufer for all features */
	i = 8;
	while (i <= dsz - 4) {
		if (dsz - i < (u_int32_t)buf[i + 3] + 4)
			break;	/* partial feature descriptor */
		feature = betoh16(*(u_int16_t *)(buf + i));

		if (feature == MMC_FEATURE_CDRW_CAV)
			*cap |= MEDIACAP_CDRW_CAV;
		else if (feature == MMC_FEATURE_CD_TAO)
			*cap |= MEDIACAP_TAO;
		else if (feature == MMC_FEATURE_CDRW_WRITE)
			*cap |= MEDIACAP_CDRW_WRITE;

		i += 4 + buf[i + 3];
	}

	return (0);
}

int
set_speed(int wspeed)
{
	scsireq_t scr;
	int r;

	memset(&scr, 0, sizeof(scr));
	scr.cmd[0] = SCSI_SET_SPEED;
	scr.cmd[1] = (mediacap & MEDIACAP_CDRW_CAV) != 0;
	*(u_int16_t *)(scr.cmd + 2) = htobe16(DRIVE_SPEED_OPTIMAL);
	*(u_int16_t *)(scr.cmd + 4) = htobe16(wspeed);

	scr.cmdlen = 12;
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
blank(void)
{
	struct scsi_blank *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_blank *)scr.cmd;
	scb->opcode = BLANK;
	scb->byte2 |= BLANK_MINIMAL;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
unit_ready(void)
{
	struct scsi_test_unit_ready *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_test_unit_ready *)scr.cmd;
	scb->opcode = TEST_UNIT_READY;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
synchronize_cache(void)
{
	struct scsi_synchronize_cache *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_synchronize_cache *)scr.cmd;
	scb->opcode = SYNCHRONIZE_CACHE;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
close_session(void)
{
	struct scsi_close_track *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_close_track *)scr.cmd;
	scb->opcode = CLOSE_TRACK;
	scb->closefunc = CT_CLOSE_SESS;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 0;
	scr.timeout = 120000;
	scr.flags = SCCMD_ESCAPE;
	scr.senselen = SENSEBUFLEN;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
writetao(struct track_head *thp)
{
	u_char modebuf[70], bdlen;
	struct track_info *tr;
	int r, track = 0;

	if ((r = mode_sense_write(modebuf)) != SCCMD_OK) {
		warnx("mode sense failed: %d", r);
		return (r);
	}
	bdlen = modebuf[7];
	modebuf[2+8+bdlen] |= 0x40; /* Buffer Underrun Free Enable */
	modebuf[2+8+bdlen] |= 0x01; /* change write type to TAO */

	SLIST_FOREACH(tr, thp, track_list) {
		track++;
		switch (tr->type) {
		case 'd':
			modebuf[3+8+bdlen] = 0x04; /* track mode = data */
			modebuf[4+8+bdlen] = 0x08; /* 2048 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			break;
		case 'a':
			modebuf[3+8+bdlen] = 0x00; /* track mode = audio */
			modebuf[4+8+bdlen] = 0x00; /* 2352 block track mode */
			modebuf[8+8+bdlen] = 0x00; /* turn off XA */
			break;
		default:
			warn("impossible tracktype detected");
			break;
		}
		while (unit_ready() != SCCMD_OK)
			continue;
		if ((r = mode_select_write(modebuf)) != SCCMD_OK) {
			warnx("mode select failed: %d", r);
			return (r);
		}

		set_speed(tr->speed);
		writetrack(tr, track);
		synchronize_cache();
	}
	fprintf(stderr, "Closing session.\n");
	close_session();
	return (0);
}

int
writetrack(struct track_info *tr, int track)
{
	struct timeval tv, otv, atv;
	u_char databuf[65536], nblk;
	u_int end_lba, lba, tmp;
	scsireq_t scr;
	int r;

	nblk = 65535/tr->blklen;
	bzero(&scr, sizeof(scr));
	scr.timeout = 300000;
	scr.cmd[0] = WRITE_BIG;
	scr.cmd[1] = 0x00;
	scr.cmd[8] = nblk; /* Transfer length in blocks (LSB) */
	scr.cmdlen = 10;
	scr.databuf = (caddr_t)databuf;
	scr.datalen = nblk * tr->blklen;
	scr.senselen = SENSEBUFLEN;
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;

	timerclear(&otv);
	atv.tv_sec = 1;
	atv.tv_usec = 0;

	if (get_nwa(&lba) != SCCMD_OK) {
		warnx("cannot get next writable address");
		return (-1);
	}
	tmp = htobe32(lba); /* update lba in cdb */
	memcpy(&scr.cmd[2], &tmp, sizeof(tmp));

	if (tr->sz / tr->blklen + 1 > UINT_MAX || tr->sz < tr->blklen) {
		warnx("file %s has invalid size", tr->file);
		return (-1);
	}
	if (tr->sz % tr->blklen) {
		warnx("file %s is not multiple of block length %d",
		    tr->file, tr->blklen);
		end_lba = tr->sz / tr->blklen + lba + 1;
	} else {
		end_lba = tr->sz / tr->blklen + lba;
	}
	if (lseek(tr->fd, tr->off, SEEK_SET) == -1)
		err(1, "seek failed for file %s", tr->file);
	while (lba < end_lba && nblk != 0) {
		while (lba + nblk <= end_lba) {
			read(tr->fd, databuf, nblk * tr->blklen);
			scr.cmd[8] = nblk;
			scr.datalen = nblk * tr->blklen;
again:
			r = ioctl(fd, SCIOCCOMMAND, &scr);
			if (r != 0) {
				printf("\r%60s", "");
				warn("ioctl failed while attempting to write");
				return (-1);
			}
			if (scr.retsts == SCCMD_SENSE && scr.sense[2] == 0x2) {
				usleep(1000);
				goto again;
			}
			if (scr.retsts != SCCMD_OK) {
				printf("\r%60s", "");
				warnx("ioctl returned bad status while "
				    "attempting to write: %d",
				    scr.retsts);
				return (r);
			}
			lba += nblk;

			gettimeofday(&tv, NULL);
			if (lba == end_lba || timercmp(&tv, &otv, >)) {
				fprintf(stderr,
				    "\rtrack %02d '%c' %08u/%08u %3d%%",
				    track, tr->type,
				    lba, end_lba, 100 * lba / end_lba);
				timeradd(&tv, &atv, &otv);
			}
			tmp = htobe32(lba); /* update lba in cdb */
			memcpy(&scr.cmd[2], &tmp, sizeof(tmp));
		}
		nblk--;
	}
	printf("\n");
	close(tr->fd);
	return (0);
}

int
mode_sense_write(unsigned char buf[])
{
	struct scsi_mode_sense_big *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_mode_sense_big *)scr.cmd;
	scb->opcode = MODE_SENSE_BIG;
	/* XXX: need to set disable block descriptors and check SCSI drive */
	scb->page = WRITE_PARAM_PAGE;
	scb->length[1] = 0x46; /* 16 for the header + size from pg. 89 mmc-r10a.pdf */
	scr.cmdlen = sizeof(*scb);
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.datalen= 0x46;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
mode_select_write(unsigned char buf[])
{
	struct scsi_mode_select_big *scb;
	scsireq_t scr;
	int r;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_mode_select_big *)scr.cmd;
	scb->opcode = MODE_SELECT_BIG;

	/*
	 * INF-8020 says bit 4 in byte 2 is '1'
	 * INF-8090 refers to it as 'PF(1)' then doesn't
	 * describe it.
	 */
	scb->byte2 = 0x10;
	scb->length[1] = 2 + buf[1] + 256 * buf[0];
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmdlen = sizeof(*scb);
	scr.datalen = 2 + buf[1] + 256 * buf[0];
	scr.flags = SCCMD_ESCAPE|SCCMD_WRITE;
	scr.databuf = (caddr_t)buf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	return (r == 0 ? scr.retsts : -1);
}

int
get_disc_size(off_t *availblk)
{
	u_char databuf[28];
	struct scsi_read_track_info *scb;
	scsireq_t scr;
	int r, tmp;

	bzero(&scr, sizeof(scr));
	scb = (struct scsi_read_track_info *)scr.cmd;
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scb->opcode = READ_TRACK_INFO;
	scb->addrtype = RTI_TRACK;
	scb->addr[3] = 1;
	scb->data_len[1] = 0x1c;
	scr.cmdlen = sizeof(*scb);
	scr.datalen= 0x1c;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)databuf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	memcpy(&tmp, &databuf[16], sizeof(tmp));
	*availblk = betoh32(tmp);
	return (r == 0 ? scr.retsts : -1);
}

int
get_nwa(int *nwa)
{
	u_char databuf[28];
	scsireq_t scr;
	int r, tmp;

	bzero(&scr, sizeof(scr));
	scr.timeout = 4000;
	scr.senselen = SENSEBUFLEN;
	scr.cmd[0] = 0x52; /* READ TRACK INFO */
	scr.cmd[1] = 0x01;
	scr.cmd[5] = 0xff; /* Invisible Track */
	scr.cmd[7] = 0x00;
	scr.cmd[8] = 0x1c;
	scr.cmdlen = 10;
	scr.datalen= 0x1c;
	scr.flags = SCCMD_ESCAPE|SCCMD_READ;
	scr.databuf = (caddr_t)databuf;

	r = ioctl(fd, SCIOCCOMMAND, &scr);
	memcpy(&tmp, &databuf[12], sizeof(tmp));
	*nwa = betoh32(tmp);
	return (r == 0 ? scr.retsts : -1);
}
