/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: LAYERS, after SLOOP 2.2 (isod89/sloop-fm1, GPL-3.0; teenage-engineering style). Hold a
 * function button: the 16 white keys and KNOB 1..4 change job while it is held, and after SHOW_MS the
 * screen shows the keys as 16 tiles (4 x 4) and the knobs as dials. Tapped (let go within TAP_MS,
 * nothing touched) the button opens its pages as before.
 *   FX    the 16 punch-in effects (punch.c, run by seq.c keyboard_block)   knobs: FILTER DUST DUCK
 *   GLO   keys 1..4 mute, 5..8 solo, the last white key: tap tempo         knobs: the levels of tracks 1..4
 *   SEQ   the 16 steps of the page (Elektron style): an empty step is set at once with the note played
 *         last, a set one is cleared when its key is let go, unless a knob edited it meanwhile; the
 *         first four black keys pick the page (steps 1-16 .. 49-64)
 *         knobs, no step held: NOTE (the pen)  DIV  SWING  LEN;  steps held: NOTE  RTCH  CHNC  FLAG
 * HOME tapped while a layer button is held locks the layer open (both hands free); any other button
 * lets it go and does only that, PLAY, REC and OCT- / OCT+ keep working inside it.
 * Colours: the palette's (CHOQUE by default), white for what is on. */
#define TAP_MS 450u                                     /* a press shorter than this, untouched: a tap */
#define SHOW_MS 140u                                    /* the layer shows after this (a tap does not flash it) */
static const uint8_t LAYER_BTN[LY_COUNT] = {NB, B_FX, B_GLO, B_SEQ};
static const char *const LAYER_NAME[LY_COUNT] = {"", "PUNCH", "MIX", "STEPS"};

static struct {
    uint8_t btn;                 /* the layer whose button is held (0 none) */
    uint8_t lock;                /* the layer locked open by HOME (0 none) */
    uint8_t used;                /* a key or a knob was touched while the button was held: no tap */
    uint8_t shown;               /* the screen holds a layer */
    uint32_t t0;                 /* the press, fm1_ms */
    uint8_t tap_n;               /* tap tempo */
    uint32_t tap_ms[4];
    uint32_t head, tiles, foot;  /* drawn-state signatures */
    uint8_t klay[27];            /* the layer each key went down in (its key-up goes there) */
    uint8_t page;                /* SEQ: steps page * 16 .. */
    uint16_t held;               /* SEQ: step keys down (white key bits) */
    uint16_t pend_off;           /* SEQ: steps that clear when their key is let go */
} ly;

static uint32_t layer_now(void) { return ly.lock ? ly.lock : ly.btn; }
static int layer_visible(void) { return layer_now() && (ly.lock || fm1_ms - ly.t0 >= SHOW_MS); }
static uint32_t key_of_white(uint32_t w)                /* white key 0..15 (from the lowest F) -> key index */
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 7, 9, 11};
    return (w / 7u) * 12u + OFF[w % 7u];
}
static int16_t *trk_level_p(uint32_t i)                 /* the track's LEVEL (the drum track: GLO > DRUMS) */
{
    return is_drum(&trk[i]) ? &song.g[G_DRLVL] : &trk[i].p[P_LEVEL];
}

