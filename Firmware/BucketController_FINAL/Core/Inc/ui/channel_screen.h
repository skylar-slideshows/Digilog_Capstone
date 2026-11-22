/**
  **********************************************************************************
  * CHANNEL SCREEN LAYOUT - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file ui/channel_screen.h
  * @brief What each channel's screen shows, laid out after the index.html mockup:
  *        - a grey title strip: channel number on the left, input type (MIC, LINE,
  *          HI-Z) on the right; it flashes lighter while the channel is selected
  *        - under it, when muted or soloed, a red MUTE or yellow SOLO bar
  *        - the EQ curve in the middle, when the knobs control the EQ
  *        - the pan bar along the bottom
  *        Muted, the rest of the screen turns dim red and the number and curve red;
  *        soloed, the same in yellow. Muted wins if both.
  *        Plain C with no hardware calls, so the PC preview runs the same code.
  **********************************************************************************
*/

#ifndef CHANNEL_SCREEN_H
#define CHANNEL_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui/ui_render.h"

/* Colours, mostly from the mockup's colour table (index.html, const C) */
#define CH_COLOR_SCREEN        UI_RGB(0x00, 0x00, 0x00) // screen background (mockup bg #08080b, black on the panel)
#define CH_COLOR_SCREEN_MUTED  UI_RGB(0x38, 0x0E, 0x0E) // background when muted
#define CH_COLOR_SCREEN_SOLO   UI_RGB(0x3A, 0x32, 0x08) // background when soloed
#define CH_COLOR_HEADER        UI_RGB(0x2C, 0x2C, 0x38) // title strip (mockup C.edge)
#define CH_COLOR_HEADER_LIT    UI_RGB(0x60, 0x60, 0x6E) // title strip's lighter flash while selected
#define CH_COLOR_NUMBER        UI_RGB(0xA0, 0xA0, 0xB4) // channel number
#define CH_COLOR_INPUT_MIC     UI_RGB(0xFF, 0x48, 0x48) // input type in the title strip: MIC red
#define CH_COLOR_INPUT_LINE    UI_RGB(0xFF, 0xFF, 0xFF) //                                LINE white
#define CH_COLOR_INPUT_HIZ     UI_RGB(0x8C, 0xFF, 0x8C) //                                HI-Z light green
#define CH_COLOR_POPUP_EQ      UI_RGB(0xD6, 0x8A, 0x3A) // value pop-up: EQ values orange (C.eq)
#define CH_COLOR_POPUP_INPUT   UI_RGB(0xFF, 0xFF, 0xFF) //                input gain white
#define CH_COLOR_POPUP_PAN     UI_RGB(0xB0, 0xB0, 0xC6) //                pan in the pan bar's colour
#define CH_COLOR_SEND_MASTER   UI_RGB(0x7F, 0xC8, 0xFF) // sends window: master level bar and its label (light blue)
#define CH_COLOR_SEND_AUX      UI_RGB(0x8C, 0xFF, 0x8C) // sends window: aux bars, labels and pans (light green)
#define CH_COLOR_POPUP_SEND    CH_COLOR_SEND_AUX        // value pop-up: send levels and pans
#define CH_COLOR_BYPASSED      UI_RGB(0x80, 0x80, 0x8C) // EQ curve and aux bars while bypassed (grey)
#define CH_COLOR_BYPASSED_FILL UI_RGB(0x30, 0x30, 0x38) // fill under the EQ curve while bypassed
#define CH_COLOR_BYPASS_TEXT   UI_RGB(0xE0, 0xE0, 0xE8) // "EQ OFF" / "SENDS OFF"
#define CH_COLOR_BADGE_48V     UI_RGB(0xFF, 0x30, 0x30) // badges under the title strip, right: +48 V red...
#define CH_COLOR_BADGE_PHASE   UI_RGB(0xFF, 0xFF, 0xFF) // ...phase white...
#define CH_COLOR_BADGE_HPF     UI_RGB(0xFF, 0xD8, 0x20) // ...HPF yellow, all with black text
#define CH_COLOR_BADGE_TEXT    UI_RGB(0x00, 0x00, 0x00)
#define CH_COLOR_HPF_LINE      CH_COLOR_BADGE_HPF       // the HPF's dotted line on the EQ graph
#define CH_COLOR_POPUP_COMP    UI_RGB(0xFF, 0xFF, 0xFF) // value pop-up: compressor knobs (white for now)
#define CH_COLOR_COMP_MAJOR    UI_RGB(0xEC, 0xEC, 0xF6) // gain-reduction meter (the mockup's colours): numbered ticks...
#define CH_COLOR_COMP_MINOR    UI_RGB(0x6E, 0x6E, 0x7E) // ...in-between ticks, the "GR" reading...
#define CH_COLOR_COMP_ARC      UI_RGB(0x3A, 0x3A, 0x48) // ...the scale's arc and the needle's hub...
#define CH_COLOR_COMP_NEEDLE   UI_RGB(0xFA, 0xF6, 0xEC) // ...and the needle
#define CH_COLOR_COMP_PEAK     UI_RGB(0xFF, 0xD8, 0x20) // detector, bottom left: PEAK yellow...
#define CH_COLOR_COMP_RMS      UI_RGB(0x7F, 0xC8, 0xFF) // ...RMS blue
#define CH_COLOR_NUMBER_MUTED  UI_RGB(0xBE, 0x5A, 0x5A) // channel number when muted
#define CH_COLOR_NUMBER_SOLO   UI_RGB(0xE6, 0xC8, 0x4A) // channel number when soloed
#define CH_COLOR_MUTE_BAR      UI_RGB(0xFF, 0x20, 0x20) // MUTE bar
#define CH_COLOR_SOLO_BAR      UI_RGB(0xFF, 0xD8, 0x20) // SOLO bar
#define CH_COLOR_BAR_TEXT      UI_RGB(0x00, 0x00, 0x00) // MUTE and SOLO lettering
#define CH_COLOR_TROUGH        UI_RGB(0x22, 0x22, 0x2A) // empty part of the pan bar (C.trough)
#define CH_COLOR_PAN           UI_RGB(0xB0, 0xB0, 0xC6) // filled part of the pan bar (C.pan)
#define CH_COLOR_CENTRE        UI_RGB(0x6E, 0x6E, 0x7E) // centre line of the pan bar (C.dim)
#define CH_COLOR_EQ            UI_RGB(0xD6, 0x8A, 0x3A) // EQ curve, and markers of bands that boost or cut (C.eq)
#define CH_COLOR_EQ_FILL       UI_RGB(0x4A, 0x30, 0x14) // between the curve and 0 dB (C.eqFill)
#define CH_COLOR_EQ_MUTED      UI_RGB(0xD6, 0x3E, 0x3E) // curve when muted (C.mute)
#define CH_COLOR_EQ_FILL_MUTED UI_RGB(0x6A, 0x1E, 0x1E) // fill when muted: lighter than the dim red screen
#define CH_COLOR_EQ_SOLO       UI_RGB(0xF0, 0xD0, 0x40) // curve when soloed
#define CH_COLOR_EQ_FILL_SOLO  UI_RGB(0x5C, 0x4C, 0x12) // fill when soloed: lighter than the dim yellow screen
#define CH_COLOR_EQ_GRID       UI_RGB(0x1A, 0x1A, 0x22) // 100 Hz, 1 kHz, 10 kHz, +/-10 and +/-20 dB lines
#define CH_COLOR_EQ_ZERO       UI_RGB(0x3A, 0x3A, 0x48) // 0 dB line, and markers of bands left at 0 dB (C.faint)

