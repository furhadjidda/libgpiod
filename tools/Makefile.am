# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2017-2021 Bartosz Golaszewski <bartekgola@gmail.com>

AM_CFLAGS = -I$(top_srcdir)/include/ -include $(top_builddir)/config.h
AM_CFLAGS += -Wall -Wextra -g -std=gnu89

noinst_LTLIBRARIES = libtools-common.la
libtools_common_la_SOURCES = tools-common.c tools-common.h

LDADD = libtools-common.la $(top_builddir)/lib/libgpiod.la

bin_PROGRAMS = gpiodetect gpioinfo gpioget gpioset gpiomon gpiofind

gpiodetect_SOURCES = gpiodetect.c

gpioinfo_SOURCES = gpioinfo.c

gpioget_SOURCES = gpioget.c

gpioset_SOURCES = gpioset.c

gpiomon_SOURCES = gpiomon.c

gpiofind_SOURCES = gpiofind.c

EXTRA_DIST = gpio-tools-test gpio-tools-test.bats

if WITH_TESTS

bin_SCRIPTS = gpio-tools-test gpio-tools-test.bats

endif
