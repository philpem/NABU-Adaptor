/******************************************
 * NABU-PC Adaptor datapump
 * 
 * Generates scrambled NABU Adaptor data streams
 */

#include <SPI.h>
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "crc16_genibus.h"

// Oscilloscope trigger output pin and enable
#define DEBUG_PIN 2
#define SCOPE_TRIGGER_OUT

// SPI clock speed
// RP2040 generally cant
//#define SPI_SPEED 6312000
#define SPI_SPEED 8000000

// Diagnostic printout enable/disable
//#define DIAGPRINT

// Use DMA for SPI transmission
#define DMA


// Buffering --
// Number of transmit buffers
#define BITBUF_NBUFFERS	2
// Size of a transmit buffer
#define BITBUF_BUFLEN 	32768


// Number of flag bytes at start of packet
#define PACKET_START_FLAG_N 10

// Number of flag bytes between packets
//#define INTERPACKET_FLAG_N 1024

// Number of times a packet should be repeated
#define PACKET_REPEAT_COUNT 1


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

uint8_t bb_locked[BITBUF_NBUFFERS] = {0};	// bit buffer lock flags
uint8_t bb_buffer[BITBUF_NBUFFERS][BITBUF_BUFLEN];	// bit buffers
uint8_t *bb_wrptr = bb_buffer[0];			// pointer to current byte in current bitbuffer
size_t bb_length[BITBUF_NBUFFERS];			// number of buffered bytes
uint32_t bb_cur_bitbuf = 0;	// current bitbuffer
uint8_t bb_bits = 0;		// number of bits clocked in
size_t bb_bytes = 0;		// number of bytes clocked in

// Don't start transmitting until the initial buffer fill is done
// The RP2040 FIFO is 32 bits wide and 8 levels deep, so we have to limit ourselves to an initial fill of 8 buffers.
int bb_initial_fill = BITBUF_NBUFFERS>8 ? 8 : BITBUF_NBUFFERS;


// Check if the other core has released any buffers
// Returns true if at least one buffer was unlocked
bool bb_check_unlocks(void)
{
	uint32_t bufnum;
	bool ok;
	bool haveFreeBuffers = false;
	
	do {
		ok = rp2040.fifo.pop_nb(&bufnum);
		if (ok) {
			bb_locked[bufnum] = 0;
			haveFreeBuffers = true;
		}
	} while (ok);

	return haveFreeBuffers;
}