/* --------------------------------------------------------------- GLO --- */
static void tap_tempo(uint32_t now)
{
    uint32_t i, n, sum = 0;
    if (ly.tap_n && now - ly.tap_ms[(ly.tap_n - 1u) & 3u] > 2000u)
        ly.tap_n = 0;                                   /* a pause: a new count */
    ly.tap_ms[ly.tap_n & 3u] = now;
    ly.tap_n++;
    if (ly.tap_n < 2u)
        return;
    n = ly.tap_n - 1u > 3u ? 3u : ly.tap_n - 1u;
    for (i = 0; i < n; i++)
        sum += ly.tap_ms[(ly.tap_n - 1u - i) & 3u] - ly.tap_ms[(ly.tap_n - 2u - i) & 3u];
    if (sum) {
        song.g[G_BPM] = (int16_t)clamp((int32_t)((60000u * n + sum / 2u) / sum), GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
}

/* --------------------------------------------------------------- SEQ --- */
static uint32_t trk_len(const track_t *t) { return t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u; }

static void step_down(uint32_t w)
{
    track_t *t = TSEL;
    uint32_t idx = ly.page * 16u + w;
    step_t *st = &t->step[idx];
    if (idx >= trk_len(t))
        return;
    if (step_on(st)) {                                  /* set: cleared when let go (unless edited) */
        ly.pend_off |= (uint16_t)(1u << w);
        return;
    }
    fm1_irq_off();
    st->note[0] = last_note;
    st->n = 1;
    st->time = ST_NOTE;
    st->flags = 0;
    st->vel = 100;
    fm1_irq_on();
}

static void step_up(uint32_t w)
{
    track_t *t = TSEL;
    uint32_t idx = ly.page * 16u + w;
    if (!((ly.pend_off >> w) & 1u))
        return;
    ly.pend_off &= (uint16_t)~(1u << w);
    if (idx < trk_len(t)) {
        fm1_irq_off();
        step_clear(&t->step[idx]);
        fm1_irq_on();
    }
}

/* a knob with step keys held: k 0 NOTE, 1 RTCH, 2 CHNC, 3 FLAG (- ACC SLD A+S), on every held step */
static void steps_held_edit(uint32_t k, int32_t s)
{
    track_t *t = TSEL;
    uint32_t w, i;
    ly.pend_off &= (uint16_t)~ly.held;                  /* edited: kept when let go */
    fm1_irq_off();
    for (w = 0; w < 16u; w++) {
        uint32_t idx = ly.page * 16u + w;
        step_t *st = &t->step[idx];
        if (!((ly.held >> w) & 1u) || idx >= trk_len(t) || !step_on(st))
            continue;
        if (k == 0u) {
            for (i = 0; i < st->n; i++)
                st->note[i] = (uint8_t)clamp(st->note[i] + s, 1, 127);
            last_note = st->note[0];
        } else {
            uint32_t sh = k == 1u ? SF_RATCH_SH : k == 2u ? SF_CHANCE_SH : 0u, m = 3u << sh;
            int32_t v = (int32_t)((st->flags & m) >> sh) + (s > 0 ? 1 : -1);
            st->flags = (uint8_t)((st->flags & ~m) | ((uint32_t)clamp(v, 0, 3) << sh));
        }
    }
    fm1_irq_on();
}

/* a key of the layer (seq.c lk_q): k the key index, down / up, now its time */
static void layer_key(uint32_t layer, uint32_t k, uint32_t down, uint32_t now)
{
    int32_t w = punch_key(k);                           /* white key 0..15, -1 black */
    if (layer == LY_STEP) {
        if (w < 0) {                                    /* the first four black keys: pages 1..4 */
            static const int8_t PG[12] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, -1, -1};
            if (down && k < 12u && PG[k] >= 0 && (uint32_t)PG[k] * 16u < trk_len(TSEL))
                ly.page = (uint8_t)PG[k];
            return;
        }
        if (down) {
            ly.held |= (uint16_t)(1u << w);
            step_down((uint32_t)w);
        } else {
            ly.held &= (uint16_t)~(1u << w);
            step_up((uint32_t)w);
        }
        return;
    }
    if (layer != LY_MIX || w < 0 || !down)
        return;
    if (w < 4) {
        trk[w].p[P_MUTE] = (int16_t)!trk[w].p[P_MUTE];
    } else if (w < 8) {
        song.solo ^= (uint8_t)(1u << (w - 4));
    } else if (w == 15) {
        tap_tempo(now);
    }
}

/* the layer buttons: tap / hold / lock. pressed: this frame's press edges; the ones handled here are
 * taken out of it. Returns 1 when HOME's press belongs to the layer (it locked or let go a lock). */
static uint32_t fam_of_btn(uint32_t b)
{
    uint32_t f;
    for (f = FAM_HOME + 1u; f < FAM_COUNT; f++)
        if (FAM_BTN[f] == b)
            return f;
    return FAM_HOME;
}

static void layers_input(uint32_t *pressed, uint32_t now)
{
    uint32_t l, id, bit;
    const uint32_t keep = (1u << panel.btn[B_PLAY]) | (1u << panel.btn[B_REC]) | (1u << panel.btn[B_OCTDN]) |
                          (1u << panel.btn[B_OCTUP]);
    if (ui.menu || ui.confirm) {                        /* no layer over the menu or a dialog */
        ly.btn = ly.lock = 0;
        kb_layer = 0;
        return;
    }
    if (ly.lock) {                                      /* locked: another button lets it go, and does only that */
        uint32_t other = *pressed & ~keep;
        if (other) {
            ly.lock = 0;
            ly.btn = 0;
            *pressed &= ~other;
            if ((other >> panel.btn[B_HOME]) & 1u)
                ui.home_t0 |= 2u;                       /* (no HOME tap on its release) */
            ui.force = 1;
        }
    }
    for (l = LY_FX; l < LY_COUNT; l++) {               /* a layer button pressed: never acts on the press */
        bit = 1u << panel.btn[LAYER_BTN[l]];
        if (!(*pressed & bit))
            continue;
        *pressed &= ~bit;
        ly.btn = (uint8_t)l;
        ly.t0 = now;
        ly.used = 0;
    }
    if (ly.btn && !((fm1_in.buttons >> panel.btn[LAYER_BTN[ly.btn]]) & 1u)) {   /* let go */
        if (!ly.used && !ly.lock && now - ly.t0 < TAP_MS)
            open_family(fam_of_btn(LAYER_BTN[ly.btn]));  /* a tap: its pages */
        ly.btn = 0;
        ui.force = 1;
    }
    if (ly.btn) {
        if ((*pressed >> panel.btn[B_HOME]) & 1u) {     /* HOME while held: lock it open */
            ly.lock = ly.btn;
            ly.used = 1;
            *pressed &= ~(1u << panel.btn[B_HOME]);
            ui.home_t0 |= 2u;
            ui_message("LOCKED");
        }
        if (fm1_in.notes)
            ly.used = 1;
        for (id = 0; id < 14u; id++)                    /* other buttons while held: no tap either */
            if ((*pressed >> id) & 1u)
                ly.used = 1;
    }
    kb_layer = (uint8_t)layer_now();
    while (lk_r != lk_w) {                              /* the layer keys from the ISR */
        uint32_t v = lk_q[lk_r % LKQ], t = lk_t[lk_r % LKQ], k = (v & 0x7Fu) % 27u, up = (v & LK_UP) != 0u;
        lk_r++;
        if (!up)
            ly.klay[k] = (uint8_t)(v >> 8);
        layer_key(ly.klay[k], k, !up, t);
    }
}

/* KNOB 1..4 while a layer is held: what the layer gives them (the page does not see them) */
static void layers_knobs(uint32_t layer)
{
    uint32_t k;
    int32_t s;
    for (k = 0; k < 4u; k++) {
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        ly.used = 1;
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
        if (layer == LY_FX) {
            if (k == 0u)
                song.g[G_FILT] = (int16_t)clamp(song.g[G_FILT] + accel(EN_K1, s, 127), -64, 63);
            else if (k == 1u)
                song.g[G_DUST] = (int16_t)clamp(song.g[G_DUST] + accel(EN_K2, s, 127), 0, 127);
            else if (k == 2u)
                song.g[G_DUCK] = (int16_t)clamp(song.g[G_DUCK] + accel(EN_K3, s, 127), 0, 127);
        } else if (layer == LY_MIX) {
            int16_t *lv = trk_level_p(k);
            *lv = (int16_t)clamp(*lv + accel(EN_K1 + k, s, 127), 0, 127);
        } else if (layer == LY_STEP) {
            track_t *t = TSEL;
            if (ly.held) {
                steps_held_edit(k, s);
            } else if (k == 0u) {
                last_note = (uint8_t)clamp(last_note + s, 1, 127);   /* the pen: the note a new step gets */
            } else if (k == 1u) {
                t->p[P_SDIV] = (int16_t)clamp(t->p[P_SDIV] + s, TP[P_SDIV].min, TP[P_SDIV].max);
            } else if (k == 2u) {
                const param_desc_t *d = track_desc(t, P_SSWING);
                t->p[P_SSWING] = (int16_t)clamp(t->p[P_SSWING] + accel(EN_K3, s, d->max - d->min), d->min, d->max);
            } else {
                t->p[P_SLEN] = (int16_t)clamp(t->p[P_SLEN] + accel(EN_K4, s, 63), 1, NSTEP);
                if ((uint32_t)ly.page * 16u >= trk_len(t))
                    ly.page = (uint8_t)((trk_len(t) - 1u) / 16u);
            }
        }
    }
}

/* the key LEDs while a layer is shown: what is on (an effect, an unmuted track, a solo), else the first
 * key of each row of tiles (1, 5, 9, 13) to find them without looking */
static uint32_t layers_key_leds(void)
{
    uint32_t w, m = 0, layer = layer_now();
    for (w = 0; w < 16u; w++) {
        int on = 0;
        if (layer == LY_FX)
            on = punch.req == (int8_t)w || (punch.req < 0 && (w & 3u) == 0u);
        else if (layer == LY_MIX)
            on = w < 4u ? !trk[w].p[P_MUTE] : w < 8u ? (int)((song.solo >> (w - 4u)) & 1u) : w == 15u;
        else if (layer == LY_STEP) {                    /* the set steps; the playhead blinks off */
            uint32_t idx = ly.page * 16u + w;
            on = idx < trk_len(TSEL) && step_on(&TSEL->step[idx]);
            if (song.playing && idx == TSEL->seq_idx)
                on = !on;
        }
        if (on)
            m |= 1u << key_of_white(w);
    }
    return m;
}

static void layers_leds(uint8_t *nl)                   /* ui_leds: the keys, and the layer's button */
{
    uint32_t k, m = layers_key_leds() | fm1_in.notes;
    for (k = 0; k < 27u; k++)
        led_put(nl, 14u + k, (int)((m >> k) & 1u));
    led_put(nl, panel.btn[LAYER_BTN[layer_now()]], ly.lock ? (int)((fm1_ms >> 8) & 1u) : 1);   /* locked: blinks */
}

/* ----------------------------------------------------------- drawing --- */
typedef struct {
    char lab[8];
    uint16_t bg, fg, top;        /* fill, text, the 3-pixel top band (0 = none) */
    uint8_t marks;               /* small squares under the label (a ratchet), 0 = none */
} tile_t;

static uint32_t ly_hash(uint32_t h, const char *p) { while (*p) h = h * 31u + (uint8_t)*p++; return h; }

static void tiles_draw(const tile_t *tl)
{
    uint32_t r, c, sig = 7u;
    for (r = 0; r < 16u; r++)
        sig = ly_hash(sig * 31u + tl[r].bg * 3u + tl[r].fg * 5u + tl[r].top * 7u + tl[r].marks, tl[r].lab);
    if (!ui.force && sig == ly.tiles)
        return;
    ly.tiles = sig;
    for (r = 0; r < 4u; r++) {
        cv_begin(240, 36, C_BLACK);
        for (c = 0; c < 4u; c++) {
            const tile_t *t = &tl[r * 4u + c];
            int32_t x = 2 + (int32_t)c * 60;
            uint32_t m;
            cv_rect(x, 2, 56, 32, t->bg);
            if (t->top)
                cv_rect(x, 2, 56, 3, t->top);
            cv_text(x + 28 - text_w(&FONT_S, t->lab) / 2, 8, &FONT_S, t->lab, t->fg);
            for (m = 0; m < t->marks; m++)
                cv_rect(x + 22 + (int32_t)m * 5, 27, 3, 3, t->fg);
        }
        cv_blit(0, 40 + r * 36);
    }
}

static void layer_title(const char *name, const char *sub)
{
    uint32_t sig = ly_hash(ly_hash(ly.lock * 7919u, name), ui.msg_t ? ui.msg : sub);
    if (!ui.force && sig == ly.head)
        return;
    ly.head = sig;
    cv_begin(240, 40, C_BLACK);
    cv_text(4, 2, &FONT_L, name, C_HI);
    cv_text(4 + text_w(&FONT_L, name) + 10, 18, &FONT_S, ui.msg_t ? ui.msg : sub, ui.msg_t ? C_WHITE : C_GRAY);
    if (ly.lock) {                                      /* locked open: any button lets it go */
        int32_t w = text_w(&FONT_S, "LOCK") + 8;
        cv_rect(236 - w, 4, w, 18, C_WHITE);
        cv_text(240 - w, 5, &FONT_S, "LOCK", C_BLACK);
    }
    cv_rect(0, 38, 240, 1, C_LINE);
    cv_blit(0, 0);
}

/* a dial: a 270-degree ring lit up to ratio (0..1000) with a pointer; -1 = a plain ring */
static void ly_dial(int32_t cx, int32_t cy, int32_t r, int32_t ratio, uint16_t c, uint16_t dim)
{
    int32_t i, k, end = ratio < 0 ? 768 : ratio * 768 / 1000;
    for (i = 0; i <= 768; i += 8) {
        uint32_t a = (uint32_t)(384 + i) & 1023u;
        int32_t co = SINE[(a + 256u) & 1023u], si = SINE[a];
        for (k = r - 2; k <= r; k++)
            cv_pset(cx + ((co * k) >> 15), cy + ((si * k) >> 15), ratio < 0 || i <= end ? c : dim);
    }
    if (ratio >= 0) {
        uint32_t a = (uint32_t)(384 + end) & 1023u;
        int32_t co = SINE[(a + 256u) & 1023u], si = SINE[a];
        cv_line(cx, cy, cx + ((co * (r - 4)) >> 15), cy + ((si * (r - 4)) >> 15), C_WHITE);
        cv_rect(cx - 1, cy - 1, 3, 3, C_WHITE);
    }
}

/* the dial strip: KNOB 1..4, a label and a value under each (no label: an empty column) */
static void layer_dials(const char *const lab[4], const char *const val[4], const int32_t ratio[4], uint32_t sig)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        sig = ly_hash(ly_hash(sig * 7u + (uint32_t)ratio[k] + (ui.hot_t && ui.hot_col == k) * 5003u, lab[k]), val[k]);
    if (!ui.force && sig == ly.foot)
        return;
    ly.foot = sig;
    cv_begin(240, 56, C_BLACK);
    for (k = 0; k < 4u; k++) {
        int32_t cx = 30 + 60 * (int32_t)k;
        uint16_t hot = ui.hot_t && ui.hot_col == k ? C_WHITE : C_AMB;
        if (!lab[k][0])
            continue;
        ly_dial(cx, 13, 11, ratio[k], C_HI, C_DIM);
        cv_text(cx - text_w(&FONT_S, lab[k]) / 2, 25, &FONT_S, lab[k], C_GRAY);
        cv_text(cx - text_w(&FONT_S, val[k]) / 2, 40, &FONT_S, val[k], hot);
    }
    cv_blit(0, 184);
}

