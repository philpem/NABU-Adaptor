/**
 * @brief Scrambler/whitener for NABU PC HCCA (Home Computer Cable Adaptor aka NABU Adaptor).
 * @author Phil Pemberton <philpem@philpem.me.uk>
 *
 * Licence: GPL v2
 *
 * A reference implementation of the spectral whitener (scrambler) used
 * in the NABU PC cable link protocol. This module precedes the D-OQPSK
 * modulator in the transmitter, and succeeds the demodulator in the
 * receiver (the NA/HCCA).
 *
 * Test functions are included to make sure the code is doing the right
 * thing.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

uint32_t descrambler_fsr = 0x7FFFFF;

uint8_t descramble_bit(const uint8_t incoming_bit)
{
	uint8_t out_bit;

	// clock in new bit
	descrambler_fsr = (descrambler_fsr << 1) | (incoming_bit ? 1 : 0);

	// new incoming_bit ends up in the lowest bit, just like the real hardware
	out_bit = incoming_bit;
	out_bit ^= 1;
	out_bit ^= (descrambler_fsr >> 3) & 1;
	out_bit ^= (descrambler_fsr >> 20) & 1;

	// remove unused bits so we can track the relevant state
	descrambler_fsr &= 0x7FFFFF;

	return out_bit;
}

uint32_t scrambler_fsr = 0x7FFFFF;
uint8_t scramble_bit(const uint8_t incoming_bit)
{
	uint8_t out_bit;

	// scrambler is the same as descrambler but the transmitted data goes into the LFSR
	out_bit = incoming_bit ? 1 : 0;
	out_bit ^= 1;
	out_bit ^= (scrambler_fsr >> 2) & 1;
	out_bit ^= (scrambler_fsr >> 19) & 1;

	// clock the output bit into the register
	scrambler_fsr = (scrambler_fsr << 1) | out_bit;

	// remove unused bits so we can track the relevant state
	scrambler_fsr &= 0x7FFFFF;

	return out_bit;
}

//////

size_t test_scrambler_len(void)
{
	// clock in a constant stream of zeroes to make sure we're in a reachable state
	scrambler_fsr = 0;
	for (size_t i=0; i<32; i++) {
		scramble_bit(0);
	}

	// save the initial known-reachable state
	uint32_t initial_state = scrambler_fsr;

	for (size_t n=0; n < 0xFFFFFF; n++) {
		scramble_bit(0);

		if (scrambler_fsr == initial_state) {
			printf("loop after %zu (0x%08lX) cycles\n", n+1, n+1);
			return n+1;
		}
	}

	return 0;
}

/**
 * Test that the descrambler successfully descrambles the scrambled data
 */
void test_scramble_descramble(void)
{
	srand(time(NULL));

	// clock in a constant stream of zeroes to make sure we're in a reachable state
	scrambler_fsr = 0;
	for (size_t i=0; i<32; i++) {
		descramble_bit(scramble_bit(0));
	}

	// now make sure the scrambler and descrambler produce the same bitstream
	for (size_t n=0; n<1048576*2; n++) {
		uint8_t bit = rand() & 1;
		uint8_t sbit = scramble_bit(bit);
		uint8_t dbit = descramble_bit(sbit);

		if (bit != dbit) {
			fprintf(stderr, "ERROR: Deviation at bit %zu\n", n);
			exit(EXIT_FAILURE);
		}
	}
}

int main(void)
{
	test_scrambler_len();
	test_scramble_descramble();

	return 0;
}
