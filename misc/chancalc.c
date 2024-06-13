/****
 * Calculate NABU Adaptor channel code.
 * Phil Pemberton <philpem@philpem.me.uk> - 7-Jun-2024
 *
 * Takes a 4-digit NABU Adaptor PLL tuning word in hex, and generates the
 * checksum nibble.
 * Prints the complete 5-digit channel code to be entered into the NABU PC.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	uint16_t code;
	uint8_t codebuf[4];

	if (argc < 2) {
		fprintf(stderr, "Syntax: %s code_in_hex\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (strlen(argv[1]) != 4) {
		fprintf(stderr, "Please provide the first four digits of the channel code, in hex\n");
		return EXIT_FAILURE;
	}

	// convert incoming hex code to a nibble array
	code = strtol(argv[1], NULL, 16);
	codebuf[0] = (code >> 12) & 0x0F;
	codebuf[1] = (code >> 8)  & 0x0F;
	codebuf[2] = (code >> 4)  & 0x0F;
	codebuf[3] = (code)       & 0x0F;

	// This algorithm is from NEWCODE to just after NEWCOD3 in the NABU ROM.
	// Source: https://github.com/drh1802/NABU-ROM/blob/main/nabu.asm
	uint8_t a=0, c=0;
	for (uint8_t b=0; b<4; b++) {
		a = codebuf[b];
		if (b & 1) {
			a <<= 1;
			if (a & 16) {
				a &= 0xEF;
				a++;
			}
		}
		c += a;
	}
	c &= 0x0F;

	printf("Channel Code:        %04X%X\n", code, c);
	printf("Local osc frequency: %d MHz\n", code);

	// Tuner and filter characteristics from https://www.youtube.com/watch?v=8IflOWH8fzY

	// Calculate tuned frequency.
	// LO frequency is 57MHz, with a 12.5kHz FCC-mandated offset
	float freq = ((float)code) - 56.9875; // 54.0 - 3.0 + 0.0125;
	printf("Centre frequency:    %0.4f MHz\n", freq);

	// Check the frequency is reasonably sensible
	float low_lobe = freq - 3.0;
	float high_lobe = freq + 3.0;
	if ((low_lobe < 260.0) || (high_lobe > 280.0)) {
		fprintf(stderr, "Warning! Frequency is outside of the range of the input filter (264 MHz to 270 MHz)!\n");
	}

	return EXIT_SUCCESS;
}
