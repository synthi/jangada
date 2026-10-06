/* SPDX-License-Identifier: GPL-3.0-only */
/* From SLOOP (tests/drum_level.c), for Jangada's G_KIT: every synthesised kit (or the ones named) x every
 * sound: one hit at velocity 110, 2 s each, mono int32 into argv[1] (kit-major, the 16 lanes in
 * gen_drumkits.py order). tools/level_drumkits.py measures them and writes tools/drumkit_levels.json.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src tests/drum_level.c -lm -o build/host/drum_level
 *   build/host/drum_level build/host/hits.raw [KIT ...] */
#define main hostsim_main
#include "hostsim.c"
#undef main
static const uint8_t LANE_GM[16] = {36, 38, 39, 42, 46, 43, 48, 49, 51, 70, 63, 37, 56, 75, 35, 40};

static void render_kit(FILE *f, uint32_t kit)
{
    uint32_t ln, b;
    int i;
    for (ln = 0; ln < 16u; ln++) {
        memset(&drums, 0, sizeof drums);
        drums.set = -2;
        song.g[G_KIT] = (int16_t)kit;
        drum_on(LANE_GM[ln], 110);
        for (b = 0; b < FS * 2u / CTL; b++) {
            int32_t l[CTL] = {0}, r[CTL] = {0}, rv[CTL] = {0};
            drums_render(l, r, rv, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t m = (l[i] + r[i]) / 2;
                fwrite(&m, 4, 1, f);
            }
        }
    }
}

int main(int argc, char **argv)
{
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0;
    uint32_t kit, n = 0;
    int a;
    if (!f)
        return 1;
    host_tracks_init();
    song.g[G_DRLVL] = 100;
    for (kit = 1; kit < DRUM_KITS; kit++) {
        int want = argc <= 2;
        for (a = 2; a < argc; a++)
            if (!strcmp(argv[a], DRUM_KIT_NAMES[kit]))
                want = 1;
        if (want) {
            render_kit(f, kit);
            n++;
        }
    }
    fclose(f);
    printf("%u kits x 16 sounds -> %s\n", n, argv[1]);
    return 0;
}
