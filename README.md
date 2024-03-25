# NABU-Adaptor

This repository contains my work on reverse-engineering the cable network adapter for the NABU PC. There's been a lot of work done on the NABU PC itself (which is very similar to the MSX), but very little it seems on the cable system.

**Git LFS is in use for the PCB scans: you will need Git LFS configured to download them.**

Please only download the original scans and XCF (GIMP stacked layers) file if you need them. The JPEGs in the main repository should be fine for most use cases.


## What is here

  * Reverse-engineered schematic for the NABU Adaptor (this is a work in progress)
  * Dumps of the 6805P2 (control processor) and security PAL
  * Speculation on how this all works

I've been collaborating with Jared Boone, and you may find his work on the RF deck and modulation interesting: https://github.com/jboone/nabu-network-adapter


## Channel codes

According to Leo Binkowski, channel code `000E` is CATV channel 31. In the US/Canada CATV frequency scheme, this would be a frequency band of 264 - 270 MHz (centre frequency 267 MHz).

*NOTE/TBD: There may be some kind of checksum embedded in the channel code; if this is the case, the code is probably hiding in the 6805.*


## RF protocol

Jared's repository is a better source for this, but in short: the NABU network transmits data in a repeating "cycle" (the modern term for this is a "carousel"). Each cycle contains a bunch of "packets" of data, and several streamed packets form a complete programme (e.g. the main menu). In the context of NABU, there is also likely some housekeeping data sent as part of the cycle: specifically, authorization metadata. Each Adaptor has a unique serial number (the "PAL number"), which is encoded into the Addressing PAL at manufacture. I assume the network must send a packet which addresses that PAL, and the microcontroller receives the authorizations.

### Physical layer

