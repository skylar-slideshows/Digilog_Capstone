/**
  **********************************************************************************
  * MIXES SCREEN - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/ui_mixes.c
  * @brief See ui_mixes.h. Laid out for 1024 x 600. The list only ever has ROWS row objects,
  *        whose text changes as it scrolls, so a long list costs no more of LVGL's memory.
  **********************************************************************************
*/

#include "display/ui_mixes.h"
#include "mix/mix_file.h"
#include "FreeRTOS_apps/bucket_link.h"
#include "FreeRTOS_apps/daw_control.h"
#include "mix/automation.h"
#include "control_integration/control_interface.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#define SCREEN_W 1024
#define SCREEN_H 600

#define HEADER_H  64
#define LIST_X    24
#define LIST_Y    112
#define LIST_W    (SCREEN_W - 2 * LIST_X)
#define ROW_H     40
#define ROWS      9
#define KEY_W     196
#define KEY_H     52
#define KEY_Y     (SCREEN_H - KEY_H - 8)
#define MSG_Y     (KEY_Y - 44)

#define MSG_MS         5000U // a message stays this long
#define MOUNT_RETRY_MS 2000U // no card: try again this often

// centres of the four soft keys: over B0-B3 (panel: buttons at 57.6, 102.6, 147.6, 192.6 mm, screen
// window 46-204 mm), the outer two pulled in to stay on the screen
static const int16_t KEY_CX[SCREEN_BUTTONS] = {102, 367, 658, 922};

#define C_BG       0x101418
#define C_HEADER   0x1C2228
#define C_TEXT     0xFFFFFF
#define C_DIM      0x8A949E
#define C_CURSOR   0x2E6FD8 // the row the knob is on
#define C_CHOSEN   0xE08A1E // the mix chosen (load / delete offered)
#define C_KEY      0x2A3138
#define C_MSG      0xF2C400

typedef enum
{
    MODE_BROWSE,
    MODE_CHOSEN,
    MODE_CONFIRM_DELETE,
    MODE_CONFIRM_FORMAT,
} ui_mode_t;

static struct
{
    char names[MIX_LIST_MAX][MIX_NAME_MAX + 1];
    uint16_t count, cursor, top;
    ui_mode_t mode;
    mix_result_t card;
    TickType_t last_mount_try, msg_until;
    uint32_t clock_shown; // seconds of the clock last drawn
    int8_t box_status[BUCKET_COUNT];

    lv_obj_t *row[ROWS], *empty, *clock, *card_label, *msg, *timecode, *auto_rec;
    char tc_text[24];       // the timecode last drawn
    uint32_t moves_shown;   // the automation count last drawn
    int8_t wait_shown;      // why AUTO REC was waiting, as last drawn
    uint32_t autosaves;     // autosaves announced
    int8_t rec_shown;       // AUTO REC as last drawn (and its flash)
    lv_obj_t *key[SCREEN_BUTTONS];
    lv_obj_t *box[BUCKET_COUNT];
} s;


/*=============================== STYLES ================================*/
// Shared styles: set once, used by many objects. Styles set on each object separately would each take
// LVGL heap, and LVGL's heap is kept small on the master (lv_conf.h).

static lv_style_t st_plain;  // a filled, square box with nothing else
static lv_style_t st_row;    // a list row: a label with its own background
static lv_style_t st_key;    // a soft key: a label with its own background, text centred
static lv_style_t st_box;    // a bucket's box in the header
static lv_style_t st_big;    // white text, the large font
static lv_style_t st_small;  // grey text, the small font

