CC ?= cc
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -std=c11

all: flashsh flashwm

flashsh: flashsh.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

flashwm: flashwm.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lX11

install: all
	install -D -m 0755 flashsh $(DESTDIR)/usr/bin/flashsh
	install -D -m 0755 flashwm $(DESTDIR)/usr/bin/flashwm

clean:
	rm -f flashsh flashwm

.PHONY: all install clean
