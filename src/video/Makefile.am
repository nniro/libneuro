#!/bin/bash

EXTRA_DIST = video.h pool.h

noinst_LTLIBRARIES = libvideo.la

lib_sources = 	\
		painter.c \
		coredraw.c \
		graphics.c \
		debug.c \
		interface.c  \
		pixels.c

libvideo_la_SOURCES = $(lib_sources)

AM_CFLAGS= @NEUROFLAGS@

#include_HEADERS= \
#		     graphics.h

srcdir = @srcdir@
VPATH = @srcdir@
