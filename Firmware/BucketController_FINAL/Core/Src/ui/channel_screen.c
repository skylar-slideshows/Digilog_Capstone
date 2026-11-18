/**
  **********************************************************************************
  * CHANNEL SCREEN LAYOUT - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file ui/channel_screen.c
  * @brief See channel_screen.h.
  **********************************************************************************
*/

#include "ui/channel_screen.h"
#include <math.h>
#include <stddef.h>

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
static float db_row (float db) { return (float)CH_EQ_MID - db / CH_EQ_RANGE_DB * (float)CH_EQ_DB_PX; }

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

static void set_eq_colours (channel_eq_view_t *eq, const channel_screen_state_t *s)
{
    eq->curve_rgb = s->muted ? CH_COLOR_EQ_MUTED : (s->soloed ? CH_COLOR_EQ_SOLO : CH_COLOR_EQ);
    eq->fill_rgb = s->muted ? CH_COLOR_EQ_FILL_MUTED : (s->soloed ? CH_COLOR_EQ_FILL_SOLO : CH_COLOR_EQ_FILL);
}


/*=============================== EQ CURVE ================================*/

float channel_eq_gain_db (int16_t gain) { return (float)gain * (18.0f / 32768.0f); }
float channel_eq_q (uint16_t q) { return 0.3f * powf(10.0f / 0.3f, (float)q / 65535.0f); }

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
} band_t;

static band_t band (const channel_eq_band_t *b, channel_eq_band_index_t index, bool has_q)
{
    return (band_t){channel_eq_gain_db(b->gain), channel_eq_freq_hz(index, b->freq), has_q ? channel_eq_q(b->q) : 1.0f};
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

/** Response in dB at frequency f, as the mockup's eqMagnitude(); limited to +/-CH_EQ_RANGE_DB */
static float response_db (const channel_eq_t *eq, const band_t bands[4], float f)
{
    float db = 0.0f;
    if (eq->hpf)
    {
        const float x = powf(f / 80.0f, 4.0f);
        db += 10.0f * log10f(x / (1.0f + x));
    }
    db += shelf(&bands[CH_EQ_HF], f, true) + bell(&bands[CH_EQ_HMF], f) + bell(&bands[CH_EQ_LMF], f) +
          shelf(&bands[CH_EQ_LF], f, false);
    return clampf(db, -CH_EQ_RANGE_DB, CH_EQ_RANGE_DB);
}

void channel_screen_eq_compute (channel_eq_view_t *view, const channel_eq_t *eq)
{
    const band_t bands[4] = {
        [CH_EQ_LF] = band(&eq->lf, CH_EQ_LF, false),
        [CH_EQ_LMF] = band(&eq->lmf, CH_EQ_LMF, true),
        [CH_EQ_HMF] = band(&eq->hmf, CH_EQ_HMF, true),
        [CH_EQ_HF] = band(&eq->hf, CH_EQ_HF, false),
    };

    static float y[UI_SCREEN_W]; // the curve's row in each column (static: too big for a task's stack)
    for (int16_t x = 0; x < UI_SCREEN_W; x++)
    {
        const float f = 20.0f * powf(1000.0f, (float)x / (float)(UI_SCREEN_W - 1));
        y[x] = db_row(response_db(eq, bands, f));
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

    const char *input = INPUT_NAMES[s->input <= CH_INPUT_HIZ ? s->input : CH_INPUT_LINE];

    rects[(*rect_count)++] = (ui_rect_t){0, 0, UI_SCREEN_W - 1, CH_HEADER_H - 1, s->title_lit ? CH_COLOR_HEADER_LIT : CH_COLOR_HEADER};
    texts[(*text_count)++] = (ui_text_t){CH_TEXT_MARGIN, CH_TEXT_Y, CH_TEXT_SIZE, number_colour(s), number};
    texts[(*text_count)++] = (ui_text_t){
        (int16_t)(UI_SCREEN_W - CH_TEXT_MARGIN - ui_text_width(input, CH_INPUT_SIZE)), CH_INPUT_Y, CH_INPUT_SIZE, CH_COLOR_NUMBER, input,
    };
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
                          channel_eq_view_t *eq)
{
    uint8_t rect_count = 0, text_count = 0;
    header_shapes(state, parts->number, parts->rects, &rect_count, parts->texts, &text_count);

    if (state->muted && state->soloed) // both: share the bar
    {
        bar_shapes(0, UI_SCREEN_W / 2 - 1, CH_COLOR_MUTE_BAR, "MUTE", parts->rects, &rect_count, parts->texts, &text_count);
        bar_shapes(UI_SCREEN_W / 2, UI_SCREEN_W - 1, CH_COLOR_SOLO_BAR, "SOLO", parts->rects, &rect_count, parts->texts, &text_count);
    }
    else if (state->muted)
    {
        bar_shapes(0, UI_SCREEN_W - 1, CH_COLOR_MUTE_BAR, "MUTE", parts->rects, &rect_count, parts->texts, &text_count);
    }
    else if (state->soloed)
    {
        bar_shapes(0, UI_SCREEN_W - 1, CH_COLOR_SOLO_BAR, "SOLO", parts->rects, &rect_count, parts->texts, &text_count);
    }

    rect_count = (uint8_t)(rect_count + pan_rects(&parts->rects[rect_count], state->pan_fill, true));
    set_eq_colours(eq, state);

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
        .paint = state->show_eq ? paint_eq : NULL, // other control sets: the graph's area stays empty
        .paint_context = eq,
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

void channel_screen_eq (ui_region_t *region, channel_eq_view_t *eq, const channel_screen_state_t *state)
{
    set_eq_colours(eq, state);
    *region = (ui_region_t){
        .x0 = 0,
        .y0 = CH_EQ_Y,
        .x1 = UI_SCREEN_W - 1,
        .y1 = EQ_LAST_ROW,
        .background = background(state),
        .paint = state->show_eq ? paint_eq : NULL,
        .paint_context = eq,
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
