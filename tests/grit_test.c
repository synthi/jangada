/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada GRIT on the host (grit.c): TAPE and HUM on the master, the DIST types of a track (FUZZ, FOLD,
 * CRUSH, RING). Each one changes the sound, stays bounded, lets go to exactly what it was (no offset,
 * no noise left), and does not click when it comes or goes; SOFT is the old DIST (the golden renders
 * hold it bit for bit). Prints what they cost on the host, in instructions a sample.
 *   grit_test [DIR]   DIR: also renders the demos (WAV) there: one short song dry and through each.
 * Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "instr.h"

#define SEC (FS / CTL)                      /* blocks in a second */
#define MAXB (3u * SEC)
static int32_t out[2 * CTL];
static int16_t ref[MAXB * CTL], cur[MAXB * CTL];   /* left channel of two runs, to compare */
static uint32_t fails;

static void ok(int c, const char *what)
{
    printf("%-58s %s\n", what, c ? "ok" : "FAIL");
    fails += !c;
}

/* a clean instrument and master (the master's own states: as at boot) */
static void setup(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    song.g[G_BPM] = 120;
    host_preset(&trk[0], 0, 0);              /* ANALOG SAW LEAD on track 1 */
    trk[0].p[P_REL] = 60;
    punch.req = -1;
    memset(&tape, 0, sizeof tape);
    tape.rnd = 0x1D872B41;
    memset(&hum, 0, sizeof hum);
    hum.rnd = 0x6A09E667;
    memset(&dust, 0, sizeof dust);
    dust.rnd = 0x2545F491;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    memset(&fx, 0, sizeof fx);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(&rev_u, 0, sizeof rev_u);
    memset(&duck, 0, sizeof duck);
    duck.t = 0xFFFFFFFFu, duck.g0 = duck.g1 = 32767;
}

/* blocks of the mix into buf (left, 16 bits) from block b0; the largest sample step and the peak */
static void run(uint32_t blocks, int16_t *buf, uint32_t b0, int32_t *step, int32_t *peak)
{
    static int32_t last;
    uint32_t b, i;
    int32_t s = 0, p = 0;
    for (b = 0; b < blocks; b++) {
        mix_block(out, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t x = out[2 * i], d = x - last, a = x < 0 ? -x : x;
            d = d < 0 ? -d : d;
            s = d > s ? d : s;
            p = a > p ? a : p;
            last = x;
            if (buf && (b0 + b) * CTL + i < MAXB * CTL)
                buf[(b0 + b) * CTL + i] = (int16_t)x;
        }
    }
    if (step)
        *step = s;
    if (peak)
        *peak = p;
}

static void chord(void)
{
    input_on(&trk[0], 52, 100);
    input_on(&trk[0], 59, 100);
    input_on(&trk[0], 64, 100);
    seq_start();
}

static double energy(const int16_t *a, uint32_t from, uint32_t to)   /* in samples */
{
    double e = 0;
    uint32_t i;
    for (i = from; i < to; i++)
        e += (double)a[i] * a[i];
    return e / (double)(to - from);
}

static double diff_energy(uint32_t from, uint32_t to)
{
    double e = 0;
    uint32_t i;
    for (i = from; i < to; i++)
        e += (double)(cur[i] - ref[i]) * (cur[i] - ref[i]);
    return e / (double)(to - from);
}

static int32_t max_diff(uint32_t from, uint32_t to)
{
    int32_t m = 0;
    uint32_t i;
    for (i = from; i < to; i++) {
        int32_t d = cur[i] - ref[i];
        d = d < 0 ? -d : d;
        m = d > m ? d : m;
    }
    return m;
}

/* amplitude of frequency hz in a[from, to) (Goertzel) */
static double tone(const int16_t *a, uint32_t from, uint32_t to, double hz)
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = from; i < to; i++) {
        double s0 = a[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) * 2 / (double)(to - from);
}

/* the master effect id at value v for 1 s of a held chord (from 0.5 s), then back to 0: against the
 * same run dry. It changes the mix, stays bounded, and once back the output is the dry one again */
