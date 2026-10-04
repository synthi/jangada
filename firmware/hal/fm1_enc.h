/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: the encoders' quadrature decoder with detent learning, no hardware here, so the host
 * can test it (tests/enc_test.c). fm1_input.h runs it once a scan frame per encoder.
 *
 * A click is one step at any speed, whether a detent is a full quadrature cycle (one rest state)
 * or a half (two complementary rest states, 00/11 or 01/10). Felucca learned the rest states from
 * any 40-frame pause (~25 ms): a slow turn that paused part way through a click taught a wrong
 * set, and from then on the knob skipped every other click or gave two steps a click (Felucca
 * #23). Here a rest has to last FM1_ENC_REST frames, and each counts as evidence: the set is the
 * state rested in most, plus its complement when that is seen too (at least a quarter as often).
 * A detent is where the knob rests every time it is let go; a pause part way is rare and
 * outvoted. */
#pragma once
#include <stdint.h>

#define FM1_ENC_REST 240u         /* frames still (~150 ms at ~0.6 ms a frame) = the knob let go */
#define FM1_ENC_SEEN_MAX 64u      /* evidence is halved there: it keeps adapting, slowly */

typedef struct {
    uint8_t prev, last;           /* the last decoded state; the last raw sample (2-sample filter) */
    uint8_t still;                /* frames in the same state, up to 255 */
    uint8_t rest;                 /* the rest (detent) states, bit per state */
    int8_t sub;                   /* net transitions since the last rest state */
    uint8_t seen[4];              /* rests in each state */
} fm1_enc_t;

static void fm1_enc_init(fm1_enc_t *s)
{
    uint32_t i;
    s->prev = s->last = 0xFF;     /* seeded by the first frame */
    s->still = 0;
    s->rest = 0;
    s->sub = 0;
    for (i = 0; i < 4u; i++)
        s->seen[i] = 0;
}

/* the rest set from the evidence */
static uint8_t fm1__enc_set(const fm1_enc_t *s)
{
    uint32_t i, best = 0, c;
    for (i = 1; i < 4u; i++)
        if (s->seen[i] > s->seen[best])
            best = i;
    c = best ^ 3u;
    return (uint8_t)(1u << best | (s->seen[c] && s->seen[c] * 4u >= s->seen[best] ? 1u << c : 0u));
}

/* one frame: cur = this frame's A (bit 1) and B (bit 0); returns the step it emits (-1, 0, +1) */
static int fm1_enc_frame(fm1_enc_t *s, uint32_t cur)
{
    uint32_t idx;
    int step = 0;
    if (cur != s->last) {                          /* 2-sample filter */
        s->last = (uint8_t)cur;
        s->still = 0;
        return 0;
    }
    if (s->prev == 0xFF) {                         /* first frame: the knob rests here */
        s->prev = (uint8_t)cur;
        s->seen[cur] = 1;
        s->rest = (uint8_t)(1u << cur);
    }
    if (s->still < 255u && ++s->still == FM1_ENC_REST) {   /* let go here: evidence */
        if (++s->seen[cur] >= FM1_ENC_SEEN_MAX)
            for (idx = 0; idx < 4u; idx++)
                s->seen[idx] >>= 1;
        s->rest = fm1__enc_set(s);
    }
    if (cur == s->prev)
        return 0;
    idx = (uint32_t)s->prev << 2 | cur;
    if ((0x4182u >> idx) & 1u)
        s->sub++;
    else if ((0x2814u >> idx) & 1u)
        s->sub--;
    s->prev = (uint8_t)cur;
    if ((s->rest >> cur) & 1u) {                   /* back on a detent */
        if (s->sub >= 2)
            step = 1;
        else if (s->sub <= -2)
            step = -1;
        s->sub = 0;
    }
    return step;
}
