/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: the encoder decoder (firmware/hal/fm1_enc.h) against turns a hand makes, next to
 * Felucca's (copied below) to show what it got wrong (Felucca #23: knobs that skip clicks or
 * jump two steps a click). One "frame" is one scan of the panel (~0.6 ms). */
#include <stdio.h>
#include <string.h>
#include "../firmware/hal/fm1_enc.h"

/* ---- Felucca's decoder, as it was (fm1_input.h fm1__frame), for comparison */
typedef struct { uint8_t prev, last, rest, still; int8_t sub; } old_t;
static int old_frame(old_t *s, uint32_t cur)
{
    uint32_t idx;
    int step = 0;
    if (cur != s->last) {
        s->last = (uint8_t)cur;
        s->still = 0;
        return 0;
    }
    if (s->prev == 0xFF) {
        s->prev = (uint8_t)cur;
        s->rest = (uint8_t)(1u << cur);
    }
    if (s->still < 255u && ++s->still == 40u) {
        uint32_t r = s->rest, bit = 1u << cur, comp = 1u << (cur ^ 3u);
        if (!(r & bit))
            s->rest = (uint8_t)(r == comp ? (r | bit) : bit);
    }
    if (cur == s->prev)
        return 0;
    idx = (uint32_t)s->prev << 2 | cur;
    if ((0x4182u >> idx) & 1u)
        s->sub++;
    else if ((0x2814u >> idx) & 1u)
        s->sub--;
    s->prev = (uint8_t)cur;
    if ((s->rest >> cur) & 1u) {
        if (s->sub >= 2)
            step = 1;
        else if (s->sub <= -2)
            step = -1;
        s->sub = 0;
    }
    return step;
}

/* ---- a hand on a knob */
static const uint8_t CW[4] = {0, 1, 3, 2};        /* clockwise quadrature order (00 01 11 10) */
static fm1_enc_t nw;
static old_t od;
static long got_new, got_old;
static uint32_t pos;                               /* index into CW */

static void hold(uint32_t frames)
{
    while (frames--) {
        got_new += fm1_enc_frame(&nw, CW[pos & 3u]);
        got_old += old_frame(&od, CW[pos & 3u]);
    }
}
static void start(uint32_t detent)
{
    fm1_enc_init(&nw);
    memset(&od, 0, sizeof od);
    od.prev = od.last = 0xFF;
    got_new = got_old = 0;
    pos = detent;
    hold(400);                                     /* powered on resting on a detent */
}
/* a few clicks let go on each detent: until then a half-cycle encoder only knows the detent it
 * was powered on at, and the first arrivals at the other one are lost (Felucca's too) */
static void click(uint32_t per, uint32_t fast, uint32_t mid, uint32_t rest);
static void learn(uint32_t per)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        click(per, 3, 0, 300);
    got_new = got_old = 0;
}
/* one click clockwise of an encoder with `per` transitions a click (4 full cycle, 2 half);
 * each state held `fast` frames; `mid` frames of pause half way (0 = none); `rest` after */
static void click(uint32_t per, uint32_t fast, uint32_t mid, uint32_t rest)
{
    uint32_t k;
    for (k = 0; k < per; k++) {
        pos++;
        hold(fast);
        if (k == per / 2u - 1u && mid)
            hold(mid);                             /* the finger stops part way */
    }
    hold(rest);
}

static int fails;
static void check(const char *what, long want)
{
    int ok = got_new == want;
    printf("%-58s new %3ld  old %3ld  want %3ld  %s\n", what, got_new, got_old, want, ok ? "ok" : "FAIL");
    fails += !ok;
}

int main(void)
{
    static const uint32_t TYPE[2] = {4, 2};
    static const char *const NAME[2] = {"full-cycle", "half-cycle"};
    uint32_t t, i;
    char what[96];
    for (t = 0; t < 2u; t++) {
        uint32_t per = TYPE[t];
        start(0);
        learn(per);
        for (i = 0; i < 20u; i++)
            click(per, 3, 0, i % 3u ? 300 : 20);   /* let go on most clicks */
        snprintf(what, sizeof what, "%s: 20 clicks, let go between", NAME[t]);
        check(what, 20);

        start(0);
        learn(per);
        for (i = 0; i < 10u; i++)
            click(per, 3, 0, 300);
        for (i = 0; i < 20u; i++)
            click(per, 3, 70, 300);                /* slow turns: 70-frame pauses part way */
        snprintf(what, sizeof what, "%s: + 20 slow clicks with pauses part way", NAME[t]);
        check(what, 30);

        start(0);
        learn(per);
        for (i = 0; i < 12u; i++)
            click(per, 3, 0, 300);
        click(per, 3, 300, 300);                   /* once, held part way as long as a rest */
        for (i = 0; i < 20u; i++)
            click(per, 3, 0, 300);
        snprintf(what, sizeof what, "%s: one long hold part way, 33 clicks", NAME[t]);
        check(what, 33);

        start(0);
        learn(per);
        for (i = 0; i < 6u; i++)
            click(per, 3, 0, 300);
        for (i = 0; i < 40u; i++)
            click(per, 2, 0, 0);                   /* a fast spin */
        hold(300);
        snprintf(what, sizeof what, "%s: + a fast spin of 40", NAME[t]);
        check(what, 46);
    }
    puts(fails ? "ENCODER TEST FAILED" : "encoders: all ok");
    return fails != 0;
}
