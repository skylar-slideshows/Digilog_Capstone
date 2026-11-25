/**
  **********************************************************************************
  * CONSOLE TEST SCREEN - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file display/ui_console_test.c
  * @brief See ui_console_test.h. Laid out for 1024 x 600: the moves from the top left, the
  *        channel boxes in a row along the bottom.
  **********************************************************************************
*/

#include "display/ui_console_test.h"
#include "FreeRTOS_apps/bucket_link.h"
#include "CONFIG.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#define SCREEN_W      1024
#define SCREEN_H      600
#define BOXES         16    // channels 01-16: the buckets on CS_Bucket0-3
#define BOX_W         56
#define BOX_H         56
#define BOX_GAP       8
#define BOX_Y         (SCREEN_H - BOX_H - 8)
#define MOVE_LINES    14    // lines of moves that fit above the boxes
#define MOVE_SHOW_MS  1000  // a control counts as being moved for this long after its last change

static lv_obj_t *s_moves;
static lv_obj_t *s_box[BOXES];
static int8_t s_box_status[BOXES];
static char s_text[MOVE_LINES * 32];

static lv_color_t status_colour (bucket_status_t status)
{
    switch (status)
    {
        case BUCKET_PRESENT: return lv_color_hex(0x22C24A); // green: there
        case BUCKET_GONE: return lv_color_hex(0xD8302C);    // red: taken as unplugged (still asked)
        default: return lv_color_hex(0xF2C400);             // yellow: asking, connecting, gone quiet
    }
}

void ui_console_test_create (void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollable(scr, false);

    s_moves = lv_label_create(scr);
    lv_label_set_text(s_moves, "");
    lv_obj_set_style_text_font(s_moves, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_moves, lv_color_white(), 0);
    lv_obj_set_pos(s_moves, 24, 16);

    const int32_t x0 = (SCREEN_W - (BOXES * BOX_W + (BOXES - 1) * BOX_GAP)) / 2;
    for (uint8_t i = 0; i < BOXES; i++)
    {
        lv_obj_t *box = lv_obj_create(scr);
        lv_obj_set_pos(box, x0 + i * (BOX_W + BOX_GAP), BOX_Y);
        lv_obj_set_size(box, BOX_W, BOX_H);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, status_colour(BUCKET_ASKING), 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_style_radius(box, 6, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_set_scrollable(box, false);

        lv_obj_t *number = lv_label_create(box);
        lv_label_set_text_fmt(number, "%02u", (unsigned)(i + 1U));
        lv_obj_set_style_text_font(number, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(number, lv_color_black(), 0);
        lv_obj_center(number);

        s_box[i] = box;
        s_box_status[i] = (int8_t)BUCKET_ASKING;
    }
}

void ui_console_test_update (void)
{
    // the channel boxes: the status of the bucket each channel is on
    for (uint8_t i = 0; i < BOXES; i++)
    {
        const bucket_status_t st = bucket_link_status((uint8_t)(i / CHANNELS_PER_BUCKET));
        if ((int8_t)st == s_box_status[i]) continue;
        s_box_status[i] = (int8_t)st;
        lv_obj_set_style_bg_color(s_box[i], status_colour(st), 0);
    }

    // the controls being moved, newest first, with their values right now
    bucket_ctrl_move_t moves[MOVE_LINES];
    const uint8_t count = bucket_link_recent(moves, MOVE_LINES, MOVE_SHOW_MS);
    char text[sizeof(s_text)];
    size_t len = 0;
    text[0] = '\0';
    for (uint8_t i = 0; i < count && len < sizeof(text); i++)
    {
        const unsigned percent = (unsigned)(((uint32_t)moves[i].value * 100U + 32767U) / 65535U);
        len += (size_t)snprintf(&text[len], sizeof(text) - len, "%sCHANNEL %u: CTRL %u%%", i ? "\n" : "",
                                (unsigned)(moves[i].channel + 1U), percent);
    }
    if (strcmp(text, s_text) != 0) // only redraw when it changed
    {
        memcpy(s_text, text, sizeof(s_text));
        lv_label_set_text(s_moves, s_text);
    }
}
