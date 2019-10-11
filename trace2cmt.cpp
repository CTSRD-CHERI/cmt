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
  * The purpose of this thing is to convert the malloc traces,
  * which look like this:
  *
  * call-trace	1536334807907427983	50a0805 14	malloc	96	81d21e160
  * call-trace	1536334807907430218		free	81d2899c0	
  * call-trace	1536334807907432412	50a07fa 14	malloc	20	81d2899c0
  *
  * ... into a compact form, about %10 of the original textual malloc
  * trace, which can then be replayed using cmtreplay.
  *
  * Use the -s flag for a human-readable form.
  */

#include <err.h>
#include <map>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool sflag = false;
static int next_tag = 1;
static std::map<intptr_t, unsigned int> ptr2tag;

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-o outfile] [-s] input-path\n", getprogname());
	exit(1);
}

static void
emit_x(FILE *outfp, unsigned int x)
{
	size_t written;
	unsigned char b;

	do {
		b = (unsigned char)x;
		b &= 0x7f;
		x >>= 7;

		if (x == 0)
			b |= 0x80;

		written = fwrite(&b, 1, 1, outfp);
		if (written != 1)
			err(1, "fwrite");
	} while (x != 0);
}

static void
emit(FILE *outfp, unsigned int oldtag, size_t size, unsigned int newtag)
{
	static unsigned int oldnewtag = 0;

	if (newtag == 0)
		assert(size == 0);
	assert(newtag == 0 || newtag > oldnewtag);
	assert(oldtag == 0 || oldtag < oldnewtag + 1);

	emit_x(outfp, size);

	if (oldtag == 0) {
		emit_x(outfp, 0);
	} else {
		emit_x(outfp, (oldnewtag + 1) - oldtag);
	}

	if (size != 0) {
		if (newtag == 0) {
			emit_x(outfp, 0);
		} else {
			emit_x(outfp, newtag - oldnewtag);
			oldnewtag = newtag;
		}
	}
}

static unsigned int
tag_alloc(intptr_t newptr)
{
	unsigned int newtag;

	newtag = next_tag;
	next_tag++;

	if (next_tag < 0)
		errx(1, "tag overflow");

	auto inserted = ptr2tag.insert(std::make_pair(newptr, newtag));
	if (inserted.second == false)
		errx(1, "already have %lx as tag %d", newptr, newtag);

	return (newtag);
}

static unsigned int
tag_free(intptr_t oldptr)
{
	unsigned int oldtag;

	auto oldpair = ptr2tag.find(oldptr);
	if (oldpair == ptr2tag.end())
		errx(1, "no tag for %lx", oldptr);
	oldtag = oldpair->second;
	ptr2tag.erase(oldpair);

	return (oldtag);
}
	
static void
emit_malloc(FILE *outfp, size_t size, intptr_t newptr)
{
	unsigned int newtag;

	if (size == 0)
		errx(1, "malloc with zero size");

	newtag = tag_alloc(newptr);

	if (sflag) {
		fprintf(outfp, "malloc(%zd) = %lx<%d>\n", size, newptr, newtag);
	} else {
		emit(outfp, 0, size, newtag);
	}
}

static void
emit_free(FILE *outfp, intptr_t oldptr)
{
	unsigned int oldtag;

	oldtag = tag_free(oldptr);

	if (sflag) {
		fprintf(outfp, "free(%lx<%d>)\n", oldptr, oldtag);
	} else {
		emit(outfp, oldtag, 0, 0);
	}
}

static void
emit_realloc(FILE *outfp, intptr_t oldptr, size_t size, intptr_t newptr)
{
	unsigned int newtag, oldtag;

	if (size == 0)
		errx(1, "realloc with zero size");

	oldtag = tag_free(oldptr);
	newtag = tag_alloc(newptr);

	if (sflag) {
		fprintf(outfp, "realloc(%lx<%d>, %zd) = %lx<%d>\n", oldptr, oldtag, size, newptr, newtag);
	} else {
		emit(outfp, oldtag, size, newtag);
	}
}

static void
scan(const char *line, FILE *outfp)
{
	int matched, meh, size;
	intptr_t oldptr, newptr;

	matched = sscanf(line, "call-trace %d %x %d malloc %d %lx\n", &meh, &meh, &meh, &size, &newptr);
	if (matched == 5) {
		emit_malloc(outfp, size, newptr);
		return;
	}

	matched = sscanf(line, "call-trace %d free %lx\n", &size, &oldptr);
	if (matched == 2) {
		emit_free(outfp, oldptr);
		return;
	}

	matched = sscanf(line, "call-trace %d %x %d realloc %lx %d %lx\n", &meh, &meh, &meh, &oldptr, &size, &newptr);
	if (matched == 6) {
		emit_realloc(outfp, oldptr, size, newptr);
		return;
	}

	matched = sscanf(line, "call-trace %d %x %d mmap\n", &meh, &meh, &meh);
	if (matched == 3)
		return;

	if (strncmp(line, "call-trace", strlen("call-trace")) != 0)
		return;

	errx(1, "can't parse: %s", line);
}

int
main(int argc, char **argv)
{
	FILE *infp, *outfp;
	char *line, *outpath = NULL;
	size_t linecap;
	int ch;

	while ((ch = getopt(argc, argv, "o:s")) != -1) {
		switch (ch) {
		case 'o':
			outpath = optarg;
			break;
		case 's':
			sflag = true;
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

	if (outpath == NULL) {
		if (!sflag)
			errx(1, "specify either -s or -o");
		outfp = stdout;
	} else {
		outfp = fopen(outpath, "w");
		if (outfp == NULL)
			err(1, "%s", outpath);
	}

	line = NULL;
	linecap = 0;

	while (getline(&line, &linecap, infp) > 0)
		scan(line, outfp);

	return (0);
}
