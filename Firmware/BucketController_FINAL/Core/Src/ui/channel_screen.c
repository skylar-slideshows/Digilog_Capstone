/**
  **********************************************************************************
  * CHANNEL SCREEN LAYOUT - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file ui/channel_screen.c
  * @brief See channel_screen.h.
  **********************************************************************************
*/

#include "ui/channel_screen.h"
#include "cv_mapping.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>

/* EQ graph details from the mockup's drawEq() */
#define EQ_LAST_ROW   (CH_EQ_Y + CH_EQ_H - 1)  // 219
#define EQ_GRID_X1    56                       // 100 Hz
#define EQ_GRID_X2    135                      // 1 kHz
#define EQ_GRID_X3    215                      // 10 kHz
#define EQ_MARKER_H   3                        // band markers: the bottom 3 rows, 2 columns wide

static const char *const INPUT_NAMES[] = {
    [CH_INPUT_MIC] = "MIC",
    [CH_INPUT_LINE] = "LINE",
    [CH_INPUT_HIZ] = "HI-Z",
};

static float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/** Screen row of a level in dB (0 dB is CH_EQ_MID), not rounded */
static float db_row (float db)
{
    static float scale = 0.0f; // rows per unit of asinh, worked out once
    if (scale == 0.0f) scale = (float)CH_EQ_DB_PX / asinhf(CH_EQ_RANGE_DB / CH_EQ_SOFT_DB);
    return (float)CH_EQ_MID - scale * asinhf(db / CH_EQ_SOFT_DB);
}

/* Rows of the +/-10 dB and +/-20 dB grid lines, worked out once */
static int16_t s_grid_rows[4];
static bool s_grid_ready;


/*=============================== COLOURS BY STATE ================================*/

static uint32_t background (const channel_screen_state_t *s)
{
    return s->muted ? CH_COLOR_SCREEN_MUTED : (s->soloed ? CH_COLOR_SCREEN_SOLO : CH_COLOR_SCREEN);
}

static uint32_t number_colour (const channel_screen_state_t *s)
{
    return s->muted ? CH_COLOR_NUMBER_MUTED : (s->soloed ? CH_COLOR_NUMBER_SOLO : CH_COLOR_NUMBER);
}

/** "EQ OFF" / "SENDS OFF", centred on the status row */
static ui_text_t bypass_text (const char *text)
{
    return (ui_text_t){(int16_t)((UI_SCREEN_W - ui_text_width(text, CH_SENDS_TEXT_SIZE)) / 2), CH_STATUS_Y, CH_SENDS_TEXT_SIZE,
                       CH_COLOR_BYPASS_TEXT, text};
}

static void set_eq_colours (channel_eq_view_t *eq, const channel_screen_state_t *s)
{
    eq->off = s->eq_off;
    if (s->eq_off) // bypassed: the graph goes grey; the curve still shows the stored settings
    {
        eq->curve_rgb = CH_COLOR_BYPASSED;
        eq->fill_rgb = CH_COLOR_BYPASSED_FILL;
        return;
    }
    eq->curve_rgb = s->muted ? CH_COLOR_EQ_MUTED : (s->soloed ? CH_COLOR_EQ_SOLO : CH_COLOR_EQ);
    eq->fill_rgb = s->muted ? CH_COLOR_EQ_FILL_MUTED : (s->soloed ? CH_COLOR_EQ_FILL_SOLO : CH_COLOR_EQ_FILL);
}


/*=============================== EQ CURVE ================================*/

float channel_eq_gain_db (int16_t gain, bool shelf)
{
    return cv_eq_gain_db(cv_signed_to_p(gain), shelf); // the same tapered law the CVs use
}
float channel_eq_q (uint16_t q) { return 0.5f * powf(10.0f, (float)q / 65535.0f); }

float channel_eq_freq_hz (channel_eq_band_index_t band, uint16_t freq)
{
    static const float range[4][2] = {
        [CH_EQ_LF] = {CH_EQ_LF_MIN_HZ, CH_EQ_LF_MAX_HZ},
        [CH_EQ_LMF] = {CH_EQ_LMF_MIN_HZ, CH_EQ_LMF_MAX_HZ},
        [CH_EQ_HMF] = {CH_EQ_HMF_MIN_HZ, CH_EQ_HMF_MAX_HZ},
        [CH_EQ_HF] = {CH_EQ_HF_MIN_HZ, CH_EQ_HF_MAX_HZ},
    };
    const float low = range[band][0], high = range[band][1];
    return low * powf(high / low, (float)freq / 65535.0f);
}

