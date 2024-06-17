# NABU Adaptor cable packet format

# Type 1:  (PAL address match)

|  Byte  |  Label     |  Notes
|--------|------------|--------|
|  FLAG  | HDLC FLAG  |        |
|  `0`   | Addr3      | Address  |
|  `1`   | Addr2      |  |
|  `2`   | Addr1      |  |
|  `3`   | Addr0      | Address |
|  `4`   | PktType    | Packet type. Sent to the NABU when the packet arrives. Most significant bit must be set.  |
|  `5`...   | Payload | |

If `FLAGS54_6` is set, and the PAL signals an address match:

  * On packet header arrival, the `PktType` byte is sent to the NABU.
    * The NABU may respond `F0` hex - this is an ack. Any non-`F0` byte will make the Link LED turn off.
  * Four further bytes are read from the NABU
    * These are expected to be `0F AA CC A8 E6`. 
  * ??

If `FLAGS54_6` is clear:

  * Check if PktType bit 7 is set. If so:
    * Reset CRC to 0xFFFF
    * Add Addr0..PktType (inclusive) to CRC
  * Check if type 0x81
    * FUN_057E with temp62=13, temp63=0, temp64=3
      * FLAGS54 bit 4 set while running


## Type 1 PTY 0x81: Set Tiers

|  Byte  |  Label     |  Notes
|--------|------------|--------|
|  FLAG  | HDLC FLAG  |        |
|  `0`   | Addr3      | Address high byte. MSB must be set.  |
|  `1`   | Addr2      |  |
|  `2`   | Addr1      |  |
|  `3`   | Addr0      | Address low byte    |
|  `4`   | PktType    | Packet type. 0x81 = "Set tiers"  |
|  `5`   | Tier3      | Tier bit high. MSB clear for AND tiers, set for XOR tiers.  |
|  `6`   | Tier2      | |
|  `7`   | Tier1      | |
|  `8`   | Tier0      | |
|  `9`   |            | |
|  `10`  |            | |
|  `11`  |            | |
|  `12`  |            | |
|  `13`  |            | |
|  `14`  |            | |
|  `15`  |            | |
|  `16`  |            | |
|  `17`  | FCSa       |  |
|  `18`  | FCSb       |  |

Sets the tier bits for the box to the specified value.

There are two sets of tier bits, AND tiers and XOR tiers.

  * AND tiers: The adaptor's tier bits and the packet's tier bits are ANDed together. If the result is nonzero, access is allowed.
  * XOR tiers: The adaptor's tier bits and the packet's tier bits are XORed together. If the result is zero, access is allowed.

A receiver which has just powered on will have its AND tier bits set to `0x40000000`, and its XOR tier bits set to `0x00000000`.

The frame check sequence bytes are selected so that the Genibus CRC16 (init=0xFFFF, fini=0) from Addr3 to the end of the packet results in a value of 0x1D0F.


## Type 1 PTY 0x82: Message

|  Byte  |  Label     |  Notes
|--------|------------|--------|
|  FLAG  | HDLC FLAG  |        |
|  `0`   | Addr3      | Address high byte. MSB must be set.  |
|  `1`   | Addr2      |  |
|  `2`   | Addr1      |  |
|  `3`   | Addr0      | Address low byte    |
|  `4`   | PktType    | Packet type. 0x82 = "Message"  |
|  `5`   |            | |
|  `6`   |            | |
|  `7`   |            | |
|  `8`   |            | |
|  `9`   |            | |
|  `10`  |            | |
|  `11`  |            | |
|  `12`  |            | |
|  `13`  |            | |
|  `14`  |            | |
|  `15`  | Mess1      | Message data - first 4 bytes possibly a sequence number (compared with previous packet). |
|  `16`  | Mess2      | |
|  `17`  | Mess3      | |
|  `18`  | Mess4      | |
|  `19`  | Mess5      | Message data - last 4 bytes possibly a message code?  |
|  `20`  | Mess6      |  |
|  `21`  | Mess7      |  |
|  `22`  | Mess8      | Message data end |
|  `23`  | FCSa       |  |
|  `24`  | FCSb       |  |

Sends a message to the cable adaptor.

The first four bytes of the message data are compared with those of the last message received. If they differ, then the MESSAGE LED lights and the NA
signals to the NPC that a message is available.

The last four bytes of the message data are likely some kind of message identifier, and are always stored.


# Segment packet

|  Byte  |  Label     |  Notes
|--------|------------|--------|
|  FLAG  | HDLC FLAG  |        |
|  `0`   | SegAddr3   | Segment Address most significant byte. Most significant bit must be clear.  |
|  `1`   | SegAddr2   | Segment Address  |
|  `2`   | SegAddr1   | Segment Address |
|  `3`   | SegAddr0   | Segment Address least significant byte. Sometimes called the "segment lsb". |
|  `4`   | Owner      | Owner byte. MSB must be clear.  //Sometimes called the Owner byte, but is really the segment type. 0=directory, 1=code, 2=data, 3=overlay.  |
|  `5`   | Tier3      | Tier bits most significant byte. If MSBit is clear, AND match is used. If MSBit is set, XOR match is used.  |
|  `6`   | Tier2      |  |
|  `7`   | Tier1      |  |
|  `8`   | Tier0      |  |
|  `9`   | Mystery1   | Function unknown  |
|  `10`  | Mystery2   | Function unknown  |
|  `11`  | PtyFlags   | Packet type and flags - bit 4 marks end of segment |
|  `12`  | SegLSB     | Segment number, little-endian, least-significant byte |
|  `13`  | SegMSB     | Segment number, little-endian, most-significant byte |
|  `14`  | OffsetHi   |  |
|  `15`  | OffsetLo   |  |
|  `15..n-2`  | Payload  | Payload data  |
|  `n-2` | CRCHigh    | Genibus CRC of the data payload - high byte  |
|  `n-1` | CRCLow     | Genibus CRC of the data payload - low byte   |
 

## PtyFlags bits

|  Bit  |  Value  |  Notes    |
|-------|---------|-----------|
|  7    |  0x80   | 1 = First packet in sequence |
|  6    |  0x40   | (unused? never set) |
|  5    |  0x20   | ??? - always set |
|  4    |  0x10   | 1 = Last packet in sequence |
|  3    |  0x08   |  |
|  2    |  0x04   |  |
|  1    |  0x02   | Type bit 1 |
|  0    |  0x01   | Type bit 0 |

  * Type: File type.
    * 0: directory segment with or without code-to-load
    * 1: code segment
    * 2: data segment
    * 3: overlay segment
    * see https://www.nabunetwork.com/stuff/docs/Nabu_PC_Technical_Specifications.pdf page 48

