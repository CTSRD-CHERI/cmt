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
 * Bump-the-pointer allocator, used to benchmark difference compared
 * to proper malloc implementations.  Use like this:
 *
 * $ export LD_PRELOAD=`pwd`/bumpmalloc.so
 * $ run-your-app
 *
 * You can set the BUMPMALLOC_UTRACE environment variable to log using
 * utrace(2), to use with 'ktrace -t u'.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct utrace_malloc {
	void *p;
	size_t s;
	void *r;
};

static uintptr_t ptr;
static bool do_utrace;

static void
log_utrace(void *p, size_t s, void *r)
{
	struct utrace_malloc ut;

	ut.p = p;
	ut.s = s;
	ut.r = r;
	utrace(&ut, sizeof(ut));
}

static void *
malloc_nolog(size_t size)
{
	void *tmp;

	tmp = (void *)ptr;
	ptr += roundup2(size, 8);

	return (tmp);
}

void *
malloc(size_t size)
{
	void *tmp;

	tmp = malloc_nolog(size);

	if (__predict_false(do_utrace))
		log_utrace(NULL, size, tmp);

	return (tmp);
}

void *
calloc(size_t number, size_t size)
{
	void *tmp;

	tmp = malloc(number * size);
	memset(tmp, 0, number * size);

	return (tmp);
}

void *
realloc(void *tmp, size_t size)
{
	void *newptr;

	newptr = malloc_nolog(size);

	// Surprisingly, this is actually correct.
	if (tmp != NULL)
		memmove(newptr, tmp, size);

	if (__predict_false(do_utrace))
		log_utrace(tmp, size, newptr);

	return (newptr);
}

void
free(void *tmp)
{

	if (__predict_false(do_utrace))
		log_utrace(tmp, 0, NULL);
}

static void __attribute__ ((constructor))
bumpmalloc_init(void)
{

	ptr = (uintptr_t)mmap(0, 1 * 1024 * 1024 * 1024,
	    PROT_READ | PROT_WRITE | PROT_EXEC,
	    MAP_ALIGNED_SUPER | MAP_ANON | MAP_PRIVATE | MAP_NOCORE, -1, 0);
	if ((void *)ptr == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		exit (-1);
	}

	if (getenv("BUMPMALLOC_UTRACE") != NULL)
		do_utrace = true;
}
