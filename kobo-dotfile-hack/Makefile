CC      := $(CROSS_TC)-gcc
STRIP   ?= $(CROSS_TC)-strip
DESTDIR ?= /
PREFIX  ?= /usr/local/geek1011
LIBDIR  := $(PREFIX)/lib
BINDIR  := $(PREFIX)/bin
CONFIG  := -DUSE_FULL_PATH -DNICKEL_ONLY

# Do we want a local preload?
ifdef LOCAL
	CONFIG += -DUSE_LOCAL_PRELOAD
endif

CFLAGS  ?= -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -mthumb

override CFLAGS := $(CONFIG) -Wall -Wextra -shared -fPIC $(CFLAGS)
LIBS := -ldl

.PHONY: all install uninstall clean scripts/98-dotfile.rules scripts/dotfile-patch.sh

all: libhidedir_kobo.so ld.so.preload KoboRoot.tgz

install: all
	install -Dm 0644 libhidedir_kobo.so $(DESTDIR)$(LIBDIR)
	install -Dm 0644 ld.so.preload $(DESTDIR)/etc

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libhidedir_kobo.so
	rm -f $(DESTDIR)/etc/ld.so.preload

clean:
	rm -f libhidedir_kobo.so ld.so.preload KoboRoot.tgz

ifndef LOCAL
KoboRoot.tgz: libhidedir_kobo.so ld.so.preload
	tar czf $@ $^ --group root:0 --owner root:0 \
		--transform "s,libhidedir_kobo.so,.$(LIBDIR)/libhidedir_kobo.so," \
		--transform "s,ld.so.preload,./etc/ld.so.preload,"
	tar tvf $@
else
KoboRoot.tgz: libhidedir_kobo.so scripts/98-dotfile.rules scripts/dotfile-patch.sh
	tar czf $@ $^ --group root:0 --owner root:0 \
		--transform "s,libhidedir_kobo.so,.$(LIBDIR)/libhidedir_kobo.so," \
		--transform "s,scripts/98-dotfile.rules,./etc/udev/rules.d/98-dotfile.rules," \
		--transform "s,scripts/dotfile-patch.sh,.$(BINDIR)/dotfile-patch.sh,"
	tar tvf $@
endif

libhidedir_kobo.so: hidedir.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)
	$(STRIP) --strip-unneeded $@

ld.so.preload: libhidedir_kobo.so
	echo $(LIBDIR)/$< > $@
