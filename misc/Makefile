CC=gcc
CFLAGS=

.PHONY: all clean

TARGETS=chancalc scrambler

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

chancalc: chancalc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

scrambler: scrambler.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

