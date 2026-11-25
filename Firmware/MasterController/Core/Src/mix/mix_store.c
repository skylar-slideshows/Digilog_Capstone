/**
  **********************************************************************************
  * MOVE STORE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_store.c
  * @brief See mix_store.h. Plain files of 8-byte moves, read and written in chunks.
  **********************************************************************************
*/

#include "mix/mix_store.h"
#include "ff.h"
#include <string.h>

#define STORE_FILE "0:/AUTOMOVE.DAT"
#define NEW_FILE   "0:/AUTOMOVN.DAT"
#define PASS_FILE  "0:/AUTOPASS.DAT"
#define CHUNK      32 // moves read or written at a time

static FIL f_store, f_new, f_pass;
static bool store_open, new_open, pass_open;
static uint32_t s_count, s_pass_count, s_new_count;
static store_ev_t buf_a[CHUNK], buf_b[CHUNK], buf_out[CHUNK];
static uint16_t out_n;

_Static_assert(sizeof(store_ev_t) == 8, "a move is 8 bytes on the card");

static bool open_store (void)
{
    if (store_open) return true;
    if (f_open(&f_store, STORE_FILE, FA_READ) != FR_OK) { s_count = 0; return false; }
    store_open = true;
    s_count = (uint32_t)(f_size(&f_store) / sizeof(store_ev_t));
    return true;
}

static void close_store (void)
{
    if (store_open) (void)f_close(&f_store);
    store_open = false;
}

void store_reset (void)
{
    close_store();
    if (new_open) { (void)f_close(&f_new); new_open = false; }
    if (pass_open) { (void)f_close(&f_pass); pass_open = false; }
    (void)f_unlink(STORE_FILE);
    (void)f_unlink(NEW_FILE);
    (void)f_unlink(PASS_FILE);
    s_count = s_pass_count = 0;
}

uint32_t store_count (void)
{
    return s_count;
}

bool store_read (uint32_t index, store_ev_t *out, uint16_t max, uint16_t *got)
{
    *got = 0;
    if (index >= s_count || max == 0U) return true;
    if (!open_store()) return false;
    if (f_lseek(&f_store, (FSIZE_t)index * sizeof(store_ev_t)) != FR_OK) return false;
    const uint32_t left = s_count - index;
    const UINT want = (UINT)((left < max ? left : max) * sizeof(store_ev_t));
    UINT br = 0;
    if (f_read(&f_store, out, want, &br) != FR_OK) return false;
    *got = (uint16_t)(br / sizeof(store_ev_t));
    return true;
}

bool store_first_after (uint32_t ms, uint32_t *index)
{
    uint32_t lo = 0, hi = s_count;
    while (lo < hi)
    {
        const uint32_t mid = lo + (hi - lo) / 2U;
        store_ev_t e;
        uint16_t got;
        if (!store_read(mid, &e, 1, &got) || got != 1U) return false;
        if (e.t <= ms) lo = mid + 1U;
        else hi = mid;
    }
    *index = lo;
    return true;
}

/*----- building a new store -----*/

static bool out_flush (void)
{
    if (out_n == 0U) return true;
    UINT bw = 0;
    const UINT n = (UINT)(out_n * sizeof(store_ev_t));
    const bool ok = f_write(&f_new, buf_out, n, &bw) == FR_OK && bw == n;
    out_n = 0;
    return ok;
}