/* Title strip across the top: channel number at the left, input type at the right */
#define CH_HEADER_H    28 // rows 0-27
#define CH_TEXT_MARGIN 4  // from the left and right edges
#define CH_TEXT_Y      2  // from the top of a strip or bar
#define CH_TEXT_SIZE   24 // the 24-pixel font (12 x 24 per character)
#define CH_INPUT_SIZE  16 // input type (MIC, LINE, HI-Z): the 16-pixel font (8 x 16 per character)
#define CH_INPUT_Y     ((CH_HEADER_H - CH_INPUT_SIZE) / 2) // centred in the strip: row 6

/* Right under it, when muted or soloed: a bar the same height. Both at once: MUTE left half, SOLO right half. */
#define CH_BAR_Y       CH_HEADER_H // rows 28-55
#define CH_BAR_H       CH_HEADER_H

/* EQ graph: full width, 20 Hz to 20 kHz across, below where the bars go. Taller than the mockup's
   (rows 74-195, +/-15 dB) so a band turned all the way up or down (+/-18 dB) fits with room to spare. */
#define CH_EQ_Y        60
#define CH_EQ_H        160                     // rows 60-219
#define CH_EQ_MID      (CH_EQ_Y + CH_EQ_H / 2) // row 140 is 0 dB
#define CH_EQ_RANGE_DB 24.0f                   // the curve stops at +/-24 dB
#define CH_EQ_DB_PX    ((CH_EQ_H / 2) - 3)     // 77 rows from 0 dB to +/-24 dB
/* The height is gently squeezed for big boosts and cuts, so the small moves most settings are can
   be seen: rows = CH_EQ_DB_PX * asinh(dB / CH_EQ_SOFT_DB) / asinh(CH_EQ_RANGE_DB / CH_EQ_SOFT_DB).
   At 10 dB, 1 dB near 0 dB is about 1.5x as tall as on a straight scale, and 0 to 3 dB is about 1.65x
   the height of 12 to 15 dB. Smaller squeezes harder; much larger is close to a straight scale. */
