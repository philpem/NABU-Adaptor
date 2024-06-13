/******************************************
 * NABU-PC Adaptor datapump
 * 
 * Generates scrambled NABU Adaptor data streams
 */

#include <SPI.h>

//#define HARD_RESET_SCRAMBLER

/****
 * Packet management
 */
typedef struct {
	const uint16_t len;
	const uint8_t *const data;
} NABU_PACKET;

/*
typedef struct {
	uint16_t nsegs;					// Number of segments in this file
	NABU_PACKET PROGMEM *packets;	// All the packets in this file
} NABU_WHEEL;
*/

// Pull in the packets
#include "packets.h"



/****
 * Scrambler
 */

// Current scrambler state.
uint32_t scrambler_state = 0;

// Scramble a bit
// Bit_in: zero = 0, any other value = 1
static inline uint8_t scramble_bit(const uint8_t bit_in)
{
	uint8_t b = (bit_in & 1) ^ 1;	// data is inverted
	
	// Scramble taps
	b ^= (scrambler_state >> 2) ^ (scrambler_state >> 19);
	b &= 1;

	// Update the scrambler state
	scrambler_state = (scrambler_state << 1) | b;

	// Return the scrambled bit
	return b;
}


/****
 * Transmit bit buffer
 * 
 * Stores data ready to send to the SPI module.
 */

// Length (in bytes) of the bit buffer
#define BB_LEN 2048

uint8_t bb_buffer[BB_LEN];	// bit buffer
uint8_t bb_bits = 0;		// number of bits clocked in
size_t bb_bytes = 0;		// number of bytes clocked in

// Clock a bit into the buffer
static inline void bb_clockin(const byte b)
{
	// shift in
	bb_buffer[bb_bytes] = (bb_buffer[bb_bytes] << 1) | (b&1);
	bb_bits++;
	if (bb_bits == 8) {
		bb_bits = 0;
		bb_bytes++;
	}

	if (bb_bytes >= BB_LEN) {
		Serial.println("FATAL: Bitbuffer overflow");
		for (;;);
	}
}

// Finalise the buffer by clocking in some idle bits
static inline void bb_final(void)
{
	while (bb_bits != 0) {
		bb_clockin(scramble_bit(0));
	}
}


/****
 * SDLC transmission
 */

// Number of consecutive '1' bits sent (for zero stuffing)
static uint8_t sdlc_nOnes = 0;

// Send a flag, 7E hex (raw)
void sdlc_send_flag(void)
{
	uint8_t b = 0x7E;

	for (int i=0; i<8; i++) {
		bb_clockin(scramble_bit(b >> 7));
		b <<= 1;
	}

	// flag starts and ends with a zero, so reset the ones counter.
	sdlc_nOnes = 0;
}

// Send an arbitrary byte
void sdlc_send_byte(const uint8_t bval)
{
	uint8_t b = bval;

	uint8_t v;
	for (int i=0; i<8; i++) {
		// get current bit and shift it out thru the scrambler
		v = b >> 7;
		bb_clockin(scramble_bit(v));

		// track number of consecutive '1' bits
		if (v) {
			sdlc_nOnes++;
		} else {
			sdlc_nOnes = 0;
		}

		// perform zero stuffing if needed
		if (sdlc_nOnes == 5) {
			bb_clockin(scramble_bit(0));
			sdlc_nOnes = 0;
		}

		// move to next bit
		b <<= 1;
	}
}

void setup() {
	// put your setup code here, to run once:

	// init serial port
	Serial.begin(115200);
	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB
	}

	// init debug pin
	digitalWrite(2, LOW);
	pinMode(2, OUTPUT);

	// hello message
	Serial.println(F("Starting up."));

	// Set up SPI
	SPI.begin();

}

size_t curSeg = 0;
void loop() {
	// put your main code here, to run repeatedly:

	//Serial.print('.');

#ifdef HARD_RESET_SCRAMBLER
	// FIXME, Remove this. Debug code.
	scrambler_state = 0;
#endif

#ifdef DEBUG_SEND_CONSTANT_FLAGS
	for (size_t i=0; i<100; i++) {
		sdlc_send_flag();
	}
#else
	Serial.print("Seg ");
	Serial.print(curSeg);
	Serial.print(", len ");
	Serial.print(DATA_000001_SEGLENS[curSeg]);

	// --- Packet Send ---

	// Send several flags because it might take up to 20 clocks for the scrambler to synchronise
	sdlc_send_flag();	// 8
	sdlc_send_flag();	// 16
	sdlc_send_flag();	// 24
	sdlc_send_flag();	// 32, and the detector should absolutely have heard this

	// Send packet data
	for (uint16_t n=0; n<DATA_000001_SEGLENS[curSeg]; n++) {
		sdlc_send_byte(pgm_read_byte(DATA_000001_SEGS[curSeg]+n));
	}

	// fill the FIFO, man!
	for (uint16_t n=DATA_000001_SEGLENS[curSeg]; n<1024; n++) {
		sdlc_send_byte(0);
	}
#endif

	Serial.print(". bblen ");
	Serial.print(bb_bytes);
	Serial.print(" bytes + ");
	Serial.print(bb_bits);
	Serial.print(" bits (");
	Serial.print((bb_bytes * 8) + bb_bits);
	Serial.println(" total bits)");

	// squash the last remaining bits into a byte
	bb_final();

	// debug pin
	digitalWrite(2, 1);
	digitalWrite(2, 0);

	// send the packet
	SPI.beginTransaction(SPISettings(6312000, MSBFIRST, SPI_MODE0));
	noInterrupts();
	for (size_t n=0; n<bb_bytes; n++) {
		// to avoid trashing the buffer
		SPI.transfer(bb_buffer[n]);
	}

#ifdef HARD_RESET_SCRAMBLER
	// FIXME more debug nonsense to get the scrambler back into a consistent state
	SPI.transfer(0);
	SPI.transfer(0);
	SPI.transfer(0);
	SPI.transfer(0);
#endif

	interrupts();
	SPI.endTransaction();

	// prepare for next packet
	bb_bytes = 0;

	curSeg++;
	if (curSeg >= DATA_000001_NSEGS) {
		curSeg = 0;
		//Serial.println();
		//Serial.print(F("Sending seg:"));
	}
}