static void master_case(uint32_t id, int16_t v, const char *name)
{
    int32_t step_d, step_w, pk;
    double ed, ew, dd;
    char what[96];
    setup();
    chord();
    run(SEC / 2u, ref, 0, 0, 0);
    run(SEC, ref, SEC / 2u, &step_d, 0);
    run(SEC + SEC / 2u, ref, SEC + SEC / 2u, 0, 0);
    setup();
    chord();
    run(SEC / 2u, cur, 0, 0, 0);
    song.g[id] = v;
    run(SEC, cur, SEC / 2u, &step_w, &pk);
    song.g[id] = 0;
    run(SEC + SEC / 2u, cur, SEC + SEC / 2u, 0, 0);
    seq_stop();
    ed = energy(ref, SEC / 2u * CTL, (SEC + SEC / 2u) * CTL);
    ew = energy(cur, SEC / 2u * CTL, (SEC + SEC / 2u) * CTL);
    dd = diff_energy(SEC / 2u * CTL, (SEC + SEC / 2u) * CTL);
    snprintf(what, sizeof what, "%s: changes the mix (%.0f %% apart), bounded (%.2f x)", name, 100 * sqrt(dd / ed), ew / ed);
    ok(dd > ed * 0.01 && ew < ed * 4 && ew > ed * 0.1 && pk < 32767, what);
    snprintf(what, sizeof what, "%s: back to 0 = the dry mix (largest difference %d after 1 s)", name,
             max_diff((5u * SEC / 2u) * CTL, MAXB * CTL));
    ok(max_diff((5u * SEC / 2u) * CTL, MAXB * CTL) <= 2, what);
    snprintf(what, sizeof what, "%s: no click on and off (step %d vs dry %d)", name, step_w, step_d);
    ok(step_w < step_d * 2, what);
}

/* track 1 through DIST type ty at drive d: a chord, released at 1 s */
static void dist_run_case(uint32_t ty, int16_t d, int16_t *buf, int32_t *peak)
{
    setup();
    trk[0].p[P_DIST] = d;
    trk[0].p[P_DTYPE] = (int16_t)ty;
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0;   /* (the tails: silence sooner) */
    input_on(&trk[0], 45, 100);
    input_on(&trk[0], 52, 100);
    input_on(&trk[0], 57, 100);
    run(SEC, buf, 0, 0, peak);
    input_off(&trk[0], 45);
    input_off(&trk[0], 52);
    input_off(&trk[0], 57);
    run(2u * SEC, buf, SEC, 0, 0);
}

static void cost(void)
{
    uint64_t i0, i1, i2, i3, i4, i5, i6;
    setup();
    chord();
    i0 = instr_now();
    run(SEC, 0, 0, 0, 0);
    i1 = instr_now();
    song.g[G_TAPE] = 127;
    run(SEC / 8u, 0, 0, 0, 0);                           /* (the glide) */
    i2 = instr_now();
    run(SEC, 0, 0, 0, 0);
    i3 = instr_now();
    song.g[G_TAPE] = 0;
    song.g[G_HUM] = 127;
    run(SEC / 8u, 0, 0, 0, 0);
    i4 = instr_now();
    run(SEC, 0, 0, 0, 0);
    i5 = instr_now();
    song.g[G_TAPE] = 90;
    song.g[G_DUST] = 90;
    song.g[G_FILT] = -40;
    punch.req = PX_WOBBLE;
    run(SEC / 8u, 0, 0, 0, 0);
    i6 = instr_now();
    run(SEC, 0, 0, 0, 0);
    seq_stop();
    if (!i0) {
        printf("cost: no instruction counter on this host (skipped)\n");
        return;
    }
    {
        uint64_t base = i1 - i0, i7 = instr_now();
        printf("%-58s %.0f / sample (mix %.0f)\n", "cost: TAPE on the host", (double)(i3 - i2 - base) / FS, (double)base / FS);
        printf("%-58s %.0f / sample\n", "cost: HUM on the host", (double)(i5 - i4 - base) / FS);
        printf("%-58s %.0f / sample\n", "cost: DUST + TAPE + HUM + FILT + WOBBLE on the host", (double)(i7 - i6 - base) / FS);
    }
    {   /* a track through each DIST type vs SOFT, held chord */
        static const char *const N[5] = {"SOFT", "FUZZ", "FOLD", "CRUSH", "RING"};
        uint64_t c[5], a, b;
        uint32_t ty;
        for (ty = 0; ty < 5u; ty++) {
            setup();
            trk[0].p[P_DIST] = 90;
            trk[0].p[P_DTYPE] = (int16_t)ty;
            chord();
            run(SEC / 8u, 0, 0, 0, 0);
            a = instr_now();
            run(SEC, 0, 0, 0, 0);
            b = instr_now();
            c[ty] = b - a;
            seq_stop();
        }
        printf("cost: a track's DIST against SOFT on the host:");
        for (ty = 1; ty < 5u; ty++)
            printf(" %s %+.0f", N[ty], ((double)c[ty] - (double)c[0]) / FS);
        printf(" / sample\n");
    }
}