The manual for the NABU PC identifies the modulation as [OQPSK (offset QPSK)](https://en.wikipedia.org/wiki/Phase-shift_keying#Offset_QPSK_.28OQPSK.29). This seems plausible, as OQPSK has some advantages in terms of reducing amplitude fluctuations. I think it's likely that differential encoding is in use, as the RF can doesn't seem to contain a PLL or other type of carrier-recovery system. That technically means we're dealing with D-OQPSK (differentially encoded OQPSK), at T2-rate, with a symbol rate of 3.156Mbps and a raw data rate of 6.312Mbps. The RF can contains a 3.156MHz crystal to recover the symbol clock, and what I assume to be a frequency doubling circuit to recover the bit clock.

Beneath the D-OQPSK modulation, the data stream is [spectrally whitened using an LFSR-based scrambler](https://en.wikipedia.org/wiki/Scrambler) . The specific scrambler is similar to the [Intelsat IESS-301E11](https://www.intelsat.com/wp-content/uploads/2020/08/IESS-308E11.pdf#page=71) or [ITU V.35](https://www.itu.int/rec/T-REC-V.35-198410-W/en) designs, but is simplified: IESS-301 and V.35 both have 5-bit synchronous counters and associated logic which aren't present in the NABU design.

The LFSR polynomial is the same as used by ITU and Intelsat: `x^20 + x^3 + 1` or `0x80004`. If you're familiar with LFSRs and common implementations, you'll recognise that as the first (fewest tap points) 20-bit maximal-length LFSR. As zero is not a valid state for an LFSR, there are 1048575 valid states.


### Data link layer

**TODO: Byte recovery, synchronisation, framing**

TBD. Requires working out more of the hardware.


## 6805P2 dump

The NABU Adaptor contains a 28-pin Motorola microcontroller - as is typical for these parts, it was "house numbered":

```
(M) SC87253P
03-90022790A
  FL JG28420
```

Generally speaking, "1980s", "Motorola", and "microcontroller", translates to a 6805 variant. These were extremely popular in industry at the time, especially in TV applications. For instance, the VideoCrypt decoders all use 68HC05 variants to control VBI data capture and decoding. The BBC Selector (Videocrypt-S) has two: one for VBI management, and another for general housekeeping.

All that aside - I partially reverse-engineered the circuitry around the processor and identified the power, ground and clock pins. Thankfully Al Kossow at https://www.bitsavers.org has scanned a mountain of 6805 documentation, including early-1980s era databooks and product line guides. With the power pins identified, it was a simple matter to identify the chip as a 6805P series part, likely a 6805P2, P4 or P6.

[SandboxElectronics had already made a 6805P2 dumper](https://github.com/SandboxElectronics/MC6805P2_clone), based on [Sean Riddle's documentation of a dumping method using the 6805P's Non-User Mode (NUM)](https://seanriddle.com/mc6805p2.html). I took this and [altered it to dump the whole 2048 bytes of CPU memory](https://github.com/philpem/MC6805P2_clone), including the bootloaders and extra ROM which isn't present on a 6805P2. The hardware is really simple, and can be built with an Arduino Uno, and a breadboard shield.

  * The analog switches aren't needed. Just connect the "read data bus" (`RD0..RD7`) to the pins marked `!W/R0..!W/R7` on the processor.
  * Ignore `!W/R`, this is just used to control the direction of the analog switches.
  * Connect up the pins on P2 (the read micro) as marked - `MISO`, `MOSI`, and `SCK` are used as GPIOs, `RD_RST` allows the Arduino to reset the processor, and `MCU_CLK4` gives the Arduino control over the target processor.
  * The resistor values for RP1/RP2 aren't critical, I used eight discrete 1k8 resistors and it worked fine.

Unlike later 6805s which need a high-voltage supply, the 6805P2 can be dumped with only a 5V supply.

The results of this work are in the `U31_MC6805P2` directory


## Addressing PAL

The Addressing PAL is the chip above the Signetics N8X60N FIFO controller, identified as U5. Under the label, it's a Monolithic Memories PAL16R6 or PAL16R6A, with the security fuse blown. Sadly the blown security fuse means not even my BP1200 can read the contents - there are [optical](https://twitter.com/johndmcmaster/status/1527017106294046720) [https://docs.google.com/presentation/d/1pVCLh80MX_Cdnd8TuS0s8gr9pOl9cneZiQchR0O-Jmw/edit#slide=id.p](attacks) to read them, but I didn't want to decap the PAL just yet (even though I have the equipment to optically image it and - at least in theory - recover the original fuse data).

Instead I opted for a non-invasive attack using a [https://github.com/DuPAL-PAL-DUmper/](DuPAL) PAL dumper. (note that [https://mastodon.social/@tubetime/112097066652462727](TubeTime has identified a limitation in DuPAL), but it's not relevant to the NABU design).

The results of this work are in the `U5_Addressing_PAL_80011887` directory, and include the processing shell script, DuPAL JSON file, Espresso logic input, and output equations from Espresso. These need to be cleaned up before the logic will make sense, but that can't really start until the pinout is fully known ... which means finishing the schematic, at least as far as the control FIFO.


## Reverse-engineered schematic

The key thing we need to understand all the programmable devices, is a schematic showing how they're wired up. To achieve this, I followed a similar process to how I derived a schematic for the Datatrak receiver:

  * Remove tall components (in the NABU case, the connectors).
  * Scan both sides of the PCB.
  * Colourise the layers and overlay them in Photoshop or GIMP.
  * Follow the tracks while entering them into Kicad.
  * Use layer masks to hide tracks which have been entered into Kicad

The hardest part of all this is following the traces, but removing the "finished" ones with layer masks makes things a lot less chaotic. Plus it gives an instant view of how much work you've done - I find this motivates me to finish the job!

The PCB scans are in the `PCB_Scans` directory, and the schematic is in the `Kicad` directory.


## Links to other sites and repos

  * Jared Boone's NABU Adaptor reverse engineering: https://github.com/jboone/nabu-network-adapter
  * RealDeuce's NABU Adaptor reverse engineering: https://github.com/RealDeuce/NABU-Network-Adaptor.git - at time of writing, this is a partial schematic.
