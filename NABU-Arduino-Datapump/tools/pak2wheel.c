#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

/***
 * PAK file format:
 *
 * 2 bytes length of payload (=1009). LSB first. Not sent to nabu.
 * 16-byte Header  \__ Payload = 1009 bytes
 * Payload data    /
 * 2-byte CRC      Not counted in payload length
 * (Repeat)
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

	sprintf(fname, "%s.pak", fn);

	FILE *fp = fopen(fname, "rb");

	printf("// Packet data for %s\n", fn);

	while (!feof(fp)) {
		// read payload length
		if (fread(buf, 1, 2, fp) != 2) {
			fprintf(stderr, "Hit end of file while reading packet length\n");
			break;
		}
		paylen = buf[0] | (buf[1] << 8);
		plens[nsegs] = paylen;

		if (paylen == 0x1A1A) {
			fprintf(stderr, "Hit CP/M EOF (0x1A) while reading packet length\n");
			break;
		}

		if (paylen > 1024) {
			fprintf(stderr, "Payload length ridiculous, l=%u\n", paylen);
			break;
		}

		// now read header, data payload and crc
		if (fread(buf, 1, paylen, fp) != paylen) {
			fprintf(stderr, "Error reading payload data\n");
			break;
		}

		printf("\nconst uint8_t DATA_%s_blk%zu[] PROGMEM = {", fn, nsegs);

		// print out the array as C code
		for (size_t n=0; n<paylen+2; n++) {
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

	printf("const uint16_t DATA_%s_SEGLENS[%zu] = {\n", fn, nsegs);
	for (size_t n=0; n<nsegs; n++) {
		printf("%zu,\n", plens[n]);
	}
	printf("};\n");
}

int main(void)
{
	convertPak("000001");
	return 0;
}
