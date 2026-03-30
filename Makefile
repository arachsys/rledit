BINDIR := $(PREFIX)/bin
CFLAGS := -O2 -Wall
LDLIBS := -lreadline -lncurses

all: rledit

rledit: rledit.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o rledit rledit.c $(LDLIBS)

install: rledit
	mkdir -p $(DESTDIR)$(BINDIR)
	install -s rledit $(DESTDIR)$(BINDIR)

clean:
	rm -f rledit

.PHONY: all install clean