bool store_new_open (void)
{
    if (new_open) { (void)f_close(&f_new); new_open = false; }
    if (f_open(&f_new, NEW_FILE, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    new_open = true;
    out_n = 0;
    s_new_count = 0;
    return true;
}

bool store_new_put (const store_ev_t *e)
{
    if (!new_open) return false;
    buf_out[out_n++] = *e;
    s_new_count++;
    return out_n < CHUNK || out_flush();
}

bool store_new_close (bool keep)
{
    if (!new_open) return false;
    bool ok = out_flush();
    ok = (f_close(&f_new) == FR_OK) && ok;
    new_open = false;
    if (!keep || !ok)
    {
        (void)f_unlink(NEW_FILE);
        return ok && !keep;
    }
    close_store();
    (void)f_unlink(STORE_FILE);
    if (f_rename(NEW_FILE, STORE_FILE) != FR_OK) { s_count = 0; return false; }
    s_count = s_new_count;
    return true;
}

/*----- the pass file -----*/

bool store_pass_reset (void)
{
    if (pass_open) { (void)f_close(&f_pass); pass_open = false; }
    s_pass_count = 0;
    if (f_open(&f_pass, PASS_FILE, FA_READ | FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    pass_open = true;
    return true;
}

bool store_pass_append (const store_ev_t *e, uint16_t n)
{
    if (n == 0U) return true;
    if (!pass_open && !store_pass_reset()) return false;
    if (f_lseek(&f_pass, (FSIZE_t)s_pass_count * sizeof(store_ev_t)) != FR_OK) return false;
    UINT bw = 0;
    const UINT len = (UINT)(n * sizeof(store_ev_t));
    if (f_write(&f_pass, e, len, &bw) != FR_OK || bw != len) return false;
    (void)f_sync(&f_pass);
    s_pass_count += n;
    return true;
}

uint32_t store_pass_count (void)
{
    return s_pass_count;
}

/*----- merging a pass -----*/

/** A move the pass replaces: an old one of a touched control, from its first touch to the end */
static bool replaced (const store_ev_t *e, const store_touch_t *touched, uint8_t n, uint32_t end_ms)
{
    for (uint8_t k = 0; k < n; k++)
    {
        if (touched[k].ch == e->ch && touched[k].lane == e->lane && e->t >= touched[k].from && e->t <= end_ms) return true;
    }
    return false;
}

/** The pass's moves one at a time: the pass file, then the tail still in RAM */
typedef struct
{
    uint32_t file_next;  // next index in the pass file
    uint16_t buf_n, buf_i;
    const store_ev_t *tail;
    uint16_t n_tail, tail_i;
} pass_reader_t;

static const store_ev_t *pass_peek (pass_reader_t *p)
{
    if (p->buf_i < p->buf_n) return &buf_b[p->buf_i];
    if (p->file_next < s_pass_count)
    {
        if (f_lseek(&f_pass, (FSIZE_t)p->file_next * sizeof(store_ev_t)) != FR_OK) return NULL;
        const uint32_t left = s_pass_count - p->file_next;
        const UINT want = (UINT)((left < CHUNK ? left : CHUNK) * sizeof(store_ev_t));
        UINT br = 0;
        if (f_read(&f_pass, buf_b, want, &br) != FR_OK || br == 0U) return NULL;
        p->buf_n = (uint16_t)(br / sizeof(store_ev_t));
        p->buf_i = 0;
        p->file_next += p->buf_n;
        return &buf_b[0];
    }
    return (p->tail_i < p->n_tail) ? &p->tail[p->tail_i] : NULL;
}

static void pass_next (pass_reader_t *p)
{
    if (p->buf_i < p->buf_n) p->buf_i++;
    else if (p->tail_i < p->n_tail) p->tail_i++;
}

bool store_merge_pass (const store_touch_t *touched, uint8_t n_touched, uint32_t end_ms, const store_ev_t *tail, uint16_t n_tail)
{
    if (!store_new_open()) return false;
    pass_reader_t p = {0, 0, 0, tail, n_tail, 0};
    uint32_t old_i = 0;
    uint16_t a_n = 0, a_i = 0;
    bool ok = true;

    for (;;)
    {
        // the next old move still in the mix
        const store_ev_t *old = NULL;
        while (ok)
        {
            if (a_i >= a_n)
            {
                ok = store_read(old_i, buf_a, CHUNK, &a_n);
                a_i = 0;
                old_i += a_n;
                if (!ok || a_n == 0U) break;
            }
            if (!replaced(&buf_a[a_i], touched, n_touched, end_ms)) { old = &buf_a[a_i]; break; }
            a_i++;
        }
        const store_ev_t *nw = pass_peek(&p);
        if (!ok || (old == NULL && nw == NULL)) break;
        // in time order; at the same time the pass's move goes after the old one, so it wins
        if (nw == NULL || (old != NULL && old->t <= nw->t))
        {
            ok = store_new_put(old);
            a_i++;
        }
        else
        {
            ok = store_new_put(nw);
            pass_next(&p);
        }
    }
    if (!store_new_close(ok)) return false;
    (void)store_pass_reset(); // the pass is in the store now
    return ok;
}
