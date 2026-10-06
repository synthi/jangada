/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada Studio: engine.c on this computer, for web/test_studio.mjs. Reads a script from stdin, one
 * call a line ("st_preset 0 3", "render 100" = 100 blocks), and writes the rendered blocks to stdout
 * as little-endian int32 (L R L R ..): the test runs the same script on the .wasm and compares.
 * Built by tools/build_studio.py --native. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../web/studio/engine.c"

typedef void (*fn0_t)(void);
typedef void (*fn1_t)(uint32_t);
typedef void (*fn2_t)(uint32_t, uint32_t);
typedef void (*fn3_t)(uint32_t, uint32_t, int32_t);
static const struct { const char *name; int argc; void *fn; } CALLS[] = {
    {"st_init", 0, (void *)st_init},         {"st_select", 1, (void *)st_select},
    {"st_engine", 2, (void *)st_engine},     {"st_preset", 2, (void *)st_preset},
    {"st_param", 3, (void *)st_param},       {"st_global", 2, (void *)st_global},
    {"st_master", 1, (void *)st_master},     {"st_keys", 1, (void *)st_keys},
    {"st_octave", 1, (void *)st_octave},     {"st_fx_layer", 1, (void *)st_fx_layer},
    {"st_midi", 3, (void *)st_midi},         {"st_play", 1, (void *)st_play},
    {"st_panic", 0, (void *)st_panic},       {"st_drone_off", 0, (void *)st_drone_off},
};

int main(void)
{
    char line[256], name[64];
    long a[3];
    while (fgets(line, sizeof line, stdin)) {
        int n = sscanf(line, "%63s %ld %ld %ld", name, &a[0], &a[1], &a[2]) - 1;
        unsigned i;
        if (n < 0)
            continue;
        if (!strcmp(name, "render")) {
            long b;
            for (b = 0; b < a[0]; b++)
                fwrite(st_render(), sizeof(int32_t), 2u * CTL, stdout);
            continue;
        }
        for (i = 0; i < sizeof CALLS / sizeof CALLS[0] && strcmp(CALLS[i].name, name); i++)
            ;
        if (i == sizeof CALLS / sizeof CALLS[0] || n != CALLS[i].argc) {
            fprintf(stderr, "engine_native: bad line: %s", line);
            return 1;
        }
        switch (n) {
        case 0: ((fn0_t)CALLS[i].fn)(); break;
        case 1: ((fn1_t)CALLS[i].fn)((uint32_t)a[0]); break;
        case 2: ((fn2_t)CALLS[i].fn)((uint32_t)a[0], (uint32_t)a[1]); break;
        default: ((fn3_t)CALLS[i].fn)((uint32_t)a[0], (uint32_t)a[1], (int32_t)a[2]); break;
        }
    }
    return 0;
}