static void layer_screen_draw(void)
{
    static tile_t tl[16];
    static char v[4][10];
    const char *lab[4] = {"", "", "", ""}, *val[4] = {v[0], v[1], v[2], v[3]}, *sub = "";
    int32_t ratio[4] = {-1, -1, -1, -1};
    uint32_t i, layer = layer_now();
    if (!ly.shown) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
        ly.shown = 1;
    }
    for (i = 0; i < 4u; i++)
        v[i][0] = 0;
    memset(tl, 0, sizeof tl);
    if (layer == LY_FX) {                               /* the 16 punch-in effects */
        static const char *const PSHORT[16] = {"LOOP4", "LOOP8", "LOOP16", "LOOP32", "STUTT", "REV", "STOP", "HALF",
                                               "LOW", "HIGH", "PHONE", "CRUSH", "ALIAS", "GATE", "ECHO", "WOBBL"};
        sub = "HOLD + KEY";
        for (i = 0; i < 16u; i++) {
            int on = punch.req == (int8_t)i;
            str_cpy(tl[i].lab, PSHORT[i], 8);
            tl[i].bg = on ? C_WHITE : C_LINE;
            tl[i].fg = on ? C_BLACK : C_AMB;
            tl[i].top = on ? 0 : (i & 4u) ? C_DIM : C_GRAY;   /* rows alternate: easier to count */
        }
        lab[0] = "FILT", lab[1] = "DUST", lab[2] = "DUCK";
        {
            int32_t f = song.g[G_FILT];
            if (!f)
                str_cpy(v[0], "OFF", 10);
            else {
                str_cpy(v[0], f < 0 ? "LP" : "HP", 10);
                fmt_int(v[0] + 2, f < 0 ? (-f * 100 + 32) / 64 : (f * 100 + 31) / 63);
            }
            fmt_int(v[1], song.g[G_DUST] * 100 / 127);
            fmt_int(v[2], song.g[G_DUCK] * 100 / 127);
        }
        ratio[0] = (song.g[G_FILT] + 64) * 1000 / 127;
        ratio[1] = song.g[G_DUST] * 1000 / 127;
        ratio[2] = song.g[G_DUCK] * 1000 / 127;
    } else if (layer == LY_MIX) {                       /* mute 1..4, solo 1..4, tap */
        static const char *const L[4] = {"T1", "T2", "T3", "T4"};
        sub = "MUTE  SOLO  TAP";
        for (i = 0; i < 16u; i++) {
            tl[i].bg = C_BLACK;
            tl[i].fg = C_DIM;
        }
        for (i = 0; i < 4u; i++) {
            int m = trk[i].p[P_MUTE] != 0, so = (song.solo >> i) & 1u;
            str_cpy(tl[i].lab, "MUTE 1", 8);
            tl[i].lab[5] = (char)('1' + i);
            tl[i].bg = m ? C_LINE : C_AMB;
            tl[i].fg = m ? C_DIM : C_BLACK;
            str_cpy(tl[4 + i].lab, "SOLO 1", 8);
            tl[4 + i].lab[5] = (char)('1' + i);
            tl[4 + i].bg = so ? C_WHITE : C_LINE;
            tl[4 + i].fg = so ? C_BLACK : C_GRAY;
            tl[4 + i].top = C_DIM;
        }
        str_cpy(tl[14].lab, "TAP>", 8);
        fmt_int(tl[15].lab, song.g[G_BPM]);
        tl[15].bg = song.playing && clk_pos < BEAT_U / 4u ? C_WHITE : C_DIM;   /* the beat */
        tl[15].fg = tl[15].bg == C_WHITE ? C_BLACK : C_WHITE;
        for (i = 0; i < 4u; i++) {
            int32_t lv = *trk_level_p(i) & 127;
            lab[i] = L[i];
            fmt_int(v[i], lv * 100 / 127);
            ratio[i] = lv * 1000 / 127;
        }
    }
    else if (layer == LY_STEP) {                        /* the 16 steps of the page */
        static char pg[16];
        track_t *t = TSEL;
        uint32_t len = trk_len(t);
        for (i = 0; i < 16u; i++) {
            uint32_t idx = ly.page * 16u + i;
            const step_t *st = &t->step[idx];
            int on = step_on(st);
            if (idx >= len) {
                tl[i].bg = C_BLACK;
                continue;
            }
            if (on)
                note_name(tl[i].lab, st->note[0]);
            else if (st->time == ST_TIE && st->n)
                str_cpy(tl[i].lab, "--", 8);
            else
                fmt_int(tl[i].lab, (int32_t)idx + 1);
            tl[i].bg = on ? ((st->flags & SF_ACCENT) ? C_HI : C_AMB) : C_LINE;
            tl[i].fg = on ? C_BLACK : C_DIM;
            tl[i].marks = (uint8_t)(on ? (st->flags & SF_RATCH) >> SF_RATCH_SH : 0u);
            if (on && (st->flags & SF_CHANCE))
                tl[i].top = C_DIM;                      /* not every time */
            if (song.playing && idx == t->seq_idx)
                tl[i].top = C_WHITE;                    /* the playhead */
            if ((ly.held >> i) & 1u) {
                tl[i].bg = C_WHITE;
                tl[i].fg = C_BLACK;
            }
        }
        str_cpy(pg, "TRACK 1", sizeof pg);
        pg[6] = (char)('1' + song.sel);
        if (len > 16u) {
            str_cpy(pg + 7, "  1/1", 6);
            pg[9] = (char)('1' + ly.page);
            pg[11] = (char)('0' + (len + 15u) / 16u);
        }
        sub = pg;
        if (ly.held) {
            const step_t *st = 0;
            for (i = 0; i < 16u; i++)
                if (((ly.held >> i) & 1u) && step_on(&t->step[ly.page * 16u + i])) {
                    st = &t->step[ly.page * 16u + i];
                    break;
                }
            lab[0] = "NOTE", lab[1] = "RTCH", lab[2] = "CHNC", lab[3] = "FLAG";
            if (st) {
                static const char *const FL[4] = {"-", "ACC", "SLD", "A+S"};
                uint32_t r = (st->flags & SF_RATCH) >> SF_RATCH_SH, c = (st->flags & SF_CHANCE) >> SF_CHANCE_SH;
                note_name(v[0], st->note[0]);
                str_cpy(v[1], "X1", 10);
                v[1][1] = (char)('1' + r);
                fmt_int(v[2], 100 - 25 * (int32_t)c);
                str_cpy(v[3], FL[st->flags & 3u], 10);
                ratio[0] = st->note[0] * 1000 / 127;
                ratio[1] = (int32_t)r * 333;
                ratio[2] = 1000 - (int32_t)c * 333;
            }
        } else {
            const char *u;
            lab[0] = "NOTE", lab[1] = "DIV", lab[2] = "SWG", lab[3] = "LEN";
            note_name(v[0], last_note);
            param_format(track_desc(t, P_SDIV), t->p[P_SDIV], v[1], &u);
            param_format(track_desc(t, P_SSWING), t->p[P_SSWING], v[2], &u);
            fmt_int(v[3], (int32_t)len);
            ratio[0] = last_note * 1000 / 127;
            ratio[1] = (t->p[P_SDIV] - TP[P_SDIV].min) * 1000 / (TP[P_SDIV].max - TP[P_SDIV].min);
            ratio[2] = t->p[P_SSWING] * 10;
            ratio[3] = ((int32_t)len - 1) * 1000 / 63;
        }
    }
    layer_title(LAYER_NAME[layer % LY_COUNT], sub);
    tiles_draw(tl);
    layer_dials(lab, val, ratio, layer * 7919u);
    if (ui.msg_t)
        ui.msg_t--;
    if (ui.hot_t)
        ui.hot_t--;
    if (ui.bpm_t)
        ui.bpm_t--;
    ui.force = 0;
}

/* ui_draw: the layer's screen when one shows (1), else back to the page once */
static int layers_draw(void)
{
    if (layer_visible()) {
        layer_screen_draw();
        return 1;
    }
    if (ly.shown) {
        ly.shown = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.force = 1;
    }
    return 0;
}