static void styles_init (void)
{
    lv_style_init(&st_plain);
    lv_style_set_bg_opa(&st_plain, LV_OPA_COVER);
    lv_style_set_border_width(&st_plain, 0);
    lv_style_set_radius(&st_plain, 0);
    lv_style_set_pad_all(&st_plain, 0);

    lv_style_init(&st_row);
    lv_style_set_bg_opa(&st_row, LV_OPA_COVER);
    lv_style_set_bg_color(&st_row, lv_color_hex(C_BG));
    lv_style_set_radius(&st_row, 6);
    lv_style_set_pad_left(&st_row, 16);
    lv_style_set_pad_top(&st_row, 2);
    lv_style_set_text_font(&st_row, &lv_font_montserrat_28);
    lv_style_set_text_color(&st_row, lv_color_hex(C_TEXT));

    lv_style_init(&st_key);
    lv_style_set_bg_opa(&st_key, LV_OPA_COVER);
    lv_style_set_bg_color(&st_key, lv_color_hex(C_KEY));
    lv_style_set_radius(&st_key, 6);
    lv_style_set_pad_top(&st_key, (KEY_H - 32) / 2);
    lv_style_set_text_align(&st_key, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(&st_key, &lv_font_montserrat_28);
    lv_style_set_text_color(&st_key, lv_color_hex(C_TEXT));

    lv_style_init(&st_box);
    lv_style_set_bg_opa(&st_box, LV_OPA_COVER);
    lv_style_set_radius(&st_box, 6);
    lv_style_set_pad_top(&st_box, 9);
    lv_style_set_text_align(&st_box, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font(&st_box, &lv_font_montserrat_14);
    lv_style_set_text_color(&st_box, lv_color_hex(0x000000));

    lv_style_init(&st_big);
    lv_style_set_text_font(&st_big, &lv_font_montserrat_28);
    lv_style_set_text_color(&st_big, lv_color_hex(C_TEXT));

    lv_style_init(&st_small);
    lv_style_set_text_font(&st_small, &lv_font_montserrat_14);
    lv_style_set_text_color(&st_small, lv_color_hex(C_DIM));
}

/** A label of a fixed size and position, drawn with a style */
static lv_obj_t *label_at (lv_obj_t *parent, lv_style_t *style, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_add_style(l, style, 0);
    lv_label_set_text(l, "");
    lv_obj_set_pos(l, x, y);
    if (w > 0) lv_obj_set_size(l, w, h);
    return l;
}

/** "14-05-33.DGL" -> "14-05-33" */
static void display_name (const char *file, char *out, size_t size)
{
    strncpy(out, file, size - 1U);
    out[size - 1U] = '\0';
    char *dot = strrchr(out, '.');
    if (dot != NULL) *dot = '\0';
}

static void message (const char *t)
{
    lv_label_set_text(s.msg, t);
    s.msg_until = xTaskGetTickCount() + pdMS_TO_TICKS(MSG_MS);
}

static lv_color_t status_colour (bucket_status_t st)
{
    switch (st)
    {
        case BUCKET_PRESENT: return lv_color_hex(0x22C24A); // green: there
        case BUCKET_GONE:    return lv_color_hex(0xD8302C); // red: not connected (still asked)
        default:             return lv_color_hex(0xF2C400); // yellow: asking, connecting, gone quiet
    }
}


/*=============================== DRAWING ================================*/

static void draw_list (void)
{
    // keep the cursor in view
    if (s.cursor < s.top) s.top = s.cursor;
    if (s.cursor >= s.top + ROWS) s.top = (uint16_t)(s.cursor - ROWS + 1U);

    for (uint16_t i = 0; i < ROWS; i++)
    {
        const uint16_t idx = (uint16_t)(s.top + i);
        if (idx >= s.count)
        {
            lv_obj_set_hidden(s.row[i], true);
            continue;
        }
        lv_obj_set_hidden(s.row[i], false);
        char name[MIX_NAME_MAX + 1];
        display_name(s.names[idx], name, sizeof(name));
        lv_label_set_text(s.row[i], name);

        // the cursor's row: its own colour (one local style); the others: the row style's
        if (idx == s.cursor) lv_obj_set_style_bg_color(s.row[i], lv_color_hex(s.mode == MODE_BROWSE ? C_CURSOR : C_CHOSEN), 0);
        else lv_obj_remove_local_style_prop(s.row[i], LV_STYLE_BG_COLOR, 0);
    }

    if (s.card != MIX_OK) lv_label_set_text(s.empty, mix_file_result_text(s.card));
    else lv_label_set_text(s.empty, "No mixes on the card yet: SAVE NEW saves the current one");
    lv_obj_set_hidden(s.empty, s.count != 0U);
}

/** The soft keys and their buttons' lights for the mode */
static void draw_keys (void)
{
    const char *label[SCREEN_BUTTONS] = {"", "", "", ""};
    uint8_t on = 0, flash = 0;
    switch (s.mode)
    {
        case MODE_BROWSE:
            if (s.card == MIX_OK) { label[0] = "SAVE NEW"; on = 0x1; }
            else if (s.card == MIX_NOT_FORMATTED) { label[3] = "FORMAT"; on = 0x8; }
            break;
        case MODE_CHOSEN:
            label[0] = "LOAD"; label[1] = "DELETE"; label[3] = "CANCEL";
            flash = 0x1 | 0x2 | 0x8;
            break;
        case MODE_CONFIRM_DELETE:
            label[1] = "DELETE"; label[3] = "CANCEL";
            flash = 0x2; on = 0x8;
            break;
        case MODE_CONFIRM_FORMAT:
            label[0] = "CANCEL"; label[3] = "FORMAT";
            flash = 0x8; on = 0x1;
            break;
    }
    for (uint8_t k = 0; k < SCREEN_BUTTONS; k++)
    {
        lv_label_set_text(s.key[k], label[k]);
        lv_obj_set_hidden(s.key[k], label[k][0] == '\0');
        if (((flash >> k) & 1U) != 0U) lv_obj_set_style_bg_color(s.key[k], lv_color_hex(C_CHOSEN), 0); // offered
        else lv_obj_remove_local_style_prop(s.key[k], LV_STYLE_BG_COLOR, 0);
    }
    control_set_screen_leds(on, flash);
}

static void draw_card_line (void)
{
    char t[128];
    int n;
    if (s.card == MIX_OK) n = snprintf(t, sizeof(t), "SD card: %u mix%s", (unsigned)s.count, s.count == 1U ? "" : "es");
    else n = snprintf(t, sizeof(t), "%s", mix_file_result_text(s.card));
    const uint32_t moves = automation_event_count();
    if (n > 0 && (size_t)n < sizeof(t))
    {
        const char *note = "";
        switch (automation_waiting())
        {
            case AUTO_WAIT_PLAY:     note = "  -  AUTO REC: waiting for play"; break;
            case AUTO_WAIT_TIMECODE: note = "  -  AUTO REC: no SMPTE timecode from the DAW"; break;
            case AUTO_WAIT_SAVING:   note = "  -  AUTO REC: writing the last pass"; break;
            default: if (automation_full()) note = "  -  moves lost: the card couldn't keep up"; break;
        }
        snprintf(t + n, sizeof(t) - (size_t)n, "     Automation: %lu moves%s", (unsigned long)moves, note);
    }
    lv_label_set_text(s.card_label, t);
    s.moves_shown = moves;
    s.wait_shown = (int8_t)automation_waiting();
}

static void redraw (void)
{
    draw_list();
    draw_keys();
    draw_card_line();
}


/*=============================== THE CARD ================================*/

static void reread_list (void)
{
    s.count = (s.card == MIX_OK) ? mix_file_list(s.names, MIX_LIST_MAX) : 0U;
    if (s.cursor >= s.count) s.cursor = s.count ? (uint16_t)(s.count - 1U) : 0U;
}

static void try_mount (void)
{
    s.last_mount_try = xTaskGetTickCount();
    const mix_result_t before = s.card;
    s.card = mix_file_mount();
    if (s.card != before || s.card == MIX_OK) reread_list();
    if (s.card != MIX_OK && s.mode != MODE_CONFIRM_FORMAT) s.mode = MODE_BROWSE;
}

/** Shows a message now, before a long card operation holds the screen up */
static void busy (const char *t)
{
    lv_label_set_text(s.msg, t);
    lv_refr_now(NULL);
}


/*=============================== ACTIONS ================================*/

static void save_new (void)
{
    busy("Saving...");
    char name[MIX_NAME_MAX + 1] = "";
    const mix_result_t r = mix_file_save_new(name);
    if (r != MIX_OK)
    {
        s.card = mix_file_mount();
        message(mix_file_result_text(r));
        reread_list();
        return;
    }
    reread_list();
    for (uint16_t i = 0; i < s.count; i++) if (strcmp(s.names[i], name) == 0) s.cursor = i; // onto the new one
    char shown[MIX_NAME_MAX + 1], t[MIX_NAME_MAX + 24];
    display_name(name, shown, sizeof(shown));
    snprintf(t, sizeof(t), "Saved %s", shown);
    message(t);
}

static void load_chosen (void)
{
    char shown[MIX_NAME_MAX + 1], t[128];
    display_name(s.names[s.cursor], shown, sizeof(shown));
    snprintf(t, sizeof(t), "Loading %s...", shown);
    busy(t);

    mix_load_report_t rep;
    const mix_result_t r = mix_file_load(s.names[s.cursor], &rep);
    if (r != MIX_OK)
    {
        snprintf(t, sizeof(t), "%s: %s", shown, mix_file_result_text(r));
    }
    else
    {
        int n = snprintf(t, sizeof(t), "Loaded %s: %u connected, %u not connected", shown, (unsigned)rep.sent, (unsigned)rep.skipped);
        if (rep.moves > 0U && n > 0 && (size_t)n < sizeof(t)) n += snprintf(t + n, sizeof(t) - (size_t)n, ", %lu moves", (unsigned long)rep.moves);
        if (rep.moves_lost > 0U && n > 0 && (size_t)n < sizeof(t)) n += snprintf(t + n, sizeof(t) - (size_t)n, " (%lu skipped)", (unsigned long)rep.moves_lost);
        if (rep.old_layout && n > 0 && (size_t)n < sizeof(t)) snprintf(t + n, sizeof(t) - (size_t)n, " (old automation not read)");
    }
    message(t);
}

static void delete_chosen (void)
{
    char shown[MIX_NAME_MAX + 1], t[MIX_NAME_MAX + 24];
    display_name(s.names[s.cursor], shown, sizeof(shown));
    const mix_result_t r = mix_file_delete(s.names[s.cursor]);
    if (r == MIX_OK) snprintf(t, sizeof(t), "Deleted %s", shown);
    else snprintf(t, sizeof(t), "%s: %s", shown, mix_file_result_text(r));
    message(t);
    reread_list();
}

static void format_card (void)
{
    busy("Formatting the SD card...");
    const mix_result_t r = mix_file_format();
    s.card = mix_file_mount();
    reread_list();
    message(r == MIX_OK ? "SD card formatted" : mix_file_result_text(r));
}

static void press (uint8_t key)
{
    switch (s.mode)
    {
        case MODE_BROWSE:
            if (key == 0U && s.card == MIX_OK) save_new();
            else if (key == 3U && s.card == MIX_NOT_FORMATTED)
            {
                s.mode = MODE_CONFIRM_FORMAT;
                message("Press FORMAT again to format the SD card (anything on it is lost)");
            }
            break;
        case MODE_CHOSEN:
            if (key == 0U) { load_chosen(); s.mode = MODE_BROWSE; }
            else if (key == 1U)
            {
                char shown[MIX_NAME_MAX + 1], t[MIX_NAME_MAX + 32];
                display_name(s.names[s.cursor], shown, sizeof(shown));
                snprintf(t, sizeof(t), "Press DELETE again to delete %s", shown);
                message(t);
                s.mode = MODE_CONFIRM_DELETE;
            }
            else if (key == 3U) s.mode = MODE_BROWSE;
            break;
        case MODE_CONFIRM_DELETE:
            if (key == 1U) { delete_chosen(); s.mode = MODE_BROWSE; }
            else { s.mode = MODE_CHOSEN; message(""); }
            break;
        case MODE_CONFIRM_FORMAT:
            if (key == 3U) format_card();
            else message("");
            s.mode = MODE_BROWSE;
            break;
    }
}


/*=============================== PUBLIC ================================*/

void ui_mixes_create (void)
{
    memset(&s, 0, sizeof(s));
    for (uint8_t b = 0; b < BUCKET_COUNT; b++) s.box_status[b] = -1;
    styles_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollable(scr, false);

    // header: title, clock, a box per bucket
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_add_style(header, &st_plain, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(C_HEADER), 0);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_scrollable(header, false);
    lv_obj_t *title = label_at(header, &st_big, 24, (HEADER_H - 32) / 2, 0, 0);
    lv_label_set_text(title, "MIXES");
    s.clock = label_at(header, &st_big, 160, (HEADER_H - 32) / 2, 0, 0);
    lv_obj_set_style_text_color(s.clock, lv_color_hex(C_DIM), 0);

    // AUTO REC: shown while it's on (steady), flashing while it records
    s.auto_rec = label_at(header, &st_box, 330, (HEADER_H - 36) / 2, 130, 36);
    lv_obj_set_style_bg_color(s.auto_rec, lv_color_hex(0xD8302C), 0);
    lv_obj_set_style_text_color(s.auto_rec, lv_color_hex(C_TEXT), 0);
    lv_label_set_text(s.auto_rec, "AUTO REC");
    lv_obj_set_hidden(s.auto_rec, true);
    s.rec_shown = -1;

    // top right: the DAW's timecode (where the playhead is), then one box per bucket
    const int32_t box_w = 40, box_gap = 6;
    const int32_t boxes_x = SCREEN_W - 24 - BUCKET_COUNT * box_w - (BUCKET_COUNT - 1) * box_gap;
    for (uint8_t b = 0; b < BUCKET_COUNT; b++)
    {
        s.box[b] = label_at(header, &st_box, boxes_x + b * (box_w + box_gap), (HEADER_H - 36) / 2, box_w, 36);
        lv_label_set_text_fmt(s.box[b], "%u", (unsigned)(b + 1U));
    }
    s.timecode = label_at(header, &st_big, boxes_x - 24 - 230, (HEADER_H - 32) / 2, 230, 34);
    lv_obj_set_style_text_align(s.timecode, LV_TEXT_ALIGN_RIGHT, 0);

    s.card_label = label_at(scr, &st_small, LIST_X, HEADER_H + 16, 0, 0);

    // the list: ROWS rows whose text scrolls
    for (uint8_t i = 0; i < ROWS; i++)
    {
        s.row[i] = label_at(scr, &st_row, LIST_X, LIST_Y + i * ROW_H, LIST_W, ROW_H - 4);
    }
    s.empty = label_at(scr, &st_big, LIST_X + 16, LIST_Y + 6, 0, 0);
    lv_obj_set_style_text_color(s.empty, lv_color_hex(C_DIM), 0);

    s.msg = label_at(scr, &st_big, LIST_X, MSG_Y, 0, 0);
    lv_obj_set_style_text_color(s.msg, lv_color_hex(C_MSG), 0);

    // the soft keys, over B0-B3
    for (uint8_t k = 0; k < SCREEN_BUTTONS; k++)
    {
        s.key[k] = label_at(scr, &st_key, KEY_CX[k] - KEY_W / 2, KEY_Y, KEY_W, KEY_H);
    }

    try_mount();
    redraw();
}

void ui_mixes_update (void)
{
    const TickType_t now = xTaskGetTickCount();
    bool changed = false;

    // the card: try again every couple of seconds while it isn't usable
    if (s.card != MIX_OK && s.mode != MODE_CONFIRM_FORMAT && (TickType_t)(now - s.last_mount_try) >= pdMS_TO_TICKS(MOUNT_RETRY_MS))
    {
        try_mount();
        changed = true;
    }

    // the knob and the buttons
    screen_input_t in;
    control_take_screen_input(&in);
    if (in.turns != 0)
    {
        if (s.count > 0U)
        {
            int32_t c = (int32_t)s.cursor + in.turns;
            if (c < 0) c = 0;
            if (c >= (int32_t)s.count) c = (int32_t)s.count - 1;
            s.cursor = (uint16_t)c;
        }
        if (s.mode != MODE_BROWSE && s.mode != MODE_CONFIRM_FORMAT) message("");
        if (s.mode != MODE_CONFIRM_FORMAT) s.mode = MODE_BROWSE; // moving on: nothing chosen
        changed = true;
    }
    for (uint8_t p = 0; p < in.knob_presses; p++)
    {
        if (s.mode == MODE_BROWSE && s.count > 0U && s.card == MIX_OK) s.mode = MODE_CHOSEN;
        else if (s.mode != MODE_BROWSE) { s.mode = MODE_BROWSE; message(""); }
        changed = true;
    }
    for (uint8_t k = 0; k < SCREEN_BUTTONS; k++)
    {
        for (uint8_t p = 0; p < in.button_presses[k]; p++)
        {
            press(k);
            changed = true;
        }
    }
    if (changed) redraw();

    // the message clears itself
    if (s.msg_until != 0U && (int32_t)(now - s.msg_until) >= 0)
    {
        s.msg_until = 0;
        lv_label_set_text(s.msg, "");
    }

    // the clock, once a second
    mix_time_t t;
    mix_clock_now(&t);
    const uint32_t secs = (uint32_t)t.hour * 3600U + t.minute * 60U + t.second;
    if (secs != s.clock_shown || lv_label_get_text(s.clock)[0] == '\0')
    {
        s.clock_shown = secs;
        if (mix_clock_is_set()) lv_label_set_text_fmt(s.clock, "%02u:%02u:%02u", t.hour, t.minute, t.second);
        else lv_label_set_text_fmt(s.clock, "%02u:%02u:%02u?", t.hour, t.minute, t.second); // ?: the app hasn't set the clock
    }

    // the DAW's timecode: where the playhead is
    daw_timecode_t tc;
    daw_control_timecode(&tc);
    char tc_text[24];
    if (!tc.valid) snprintf(tc_text, sizeof(tc_text), "--:--:--:--");
    else if (tc.mode == DAW_TIME_BEATS) snprintf(tc_text, sizeof(tc_text), "%u.%u.%u.%03u", tc.field[0], tc.field[1], tc.field[2], tc.field[3]);
    else snprintf(tc_text, sizeof(tc_text), "%02u:%02u:%02u:%02u", tc.field[0], tc.field[1], tc.field[2], tc.field[3]);
    if (strcmp(tc_text, s.tc_text) != 0)
    {
        memcpy(s.tc_text, tc_text, sizeof(tc_text));
        lv_label_set_text(s.timecode, tc_text);
    }

    // AUTO REC, and the automation count
    const auto_rec_t rec = automation_rec_state();
    const bool flash_on = ((now / pdMS_TO_TICKS(250)) & 1U) != 0U;
    const int8_t rec_draw = (int8_t)(rec == AUTO_REC_OFF ? 0 : (rec == AUTO_REC_ARMED || flash_on) ? 1 : 2);
    if (rec_draw != s.rec_shown)
    {
        s.rec_shown = rec_draw;
        lv_obj_set_hidden(s.auto_rec, rec_draw == 0);
        lv_obj_set_style_bg_opa(s.auto_rec, rec_draw == 1 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    if (automation_event_count() != s.moves_shown || (int8_t)automation_waiting() != s.wait_shown) draw_card_line();

    // an autosave happened: say so, and list it
    char autosaved[MIX_NAME_MAX + 1];
    const uint32_t autosaves = mix_file_autosaves(autosaved);
    if (autosaves != s.autosaves)
    {
        s.autosaves = autosaves;
        char shown[MIX_NAME_MAX + 1], t[MIX_NAME_MAX + 16];
        display_name(autosaved, shown, sizeof(shown));
        snprintf(t, sizeof(t), "Autosaved %s", shown);
        message(t);
        reread_list();
        draw_list();
        draw_card_line();
    }

    // the buckets' boxes
    for (uint8_t b = 0; b < BUCKET_COUNT; b++)
    {
        const bucket_status_t st = bucket_link_status(b);
        if ((int8_t)st == s.box_status[b]) continue;
        s.box_status[b] = (int8_t)st;
        lv_obj_set_style_bg_color(s.box[b], status_colour(st), 0);
    }
}