typedef struct
{
    float gain_db, freq_hz, q;
    bool bell;
} band_t;

/** has_q: LMF/HMF (always bells with a Q knob); otherwise LF/HF, a shelf unless switched to bell */
static band_t band (const channel_eq_band_t *b, channel_eq_band_index_t index, bool has_q)
{
    return (band_t){
        channel_eq_gain_db(b->gain, !has_q && !b->bell), // shelves taper differently from bells
        channel_eq_freq_hz(index, b->freq),
        has_q ? channel_eq_q(b->q) : CH_EQ_LF_HF_BELL_Q,
        has_q || b->bell,
    };
}

static float bell (const band_t *b, float f)
{
    const float octaves = log2f(f / b->freq_hz);
    const float width = 0.9f / fmaxf(0.3f, b->q);
    return b->gain_db * expf(-(octaves * octaves) / (2.0f * width * width));
}

static float shelf (const band_t *b, float f, bool high)
{
    return b->gain_db * 0.5f * (1.0f + tanhf(1.4f * log2f(f / b->freq_hz) * (high ? 1.0f : -1.0f)));
}

/** The four bands' response in dB at frequency f, as the mockup's eqMagnitude(); not limited */
static float bands_db (const band_t bands[4], float f)
{
    float db = bands[CH_EQ_HF].bell ? bell(&bands[CH_EQ_HF], f) : shelf(&bands[CH_EQ_HF], f, true);
    db += bell(&bands[CH_EQ_HMF], f) + bell(&bands[CH_EQ_LMF], f);
    db += bands[CH_EQ_LF].bell ? bell(&bands[CH_EQ_LF], f) : shelf(&bands[CH_EQ_LF], f, false);
    return db;
}

/** The HPF's own response in dB at f: 12 dB/octave below 80 Hz, as the mockup */
static float hpf_db (float f)
{
    const float x = powf(f / 80.0f, 4.0f);
    return 10.0f * log10f(x / (1.0f + x));
}

static float limited (float db) { return clampf(db, -CH_EQ_RANGE_DB, CH_EQ_RANGE_DB); }

void channel_screen_eq_compute (channel_eq_view_t *view, const channel_eq_t *eq)
{
    const band_t bands[4] = {
        [CH_EQ_LF] = band(&eq->lf, CH_EQ_LF, false),
        [CH_EQ_LMF] = band(&eq->lmf, CH_EQ_LMF, true),
        [CH_EQ_HMF] = band(&eq->hmf, CH_EQ_HMF, true),
        [CH_EQ_HF] = band(&eq->hf, CH_EQ_HF, false),
    };

    static float y[UI_SCREEN_W]; // the curve's row in each column (static: too big for a task's stack)
    view->hpf_cols = 0;
    for (int16_t x = 0; x < UI_SCREEN_W; x++)
    {
        const float f = 20.0f * powf(1000.0f, (float)x / (float)(UI_SCREEN_W - 1));
        const float eq_db = bands_db(bands, f);
        y[x] = db_row(limited(eq_db)); // the bands only: the HPF isn't drawn as part of the EQ
        if (eq->hpf)
        {
            const float h = hpf_db(f);
            view->hpf_row[0][x] = (int16_t)lroundf(db_row(limited(eq_db + h))); // EQ on: what you hear
            view->hpf_row[1][x] = (int16_t)lroundf(db_row(limited(h)));         // EQ off: the HPF alone
            if (h < -0.25f) view->hpf_cols = (int16_t)(x + 1);                   // where it makes a difference
        }
    }

    for (int16_t x = 0; x < UI_SCREEN_W; x++)
    {
        // between the curve and 0 dB (the line between rows CH_EQ_MID-1 and CH_EQ_MID), by pixel centre
        if (y[x] < (float)CH_EQ_MID)
        {
            view->fill_from[x] = (int16_t)ceilf(y[x] - 0.5f);
            view->fill_to[x] = CH_EQ_MID - 1;
        }
        else
        {
            view->fill_from[x] = CH_EQ_MID;
            view->fill_to[x] = (int16_t)(ceilf(y[x] - 0.5f) - 1.0f);
        }

        // a 2-pixel line through the curve, reaching halfway to the neighbouring columns so steep parts join up
        const float left = x > 0 ? (y[x - 1] + y[x]) * 0.5f : y[x];
        const float right = x < UI_SCREEN_W - 1 ? (y[x] + y[x + 1]) * 0.5f : y[x];
        const float top = fminf(y[x], fminf(left, right)) - 1.0f;
        const float bottom = fmaxf(y[x], fmaxf(left, right)) + 1.0f;
        view->line_from[x] = (int16_t)ceilf(top - 0.5f);
        view->line_to[x] = (int16_t)floorf(bottom - 0.5f);
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        const float x = log10f(bands[i].freq_hz / 20.0f) / 3.0f * (float)(UI_SCREEN_W - 1);
        view->marker_x[i] = (int16_t)lroundf(clampf(x - 1.0f, 0.0f, (float)(UI_SCREEN_W - 2)));
        view->marker_active[i] = fabsf(bands[i].gain_db) > 0.5f;
    }

    if (!s_grid_ready)
    {
        const float levels[4] = {20.0f, 10.0f, -10.0f, -20.0f};
        for (uint8_t i = 0; i < 4; i++) s_grid_rows[i] = (int16_t)lroundf(db_row(levels[i]));
        s_grid_ready = true;
    }
}