#define CH_EQ_SOFT_DB  10.0f

/* Value pop-up while a knob is turned, in the large font: input gain and EQ values just under the title
   strip (rows 28-69: over the MUTE/SOLO bar and the top of the EQ graph while it shows); the pan just
   above the pan bar (rows 182-223, over the bottom of the EQ graph). Those rows are for a 40 px font;
   the pop-ups follow whatever size ui_font_big.h was made at. */
#define CH_POPUP_SIZE    ui_big_font_size() // UI_FONT_BIG_SIZE in ui_font_big.h
#define CH_POPUP_H       (CH_POPUP_SIZE + 2)
#define CH_POPUP_TOP_Y   CH_BAR_Y                     // rows 28-69
#define CH_POPUP_LOW_Y   (CH_PAN_Y - 1 - CH_POPUP_H)  // rows 182-223

/* Pan bar along the bottom. The centre line is in the middle column; each side has CH_PAN_HALF columns. */
#define CH_PAN_HALF      112
#define CH_PAN_X         8
#define CH_PAN_W         (2 * CH_PAN_HALF + 1) // 225: odd, so both sides are the same width
#define CH_PAN_H         11
#define CH_PAN_Y         225 // rows 225-235
#define CH_PAN_CENTRE_X  (CH_PAN_X + CH_PAN_HALF) // 120

/** Input type, shown at the right of the title strip */
typedef enum
{
    CH_INPUT_MIC,
    CH_INPUT_LINE,
    CH_INPUT_HIZ,
} channel_input_t;

/*
 * Sends window, shown in the EQ graph's area while the knobs control the sends: five bars side by side,
 * M (the channel's master level, which is its fader) then A1-A4 (the aux sends). Labels above them,
 * the pan of the stereo sends A1 and A2 below theirs. Each bar is an outline that fills from the bottom
 * with the knob's (or fader's) position; a dash to its left marks 0 dB. Fully down, a bar is empty.
 */
