/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada (after SLOOP 2.3): the overload shedder (voice.c shed_voice) and a key let go after a change
 * of VOICE. Build with the same generated headers and flags as hostsim.c.
 *   shed   one voice a call, with a fade (stage 4); released voices first, the quietest; then the
 *          oldest held one that is not a POLY part's lowest note nor a MONO / LEGATO / UNISON lead
 *   VOICE  MONO, a key held, VOICE to POLY, the key let go, back to MONO: no note left in the MONO
 *          stack (0.2: the next key let go fell back to it, and it hung) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("%-66s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static int32_t buf[2 * CTL];
static void run(uint32_t blocks)
{
    while (blocks--)
        mix_block(buf, CTL);
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
static voice_t *voice_of(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].note == note && t->v[i].stage != 4u)
            return &t->v[i];
    return 0;
}
static uint32_t fading(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].stage == 4u;
    return n;
}
static uint32_t sounding(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}

int main(void)
{
    track_t *a = &trk[0], *b = &trk[1];
    uint32_t i, k0;

    fresh();
    a->p[P_VOICE] = V_POLY;
    a->p[P_SUS] = 127;
    for (i = 0; i < 4u; i++)                         /* a POLY chord on track 1: 48 52 55 59 */
        trk_note_on(a, 48u + (uint32_t[]){0, 4, 7, 11}[i], 100);
    b->p[P_VOICE] = V_MONO;
    trk_note_on(b, 72, 100);                         /* a MONO lead on track 2 */
    run(40);
    k0 = shed_count;
    shed_voice();
    check("shed: one voice a call, faded (stage 4)", shed_count == k0 + 1u && fading() == 1u);
    check("shed: the oldest held note above the bass goes (52)", !voice_of(a, 52) && voice_of(a, 48) && voice_of(b, 72));
    run(2);
    shed_voice();
    shed_voice();
    run(2);
    check("shed: then 55 and 59; the bass (48) and the MONO lead stay", !voice_of(a, 55) && !voice_of(a, 59) &&
          voice_of(a, 48) && voice_of(b, 72));
    k0 = shed_count;
    shed_voice();
    check("shed: only bass and lead left: nothing more is taken", shed_count == k0 && voice_of(a, 48) && voice_of(b, 72));
    trk_note_on(a, 64, 100);
    trk_note_on(a, 67, 30);
    run(20);
    trk_note_off(a, 64);
    trk_note_off(a, 67);
    run(4);
    k0 = shed_count;
    shed_voice();
    check("shed: a releasing voice before any held one", shed_count == k0 + 1u && voice_of(a, 48) && voice_of(b, 72) &&
          (!voice_of(a, 64) || !voice_of(a, 67)));

    fresh();                                         /* a key let go after a change of VOICE */
    a->p[P_VOICE] = V_MONO;
    trk_note_on(a, 60, 100);                         /* held in MONO */
    a->p[P_VOICE] = V_POLY;                          /* changed (the editor, no release on the way) */
    trk_note_off(a, 60);                             /* let go in POLY */
    check("VOICE: the key let go leaves the MONO stack", a->nmono == 0);
    a->p[P_VOICE] = V_MONO;
    trk_note_on(a, 64, 100);
    trk_note_off(a, 64);
    run(800);
    check("VOICE: back in MONO, the next note ends: nothing hangs", sounding(a) == 0 && a->mono_note == 0);

    puts(fails ? "VOICE TEST FAILED" : "voices: all ok");
    return fails != 0;
}