/** ui_region_t painter for the EQ graph; draws nothing outside the graph's rows */
static void paint_eq (const void *context, int16_t y, uint32_t *row, int16_t x0, int16_t x1)
{
    const channel_eq_view_t *view = context;
    if (y < CH_EQ_Y || y > EQ_LAST_ROW) return;

    const bool grid_row = (y == s_grid_rows[0] || y == s_grid_rows[1] || y == s_grid_rows[2] || y == s_grid_rows[3]);
    const bool marker_row = (y > EQ_LAST_ROW - EQ_MARKER_H);
    for (int16_t x = x0; x <= x1; x++)
    {
        uint32_t *px = &row[x - x0];
        if (grid_row || x == EQ_GRID_X1 || x == EQ_GRID_X2 || x == EQ_GRID_X3) *px = CH_COLOR_EQ_GRID;
        if (y == CH_EQ_MID) *px = CH_COLOR_EQ_ZERO;
        if (y >= view->fill_from[x] && y <= view->fill_to[x]) *px = view->fill_rgb;
        if (y >= view->line_from[x] && y <= view->line_to[x]) *px = view->curve_rgb;
        if (marker_row)
        {
            for (uint8_t i = 0; i < 4; i++)
            {
                if (x == view->marker_x[i] || x == view->marker_x[i] + 1)
                {
                    *px = view->marker_active[i] ? view->curve_rgb : CH_COLOR_EQ_ZERO;
                }
            }
        }
    }
    // the HPF: a dotted yellow line, 2 pixels on and 2 off, never filled; still yellow when the EQ is off
    if (view->hpf_cols > 0)
    {
        const int16_t *line = view->hpf_row[view->off ? 1 : 0];
        for (int16_t x = x0; x <= x1 && x < view->hpf_cols; x++)
        {
            if ((x & 3) >= 2) continue;
            // the line's row here, reaching halfway to its neighbours so steep parts stay joined
            const int16_t here = line[x];
            const int16_t left = x > 0 ? (int16_t)((line[x - 1] + here) / 2) : here;
            const int16_t right = x < UI_SCREEN_W - 1 ? (int16_t)((line[x + 1] + here) / 2) : here;
            const int16_t top = here < left ? (here < right ? here : right) : (left < right ? left : right);
            const int16_t bottom = here > left ? (here > right ? here : right) : (left > right ? left : right);
            if (y >= top && y <= bottom) row[x - x0] = CH_COLOR_HPF_LINE;
        }
    }
    if (view->off)
    {
        const ui_text_t status = bypass_text("EQ OFF");
        ui_draw_text_row(&status, y, row, x0, x1);
    }
}


/*=============================== SENDS WINDOW ================================*/

/* Bars left to right: M (wider), A1-A4. A dash to each bar's left marks 0 dB. */
static const int16_t SEND_BAR_X[CH_SENDS_BARS] = {20, 68, 112, 156, 200};
static const int16_t SEND_BAR_W[CH_SENDS_BARS] = {32, 24, 24, 24, 24};
static const char *const SEND_LABELS[CH_SENDS_BARS] = {"M", "A1", "A2", "A3", "A4"};
#define SEND_INNER_H (CH_SENDS_BAR_BOTTOM - CH_SENDS_BAR_TOP - 1) // rows inside the outline
#define SEND_DASH_W  5                                            // the 0 dB dash, ending 2 px before the bar