#define CH_SENDS_BARS        5
#define CH_STATUS_Y          (CH_POPUP_TOP_Y + CH_POPUP_H + 1) // "EQ OFF" on the EQ graph: 16 px, just under the top pop-up (row 71 at 40 px)
#define CH_SENDS_LABEL_Y     (CH_BAR_Y + CH_BAR_H + 2)        // labels, and "SENDS OFF": 16 px, rows 58-73, just under the MUTE/SOLO bar
#define CH_SENDS_BAR_TOP     (CH_SENDS_LABEL_Y + 19)          // bar outlines from row 77...
#define CH_SENDS_BAR_BOTTOM  196                              // ...to row 196 (120 rows)
/* On the sends page the top pop-up's patch reaches down over the labels too, so a pop-up never shows half a label. */
#define CH_SENDS_PAN_Y       200 // pan of A1 and A2: 16 px font, rows 200-215
#define CH_SENDS_TEXT_SIZE   16

/** The sends window worked out from the levels, ready to draw */
typedef struct
{
    uint16_t level[CH_SENDS_BARS];   //!< 0-65535: [0] master (fader), [1-4] A1-A4
    char pan_text[2][6];             //!< under A1 and A2: "C", "50L", "33R"
    ui_text_t texts[CH_SENDS_BARS + 2];
    uint8_t text_count;
    bool off;                        //!< sends bypassed: aux bars grey, "SENDS OFF"; set by the region functions
} channel_sends_view_t;

/*
 * Compressor page, in the same area: a gain-reduction meter like the mockup's. The needle rests on 0 at
 * the right and swings left to 20 dB across a 130 degree scale; ticks every 2.5 dB, numbered every 5.
 * The reading at the top left, the detector (PEAK yellow / RMS blue) at the bottom left, and "COMP OFF"
 * (needle and scale grey) while the compressor is bypassed.
 */
#define CH_COMP_PIVOT_X    120
#define CH_COMP_PIVOT_Y    206
#define CH_COMP_RADIUS     100  // the scale's arc
#define CH_COMP_NEEDLE     96   // the needle's length
#define CH_COMP_GR_MAX_DB  20.0f
#define CH_COMP_READOUT_Y  72   // "GR 4.3 dB", 16 px, top left (and "COMP OFF" further along the row)
#define CH_COMP_MODE_Y     204  // PEAK / RMS, 16 px, bottom left

/** The compressor page worked out from the gain reduction and the settings, ready to draw */
typedef struct
{
    float gr_db;              //!< as shown: 0 ... CH_COMP_GR_MAX_DB, to 0.1 dB
    float tip_x, tip_y;       //!< the needle's tip
    char readout[24];         //!< "GR 4.3 dB"
    ui_text_t texts[8];       //!< scale numbers, reading, detector, COMP OFF
    uint8_t text_count;
    bool off;                 //!< bypassed: grey, "COMP OFF"
} channel_comp_view_t;

/** Where the value pop-up is */
typedef enum
{
    CH_POPUP_NONE,
    CH_POPUP_TOP,    //!< under the title strip
    CH_POPUP_LOW,    //!< just above the pan bar
} channel_popup_where_t;

/** Everything on a channel's screen apart from the EQ curve itself */
typedef struct
{
    uint8_t number;        //!< 1-99, shown as two digits
    channel_input_t input;
    bool muted;
    bool soloed;
    bool title_lit;        //!< the selected channel's flash: title strip lighter right now
    bool show_eq;          //!< the knobs control the EQ, so the graph shows
    bool show_sends;       //!< the knobs control the sends, so the sends window shows
    bool show_comp;        //!< the knobs control the compressor, so its gain-reduction meter shows
    bool eq_off;           //!< EQ bypassed: graph grey, "EQ OFF"
    bool sends_off;        //!< sends bypassed: aux bars grey, "SENDS OFF"
    bool comp_off;         //!< compressor bypassed: meter grey, "COMP OFF"
    bool phantom;          //!< badges under the title strip, from the right: "48V"...
    bool phase;            //!< ..."PHASE"...
    bool hpf;              //!< ..."HPF" (the HPF also shows as a dotted line on the EQ graph)
    int16_t pan_fill;      //!< from channel_screen_pan_fill()
    channel_popup_where_t popup;
    const char *popup_text; //!< the value being changed, when popup isn't CH_POPUP_NONE
    uint32_t popup_rgb;
} channel_screen_state_t;

