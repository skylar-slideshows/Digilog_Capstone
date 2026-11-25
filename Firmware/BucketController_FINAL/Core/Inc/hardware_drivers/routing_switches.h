/**
  **********************************************************************************
  * ROUTING BOARD SWITCHES (74HC595 CHAIN) - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file hardware_drivers/routing_switches.h
  * @brief The routing board's eight 74HC595s (U5-U12) set every switch on the
  *        channel cards: input source, pad, phantom, phase, HPF, EQ cut/boost and
  *        shelf/bell, insert, insert source and mix-bus on. They are one 64-bit chain
  *        on the controller's SHIFTREG_Data / SHIFTREG_Clock / SHIFTREG_LAT pins (latch on PC5)
  *        (routing board J24: SR_DAT, SR_CLK, SR_LAT). SHIFTREG_OE (PF9) drives SR_OE_EXT
  *        (J24 pin 7): high keeps the board's outputs off, low turns them on.
  *
  * Chain order, traced on the PCB (the board is flipped, so KiCad's channel names run backwards):
  * SR_DAT enters U5, then U6, U7 ... U12. U5/U6 sit on the right (front view) and drive the
  * rightmost channel, channel 4 (KiCad "CH1"); U7/U8 channel 3, U9/U10 channel 2, and U11/U12, at
  * the far end of the chain on the left, channel 1 (KiCad "CH4"). The first bit sent ends up at
  * U12's QH. Each channel has 16 bits: the low byte is its first register (U11 for channel 1:
  * QA PRE ... QH P48), the high byte its second (U12: QA LF_CUT ... QH MIX_ON); bit 0 = QA.
  * See RS_CHANNEL_1_AT_FAR_END in routing_switches.c.
  * Every bit is active high (1 = the thing named is on), as the cards are wired.
  **********************************************************************************
*/

#ifndef ROUTING_SWITCHES_H
#define ROUTING_SWITCHES_H

#include <stdint.h>

/* First register (U5, U7, U9, U11): INPUT card, and the insert source on the FADER VCA card */
#define RS_PRE      (1U << 0)  //!< QA CHx_PRE_LCV: insert send source. FADER VCA card U9 (ADG1436): 1 = after the EQ (AUDIO_IN), 0 = before it ("pre", AUDIO_PRE_EQ)
#define RS_PAD      (1U << 1)  //!< QB CHx_PAD_LCV: mic pad relay; preamp +20 dB instead of +40 dB
#define RS_HPF      (1U << 2)  //!< QC CHx_HPF_LCV: 90 Hz high-pass in
#define RS_PHASE    (1U << 3)  //!< QD CHx_PHASE_LCV: polarity reversed
#define RS_HIZ_SEL  (1U << 4)  //!< QE CHx_HIZ_SEL_LCV: Hi-Z input (overrides line/mic)
#define RS_LINE_SEL (1U << 5)  //!< QF CHx_LINE_SEL_LCV: line input (0 = mic)
#define RS_MIC_SEL  (1U << 6)  //!< QG CHx_MIC_SEL_LCV: mic selected (phantom interlock on the card)
#define RS_P48      (1U << 7)  //!< QH CHx_P48_LCV: +48 V phantom (only with MIC_SEL, by hardware)

/* Second register (U6, U8, U10, U12): EQ card, FADER VCA card insert, routing board mix switch */
#define RS_LF_CUT   (1U << 8)  //!< QA CHx_LF_CUT_LCV: LF band in its cut wiring (0 = boost)
#define RS_LF_MODE  (1U << 9)  //!< QB CHx_LF_MODE_LCV: LF bell (0 = shelf)
#define RS_HF_CUT   (1U << 10) //!< QC CHx_HF_CUT_LCV: HF cut (0 = boost)
#define RS_HF_MODE  (1U << 11) //!< QD CHx_HF_MODE_LCV: HF bell (0 = shelf)
#define RS_LMF_CUT  (1U << 12) //!< QE CHx_LMF_CUT_LCV: LMF cut (0 = boost)
#define RS_HMF_CUT  (1U << 13) //!< QF CHx_HMF_CUT_LCV: HMF cut (0 = boost)
#define RS_INSERT   (1U << 14) //!< QG CHx_INSERT_LCV: insert return feeds the fader VCA
#define RS_MIX_ON   (1U << 15) //!< QH CHx_MIX_ON: channel connected to the mix bus (ADG1412, 0 = muted)

/**
 * Holds the outputs off, loads all zeros into the chain (mic input, no pad, phantom off, EQ
 * boost/shelf, no insert, channel off the mix bus), latches it, then turns the outputs on.
 * Call once before the I2C tasks start.
 */
void routing_switches_init (void);

/**
 * Sets one channel's 16 bits (RS_* flags). Sends the whole chain and latches it, but only
 * when something changed. Safe to call from any task.
 */
void routing_switches_set (uint8_t channel, uint16_t bits);

/** A channel's bits as last sent. */
uint16_t routing_switches_get (uint8_t channel);

#endif
