/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Edward Tomasz Napierala <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University
 * of Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part
 * of the DARPA SSITH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

 /*
  * The purpose of this thing is to replay compact memory traces,
  * generated using trace2cmt.
  *
  * Use the -v flag to see what it's doing.
  */

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	MAXTAGS		100000000

#if 1
#define	MALLOC(X)	malloc(X)
#define	REALLOC(X, Y)	realloc(X, Y)
#define	FREE(X)		free(X)
#else
static uintptr_t ptr = 0xffff;

void *
MALLOC(size_t size)
{
	void *tmp;

	tmp = (void *)ptr;
	ptr += size;
	return (tmp);
}

#define	REALLOC(X, Y)	MALLOC(Y)
#define	FREE(X)		0
#endif

static bool vflag = false;
static intptr_t tag2ptr[MAXTAGS];

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-v] input-path\n", getprogname());
	exit(1);
}

static unsigned int
parse_x(FILE *infp)
{
	size_t ret;
	unsigned int x;

	ret = getc_unlocked(infp);
	if (ret == EOF) {
		if (feof(infp))
			return (-1);
		err(1, "fread");
	}

	x = ret & 0x7f;
	if (ret & 0x80)
		return (x);

	ret = getc_unlocked(infp);
	if (ret == EOF)
		err(1, "fread");

	x |= (ret & 0x7f) << 7;
	if (ret & 0x80)
		return (x);

	ret = getc_unlocked(infp);
	if (ret == EOF)
		err(1, "fread");

	x |= (ret & 0x7f) << 14;
	if (ret & 0x80)
		return (x);

	ret = getc_unlocked(infp);
	if (ret == EOF)
		err(1, "fread");

	x |= (ret & 0x7f) << 21;
	if (ret & 0x80)
		return (x);

	errx(1, "cmt stream error: unterminated x");
}

static void
tag_alloc(unsigned int newtag, intptr_t newptr)
{
	if (newtag >= nitems(tag2ptr))
		errx(1, "tag overflow; bump MAXTAGS and rebuild cmtreplay");

	if (tag2ptr[newtag] != 0)
		errx(1, "already have tag %d as ptr %lx", newtag, tag2ptr[newptr]);

	tag2ptr[newtag] = newptr;
}

static intptr_t
tag_free(unsigned int oldtag)
{
	intptr_t oldptr;

	if (oldtag >= nitems(tag2ptr))
		errx(1, "tag overflow; bump MAXTAGS and rebuild cmtreplay");

	oldptr = tag2ptr[oldtag];

	if (oldptr == 0)
		errx(1, "no ptr for %d", oldtag);

	tag2ptr[oldtag] = 0;

	return (oldptr);
}
	
static void
replay(FILE *infp)
{
	intptr_t newptr, oldptr;
	static unsigned int oldnewtag = 0;
	unsigned int newtag, oldtag, size;

	for (;;) {
		size = parse_x(infp);
		if (feof(infp))
			return;

		oldtag = parse_x(infp);

		if (size != 0)
			newtag = parse_x(infp);
		else
			newtag = 0;

		if (oldtag != 0) {
			if (oldtag >= oldnewtag)
				errx(1, "cmt stream error: oldtag >= oldnewtag");
			oldtag = (oldnewtag + 1) - oldtag;
		}

		if (newtag != 0) {
			newtag += oldnewtag;
			oldnewtag = newtag;
		}

		if (newtag != 0) {
			if (oldtag != 0) {
				if (size == 0)
					errx(1, "cmt stream error: size == 0");

				oldptr = tag_free(oldtag);
				newptr = (intptr_t)REALLOC((void *)oldptr, size);
				tag_alloc(newtag, newptr);

				if (vflag)
					printf("realloc(%lx<%d>, %d) = %lx<%d>\n", oldptr, oldtag, size, newptr, newtag);
			} else {
				if (size == 0)
					errx(1, "cmt stream error: size == 0");

				newptr = (intptr_t)MALLOC(size);
				tag_alloc(newtag, newptr);

				if (vflag)
					printf("malloc(%d) = %lx<%d>\n", size, newptr, newtag);
			}
		} else {
			if (size != 0)
				errx(1, "cmt stream error: size != 0");

			oldptr = tag_free(oldtag);
			FREE((void *)oldptr);

			if (vflag)
				printf("free(%lx<%d>)\n", oldptr, oldtag);
		}
	}
}

int
main(int argc, char **argv)
{
	FILE *infp;
	int ch;

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			vflag = true;
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	infp = fopen(argv[0], "r");
	if (infp == NULL)
		err(1, "%s", argv[0]);

	replay(infp);

	return (0);
}