/* Knob (or fader) position of 0 dB: the fader's mark at P 44380 (cv_mapping.pdf section 18); for the sends
   L = 10 + 60 log10(x) is 0 dB at x = 10^(-1/6) (sections 12-15). */
#define MASTER_0DB_X (44380.0f / 65535.0f)
#define SEND_0DB_X   0.68129207f

static uint32_t send_colour (uint8_t bar) { return bar == 0 ? CH_COLOR_SEND_MASTER : CH_COLOR_SEND_AUX; }

/** The colour a bar is drawn in: the aux bars go grey while the sends are bypassed (M doesn't) */
static uint32_t send_draw_colour (const channel_sends_view_t *view, uint8_t bar)
{
    return (view->off && bar != 0) ? CH_COLOR_BYPASSED : send_colour(bar);
}

/** Rows of a bar's inner area that are filled, from the bottom, for a position 0-65535 */
static int16_t send_fill_rows (uint16_t level) { return (int16_t)lroundf((float)level / 65535.0f * (float)SEND_INNER_H); }

/** Row of the 0 dB dash */
static int16_t send_dash_row (uint8_t bar)
{
    const float x = bar == 0 ? MASTER_0DB_X : SEND_0DB_X;
    return (int16_t)(CH_SENDS_BAR_BOTTOM - 1 - lroundf(x * (float)SEND_INNER_H));
}

void channel_screen_pan_text (char *out, size_t size, int16_t pan)
{
    const uint16_t p = (uint16_t)((uint16_t)pan ^ 0x8000U);
    const float c = p <= 32767U ? ((float)p - 32767.0f) / 32767.0f : ((float)p - 32767.0f) / 32768.0f;
    const long percent = lroundf(fabsf(c) * 100.0f);
    if (percent == 0) snprintf(out, size, "C");
    else snprintf(out, size, "%ld%c", percent, c < 0.0f ? 'L' : 'R');
}

void channel_screen_sends_compute (channel_sends_view_t *view, int16_t fader, const uint16_t send_level[4],
                                   const int16_t send_pan[2])
{
    view->level[0] = (uint16_t)((uint16_t)fader ^ 0x8000U); // output_gain is signed; its knob position is P
    for (uint8_t i = 0; i < 4; i++) view->level[i + 1] = send_level[i];
    channel_screen_pan_text(view->pan_text[0], sizeof(view->pan_text[0]), send_pan[0]);
    channel_screen_pan_text(view->pan_text[1], sizeof(view->pan_text[1]), send_pan[1]);

    uint8_t n = 0;
    for (uint8_t bar = 0; bar < CH_SENDS_BARS; bar++)
    {
        const char *label = SEND_LABELS[bar];
        const int16_t centre = (int16_t)(SEND_BAR_X[bar] + SEND_BAR_W[bar] / 2);
        view->texts[n++] = (ui_text_t){(int16_t)(centre - ui_text_width(label, CH_SENDS_TEXT_SIZE) / 2), CH_SENDS_LABEL_Y,
                                       CH_SENDS_TEXT_SIZE, send_colour(bar), label};
    }
    for (uint8_t i = 0; i < 2; i++) // A1 and A2 are stereo: their pan under the bar
    {
        const int16_t centre = (int16_t)(SEND_BAR_X[i + 1] + SEND_BAR_W[i + 1] / 2);
        view->texts[n++] = (ui_text_t){(int16_t)(centre - ui_text_width(view->pan_text[i], CH_SENDS_TEXT_SIZE) / 2),
                                       CH_SENDS_PAN_Y, CH_SENDS_TEXT_SIZE, CH_COLOR_SEND_AUX, view->pan_text[i]};
    }
    view->text_count = n;
}

