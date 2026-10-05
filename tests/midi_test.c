/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: MIDI beyond notes (seq.c midi_cc, midi_clock_in / _out) on the host: pitch bend, the
 * sustain pedal, ALL NOTES OFF, the controllers as modulation sources, the clock in (tempo, START /
 * STOP) and out (24 a beat, START / STOP). Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t out[2 * CTL];
static uint32_t fails;

static void ok(int c, const char *what)
{
    printf("%-50s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2)          /* as usb.c queues it */
{
    uint32_t cin = st >= 0xF8u ? 0xFu : st >> 4;
    midi_in_q[mi_w % MQ] = cin | st << 8 | d1 << 16 | d2 << 24;
    mi_w++;
}

static void run(uint32_t blocks)
{
    while (blocks--)
        mix_block(out, CTL);
}

static uint32_t gates(const track_t *t)
{
    uint32_t i, g = 0;
    for (i = 0; i < NVOICE; i++)
        g += t->v[i].active && t->v[i].gate;
    return g;
}

static void setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    song.g[G_BPM] = 120;
    song.master_q12 = 4096;
    mi_r = mi_w = 0;
}

int main(void)
{
    track_t *t = &trk[0];
    setup();

    midi(0xE0, 0x7F, 0x7F);                                   /* bend up, full */
    run(1);
    ok(t->bend16 == 31, "pitch bend up: +2 semitones (31/16)");
    midi(0xE0, 0x00, 0x40);
    run(1);
    ok(t->bend16 == 0, "pitch bend centre: 0");
    midi(0xE1, 0x00, 0x00);
    run(1);
    ok(trk[1].bend16 == -32 && t->bend16 == 0, "bend on channel 2 moves track 2 only");

    midi(0x90, 60, 100);
    midi(0xB0, 64, 127);                                      /* pedal down */
    midi(0x80, 60, 0);
    run(4);
    ok(gates(t) == 1u, "sustain: a released note keeps sounding");
    midi(0xB0, 64, 0);                                        /* pedal up */
    run(4);
    ok(gates(t) == 0u, "sustain up: the held note is let go");

    midi(0x90, 60, 100);
    midi(0x90, 64, 100);
    run(4);
    midi(0xB0, 123, 0);
    run(8);
    ok(gates(t) == 0u, "CC123 ALL NOTES OFF: the track is quiet");

    midi(0xB0, 1, 100);
    midi(0xD0, 50, 0);
    midi(0xB0, 11, 90);
    run(1);
    ok(mod_src(t, &t->v[0], MS_MODW, 0, 0) == 100 * 258 && mod_src(t, &t->v[0], MS_AT, 0, 0) == 50 * 258 &&
           mod_src(t, &t->v[0], MS_EXPR, 0, 0) == 90 * 258, "MODW / AT / EXPR as modulation sources");
    midi(0xB0, 121, 0);
    run(1);
    ok(!t->mw && !t->at && !t->ex && !t->bend16, "CC121 RESET ALL CONTROLLERS");

    {   /* clock in: 24 clocks a beat at 100 BPM -> BPM 100; START / STOP */
        uint32_t k, per = 60u * FS / 100u / 24u / CTL;       /* blocks between two clocks */
        setup();
        song.g[G_CLOCK] = 1;
        midi(0xFA, 0, 0);
        run(2);                                               /* (the transport moves on the next block) */
        ok(song.playing, "clock in: START plays");
        for (k = 0; k < 24u * 4u; k++) {
            midi(0xF8, 0, 0);
            run(per);
        }
        ok(song.g[G_BPM] >= 99 && song.g[G_BPM] <= 101, "clock in: 24 a beat at 100 BPM -> BPM 100");
        midi(0xFC, 0, 0);
        run(2);
        ok(!song.playing, "clock in: STOP stops");
        song.g[G_CLOCK] = 0;
        song.g[G_BPM] = 120;
        midi(0xF8, 0, 0);
        midi(0xFA, 0, 0);
        run(2);
        ok(!song.playing && song.g[G_BPM] == 120, "clock INT: the host's clock is ignored");
    }

    {   /* clock out: START, 24 a beat, STOP; none while following */
        uint32_t f8 = 0, fa = 0, fc = 0, b;
        setup();
        usb.config = 1;
        song.g[G_SYNC] = 1;
        mo_r = mo_w = 0;
        transport_req = 1;
        for (b = 0; b < 4u * 60u * FS / 120u / CTL; b++) {  /* 4 beats at 120 */
            run(1);
            while (mo_r != mo_w) {
                uint32_t p = midi_out_q[mo_r++ % MQ];
                if ((p & 0xFFu) == 0x0Fu) {
                    f8 += ((p >> 8) & 0xFFu) == 0xF8u;
                    fa += ((p >> 8) & 0xFFu) == 0xFAu;
                }
            }
        }
        transport_req = 2;
        run(2);
        while (mo_r != mo_w) {
            uint32_t p = midi_out_q[mo_r++ % MQ];
            fc += (p & 0xFFu) == 0x0Fu && ((p >> 8) & 0xFFu) == 0xFCu;
        }
        ok(fa == 1u && fc == 1u, "clock out: START, STOP");
        ok(f8 >= 95u && f8 <= 97u, "clock out: 24 clocks a beat (96 in 4 beats)");
        if (f8 < 95u || f8 > 97u)
            printf("   (got %u)\n", f8);
    }
    if (fails)
        printf("MIDI: %u FAILED\n", fails);
    return fails != 0;
}
