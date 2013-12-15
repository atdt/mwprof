CFLAGS+=-Wall -g $(shell pkg-config --cflags --libs glib-2.0 gio-2.0 gthread-2.0 gio-unix-2.0)
LDLIBS+=$(shell pkg-config --libs glib-2.0 gio-2.0 gthread-2.0 gio-unix-2.0)

all: mwprof

mwprof: mwprof.c collector.c

clean:
	rm -f mwprof