/** ui_region_t painter for the sends window; draws nothing outside its rows */
static void paint_sends (const void *context, int16_t y, uint32_t *row, int16_t x0, int16_t x1)
{
    const channel_sends_view_t *view = context;
    if (y < CH_SENDS_LABEL_Y || y > CH_EQ_Y + CH_EQ_H - 1) return; // the labels start a little above the graph's area

    for (uint8_t bar = 0; bar < CH_SENDS_BARS; bar++)
    {
        const uint32_t colour = send_draw_colour(view, bar);
        const int16_t left = SEND_BAR_X[bar], right = (int16_t)(SEND_BAR_X[bar] + SEND_BAR_W[bar] - 1);
        if (y == send_dash_row(bar))
        {
            for (int16_t x = (int16_t)(left - 2 - SEND_DASH_W); x < left - 2; x++)
            {
                if (x >= x0 && x <= x1) row[x - x0] = colour;
            }
        }
        if (y < CH_SENDS_BAR_TOP || y > CH_SENDS_BAR_BOTTOM) continue;

        const bool edge = (y == CH_SENDS_BAR_TOP || y == CH_SENDS_BAR_BOTTOM);
        const bool filled = y >= CH_SENDS_BAR_BOTTOM - send_fill_rows(view->level[bar]); // fills up from the bottom
        for (int16_t x = left; x <= right; x++)
        {
            if (x < x0 || x > x1) continue;
            if (edge || x == left || x == right || filled) row[x - x0] = colour;
        }
    }
    for (uint8_t t = 0; t < view->text_count; t++)
    {
        ui_text_t text = view->texts[t];
        if (view->off && t >= 1 && t < CH_SENDS_BARS) continue;                        // A1-A4: "SENDS OFF" instead
        if (view->off && text.rgb == CH_COLOR_SEND_AUX) text.rgb = CH_COLOR_BYPASSED;  // the pans of A1 and A2
        ui_draw_text_row(&text, y, row, x0, x1);
    }
    if (view->off)
    {
        static const char status_text[] = "SENDS OFF";
        const int16_t aux_centre = (int16_t)((SEND_BAR_X[1] + SEND_BAR_X[CH_SENDS_BARS - 1] + SEND_BAR_W[CH_SENDS_BARS - 1]) / 2);
        const ui_text_t status = {(int16_t)(aux_centre - ui_text_width(status_text, CH_SENDS_TEXT_SIZE) / 2), CH_SENDS_LABEL_Y,
                                  CH_SENDS_TEXT_SIZE, CH_COLOR_BYPASS_TEXT, status_text};
        ui_draw_text_row(&status, y, row, x0, x1);
    }
}

/* The middle of the screen shows whichever window the knobs control: EQ graph, sends, or nothing */
static ui_paint_fn middle_paint (const channel_screen_state_t *s)
{
    return s->show_eq ? paint_eq : (s->show_sends ? paint_sends : NULL);
}

static const void *middle_context (const channel_screen_state_t *s, channel_views_t *views)
{
    if (s->show_eq)
    {
        set_eq_colours(&views->eq, s);
        return &views->eq;
    }
    views->sends.off = s->sends_off;
    return &views->sends;
}


/*=============================== SHAPES ================================*/

/** Title strip, number and input type. Returns the shape counts through rect_count and text_count. */
static void header_shapes (const channel_screen_state_t *s, char number[4], ui_rect_t *rects, uint8_t *rect_count,
                           ui_text_t *texts, uint8_t *text_count)
{
    const uint8_t n = (uint8_t)(s->number % 100U);
    number[0] = (char)('0' + n / 10U);
    number[1] = (char)('0' + n % 10U);
    number[2] = '\0';

    const channel_input_t which = s->input <= CH_INPUT_HIZ ? s->input : CH_INPUT_LINE;
    const char *input = INPUT_NAMES[which];
    static const uint32_t INPUT_COLOURS[] = {
        [CH_INPUT_MIC] = CH_COLOR_INPUT_MIC,
        [CH_INPUT_LINE] = CH_COLOR_INPUT_LINE,
        [CH_INPUT_HIZ] = CH_COLOR_INPUT_HIZ,
    };

    rects[(*rect_count)++] = (ui_rect_t){0, 0, UI_SCREEN_W - 1, CH_HEADER_H - 1, s->title_lit ? CH_COLOR_HEADER_LIT : CH_COLOR_HEADER};
    texts[(*text_count)++] = (ui_text_t){CH_TEXT_MARGIN, CH_TEXT_Y, CH_TEXT_SIZE, number_colour(s), number};
    texts[(*text_count)++] = (ui_text_t){
        (int16_t)(UI_SCREEN_W - CH_TEXT_MARGIN - ui_text_width(input, CH_INPUT_SIZE)), CH_INPUT_Y, CH_INPUT_SIZE, INPUT_COLOURS[which], input,
    };
}

/** The MUTE/SOLO bars for the slot under the title strip (none if neither) */
static void bars_for (const channel_screen_state_t *state, int16_t right, ui_rect_t *rects, uint8_t *rect_count,
                      ui_text_t *texts, uint8_t *text_count);