/* ------------------------------------------------------------- demos --- */
/* ~8 s at 100 BPM: RUST BASS (track 1), HURT PAD (track 2), the GM kit; set() changes the sound after
 * the presets. Runs in a child process: every demo from a cold start. */
static uint32_t demo_secs = 8u, demo_solo;             /* demo_solo: track 1 alone */
static void demo(const char *dir, const char *name, void (*set)(void))
{
    static const uint8_t BASS[16] = {40, 0, 40, 52, 0, 40, 43, 0, 40, 0, 47, 40, 0, 46, 41, 0};
    static const uint8_t BASSF[16] = {1, 0, 0, 2, 0, 1, 0, 0, 1, 0, 2, 0, 0, 0, 1, 0};
    static const uint8_t EM[4] = {52, 55, 59, 62}, CM[4] = {48, 52, 55, 59};
    char path[512];
    pid_t pid;
    fflush(stdout);
    if ((pid = fork())) {
        waitpid(pid, 0, 0);
        return;
    }
    {
        FILE *w;
        uint32_t i, f, frames = 8u * FS;
        track_t *b = &trk[0], *p = &trk[1], *d = TDRUM;
        int32_t uk = 0;
        uint32_t ai;
        setup();
        song.g[G_BPM] = 100;
        for (ai = 0; ai < ENGINES[0]->npresets; ai++)
            if (!strcmp(ENGINES[0]->presets[ai].name, "RUST BASS"))
                host_preset(b, 0, ai);
            else if (!strcmp(ENGINES[0]->presets[ai].name, "HURT PAD"))
                host_preset(p, 0, ai);
        for (i = 0; i < 16u; i++) {
            uint8_t n = BASS[i];
            put_step(b, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, BASSF[i]);
        }
        b->p[P_SLEN] = 16;
        p->p[P_SLEN] = 32;
        p->p[P_SGATE] = 120;
        p->p[P_AMODE] = 0;
        for (i = 0; i < 32u; i++)
            put_step(p, i, i % 16u == 0u ? 4u : 0u, i < 16u ? EM : CM, i % 16u == 0u ? ST_NOTE : i % 16u < 15u ? ST_TIE : ST_REST, 0);
        for (i = 0; i < 16u; i++) {                 /* kick 1, 7, 11; snare 5, 13; hats on the 8ths */
            uint8_t n[4];
            uint32_t k = 0;
            if (i == 0u || i == 6u || i == 10u)
                n[k++] = 36;
            if (i == 4u || i == 12u)
                n[k++] = 38;
            if (i % 2u == 0u)
                n[k++] = i == 14u ? 46 : 42;
            put_step(d, i, k, n, k ? ST_NOTE : ST_REST, i % 4u == 0u ? SF_ACCENT : 0u);
        }
        if (set)
            set();
        if (demo_solo) {
            p->p[P_LEVEL] = 0;
            song.g[G_DRLVL] = 0;
        }
        frames = demo_secs * FS;
        snprintf(path, sizeof path, "%s/%s.wav", dir, name);
        if (!(w = fopen(path, "wb")))
            _exit(1);
        wav_hdr(w, frames);
        transport_req = 1;
        for (f = 0; f < frames; f += CTL) {
            mix_block(out, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t a = out[2 * i] < 0 ? -out[2 * i] : out[2 * i];
                uk = a > uk ? a : uk;
                wav_put(w, out[2 * i], out[2 * i + 1]);
            }
        }
        fclose(w);
        printf("demo: %-34s peak %5.1f dBFS\n", path, 20 * log10((uk + 1) / 32768.0));
        fflush(stdout);
        _exit(0);
    }
}

