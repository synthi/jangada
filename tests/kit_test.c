/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: the GM kit plays the right zone for every note (hugelton/Felucca#25). */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

int main(void)
{
    static const uint8_t one[] = {42, 44, 46, 49, 57};   /* hats and crashes: their own one-note zones */
    uint32_t s, i, n = 0;
    for (s = 0; s < SMP_NSETS; s++) {
        const smp_set_t *set = &SMP_SETS[s];
        for (i = 0; i < sizeof one; i++) {
            uint32_t zi = smp_zone_pick(set, one[i]), k, has = 0;
            for (k = 0; k < set->nz; k++)
                has |= SMP_ZONES[set->z0 + k].lo == one[i] && SMP_ZONES[set->z0 + k].hi == one[i];
            if (!has)
                continue;                                 /* this set has no zone of its own for it */
            assert(zi != 0xFFFFu && SMP_ZONES[zi].lo == one[i] && SMP_ZONES[zi].hi == one[i]);
            n++;
        }
    }
    assert(n >= 4u);
    printf("kit: %u hat / crash notes play their own zone, not the toms       ok\n", n);
    return 0;
}