/* Badges under the title strip on the right, as tall as the strip: "48V" rightmost, then "PHASE", then
   "HPF", each only while it's on; the ones still showing close up to the right. The MUTE/SOLO bar takes
   what's left of the row. */
#define BADGE_TEXT_SIZE 16
#define BADGE_PAD       5 // each side of the text
#define BADGE_GAP       2

/** Adds the badges that are on; returns the column just left of them (the right edge for the bars) */
static int16_t badge_shapes (const channel_screen_state_t *s, ui_rect_t *rects, uint8_t *rect_count, ui_text_t *texts,
                             uint8_t *text_count)
{
    const struct { bool on; const char *text; uint32_t rgb; } badges[3] = {
        {s->phantom, "48V", CH_COLOR_BADGE_48V},
        {s->phase, "PHASE", CH_COLOR_BADGE_PHASE},
        {s->hpf, "HPF", CH_COLOR_BADGE_HPF},
    };
    int16_t right = UI_SCREEN_W - 1;
    for (uint8_t i = 0; i < 3; i++)
    {
        if (!badges[i].on) continue;
        const int16_t width = (int16_t)(ui_text_width(badges[i].text, BADGE_TEXT_SIZE) + 2 * BADGE_PAD);
        const int16_t left = (int16_t)(right - width + 1);
        rects[(*rect_count)++] = (ui_rect_t){left, CH_BAR_Y, right, CH_BAR_Y + CH_BAR_H - 1, badges[i].rgb};
        texts[(*text_count)++] = (ui_text_t){(int16_t)(left + BADGE_PAD), (int16_t)(CH_BAR_Y + (CH_BAR_H - BADGE_TEXT_SIZE) / 2),
                                             BADGE_TEXT_SIZE, CH_COLOR_BADGE_TEXT, badges[i].text};
        right = (int16_t)(left - BADGE_GAP - 1);
    }
    return right;
}

/** A pop-up's text in the large font, centred across the screen at row y */
static ui_text_t popup_text (const channel_screen_state_t *s, int16_t y)
{
    const char *text = s->popup_text != NULL ? s->popup_text : "";
    return (ui_text_t){(int16_t)((UI_SCREEN_W - ui_text_width(text, CH_POPUP_SIZE)) / 2), y, CH_POPUP_SIZE, s->popup_rgb, text};
}

/** Last row of the top pop-up's patch: on the sends page it also covers the labels, so they're never cut in half */
static int16_t top_popup_last_row (const channel_screen_state_t *s)
{
    const int16_t normal = (int16_t)(CH_POPUP_TOP_Y + CH_POPUP_H - 1);
    const int16_t labels = (int16_t)(CH_SENDS_LABEL_Y + CH_SENDS_TEXT_SIZE);
    return (s->show_sends && labels > normal) ? labels : normal;
}

/** A pop-up: a patch of background (hiding what's under it) from row y to last, with the value on it */
static void popup_shapes (const channel_screen_state_t *s, int16_t y, int16_t last, ui_rect_t *rects, uint8_t *rect_count,
                          ui_text_t *texts, uint8_t *text_count)
{
    rects[(*rect_count)++] = (ui_rect_t){0, y, UI_SCREEN_W - 1, last, background(s)};
    texts[(*text_count)++] = popup_text(s, (int16_t)(y + 1));
}

/** Under the title strip: the top pop-up while it shows, otherwise the MUTE/SOLO bars */
static void bar_slot_shapes (const channel_screen_state_t *s, ui_rect_t *rects, uint8_t *rect_count, ui_text_t *texts,
                             uint8_t *text_count)
{
    if (s->popup == CH_POPUP_TOP)
    {
        popup_shapes(s, CH_POPUP_TOP_Y, top_popup_last_row(s), rects, rect_count, texts, text_count);
    }
    else
    {
        const int16_t right = badge_shapes(s, rects, rect_count, texts, text_count);
        bars_for(s, right, rects, rect_count, texts, text_count); // MUTE/SOLO move over for the badges
    }
}

/** Just above the pan bar: the lower pop-up while it shows */
static void low_popup_shapes (const channel_screen_state_t *s, ui_rect_t *rects, uint8_t *rect_count, ui_text_t *texts,
                              uint8_t *text_count)
{
    if (s->popup == CH_POPUP_LOW)
    {
        popup_shapes(s, CH_POPUP_LOW_Y, (int16_t)(CH_POPUP_LOW_Y + CH_POPUP_H - 1), rects, rect_count, texts, text_count);
    }
}

