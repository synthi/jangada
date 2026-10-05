/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: LAYERS, after SLOOP 2.2 (isod89/sloop-fm1, GPL-3.0; teenage-engineering style). Hold a
 * function button: the 16 white keys and KNOB 1..4 change job while it is held, and after SHOW_MS the
 * screen shows the keys as 16 tiles (4 x 4) and the knobs as dials. Tapped (let go within TAP_MS,
 * nothing touched) the button opens its pages as before.
 *   FX    the 16 punch-in effects (punch.c, run by seq.c keyboard_block)   knobs: FILTER DUST DUCK
 *   GLO   keys 1..4 mute, 5..8 solo, the last white key: tap tempo         knobs: the levels of tracks 1..4
 * HOME tapped while a layer button is held locks the layer open (both hands free); any other button
 * lets it go and does only that, PLAY, REC and OCT- / OCT+ keep working inside it.
 * Colours: the palette's (CHOQUE by default), white for what is on. */
#define TAP_MS 450u                                     /* a press shorter than this, untouched: a tap */
#define SHOW_MS 140u                                    /* the layer shows after this (a tap does not flash it) */
static const uint8_t LAYER_BTN[LY_COUNT] = {NB, B_FX, B_GLO};
static const char *const LAYER_NAME[LY_COUNT] = {"", "PUNCH", "MIX"};

static struct {
    uint8_t btn;                 /* the layer whose button is held (0 none) */
    uint8_t lock;                /* the layer locked open by HOME (0 none) */
    uint8_t used;                /* a key or a knob was touched while the button was held: no tap */
    uint8_t shown;               /* the screen holds a layer */
    uint32_t t0;                 /* the press, fm1_ms */
    uint8_t tap_n;               /* tap tempo */
    uint32_t tap_ms[4];
    uint32_t head, tiles, foot;  /* drawn-state signatures */
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

/* a key-down of the layer (seq.c lk_q): k the key index, now its time */
static void layer_key(uint32_t layer, uint32_t k, uint32_t now)
{
    int32_t w = punch_key(k);                           /* white key 0..15, -1 black */
    if (layer != LY_MIX || w < 0)
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
        uint32_t k = lk_q[lk_r % LKQ], t = lk_t[lk_r % LKQ];
        lk_r++;
        layer_key(kb_layer, k, t);
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
} tile_t;

static uint32_t ly_hash(uint32_t h, const char *p) { while (*p) h = h * 31u + (uint8_t)*p++; return h; }

static void tiles_draw(const tile_t *tl)
{
    uint32_t r, c, sig = 7u;
    for (r = 0; r < 16u; r++)
        sig = ly_hash(sig * 31u + tl[r].bg * 3u + tl[r].fg * 5u + tl[r].top * 7u, tl[r].lab);
    if (!ui.force && sig == ly.tiles)
        return;
    ly.tiles = sig;
    for (r = 0; r < 4u; r++) {
        cv_begin(240, 36, C_BLACK);
        for (c = 0; c < 4u; c++) {
            const tile_t *t = &tl[r * 4u + c];
            int32_t x = 2 + (int32_t)c * 60;
            cv_rect(x, 2, 56, 32, t->bg);
            if (t->top)
                cv_rect(x, 2, 56, 3, t->top);
            cv_text(x + 28 - text_w(&FONT_S, t->lab) / 2, 10, &FONT_S, t->lab, t->fg);
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
