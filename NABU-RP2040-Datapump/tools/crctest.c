#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../crc16_genibus.h"

const uint8_t crcTabHi[32] = {
	 0x0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1,
	 0x0, 0x12, 0x24, 0x36, 0x48, 0x5A, 0x6C, 0x7E,
	0x91, 0x83, 0xB5, 0xA7, 0xD9, 0xCB, 0xFD, 0xEF
};
const uint8_t crcTabLo[32] = {
	 0x0, 0x21, 0x42, 0x63, 0x84, 0xA5, 0xC6, 0xE7,
	 0x8, 0x29, 0x4A, 0x6B, 0x8C, 0xAD, 0xCE, 0xEF,
	 0x0, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
	0x88, 0xB9, 0xEA, 0xDB, 0x4C, 0x7D, 0x2E, 0x1F
};

uint16_t crc_init(void)
{
	return 0xFFFF;
}

uint16_t crc_update(uint8_t *buf, size_t len, const uint16_t prev_crc)
{
	uint8_t crcTemp, crcAccumLo, crcAccumHi;

	crcAccumLo = prev_crc & 0xff;
	crcAccumHi = (prev_crc >> 8) & 0xff;

	for (size_t n=0; n<len; n++) {
		crcTemp = crcAccumHi ^ buf[n];

		crcAccumHi = crcAccumLo ^ crcTabHi[crcTemp & 15];
		crcAccumLo = crcTabLo[crcTemp & 15];

		crcAccumHi ^= crcTabHi[(crcTemp >> 4) | 16];
		crcAccumLo ^= crcTabLo[(crcTemp >> 4) | 16];
	}

	return (crcAccumHi << 8) | crcAccumLo;
}


/// Box address: all units
const uint32_t BOXADDR_BROADCAST = 0xFFFFFFFF;

/// Box address: this bit must be set
const uint32_t BOXADDR_IDENT_BIT = 0x80000000;

/// Tier bits: AND tiers
const uint32_t TIER_AND = 0;
/// Tier bits: XOR tiers
const uint32_t TIER_XOR = 0x80000000;
/// Tier bits: bit mask
const uint32_t TIER_AND_MAX = 0x7FFFFFFF;

void generateSetTiersPacket(const uint32_t boxAddr, const uint32_t tiers, uint8_t *buf)
{
	assert(boxAddr & BOXADDR_IDENT_BIT);

	// box address or FF_FF_FF_FF for broadcast
	buf[0] = (boxAddr >> 24) & 0xFF;
	buf[1] = (boxAddr >> 16) & 0xFF;
	buf[2] = (boxAddr >> 8)  & 0xFF;
	buf[3] = (boxAddr)       & 0xFF;

	// command 0x81, set tiers
	buf[4] = 0x81;

	// tier bits
	buf[5] = (tiers >> 24) & 0xFF;
	buf[6] = (tiers >> 16) & 0xFF;
	buf[7] = (tiers >> 8 ) & 0xFF;
	buf[8] = (tiers      ) & 0xFF;

	// not-used bytes
	for (size_t i=9; i<16; i++) {
		buf[i] = 0;
	}

	// Brute force the CRC, we want 0x1D0F.
	uint16_t startingCrc = crc16_genibus_update(buf, 16, crc16_genibus_init());

	for (uint32_t tc=0; tc<65536; tc++) {
		buf[17] = (tc >> 8) & 0xff;
		buf[18] = (tc & 0xff);

		if (crc16_genibus_update(&buf[17], 2, startingCrc) == 0x1D0F) {
			printf("Collision found, FCS=0x%04X\n", tc);
		}
	}
}


int main(void)
{
	srand(time(NULL));

	uint8_t testbuf[1009];

	// fill buffer with random data
	for (size_t n=0; n<sizeof(testbuf); n++)
		testbuf[n] = rand();

	// run Genibus CRC
	uint16_t crcGeni = crc16_genibus_update(testbuf, sizeof(testbuf), crc16_genibus_init());

	printf("Genibus (unf) CRC: 0x%04X\n", crcGeni);
	printf("Genibus (fin) CRC: 0x%04X\n", crc16_genibus_fini(crcGeni));
	printf("HCCA          CRC: 0x%04X\n", crc_update(testbuf, sizeof(testbuf), crc_init()));

	printf("\n");

	uint8_t x[2];
	x[0] = crcGeni >> 8;
	x[1] = crcGeni & 0xFF;
	printf("Xbuf             : %02X %02X\n", x[0], x[1]);
	printf("Genibus Unf +CRC : 0x%04X\n", crc16_genibus_update(x, 2, crcGeni));

	printf("\n");

	crcGeni = crc16_genibus_fini(crcGeni);
	x[0] = crcGeni >> 8;
	x[1] = crcGeni & 0xFF;
	printf("Xbuf             : %02X %02X\n", x[0], x[1]);
	printf("Genibus Fin +CRC : 0x%04X\n", crc16_genibus_update(x, 2, crcGeni));


	// Test generating a set tiers packet
	uint8_t buf[32];
	generateSetTiersPacket(0x80011887, TIER_AND_MAX, buf);
	generateSetTiersPacket(BOXADDR_BROADCAST, TIER_AND_MAX, buf);


	return EXIT_SUCCESS;
}
