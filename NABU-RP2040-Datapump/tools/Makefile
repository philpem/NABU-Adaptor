CC=gcc
CFLAGS=-g -ggdb

.PHONY: all clean

TARGETS=pak2wheel crctest
PAK2WHEEL_O=pak2wheel.o ../crc16_genibus.o
CRCTEST_O=../crc16_genibus.o crctest.o

all: $(TARGETS)

clean:
	-rm -f $(TARGETS)

pak2wheel: $(PAK2WHEEL_O)
	$(CC) $(LDFLAGS) -o $@ $^

crctest: $(CRCTEST_O)
	$(CC) $(LDFLAGS) -o $@ $^