/** A bar under the title strip from column x0 to x1 with a word centred on it */
static void bar_shapes (int16_t x0, int16_t x1, uint32_t rgb, const char *word, ui_rect_t *rects, uint8_t *rect_count,
                        ui_text_t *texts, uint8_t *text_count)
{
    rects[(*rect_count)++] = (ui_rect_t){x0, CH_BAR_Y, x1, CH_BAR_Y + CH_BAR_H - 1, rgb};
    const int16_t centre = (int16_t)((x0 + x1 + 1) / 2);
    texts[(*text_count)++] = (ui_text_t){
        (int16_t)(centre - ui_text_width(word, CH_TEXT_SIZE) / 2), CH_BAR_Y + CH_TEXT_Y, CH_TEXT_SIZE, CH_COLOR_BAR_TEXT, word,
    };
}

/** The pan bar's shapes; with_trough adds the empty bar itself as the first rectangle. Returns how many. */
static uint8_t pan_rects (ui_rect_t *rects, int16_t fill, bool with_trough)
{
    const int16_t top = CH_PAN_Y;
    const int16_t bottom = CH_PAN_Y + CH_PAN_H - 1;
    uint8_t count = 0;

    if (with_trough)
    {
        rects[count++] = (ui_rect_t){CH_PAN_X, top, CH_PAN_X + CH_PAN_W - 1, bottom, CH_COLOR_TROUGH};
    }
    if (fill < 0) // left of the centre line
    {
        rects[count++] = (ui_rect_t){(int16_t)(CH_PAN_CENTRE_X + fill), top, CH_PAN_CENTRE_X - 1, bottom, CH_COLOR_PAN};
    }
    else if (fill > 0) // right of it
    {
        rects[count++] = (ui_rect_t){CH_PAN_CENTRE_X + 1, top, (int16_t)(CH_PAN_CENTRE_X + fill), bottom, CH_COLOR_PAN};
    }
    rects[count++] = (ui_rect_t){CH_PAN_CENTRE_X, top, CH_PAN_CENTRE_X, bottom, CH_COLOR_CENTRE}; // always shown
    return count;
}


/*=============================== REGIONS ================================*/

void channel_screen_full (ui_region_t *region, channel_screen_parts_t *parts, const channel_screen_state_t *state,
                          channel_views_t *views)
{
    uint8_t rect_count = 0, text_count = 0;
    header_shapes(state, parts->number, parts->rects, &rect_count, parts->texts, &text_count);
    bar_slot_shapes(state, parts->rects, &rect_count, parts->texts, &text_count);
    low_popup_shapes(state, parts->rects, &rect_count, parts->texts, &text_count);
    rect_count = (uint8_t)(rect_count + pan_rects(&parts->rects[rect_count], state->pan_fill, true));

    *region = (ui_region_t){
        .x0 = 0,
        .y0 = 0,
        .x1 = UI_SCREEN_W - 1,
        .y1 = UI_SCREEN_H - 1,
        .background = background(state),
        .rects = parts->rects,
        .rect_count = rect_count,
        .texts = parts->texts,
        .text_count = text_count,
        .paint = middle_paint(state), // EQ graph, sends window, or empty
        .paint_context = middle_context(state, views),
    };
}

void channel_screen_header (ui_region_t *region, channel_header_parts_t *parts, const channel_screen_state_t *state)
{
    uint8_t rect_count = 0, text_count = 0;
    header_shapes(state, parts->number, parts->rects, &rect_count, parts->texts, &text_count);

    *region = (ui_region_t){
        .x0 = 0,
        .y0 = 0,
        .x1 = UI_SCREEN_W - 1,
        .y1 = CH_HEADER_H - 1,
        .background = CH_COLOR_HEADER,
        .rects = parts->rects,
        .rect_count = rect_count,
        .texts = parts->texts,
        .text_count = text_count,
    };
}

