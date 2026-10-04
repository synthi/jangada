/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: track 4 as the GM drum track (default) or a fourth synth part (GLO > DRUMS T4).
 * Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static int32_t buf[2 * CTL];
static void run(uint32_t blocks)
{
    while (blocks--)
        mix_block(buf, CTL);
}
static uint32_t active(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}
static uint32_t gates(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].gate;
    return n;
}
static uint32_t drums_on(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < NDRUM; i++)
        n += drums.v[i].active;
    return n;
}
static uint32_t peak(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < 2u * CTL; i++)
        m = (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]) > m ? (uint32_t)(buf[i] < 0 ? -buf[i] : buf[i]) : m;
    return m;
}
static void midi(uint32_t ch, uint32_t note, uint32_t vel)
{
    if (vel)
        input_on(midi_route(ch, note, 1), note, vel);
    else
        input_off(midi_route(ch, note, 0), note);
}

/* what ui.c t4_follow does (the UI is not built on the host) */
static void t4_set(uint32_t synth)
{
    track_t *t = TDRUM;
    uint32_t i;
    song.g[G_T4] = (int16_t)synth;
    for (i = 0; i < NVOICE; i++)
        t->v[i].active = t->v[i].gate = 0;
    t->nheld = t->arp_phys = t->arp_note = t->arp_nch = t->seq_n = t->rat_left = 0;
    t->nmono = t->mono_note = t->xp_n = 0;
    if (synth)
        host_preset(t, 6, 0);                       /* TRIO, as t4_follow */
}

static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(&trk[0], 0, 0);
    host_preset(&trk[1], 1, 0);
    host_preset(&trk[2], 3, 0);
}

int main(void)
{
    uint32_t i, k, r = 12345;

    fresh();
    check("default: track 4 is the drum track (T4 DRUM)", is_drum(TDRUM) && !trk_synth(TRK_DRUM));
    midi(9, 38, 100);
    run(4);
    check("default: MIDI ch 10 plays the GM kit", drums_on() > 0 && active(TDRUM) == 0);
    run(400);

    fresh();
    t4_set(1);
    check("T4 SYNTH: track 4 plays an engine", !is_drum(TDRUM) && trk_synth(TRK_DRUM) && TDRUM->engine == 6);
    midi(3, 60, 100);
    run(8);
    check("T4 SYNTH: MIDI ch 4 -> track 4, it sounds", active(TDRUM) == 1 && peak() > 100);
    midi(3, 60, 0);
    run(600);
    check("T4 SYNTH: the note ends", active(TDRUM) == 0);
    midi(9, 38, 100);
    run(4);
    check("T4 SYNTH: ch 10 no longer plays drums (the selected track)", drums_on() == 0);
    midi(9, 38, 0);
    run(600);

    for (k = 0; k < NTRK; k++)                       /* 3 notes on each of the 4 parts */
        for (i = 0; i < 3u; i++)
            midi(k, 48u + 12u * k + 4u * i, 90);
    run(8);
    {
        uint32_t busy = 0;
        for (k = 0; k < NTRK; k++)
            busy += active(&trk[k]);
        check("budget: 12 notes on 4 parts, at most NVOICE (8) sound", voices_busy() <= NVOICE && busy <= NVOICE + 2u);
    }
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < 3u; i++)
            midi(k, 48u + 12u * k + 4u * i, 0);
    run(900);
    check("budget: every voice free after the release", gates() == 0 && voices_busy() == 0);

    {   /* the sequencer on track 4 */
        static const uint8_t n1[1] = {64};
        put_step(TDRUM, 0, 1, n1, ST_NOTE, 0);
        TDRUM->p[P_SLEN] = 1;
        TDRUM->seq_idx = 0;
        TDRUM->seq_pos = 0x7FFFFFFFu;
        song.playing = 1;
        run(4);
        check("sequencer on track 4 (SYNTH) triggers its engine", active(TDRUM) >= 1);
        song.playing = 0;
        seq_stop();
        run(600);
        TDRUM->step[0].time = ST_REST;
        TDRUM->step[0].n = 0;
    }
    {   /* the arp on track 4 */
        TDRUM->p[P_AMODE] = 1;
        midi(3, 60, 100);
        midi(3, 67, 100);
        run(40);
        check("arp on track 4 (SYNTH)", TDRUM->nheld == 2 && active(TDRUM) >= 1);
        midi(3, 60, 0);
        midi(3, 67, 0);
        TDRUM->p[P_AMODE] = 0;
        run(600);
    }

    midi(3, 50, 100);                                /* back to DRUM with a note held */
    midi(3, 57, 100);
    run(8);
    t4_set(0);
    run(4);
    check("back to DRUM while notes sound: none left on track 4", active(TDRUM) == 0 && voices_busy() == 0);
    midi(9, 36, 100);
    run(4);
    check("... and the drums play again on ch 10", drums_on() > 0);
    midi(3, 50, 0);                                  /* the note-offs of before: nothing hangs */
    midi(3, 57, 0);
    run(600);
    t4_set(1);
    run(4);
    check("SYNTH again: no stale voice comes back", active(TDRUM) == 0);

    {   /* a torture: random notes on every channel, track changes, T4 flips; then all off */
        uint8_t down[16][128];
        uint32_t ev;
        memset(down, 0, sizeof down);
        for (ev = 0; ev < 20000u; ev++) {
            uint32_t ch, note;
            r = r * 1103515245u + 12345u;
            ch = (r >> 16) % 16u;
            note = 36u + (r >> 8) % 60u;
            if ((r >> 4) % 97u == 0u)
                song.sel = (uint8_t)((r >> 12) % NTRK);
            if ((r >> 5) % 1999u == 0u) {            /* T4 flips now and then (the UI's way) */
                t4_set(!song.g[G_T4]);
                memset(down, 0, sizeof down);       /* (t4_follow drops track 4's notes too) */
            }
            if (down[ch][note]) {
                midi(ch, note, 0);
                down[ch][note] = 0;
            } else {
                midi(ch, note, 40u + (r >> 20) % 80u);
                down[ch][note] = 1;
            }
            if (ev % 7u == 0u)
                run(1);
        }
        for (k = 0; k < 16u; k++)
            for (i = 0; i < 128u; i++)
                if (down[k][i])
                    midi(k, i, 0);
        for (k = 0; k < NTRK; k++)
            trk[k].p[P_AHOLD] = 0;
        run(1400);
        check("torture: 20000 random events, T4 flips: no gate left", gates() == 0);
        run(2000);
        check("torture: ... every voice free", voices_busy() == 0);
    }
    puts(fails ? "TRACK 4 TEST FAILED" : "track 4: all ok");
    return fails != 0;
}
