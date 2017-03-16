CFLAGS += -Wall -Wno-format-security -Wextra -O3
__NAME__ = xiate
__NAME_UPPERCASE__ = `echo $(__NAME__) | sed 's/.*/\U&/'`
__NAME_CAPITALIZED__ = `echo $(__NAME__) | sed 's/^./\U&\E/'`
__HOSTNAME__ = $(HOSTNAME:-=_)

INSTALL = install
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
man1dir = $(mandir)/man1

__LIB__ = lib$(__NAME__).a

.PHONY: all clean install installdirs

all: $(__NAME__) $(__NAME__)c

obj/%.o: %.c
	mkdir -p obj
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-D__NAME__=\"$(__NAME__)\" \
		-D__NAME_UPPERCASE__=\"$(__NAME_UPPERCASE__)\" \
		-D__NAME_CAPITALIZED__=\"$(__NAME_CAPITALIZED__)\" \
		-DSRVR_$(__HOSTNAME__) \
		-c $< -o $@ \
		`pkg-config --cflags --libs gtk+-3.0 vte-2.91`

obj/xiate.o: xiate.h
obj/client.o: xiate.h
obj/server.o: xiate.h config.h

$(__NAME__): obj/daemon.o obj/xiate.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-o $@ $^ \
		`pkg-config --cflags --libs gtk+-3.0 vte-2.91`

$(__NAME__)c: obj/client.o obj/xiate.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-o $@ $^ \
		`pkg-config --cflags --libs gtk+-3.0 vte-2.91`

install: $(__NAME__) $(__NAME__)c installdirs
	$(INSTALL_PROGRAM) $(__NAME__) $(DESTDIR)$(bindir)/$(__NAME__)
	$(INSTALL_PROGRAM) $(__NAME__)c $(DESTDIR)$(bindir)/$(__NAME__)c
	$(INSTALL_DATA) man1/$(__NAME__).1 $(DESTDIR)$(man1dir)/$(__NAME__).1
	$(INSTALL_DATA) man1/$(__NAME__)c.1 $(DESTDIR)$(man1dir)/$(__NAME__)c.1

installdirs:
	mkdir -p $(DESTDIR)$(bindir) $(DESTDIR)$(man1dir)

clean:
	rm -rf $(__NAME__) $(__NAME__)c obj
