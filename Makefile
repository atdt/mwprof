# Makefile for profile collector package
# $Id: Makefile 111618 2012-02-16 04:08:32Z tstarling $
#
#MacOSX Fink library paths 
#CFLAGS+=-I/sw/include/
#LDFLAGS+=-L/sw/lib/

CFLAGS+=-Wall -g $(shell pkg-config --cflags --libs glib-2.0)
LDLIBS+=$(shell pkg-config --libs glib-2.0)


all: collector

collector: collector.c export.c

clean:
	rm -f collector exporter
