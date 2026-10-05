/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: CHORD mode (seq.c chord_notes, keyboard_block) on the host: the chords of the scale, the
 * white keys walking it, one key down = the whole chord, up = all of it released.
 * Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int same(const uint8_t *c, uint32_t n, const int *want)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (c[i] != want[i])
            return 0;
    return want[n] < 0;
}

static uint32_t gates(const track_t *t)
{
    uint32_t i, g = 0;
    for (i = 0; i < NVOICE; i++)
        g += t->v[i].active && t->v[i].gate;
    return g;
}

int main(void)
{
    static const struct { int scale, root, chord, note; int want[5]; const char *what; } C[] = {
        {1, 0, 1, 60, {60, 64, 67, -1}, "C MAJ TRIAD on C: C E G"},
        {1, 0, 1, 62, {62, 65, 69, -1}, "C MAJ TRIAD on D: D F A (minor)"},
        {1, 0, 2, 67, {67, 71, 74, 77, -1}, "C MAJ 7TH on G: G B D F"},
        {1, 0, 3, 60, {60, 64, 71, 74, -1}, "C MAJ 9TH on C: C E B D"},
        {1, 0, 4, 60, {60, 65, 67, -1}, "C MAJ SUS4 on C: C F G"},
        {1, 0, 5, 60, {60, 67, 72, -1}, "POWER on C: C G C"},
        {0, 9, 1, 69, {69, 72, 76, -1}, "CHR (minor) root A: A C E"},
        {2, 2, 1, 62, {62, 65, 69, -1}, "D MIN TRIAD on D: D F A"},
    };
    uint32_t i, fails = 0;
    uint8_t c[4];
    track_t *t = &trk[0];
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    song.g[G_BPM] = 120;
    for (i = 0; i < sizeof C / sizeof C[0]; i++) {
        uint32_t n;
        t->p[P_SCALE] = (int16_t)C[i].scale;
        t->p[P_ROOT] = (int16_t)C[i].root;
        t->p[P_CHORD] = (int16_t)C[i].chord;
        n = chord_notes(t, (uint32_t)C[i].note, c);
        if (!same(c, n, C[i].want)) {
            uint32_t j;
            printf("%s: got", C[i].what);
            for (j = 0; j < n; j++)
                printf(" %u", c[j]);
            printf("\n");
            fails++;
        } else {
            printf("%-46s ok\n", C[i].what);
        }
    }

    /* the keys: CHORD walks the white keys from C4 (= the root), black keys are silent */
    t->p[P_SCALE] = 1;
    t->p[P_ROOT] = 0;
    t->p[P_CHORD] = 1;
    t->p[P_QUANT] = 0;
    assert(kb_map(t, 7) == 60u);                        /* C4 key -> C */
    assert(kb_map(t, 9) == 62u);                        /* D4 key -> D */
    assert(kb_map(t, 8) == KB_SILENT);                  /* C#4: silent */
    printf("%-46s ok\n", "CHORD: white keys walk the scale from C4");

    {   /* one key = three voices; its key-up releases all three */
        uint32_t b;
        static int32_t out[2 * CTL];
        t->p[P_VOICE] = V_POLY;
        fm1_in.notes = 1u << 9;                         /* D4: D F A */
        for (b = 0; b < 4u; b++)
            mix_block(out, CTL);
        if (gates(t) != 3u) {
            printf("CHORD key: %u voices held, want 3\n", gates(t));
            fails++;
        }
        fm1_in.notes = 0;
        for (b = 0; b < 4u; b++)
            mix_block(out, CTL);
        if (gates(t) != 0u) {
            printf("CHORD key up: %u voices still held\n", gates(t));
            fails++;
        }
        t->p[P_CHORD] = 0;                              /* OFF: one note again */
        fm1_in.notes = 1u << 9;
        for (b = 0; b < 4u; b++)
            mix_block(out, CTL);
        if (gates(t) != 1u) {
            printf("CHORD OFF: %u voices held, want 1\n", gates(t));
            fails++;
        }
        fm1_in.notes = 0;
        for (b = 0; b < 4u; b++)
            mix_block(out, CTL);
        if (!fails)
            printf("%-46s ok\n", "CHORD: key down = the chord, key up = all off");
    }
    if (fails)
        printf("CHORD: %u FAILED\n", fails);
    return fails != 0;
}