// Clock a bit into the buffer
static inline void bb_clockin(const byte b)
{
	// shift in
	*bb_wrptr = (*bb_wrptr << 1) | (b&1);
	bb_bits++;

	if (bb_bits == 8) {
		// Completed this byte - onto the next
		bb_bits = 0;
		bb_bytes++;
		bb_wrptr++;

		// Check for buffer fill
		if (bb_bytes >= BITBUF_BUFLEN) {
			// buffer is full, lock the buffer and signal the other core to tx it
			bb_locked[bb_cur_bitbuf] = 1;
			bb_length[bb_cur_bitbuf] = bb_bytes;
			rp2040.fifo.push(bb_cur_bitbuf);
	
			// advance to the next buffer
			bb_cur_bitbuf++;
			if (bb_cur_bitbuf >= BITBUF_NBUFFERS) {
				bb_cur_bitbuf = 0;
			}
			bb_bits = bb_bytes = 0;
			bb_wrptr = bb_buffer[bb_cur_bitbuf];
	
			// Check for initial buffer fill
			if (bb_initial_fill > 0) {
				bb_initial_fill--;
				if (bb_initial_fill == 0) {
					// Initial fill complete, release the other core
					Serial.println("** Initial buffer fill complete, releasing second core");
					rp2040.resumeOtherCore();
				}
			}
	
			// Check for bitbuffer locking
			if (bb_locked[bb_cur_bitbuf]) {
				Serial.println("** All buffers full, waiting for a buffer");
				while (!bb_check_unlocks()) {}
			}
		}
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


/****
 * Packet transmission
 */

// Send a segment packet
void send_segment(const uint8_t *buf, const size_t len)
{
	// --- Packet Send ---

	// SDLC FLAG bytes to start the packet
	for (int n=0; n<PACKET_START_FLAG_N; n++) {
		sdlc_send_flag();	
	}

	// Packet data
	for (uint16_t n=0; n<len; n++) {
		sdlc_send_byte(buf[n]);
	}

#ifdef DIAGPRINT
	Serial.println();
#endif
}

#define UADDR_BROADCAST (0xFFFFFFFF)
#define TIERS_AND(x) (x & 0x7FFFFFFF)
#define TIERS_XOR(x) (x | 0x80000000)

// Tier Authorization
void send_auth_tiers(const uint32_t uaddr, const uint32_t tiers)
{
	uint8_t buf[19];
	size_t i=0;

	// Make sure the top bit of the unit address is set - if not, this will look like a segment!
	uint32_t ua = uaddr | 0x80000000;

	// Build the buffer
	buf[i++] = (ua >> 24) & 0xff;		// Unit address
	buf[i++] = (ua >> 16) & 0xff;
	buf[i++] = (ua >> 8)  & 0xff;
	buf[i++] = (ua)       & 0xff;
	buf[i++] = 0x81;					// Command code, 0x81 = Set Tiers
	buf[i++] = (tiers >> 24) & 0xff;	// Tier bits
	buf[i++] = (tiers >> 16) & 0xff;
	buf[i++] = (tiers >> 8)  & 0xff;
	buf[i++] = (tiers)       & 0xff;
	// Free space to byte 17
	while (i<17) {
		buf[i++] = 0;
	}
	// Frame check sequence - Genibus CRC16
	uint16_t fcs = crc16_genibus_fini(crc16_genibus_update(buf, i, crc16_genibus_init()));
	buf[i++] = (fcs >> 8) & 0xff;
	buf[i++] = (fcs)      & 0xff;
	
	// SDLC FLAG bytes to start the packet
	for (int n=0; n<PACKET_START_FLAG_N; n++) {
		sdlc_send_flag();	
	}

	// Packet data
	for (uint16_t n=0; n<i; n++) {
		sdlc_send_byte(buf[n]);
	}
}

// Message
void send_message(const uint32_t uaddr, const uint32_t seqn, const uint32_t msgcode)
{
	uint8_t buf[25];
	size_t i=0;

	// Make sure the top bit of the unit address is set - if not, this will look like a segment!
	uint32_t ua = uaddr | 0x80000000;

	// Build the buffer
	buf[i++] = (ua >> 24) & 0xff;		// Unit address
	buf[i++] = (ua >> 16) & 0xff;
	buf[i++] = (ua >> 8)  & 0xff;
	buf[i++] = (ua)       & 0xff;
	buf[i++] = 0x82;					// Command code, 0x82 = Message
	
	// Free space to byte 15
	while (i<15) {
		buf[i++] = 0;
	}

	buf[i++] = (seqn >> 24) & 0xff;		// Sequence number
	buf[i++] = (seqn >> 16) & 0xff;
	buf[i++] = (seqn >> 8)  & 0xff;
	buf[i++] = (seqn)       & 0xff;

	buf[i++] = (msgcode >> 24) & 0xff;	// Sequence number
	buf[i++] = (msgcode >> 16) & 0xff;
	buf[i++] = (msgcode >> 8)  & 0xff;
	buf[i++] = (msgcode)       & 0xff;

	// Frame check sequence - Genibus CRC16
	uint16_t fcs = crc16_genibus_fini(crc16_genibus_update(buf, i, crc16_genibus_init()));
	buf[i++] = (fcs >> 8) & 0xff;
	buf[i++] = (fcs)      & 0xff;
	
	// SDLC FLAG bytes to start the packet
	for (int n=0; n<PACKET_START_FLAG_N; n++) {
		sdlc_send_flag();	
	}

	// Packet data
	for (uint16_t n=0; n<i; n++) {
		sdlc_send_byte(buf[n]);
	}
}


/****
 * Setup
 */

void setup() {
	// put your setup code here, to run once:
	Serial.begin();
	while (!Serial); // wait for serial port to connect. Needed for native USB

	Serial.println("Main core starting up...");

	// init debug pin
	digitalWrite(DEBUG_PIN, LOW);
	pinMode(DEBUG_PIN, OUTPUT);

	// Idle the other core until we have an initial buffer fill
	rp2040.idleOtherCore();
}


/****
 * Main loop
 */

size_t curSeg = 0;
size_t curRepeat = 0;
void loop() {
	// put your main code here, to run repeatedly:

	// Check if the other core has released any buffers - and release them
	bb_check_unlocks();

	//Serial.print('.');

#ifdef DEBUG_SEND_CONSTANT_FLAGS
	for (size_t i=0; i<100; i++) {
		sdlc_send_flag();
	}
#else
#ifdef DIAGPRINT
	Serial.print("Seg ");
	Serial.print(curSeg);
	Serial.print(", len ");
	Serial.print(DATA_000001_SEGLENS[curSeg]);
#endif
	
	send_segment(DATA_000001_SEGS[curSeg], DATA_000001_SEGLENS[curSeg]);

#ifdef DIAGPRINT
	Serial.print(". bblen ");
	Serial.print(bb_bytes);
	Serial.print(" bytes + ");
	Serial.print(bb_bits);
	Serial.print(" bits (");
	Serial.print((bb_bytes * 8) + bb_bits);
	Serial.print(" total bits)  ");
#endif

#endif

/*
	// handle incomplete final byte by carrying it into the next stream
	if (bb_bits > 0) {
		bb_buffer[0] = bb_buffer[bb_bytes];
	}

	// reset the buffer for the next round
	bb_bytes = 0;
*/
#if defined(INTERPACKET_FLAG_N) && (INTERPACKET_FLAG_N > 0)
	for (int x=0; x<INTERPACKET_FLAG_N; x++) {
		sdlc_send_flag();
	}
#endif

	if (curRepeat++ > PACKET_REPEAT_COUNT) {
		curRepeat=0;
		
		// next segment
		curSeg++;
		if (curSeg >= DATA_000001_NSEGS) {
			curSeg = 0;
		}
	}
}


/****************************
 * SECOND PROCESSOR CORE
 * 
 * The second core sits around copying data from the packet buffer to the SPI buffer.
 ****************************/

unsigned int dma_chan_spi;
dma_channel_config dma_config_spi;

void setup1()
{
	delay(1000);
	Serial.println("Transmitter core starting up...");

	// Set up SPI
	SPI.setSCK(18);	// GP18, pin 24
	SPI.setTX(19);  // GP19, pin 25
	SPI.begin();

	// Setup DMA using an unused channel
	dma_chan_spi = dma_claim_unused_channel(true);

#ifdef DMA
    Serial.println("(TX) Configure TX DMA");
    dma_config_spi = dma_channel_get_default_config(dma_chan_spi);
    channel_config_set_transfer_data_size(&dma_config_spi, DMA_SIZE_8);
    channel_config_set_dreq(&dma_config_spi, spi_get_dreq(spi_default, true));

	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
#endif
}

void loop1()
{
	// Get the index of the data block to push
	uint32_t zIndex = rp2040.fifo.pop();

	// Send data block zIndex

#ifdef SCOPE_TRIGGER_OUT
	// debug pin toggle for indication
	digitalWrite(DEBUG_PIN, 1);
	digitalWrite(DEBUG_PIN, 0);
#endif

#ifdef DMA
	dma_channel_configure(dma_chan_spi, &dma_config_spi,
						  &spi_get_hw(spi_default)->dr, // write address
						  bb_buffer[zIndex], // read address
						  bb_length[zIndex], // element count (each element is of size transfer_data_size)
						  true); // start!
	dma_channel_wait_for_finish_blocking(dma_chan_spi);
#else
	// send the packet
	SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
//	noInterrupts();
	
	for (size_t n=0; n<BITBUF_BUFLEN; n++) {
		// we do it this way to avoid trashing the buffer
		// because sending the block straight to transfer() will do that
		SPI.transfer(bb_buffer[zIndex][n]);
	}
	
//	interrupts();
	SPI.endTransaction();
#endif

	// ask the main core to release the buffer
	rp2040.fifo.push(zIndex);
}
