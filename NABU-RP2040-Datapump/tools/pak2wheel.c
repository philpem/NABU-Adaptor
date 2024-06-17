#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include "../crc16_genibus.h"

#define PROGMEM ""

/***
 * PAK file format:
 *
 * 2 bytes length of payload (=1009). LSB first. Not sent to nabu.
 *
 * Repeated for each packet:
 *   16-byte Header  \__ Payload = 1009 bytes
 *   Payload data    /
 *   2-byte CRC      Not counted in payload length
 *
 * 16-byte header:
 * ---------------
 * 3 bytes image number (big endian)
 * 1 byte  segment lsb
 * 1 byte  owner
 * 4 bytes tier bits
 * 2 bytes mystery
 * 1 byte  packet type and flags
 * 2 bytes segment number (little endian)
 * 2 bytes offset (big endian)
 *
 */

/***
 * NABU interface format:
 *   <FLAG>  (repeated 1 or more times -- ideally 4 to allow the scrambler to synchronise with discontinuous data)
 *   4 bytes address  - big endian
 *   4 bytes tierbits - big endian
 *   payload
 *
 * Address first byte MSB is set to indicate whether this packet is:
 *   a) Addressed to a specific Adaptor (address 8x_xx_xx_xx), or 
 *   b) Addressed to all Adaptors (address FF_FF_FF_FF).
 * These addresses are detected by the PAL.
 *
 * Tierbits have two modes:
 *   First byte MSB clear:  AND matching against the AND mask. If any mask bit matches a set bit in the Adaptor's Tiers, access is allowed.
 *   First byte MSB set:    EOR matching against the EOR mask. Tiermask must exactly match the Adaptor's EOR mask.
 */
void convertPak(const char *fn)
{
	uint8_t buf[1024];
	uint16_t paylen;
	char fname[PATH_MAX];
	size_t nsegs = 0;
	size_t plens[65535];

//	sprintf(fname, "../data/%s.pak", fn);
	sprintf(fname, "../pak3/%s.pak", fn);

	FILE *fp = fopen(fname, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Error opening '%s'\n", fname);
		//exit(EXIT_FAILURE);
		return;
	}

	printf("// Packet data for %s\n", fn);

	// Handle all the packets
	size_t l;
	while (!feof(fp)) {
		// read payload length
		if (fread(buf, 1, 2, fp) != 2) {
			fprintf(stderr, "Hit end of file while reading packet length\n");
			break;
		}
		paylen = buf[0] | (buf[1] << 8);

		if (paylen == 0x1A1A) {
			fprintf(stderr, "Hit CP/M EOF (0x1A) while reading packet length - EOF\n");
			break;
		}

		if (paylen > 1024) {
			fprintf(stderr, "Payload length is ridiculous (>1K), l=%u\n", paylen);
			exit(EXIT_FAILURE);
		}

		plens[nsegs] = paylen;

		// now read header, data payload and crc
		if ((l = fread(buf, 1, plens[nsegs], fp)) != plens[nsegs]) {
			// See if we hit a CP/M EOF
			if (buf[0] == 0x1A) {
				fprintf(stderr, "Hit CP/M EOF reading payload data - end of file\n");
			} else {
				fprintf(stderr, "Error reading payload data, got %zu bytes, wanted %zu\n", l, plens[nsegs]);
			}
			break;
		}

		// Check the payload data
		fprintf(stderr, "// DEBUG: Pack %02X%02X%02X/%02X rlen %4zu dlen %4zu owner %02X tiers %02X%02X%02X%02X mystery %02X%02X pty %02X seg %02X%02X ofs %02X%02X, ", 
				buf[0], buf[1], buf[2], buf[3],		// pack ID
				plens[nsegs],		// length
				plens[nsegs]-16,	// length less header
				buf[4],				// owner
				buf[5], buf[6], buf[7], buf[8],		// tiers
				buf[9], buf[10],	// mystery bytes
				buf[11],			// packet type
				buf[13], buf[12],	// segment
				buf[14], buf[15]	// offset
				);
		// TODO crc check
		uint16_t crc = crc16_genibus_update(buf, plens[nsegs], crc16_genibus_init());
		uint16_t pakcrc = (buf[plens[nsegs]-2] << 8) | buf[plens[nsegs]-1];
		uint16_t genicrc = crc16_genibus_fini(crc16_genibus_update(buf, plens[nsegs]-2, crc16_genibus_init()));
		fprintf(stderr, "CRC %s(%04X %04X %04X)\n", (genicrc == pakcrc) ? "OK" : "bad", crc, pakcrc, genicrc);

		// output array header
		printf("\nconst uint8_t DATA_%s_blk%zu[]%s = {", fn, nsegs, PROGMEM);

		// print out the array as C code
		for (size_t n=0; n<plens[nsegs]; n++) {
			if (n%8 == 0) {
				printf("\n\t");
			}
			printf("0x%02X, ", buf[n]);
		}
		printf("};\n");

		// next segment please
		nsegs++;
	}

	printf("const uint32_t DATA_%s_NSEGS = %zu;\n", fn, nsegs);

	printf("const uint8_t* const DATA_%s_SEGS[%zu] = {\n", fn, nsegs);
	for (size_t n=0; n<nsegs; n++) {
		printf("\tDATA_%s_blk%zu,\n", fn, n);
	}
	printf("};\n");

	printf("const uint16_t DATA_%s_SEGLENS[%zu] = {", fn, nsegs);
	for (size_t n=0; n<nsegs; n++) {
		if (n%8 == 0) {
			printf("\n\t");
		}
		printf("%4zu,", plens[n]);
	}
	printf("};\n");
}

int main(void)
{
#if 0
	char s[8];
	for (int i=0; i<0x29E; i++) {
		sprintf(s, "%06x", i);
		convertPak(s);
	}
#endif

	convertPak("000001");
	//convertPak("7FFFFF");

	return 0;
}
