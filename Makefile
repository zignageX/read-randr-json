prefix = /usr/local

LDCFLAGS = -lxcb -lxcb-randr

all: src/read-randr-json

src/read-randr-json: src/read-randr-json.cc
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDCFLAGS) -o $@

install: src/read-randr-json
	install -D src/read-randr-json $(DESTDIR)$(prefix)/bin/read-randr-json

clean:
	-rm -f src/read-randr-json

distclean: clean

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/read-randr-json

.PHONY: all install clean distclean uninstall
