/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: the synthesised drum kits (drum_synth.c, after SLOOP) on the drum track: every kit plays
 * every GM note, stays bounded, ends (its voice comes free), is about as loud as the GM samples, and
 * costs what it should. Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "instr.h"
#ifndef VERBOSE_KITS
#define VERBOSE_KITS 0
#endif

static int32_t out[2 * CTL];

static void setup(uint32_t kit)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    song.g[G_BPM] = 120;
    song.g[G_KIT] = (int16_t)kit;
    song.master_q12 = 4096;
}

/* one hit of note: its peak and energy over 2 s, and whether every voice came free */
static void hit(uint32_t note, int32_t *peak, double *energy, int *freed)
{
    uint32_t b, i, k;
    int32_t pk = 0;
    double e = 0;
    drum_on(note, 110);
    for (b = 0; b < 2u * FS / CTL; b++) {
        mix_block(out, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = out[i] < 0 ? -out[i] : out[i];
            if (a > pk)
                pk = a;
            e += (double)out[i] * out[i];
        }
    }
    for (k = 0, *freed = 1; k < NDRUM; k++)
        if (drums.v[k].active)
            *freed = 0;
    *peak = pk;
    *energy = e;
}

int main(void)
{
    static const uint8_t NOTES[] = {35, 36, 37, 38, 39, 40, 41, 42, 44, 45, 46, 47, 48, 49, 50, 51, 53,
                                    54, 56, 57, 62, 63, 64, 69, 70, 75, 76, 81};
    uint32_t kit, n, fails = 0;
    double gm_kick = 0, gm_snare = 0;
    for (kit = 0; kit < DRUM_KITS; kit++) {
        int32_t worst = 0;
        for (n = 0; n < sizeof NOTES; n++) {
            int32_t pk;
            double e;
            int freed;
            setup(kit);
            hit(NOTES[n], &pk, &e, &freed);
            if (pk > worst)
                worst = pk;
            if (pk >= 32767) {
                printf("kit %s note %u clips (%d)\n", DRUM_KIT_NAMES[kit], NOTES[n], pk);
                fails++;
            }
            if (!freed) {
                printf("kit %s note %u: a voice still sounds after 2 s\n", DRUM_KIT_NAMES[kit], NOTES[n]);
                fails++;
            }
            if (kit == 0u && NOTES[n] == 36u)
                gm_kick = e;
            if (kit == 0u && NOTES[n] == 38u)
                gm_snare = e;
            if (kit && (NOTES[n] == 36u || NOTES[n] == 38u)) {   /* kick and snare near the GM kit's */
                double ref = NOTES[n] == 36u ? gm_kick : gm_snare, db = 10.0 * log10(e / ref);
                if (db < -12.0 || db > 12.0) {
                    printf("kit %s note %u: %+.1f dB from the GM kit\n", DRUM_KIT_NAMES[kit], NOTES[n], db);
                    fails++;
                }
            }
        }
        if (VERBOSE_KITS)
            printf("kit %-8s peak %5d\n", DRUM_KIT_NAMES[kit], worst);
    }
    printf("%-46s %s\n", "kits: every kit, every GM note: bounded, ends", fails ? "FAIL" : "ok");
    {   /* cost: six synthesised voices at once against six GM samples */
        uint64_t i0, i1, i2;
        uint32_t b;
        setup(0);
        for (n = 0; n < NDRUM; n++)
            drum_on(36u + n * 2u, 110);
        i0 = instr_now();
        for (b = 0; b < 100u; b++)
            mix_block(out, CTL);
        i1 = instr_now();
        setup(1);
        for (n = 0; n < NDRUM; n++)
            drum_on(36u + n * 2u, 110);
        for (b = 0; b < 100u; b++)
            mix_block(out, CTL);
        i2 = instr_now();
        if (i0)
            printf("%-46s %.0f / sample (GM %.0f)\n", "cost: 6 synthesised drum voices (808)", (double)(i2 - i1) / (100.0 * CTL),
                   (double)(i1 - i0) / (100.0 * CTL));
    }
    if (fails)
        printf("KITS: %u FAILED\n", fails);
    return fails != 0;
}
