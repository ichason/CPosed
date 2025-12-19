// SPDX-License-Identifier: 0BSD

/*
 * Test application for xz_boot.c
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STATIC static
#define INIT

static void error(/*const*/ char *msg)
{
	fprintf(stderr, "%s\n", msg);
}

/*
 * Disable XZ_UNSUPPORTED_CHECK as it's not used in Linux and thus
 * decompress_unxz.c doesn't handle it either (it thinks it's a bug).
 */
#undef XZ_DEC_ANY_CHECK

/* Disable the CRC64 support even if it was enabled in the Makefile. */
#undef XZ_USE_CRC64

#include "../linux/lib/decompress_unxz.c"

static uint8_t in[1024 * 1024];
static uint8_t out[1024 * 1024];

static long fill(void *buf, unsigned long size)
{
	return fread(buf, 1, size, stdin);
}

static long flush(/*const*/ void *buf, unsigned long size)
{
	return fwrite(buf, 1, size, stdout);
}

static void test_buf_to_buf(void)
{
	size_t in_size;
	int ret;
	in_size = fread(in, 1, sizeof(in), stdin);
	ret = __decompress(in, in_size, NULL, NULL, out, sizeof(out),
			NULL, &error);
	/* fwrite(out, 1, FIXME, stdout); */
	fprintf(stderr, "ret = %d\n", ret);
}

static void test_buf_to_cb(void)
{
	size_t in_size;
	long in_used;
	int ret;
	in_size = fread(in, 1, sizeof(in), stdin);
	ret = __decompress(in, in_size, NULL, &flush, NULL, sizeof(out),
			&in_used, &error);
	fprintf(stderr, "ret = %d; in_used = %ld\n", ret, in_used);
}

static void test_cb_to_cb(void)
{
	int ret;
	ret = __decompress(NULL, 0, &fill, &flush, NULL, sizeof(out),
			NULL, &error);
	fprintf(stderr, "ret = %d\n", ret);
}

/*
 * Not used by Linux <= 2.6.37-rc4 and newer probably won't use it either,
 * but this kind of use case is still required to be supported by the API.
 */
static void test_cb_to_buf(void)
{
	long in_used;
	int ret;
	ret = __decompress(in, 0, &fill, NULL, out, sizeof(out),
			&in_used, &error);
	/* fwrite(out, 1, FIXME, stdout); */
	fprintf(stderr, "ret = %d; in_used = %ld\n", ret, in_used);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		fprintf(stderr, "Usage: %s [bb|bc|cc|cb]\n", argv[0]);
	else if (strcmp(argv[1], "bb") == 0)
		test_buf_to_buf();
	else if (strcmp(argv[1], "bc") == 0)
		test_buf_to_cb();
	else if (strcmp(argv[1], "cc") == 0)
		test_cb_to_cb();
	else if (strcmp(argv[1], "cb") == 0)
		test_cb_to_buf();
	else
		fprintf(stderr, "Usage: %s [bb|bc|cc|cb]\n", argv[0]);

	return 0;
}
