/*
 * Copyright (c) 2009 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cnc.h>
#include <stdio.h>

cnc_vec_t C;
cnc_pos_t radius;
cnc_pos_t depth;
int numHoles = 0;

int
main(int argc, char *argv)
{
	int ch, i, centerOk = 0, radiusOk = 0, depthOk = 0;

	while ((ch = getopt(argc, argv, "c:r:n:d:t:?h")) != -1) {
		switch (ch) {
		case 'c':
			if (CNC_ParseVector(optarg, &C) == -1) {
				goto fail;
			}
			centerOk = 1;
			break;
		case 'r':
			if (CNC_ParseDistance(optarg, &radius) == -1) {
				goto fail;
			}
			radiusOk = 1;
			break;
		case 'n':
			numHoles = atoi(optarg);
			break;
		case 'd':
			if (CNC_ParseDistance(optarg, &depth) == -1) {
				goto fail;
			}
			depthOk = 1;
			break;
		case 't':
			if (CNC_ParseTool(optarg, &tool) == -1) {
				goto fail;
			}
			break;
		default:
			printf("Usage: %s [-c center] [-r radius] [-n num-holes] [-d depth] [-t tool]\n", __progname);
			return (1);
		}
	}
	if (!centerOk || !radiusOk || !depthOk) {
		fprintf(stderr, "Missing center, radius or depth\n");
		return (1);
	}

	printf("G00 %f.%f.%f\n", C.v[0], C.v[1], C.v[2]);

	return (0);
fail:
	fprintf(stderr, "%s\n", CNC_GetError());
	return (1);
}

