# 85h: Tune

^    NPC    ^     NA      ^   notes    ^
|     85h   |             | CMD85h: tune     |
|           |   10h 06h   | ACK command      |
|  xxh xxh  |             | Tuning word      |
|   ... 675 ms                             |||
|           |      84h    | Command success  |

Tunes to the specified channel. The tuning word is sent as-is to the MC145155 PLL in the tuner block.


# 84h: Request Segment

^      NPC      ^     NA      ^   notes    ^
|      84h      |             | CMD85h: tune     |
|               |   10h 06h   | ACK command      |
|  xx yy zz aa  |             | Segment address, e.g. ''00 01 00 00'' for main menu  -- loaded into buffer in reverse order |
|               |  82h / 84h  | If last byte MSB clear: 84h and set MainCmdLock FLAG. \\ If last byte MSB set: 82h and clear MainCmdLock FLAG. |

Segment address format:

^   File number  ^  Segment  ^
|   xx yy zz     |   aa      |

This is sent by the NPC in reverse order, i.e. ''aa zz yy xx''.

This behaves differently depending on the state of the ''aa'' byte's most-significant bit (value ''80'' hex):

  * Bit 7 set:
    * NA clears MainCmdLock flag (FLAGS_54 bit 0)
    * Response from NA is 82h.
  * Bit 7 clear:
    * NA sets MainCmdLock flag (FLAGS_54 bit 0)
    * Response from NA is 84h.


# 83h: Clear MainCmdLock bit

^      NPC      ^     NA      ^   notes    ^
|      83h      |             | CMD85h: tune     |
|               |   10h 06h   | ACK command      |
|  Clear main command lock                       |
|               |     84h     |                  |

Clears the main command lock and returns 84h.


# 82h: Read up to 64 bytes of NA RAM starting at 40h

^      NPC      ^     NA      ^   notes    ^
|      82h      |             | CMD85h: tune     |
|               |   10h 06h   | ACK command      |
|      xx       |             | Byte count       |
|               |  ...        | Up to ''xx'' bytes of NA RAM, starting at address 40h  |
|               |  82h / 10h E1h  |                  |

Reads and returns up to 64 bytes of NA RAM, starting at address ''40h''.

Response depends on the value of ''xx'', the length byte.

xx:

  * 00h:
    * Clear main command lock (FLAGS_54 bit 0)
    * Return an ''82h'' response.
  * 01h..40h:
    * Return ''length'' bytes, escaped as needed.
    * Return a ''10 E1'' response.
  * > 40h: As if 40h was sent.


# 81h: Set adaptor status?

^      NPC      ^     NA      ^   notes    ^
|      81h      |             | CMD85h: tune     |
|               |   10h 06h   | ACK command      |
|      aa       |             | AND mask         |
|      oo       |             | OR mask          |
|               |     84h     |                  |

Reads AA into TMP_62

Reads OO, ANDs with the AND mask (AA), stores in TMP_63.

AdaptorStatus = ( AdaptorStatus & ~Temp62 ) | Temp63.

After AdaptorStatus is updated:

  * If AdaptorStatus AND 0Fh >= 3, then UARTTxCharDelay = AdaptorStatus & 0Fh
  * PORTB = ( PORTB & 0CFh ) | ( AdaptorStatus & 30h )
    * Sets MESSAGE and LINK LED state. 0 = LED on


# AdaptorStatus (RAM 0x40) bit assignment

^  7   ^  6  ^  5  ^  4  ^  3  ^  2  ^  1  ^  0  ^
|      |     |  L  |  M  |   UartTxCharDelay     |


  * UartTxCharDelay: intercharacter transmit delay
  * L: Link LED state, 0=on
  * M: Message LED state, 0=on

Default on power-on is 0BFh:

  * bit 7 high (unknown)
  * Link LED off
  * Message LED off
  * Transmit delay (reported) 15 cycles
    * The real power-on delay is 