/** The EQ bands, low to high */
typedef enum
{
    CH_EQ_LF,
    CH_EQ_LMF,
    CH_EQ_HMF,
    CH_EQ_HF,
} channel_eq_band_index_t;

/*
 * Each band's frequency range, knob fully left to fully right, as the hardware is driven
 * (cv_mapping.pdf sections 6-9). The knob sweeps logarithmically between them, so 12 o'clock is
 * the middle of the range on a log scale: about 189 Hz, 671 Hz, 1.87 kHz and 4.42 kHz.
 */
#define CH_EQ_LF_MIN_HZ   65.0f
#define CH_EQ_LF_MAX_HZ   550.0f
#define CH_EQ_LMF_MIN_HZ  180.0f
#define CH_EQ_LMF_MAX_HZ  2500.0f
#define CH_EQ_HMF_MIN_HZ  500.0f
#define CH_EQ_HMF_MAX_HZ  7000.0f
#define CH_EQ_HF_MIN_HZ   1500.0f
#define CH_EQ_HF_MAX_HZ   13000.0f

/** One EQ band as stored in the control values (eq_band_control_t in control_interface.h) */
typedef struct
{
    int16_t gain;
    uint16_t q;
    uint16_t freq;
    bool bell;     //!< LF/HF only: bell instead of shelf
} channel_eq_band_t;

/** A channel's EQ settings as stored in the control values */
typedef struct
{
    channel_eq_band_t hf, hmf, lmf, lf;
    bool hpf;
} channel_eq_t;

/** The EQ graph worked out from the settings, ready to draw. Large, so keep one and reuse it. */
typedef struct
{
    int16_t fill_from[UI_SCREEN_W], fill_to[UI_SCREEN_W]; //!< rows between the curve and 0 dB, per column
    int16_t line_from[UI_SCREEN_W], line_to[UI_SCREEN_W]; //!< rows the 2-pixel curve covers, per column
    int16_t marker_x[4];                                   //!< band markers along the bottom: LF, LMF, HMF, HF
    bool marker_active[4];                                 //!< the band boosts or cuts (more than 0.5 dB)
    uint32_t curve_rgb, fill_rgb;                          //!< set by the region functions from the screen state
    bool off;                                              //!< EQ bypassed: "EQ OFF" over the graph
    /* The HPF isn't part of the curve above: it's a dotted yellow line showing the whole response with it,
       drawn only where the HPF makes a difference. [0]: with the EQ on (bands + HPF); [1]: EQ off (HPF alone). */
    int16_t hpf_cols;                                      //!< it's drawn in columns 0 .. hpf_cols-1 (0: HPF off)
    int16_t hpf_row[2][UI_SCREEN_W];                       //!< the line's row in each column
} channel_eq_view_t;

/** Everything worked out for the middle of the screen: the EQ graph and the sends window. Keep one per screen. */
typedef struct
{
    channel_eq_view_t eq;
    channel_sends_view_t sends;
    channel_comp_view_t comp;
} channel_views_t;

/** Memory for the shapes of a whole screen; keep it alive until the region has been drawn. */
typedef struct
{
    ui_text_t texts[10];
    char number[4];
    ui_rect_t rects[12];
} channel_screen_parts_t;

/** Memory for the bar slot under the title strip (MUTE/SOLO bars and the badges, or the pop-up) */
typedef struct
{
    ui_rect_t rects[5];
    ui_text_t texts[5];
} channel_bar_parts_t;

/** Memory for what's drawn over the middle of the screen: the pop-ups and the stacked badges */
typedef struct
{
    ui_rect_t rects[5];
    ui_text_t texts[5];
} channel_popup_parts_t;

/** Memory for the shapes of the title strip on its own. */
typedef struct
{
    ui_text_t texts[2];
    char number[4];
    ui_rect_t rects[1];
} channel_header_parts_t;

/** Memory for the shapes of the pan bar on its own. */
typedef struct
{
    ui_rect_t rects[2];
} channel_pan_parts_t;