static void d_tape(void) { song.g[G_TAPE] = 127; }
static void d_tape_half(void) { song.g[G_TAPE] = 64; }
static void d_hum(void) { song.g[G_HUM] = 127; }
static void d_hum_tape(void) { song.g[G_HUM] = 100; song.g[G_TAPE] = 100; song.g[G_DUST] = 30; }
static void d_type(uint32_t ty, int16_t d) { trk[0].p[P_DTYPE] = (int16_t)ty; trk[0].p[P_DIST] = d; }
static void d_fuzz(void) { d_type(DT_FUZZ, 90); }
static void d_fold(void) { d_type(DT_FOLD, 90); }
static void d_crush(void) { d_type(DT_CRUSH, 100); }
static void d_ring(void) { d_type(DT_RING, 100); trk[0].p[P_DRING] = 40; }
static void d_ring_pad(void) { trk[1].p[P_DTYPE] = DT_RING; trk[1].p[P_DIST] = 110; trk[1].p[P_DRING] = 62; }
static void d_all(void)
{
    d_type(DT_FUZZ, 80);
    trk[1].p[P_DTYPE] = DT_FOLD;
    trk[1].p[P_DIST] = 18;
    song.g[G_TAPE] = 110;
    song.g[G_HUM] = 80;
}

int main(int argc, char **argv)
{
    uint32_t ty;
    int32_t pk, st;

    master_case(G_TAPE, 127, "TAPE 100 %");
    master_case(G_TAPE, 30, "TAPE 24 %");
    {   /* HUM alone: a stopped Jangada is silent; playing, 60 Hz and its odd harmonics; STOP: gone */
        double h60, h120, h180, h1k, e_stop;
        int32_t p_stop;
        setup();
        song.g[G_HUM] = 127;
        run(SEC / 2u, cur, 0, 0, &p_stop);
        ok(p_stop == 0, "HUM: nothing while stopped");
        seq_start();
        run(2u * SEC, cur, 0, &st, &pk);
        h60 = tone(cur, SEC * CTL / 2u, 2u * SEC * CTL, 60);
        h120 = tone(cur, SEC * CTL / 2u, 2u * SEC * CTL, 120);
        h180 = tone(cur, SEC * CTL / 2u, 2u * SEC * CTL, 180);
        h1k = tone(cur, SEC * CTL / 2u, 2u * SEC * CTL, 1010);
        {
            char what[96];
            snprintf(what, sizeof what, "HUM: 60 Hz %.0f, 180 Hz %.0f, 120 Hz %.0f, peak %d", h60, h180, h120, pk);
            ok(h60 > 400 && h180 > h60 * 0.15 && h120 < h60 * 0.05 && h1k < h60 * 0.1 && pk < 4000, what);
            snprintf(what, sizeof what, "HUM: fades in, no click (step %d)", st);
            ok(st < 2000, what);
        }
        seq_stop();
        run(SEC / 4u, cur, 0, &st, 0);
        run(SEC, cur, 0, 0, &p_stop);
        e_stop = energy(cur, 0, SEC * CTL);
        ok(p_stop == 0 && e_stop == 0, "HUM: STOP -> silence, no offset");
        master_case(G_HUM, 127, "HUM 100 %");
    }
    {   /* DIST: every type changes SOFT's sound, bounded, and dies away to silence */
        static const char *const N[5] = {"SOFT", "FUZZ", "FOLD", "CRUSH", "RING"};
        static int16_t soft[MAXB * CTL];
        int32_t pks;
        dist_run_case(DT_SOFT, 90, soft, &pks);
        for (ty = DT_FUZZ; ty <= DT_RING; ty++) {
            char what[96];
            double es, et, dd = 0;
            uint32_t i, from = SEC / 4u * CTL, to = SEC * CTL;
            int32_t tail = 0;
            dist_run_case(ty, 90, cur, &pk);
            es = energy(soft, from, to);
            et = energy(cur, from, to);
            for (i = from; i < to; i++)
                dd += (double)(cur[i] - soft[i]) * (cur[i] - soft[i]);
            dd /= to - from;
            for (i = 2u * SEC * CTL; i < 3u * SEC * CTL; i++)
                tail = cur[i] < 0 ? (-cur[i] > tail ? -cur[i] : tail) : (cur[i] > tail ? cur[i] : tail);
            snprintf(what, sizeof what, "DIST %-5s: not SOFT (%.0f %% apart), level %.2f x SOFT's, peak %d", N[ty],
                     100 * sqrt(dd / es), sqrt(et / es), pk);
            ok(dd > es * 0.05 && et < es * 4 && et > es * 0.05 && pk < 32767, what);
            snprintf(what, sizeof what, "DIST %-5s: silence after the notes (largest %d)", N[ty], tail);
            ok(tail <= 1, what);
        }
    }
    {   /* DIST: the type and the drive switched at random while a chord plays: no click above the
         * largest step of the types played steadily */
        int32_t worst = 0, steady = 0;
        uint32_t k;
        for (ty = DT_SOFT; ty <= DT_RING; ty++) {
            setup();
            trk[0].p[P_DIST] = 90;
            trk[0].p[P_DTYPE] = (int16_t)ty;
            chord();
            run(SEC / 4u, 0, 0, 0, 0);
            run(SEC / 2u, 0, 0, &st, 0);
            steady = st > steady ? st : steady;
            seq_stop();
        }
        setup();
        chord();
        trk[0].p[P_DIST] = 90;
        run(SEC / 4u, 0, 0, 0, 0);
        srand(7);
        for (k = 0; k < 80u; k++) {
            trk[0].p[P_DTYPE] = (int16_t)(rand() % 5);
            trk[0].p[P_DIST] = (int16_t)(k % 7u == 3u ? 0 : 90);
            run(1u + (uint32_t)rand() % 100u, 0, 0, &st, 0);
            worst = st > worst ? st : worst;
        }
        seq_stop();
        {
            char what[96];
            snprintf(what, sizeof what, "DIST: type changes crossfade (step %d vs steady %d)", worst, steady);
            ok(worst < steady * 3 / 2, what);
        }
    }
    {   /* TAPE and HUM on and off at random: no click above the music's own */
        int32_t worst = 0, dry;
        uint32_t k;
        setup();
        chord();
        run(SEC / 4u, 0, 0, 0, &pk);
        run(SEC / 4u, 0, 0, &dry, 0);
        srand(11);
        for (k = 0; k < 64u; k++) {
            song.g[G_TAPE] = (int16_t)((k & 1u) ? 0 : rand() % 128);
            song.g[G_HUM] = (int16_t)((k & 2u) ? 0 : rand() % 128);
            run(1u + (uint32_t)rand() % 200u, 0, 0, &st, 0);
            worst = st > worst ? st : worst;
        }
        seq_stop();
        {
            char what[96];
            snprintf(what, sizeof what, "TAPE / HUM switched at random: no click (step %d vs dry %d)", worst, dry);
            ok(worst < dry * 2, what);
        }
    }
    cost();
    if (argc > 1) {
        demo(argv[1], "01-dry", 0);
        demo(argv[1], "02-tape-50", d_tape_half);
        demo(argv[1], "03-tape-100", d_tape);
        demo(argv[1], "04-hum-100", d_hum);
        demo(argv[1], "05-hum-tape-dust", d_hum_tape);
        demo(argv[1], "06-dist-fuzz", d_fuzz);
        demo(argv[1], "07-dist-fold", d_fold);
        demo(argv[1], "08-dist-crush", d_crush);
        demo(argv[1], "09-dist-ring-bass", d_ring);
        demo(argv[1], "10-dist-ring-pad", d_ring_pad);
        demo(argv[1], "11-all-grit", d_all);
        demo_secs = 5u;                             /* the bass alone through each DIST type */
        demo_solo = 1u;
        demo(argv[1], "12-bass-soft", 0);
        demo(argv[1], "13-bass-fuzz", d_fuzz);
        demo(argv[1], "14-bass-fold", d_fold);
        demo(argv[1], "15-bass-crush", d_crush);
        demo(argv[1], "16-bass-ring", d_ring);
    }
    if (fails)
        printf("GRIT: %u FAILED\n", fails);
    return fails != 0;
}
