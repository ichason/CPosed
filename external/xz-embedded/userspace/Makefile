# SPDX-License-Identifier: 0BSD
#
# Author: Lasse Collin <lasse.collin@tukaani.org>

# In 2022, Linux finally moved forward from -std=gnu89 and switched
# to -std=gnu11. In practice quite a few C99/C11 features were used
# for decades that were extensions in GNU-C89.
#
# Use the option -Wdeclaration-after-statement for historical reasons
# for now.
CC = gcc -std=gnu11
BCJ_CPPFLAGS = -DXZ_DEC_X86 -DXZ_DEC_ARM -DXZ_DEC_ARMTHUMB -DXZ_DEC_ARM64 \
		-DXZ_DEC_RISCV -DXZ_DEC_POWERPC -DXZ_DEC_IA64 -DXZ_DEC_SPARC
CPPFLAGS = -DXZ_USE_CRC64 -DXZ_DEC_ANY_CHECK -DXZ_DEC_CONCATENATED
CFLAGS = -ggdb3 -O2 -pedantic -Wall -Wextra -Wdeclaration-after-statement
RM = rm -f
VPATH = ../linux/include/linux ../linux/lib/xz
COMMON_SRCS = xz_crc32.c xz_crc64.c xz_dec_stream.c xz_dec_lzma2.c xz_dec_bcj.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)
XZMINIDEC_OBJS = xzminidec.o
BYTETEST_OBJS = bytetest.o
BUFTEST_OBJS = buftest.o
BOOTTEST_OBJS = boottest.o
XZ_HEADERS = xz.h xz_private.h xz_stream.h xz_lzma2.h xz_config.h
PROGRAMS = xzminidec bytetest buftest boottest

ALL_CPPFLAGS = -I../linux/include/linux -I. $(BCJ_CPPFLAGS) $(CPPFLAGS)

all: $(PROGRAMS)

%.o: %.c $(XZ_HEADERS)
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) -c -o $@ $<

xzminidec: $(COMMON_OBJS) $(XZMINIDEC_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON_OBJS) $(XZMINIDEC_OBJS)

bytetest: $(COMMON_OBJS) $(BYTETEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON_OBJS) $(BYTETEST_OBJS)

buftest: $(COMMON_OBJS) $(BUFTEST_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON_OBJS) $(BUFTEST_OBJS)

boottest: $(BOOTTEST_OBJS) $(COMMON_SRCS)
	$(CC) $(ALL_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(BOOTTEST_OBJS)

.PHONY: clean
clean:
	-$(RM) $(COMMON_OBJS) $(XZMINIDEC_OBJS) $(BYTETEST_OBJS) \
		$(BUFTEST_OBJS) $(BOOTTEST_OBJS) $(PROGRAMS)