/* Control values to dB, Hz and Q, the same as the hardware (cv_mapping.pdf) */
float channel_eq_gain_db (int16_t gain, bool shelf);                     //!< INT16_MIN..INT16_MAX -> -15..+15 dB, tapered as the hardware
float channel_eq_freq_hz (channel_eq_band_index_t band, uint16_t freq); //!< 0..65535 -> the band's range above
float channel_eq_q (uint16_t q);                                         //!< 0..65535 -> Q 0.5..5, logarithmic; the middle is 1.58
#define CH_EQ_LF_HF_BELL_Q 1.0f // LF/HF in bell mode have no Q control; drawn with this Q

/** Works out the EQ curve for a channel's settings, the same way as the mockup's eqMagnitude(),
 *  but limited to +/-CH_EQ_RANGE_DB instead of +/-15 dB. */
void channel_screen_eq_compute (channel_eq_view_t *view, const channel_eq_t *eq);

/**
 * Works out the sends window: fader is the channel's output_gain control value, send_level the four sends'
 * gain values (0 = off), send_pan the pans of sends 1 and 2.
 */
void channel_screen_sends_compute (channel_sends_view_t *view, int16_t fader, const uint16_t send_level[4],
                                   const int16_t send_pan[2]);

/** Works out the compressor page: gain reduction in dB (clamped to the meter), detector, bypass */
void channel_screen_comp_compute (channel_comp_view_t *view, float gr_db, bool rms, bool off);

/** The box the needle (and its hub) covers, for redrawing just around it: x0, y0, x1, y1 */
void channel_screen_comp_needle_box (const channel_comp_view_t *view, int16_t box[4]);

/** Pan as text: "C", or how far from the centre in percent and which side, "50L", "33R". */
void channel_screen_pan_text (char *out, size_t size, int16_t pan);

/** The whole screen: the EQ graph, the sends window or neither in the middle, depending on the state. */
void channel_screen_full (ui_region_t *region, channel_screen_parts_t *parts, const channel_screen_state_t *state,
                          channel_views_t *views);

/** Just the title strip, for when only the input type or the selection flash changed. */
void channel_screen_header (ui_region_t *region, channel_header_parts_t *parts, const channel_screen_state_t *state);

/** Just the middle of the screen (rows 56-219): the EQ graph, the sends window, the compressor meter or nothing,
 *  plus anything over it (pop-ups, badges). */
void channel_screen_middle (ui_region_t *region, channel_popup_parts_t *parts, channel_views_t *views,
                            const channel_screen_state_t *state);

/** The same, but only the part inside x0..x1, y0..y1 (clipped to the middle), e.g. around the meter's needle */
void channel_screen_middle_part (ui_region_t *region, channel_popup_parts_t *parts, channel_views_t *views,
                                 const channel_screen_state_t *state, int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/** Just the area of the top pop-up (rows 28-69): the pop-up if showing, otherwise the MUTE/SOLO bars (or
 *  nothing) and the top of the EQ graph. */
void channel_screen_bar_slot (ui_region_t *region, channel_bar_parts_t *parts, channel_views_t *views,
                              const channel_screen_state_t *state);

/** Just the strip above the pan bar: the lower pop-up if showing, otherwise whatever is underneath it. */
void channel_screen_low_popup_area (ui_region_t *region, channel_popup_parts_t *parts, channel_views_t *views,
                                    const channel_screen_state_t *state);

/**
 * Converts a pan control value (INT16_MIN = hard left, 0 = centre, INT16_MAX = hard right) into
 * how many columns of the bar to fill: -CH_PAN_HALF to +CH_PAN_HALF, negative to the left.
 * Like the pan knob's LED ring, hard left or right fills one half of the bar and no more.
 */
int16_t channel_screen_pan_fill (int16_t pan);

/** Just the pan bar, for when only the pan changed. */
void channel_screen_pan (ui_region_t *region, channel_pan_parts_t *parts, int16_t fill);

#endif