/** The MUTE/SOLO bar from column 0 to right (where the badges begin) */
static void bars_for (const channel_screen_state_t *state, int16_t right, ui_rect_t *rects, uint8_t *rect_count,
                      ui_text_t *texts, uint8_t *text_count)
{
    if (right < 0) return;
    if (state->muted && state->soloed) // both: share the bar
    {
        const int16_t split = (int16_t)((right + 1) / 2);
        bar_shapes(0, (int16_t)(split - 1), CH_COLOR_MUTE_BAR, "MUTE", rects, rect_count, texts, text_count);
        bar_shapes(split, right, CH_COLOR_SOLO_BAR, "SOLO", rects, rect_count, texts, text_count);
    }
    else if (state->muted)
    {
        bar_shapes(0, right, CH_COLOR_MUTE_BAR, "MUTE", rects, rect_count, texts, text_count);
    }
    else if (state->soloed)
    {
        bar_shapes(0, right, CH_COLOR_SOLO_BAR, "SOLO", rects, rect_count, texts, text_count);
    }
}

void channel_screen_middle (ui_region_t *region, channel_popup_parts_t *parts, channel_views_t *views,
                            const channel_screen_state_t *state)
{
    uint8_t rect_count = 0, text_count = 0;
    // either pop-up reaches into the graph's area: drawn over the curve
    if (state->popup == CH_POPUP_TOP)
    {
        popup_shapes(state, CH_POPUP_TOP_Y, top_popup_last_row(state), parts->rects, &rect_count, parts->texts, &text_count);
    }
    low_popup_shapes(state, parts->rects, &rect_count, parts->texts, &text_count);
    *region = (ui_region_t){
        .x0 = 0,
        .y0 = CH_BAR_Y + CH_BAR_H, // from under the MUTE/SOLO bar: the sends labels start above the graph's area
        .x1 = UI_SCREEN_W - 1,
        .y1 = EQ_LAST_ROW,
        .background = background(state),
        .rects = parts->rects,
        .rect_count = rect_count,
        .texts = parts->texts,
        .text_count = text_count,
        .paint = middle_paint(state),
        .paint_context = middle_context(state, views),
    };
}

void channel_screen_bar_slot (ui_region_t *region, channel_bar_parts_t *parts, channel_views_t *views,
                              const channel_screen_state_t *state)
{
    uint8_t rect_count = 0, text_count = 0;
    bar_slot_shapes(state, parts->rects, &rect_count, parts->texts, &text_count);
    *region = (ui_region_t){
        .x0 = 0,
        .y0 = CH_POPUP_TOP_Y,
        .x1 = UI_SCREEN_W - 1,
        .y1 = top_popup_last_row(state), // with no pop-up this redraws the bars and whatever the pop-up covered
        .background = background(state),
        .rects = parts->rects,
        .rect_count = rect_count,
        .texts = parts->texts,
        .text_count = text_count,
        .paint = middle_paint(state),
        .paint_context = middle_context(state, views),
    };
}

void channel_screen_low_popup_area (ui_region_t *region, channel_popup_parts_t *parts, channel_views_t *views,
                                    const channel_screen_state_t *state)
{
    uint8_t rect_count = 0, text_count = 0;
    low_popup_shapes(state, parts->rects, &rect_count, parts->texts, &text_count);
    *region = (ui_region_t){
        .x0 = 0,
        .y0 = CH_POPUP_LOW_Y,
        .x1 = UI_SCREEN_W - 1,
        .y1 = CH_PAN_Y - 1, // up to the pan bar; with no pop-up this redraws what it covered
        .background = background(state),
        .rects = parts->rects,
        .rect_count = rect_count,
        .texts = parts->texts,
        .text_count = text_count,
        .paint = middle_paint(state),
        .paint_context = middle_context(state, views),
    };
}

int16_t channel_screen_pan_fill (int16_t pan)
{
    // rounded to the nearest column; INT16_MIN and INT16_MAX both come out at exactly CH_PAN_HALF
    const int32_t magnitude = pan < 0 ? -(int32_t)pan : (int32_t)pan;
    int32_t columns = (magnitude * CH_PAN_HALF + 16384) / 32768;
    if (columns > CH_PAN_HALF) columns = CH_PAN_HALF;
    return (int16_t)(pan < 0 ? -columns : columns);
}

void channel_screen_pan (ui_region_t *region, channel_pan_parts_t *parts, int16_t fill)
{
    *region = (ui_region_t){
        .x0 = CH_PAN_X,
        .y0 = CH_PAN_Y,
        .x1 = CH_PAN_X + CH_PAN_W - 1,
        .y1 = CH_PAN_Y + CH_PAN_H - 1,
        .background = CH_COLOR_TROUGH,
        .rects = parts->rects,
        .rect_count = pan_rects(parts->rects, fill, false),
    };
}
