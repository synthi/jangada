/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada GRIT: rust for the sound. Included by fx.c (the audio ISR).
 *   - the DIST types of a track (P_DTYPE): SOFT is fx.c dist_soft (the one there was: older projects
 *     sound the same), and here FUZZ, FOLD, CRUSH and RING (the carrier: P_DRING);
 *   - TAPE on the master (G_TAPE): a worn tape machine, saturation, wow, flutter, dull highs;
 *   - HUM on the master (G_HUM): what an analog recording carries under the music, mains hum (60 Hz:
 *     the Brazilian grid) with its odd harmonics, hiss, a rare crackle, while the song plays.
 * Each one is off and bit-identical at 0 (TAPE and HUM return at once, DIST takes the old path), and
 * none clicks when it comes or goes: TAPE glides its amount and fades its wet in, HUM ramps its bed,
 * a change of DIST type crossfades over a block. */

/* ------------------------------------------------------------- DIST --- */
enum { DT_SOFT, DT_FUZZ, DT_FOLD, DT_CRUSH, DT_RING };
#define RING_HZ_INC 97392u                              /* 2^32 / FS: phase increment of 1 Hz */

static inline int32_t grit_bits(int32_t v, int32_t shift) /* fewer bits, towards 0: no DC from the tails */
{
    return v >= 0 ? (v >> shift) << shift : -((-v >> shift) << shift);
}

/* FUZZ: a starved-transistor pedal. A dead zone first (the gate: small signals and the tails sputter
 * and stop instead of hissing on), then 8x .. 48x into a clip that is harder on top than below (the
 * asymmetry: even harmonics), a dark two-pole tone (~2.5 kHz) and a slow DC tracker (the asymmetry
 * leaves an offset that comes and goes with the notes).
 * FOLD: a wavefolder, the signal as the phase of a sine: past a quarter turn it folds back, more turns
 * with the drive (1x .. 8x), a little bias for the even ones; a one-pole tone (~4 kHz).
 * CRUSH: fewer samples (held 1 .. 16: 44.1 down to 2.8 kHz) and fewer bits (3 .. 12 dropped).
 * RING: the track times a sine carrier (P_DRING, 30 Hz .. 16 kHz), DIST the wet part.
 * FUZZ and FOLD keep the low cut of SOFT (the bass out of the shaper). */
static __attribute__((noinline)) void dist_grit(track_t *t, int32_t *b, uint32_t n, uint32_t type, int32_t d)
{
    int32_t i;
    if (type == DT_RING) {
        uint32_t ph = t->dist_ph, inc = (uint32_t)CUTOFF_HZ[t->p[P_DRING] & 127] * RING_HZ_INC;
        int32_t w = (d * 258) >> 3;                     /* wet, Q12 */
        for (i = 0; i < (int32_t)n; i++) {
            int32_t x = clamp(b[i], -230000, 230000), y = ((x >> 2) * sine_i(ph)) >> 13;
            ph += inc;
            y += y >> 1;                                /* (a product has half the power: made up) */
            b[i] = x + ((((y - x) >> 3) * w) >> 9);
        }
        t->dist_ph = ph;
    } else if (type == DT_CRUSH) {
        int32_t hold = 1 + d * 15 / 127, shift = 3 + d / 14;
        for (i = 0; i < (int32_t)n; i++) {
            if (--t->dist_x2 <= 0) {
                t->dist_x2 = hold;
                t->dist_x1 = grit_bits(b[i], shift);
            }
            b[i] = t->dist_x1;
        }
    } else if (type == DT_FUZZ) {
        int32_t th = d * 6, g = 2048 + d * 80;           /* the gate; the gain, Q8 */
        for (i = 0; i < (int32_t)n; i++) {
            int32_t x = b[i], y;
            t->dist_hp += (x - t->dist_hp + 64) >> 7;
            x -= t->dist_hp;
            x = x > th ? x - th : x < -th ? x + th : 0;
            x = (clamp(x, -40000, 40000) * g) >> 8;
            y = x >= 0 ? softclip(x) : (softclip(x >> 1) * 3) >> 2;
            t->dist_lp1 += mulq15(y - t->dist_lp1, 13000);
            t->dist_lp2 += mulq15(t->dist_lp1 - t->dist_lp2, 13000);
            t->dist_x1 += ((t->dist_lp2 << 8) - t->dist_x1) >> 9;   /* the offset (Q8), ~14 Hz */
            b[i] = mulq15(t->dist_lp2 - (t->dist_x1 >> 8), 16000);
        }
    } else {                                            /* FOLD */
        uint32_t gk = 32768u + (uint32_t)d * 1806u, bias = (uint32_t)d << 21;
        int32_t b0 = sine_i(bias), mk = 26000 - d * 80;
        for (i = 0; i < (int32_t)n; i++) {
            int32_t x = b[i];
            t->dist_hp += (x - t->dist_hp + 64) >> 7;
            x = clamp(x - t->dist_hp, -65536, 65535);
            t->dist_lp1 += mulq15(sine_i((uint32_t)x * gk + bias) - b0 - t->dist_lp1, 15000);
            b[i] = mulq15(t->dist_lp1, mk);
        }
    }
}

static void dist_soft(track_t *t, int32_t *b, uint32_t n, int32_t d);   /* fx.c */

static void dist_run(track_t *t, int32_t *b, uint32_t n, uint32_t mode, int32_t d)
{
    if (mode == 1u + DT_SOFT)
        dist_soft(t, b, n, d);
    else if (mode)
        dist_grit(t, b, n, mode - 1u, d);
}

/* the DIST mode changed (0 off, 1 + type) and a GRIT type comes or goes: the block through the old one
 * fades into the block through the new one; the new one starts from a clean state. (SOFT on and off
 * alone keep what they did: no fade, states kept.) */
static __attribute__((noinline)) void dist_xfade(track_t *t, int32_t *b, uint32_t n, uint32_t old, uint32_t mode,
                                                 int32_t d)
{
    static int32_t tmp[CTL];
    int32_t hp = t->dist_hp, i, step;
    if (n > CTL)
        n = CTL;
    for (i = 0; i < (int32_t)n; i++)
        tmp[i] = b[i];
    dist_run(t, tmp, n, old, d ? d : 1);
    t->dist_hp = hp;
    t->dist_lp1 = t->dist_lp2 = t->dist_x1 = t->dist_x2 = 0;
    dist_run(t, b, n, mode, d);
    step = 32768 / (int32_t)(n ? n : 1u);
    for (i = 0; i < (int32_t)n; i++) {
        int32_t g = i * step;                           /* 0 .. ~1, Q15 */
        b[i] = tmp[i] + ((((b[i] - tmp[i]) >> 3) * (g >> 3)) >> 9);
    }
}

/* ------------------------------------------------------------- TAPE --- */
/* A worn tape machine on the whole mix. G_TAPE 0..127 turns everything up together:
 *   saturation   drive 1x .. 2.5x (the square of the knob) into tanh with a bias (asymmetric: the even harmonics of tape), the
 *                level made up again (loud parts get denser, quiet ones stay where they were);
 *   wow          the speed wandering slowly: two sines, 0.63 and 0.97 Hz, beating (never quite
 *                periodic), up to ~0.5 % of pitch at the top (growing as the square of the knob);
 *   flutter      the fast scrape: 7.8 Hz plus a little filtered noise, ~0.1 %;
 *   dull highs   a one-pole low-pass from open down to ~5.5 kHz.
 * Wow and flutter are a short delay line read at a moving point (TAPE_N samples a side), the delay
 * computed per block and interpolated per sample. The knob glides (~0.1 s end to end); the wet part
 * fades in over its first eighth and the delay grows from 0 with it: no click, no comb. At 0, after the
 * glide, it is bypassed: the mix is bit-identical. ~40 operations a sample (stereo). */
#define TAPE_N 128u                                     /* power of two; the delay stays below 108 samples */
#define BLK_HZ(x100) ((uint32_t)(((uint64_t)(x100) << 32) * CTL / FS / 100u))   /* phase step a block */
static int16_t tape_buf[2][TAPE_N];
static struct {
    int32_t s;                                          /* the knob, glided: 0..127 << 8 */
    uint32_t w;                                         /* write index */
    uint32_t w1, w2, fp;                                /* wow (two sines), flutter phases */
    int32_t fn, rnd;                                    /* flutter noise (filtered), its generator */
    int32_t dly, wet;                                   /* delay (Q8 samples), wet gain (Q15) at the block end */
    int32_t ll, lr;                                     /* low-pass states */
} tape = {0, 0, 0, 0x40000000u, 0, 0, 0x1D872B41, 0, 0, 0, 0};

static int32_t tape_dry[2][CTL];                        /* the dry block, while the wet part fades */
static __attribute__((noinline)) void tape_process(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t to = song.g[G_TAPE] << 8, amt, drive, bias, b0, mk, k, dw, df, m, f, dq, dd, g0, g1, i;
    uint32_t w = tape.w;
    if (!to && !tape.s)
        return;
    if (n > CTL)
        n = CTL;
    if (!tape.s) {                                      /* coming on: from silence, no delay */
        for (i = 0; i < (int32_t)TAPE_N; i++)
            tape_buf[0][i] = tape_buf[1][i] = 0;
        tape.ll = tape.lr = tape.dly = tape.wet = 0;
    }
    tape.s += clamp(to - tape.s, -256, 256);
    amt = (tape.s * 129) >> 7;                          /* Q15 */
    drive = 4096 + ((((amt >> 7) * (amt >> 7)) * 3) >> 5);   /* Q12: 1x .. 2.5x, gentle at first */
    bias = amt >> 3;
    b0 = softclip(bias);
    mk = (int32_t)(((uint32_t)32767u << 12) / (uint32_t)drive);   /* make-up, Q15 (1 / drive) */
    k = 32767 - ((amt * 15000) >> 15);                  /* open .. ~5.5 kHz */
    /* the delay at the block end: wow (squared knob) and flutter, each from 0 to twice its depth */
    tape.w1 += BLK_HZ(63);
    tape.w2 += BLK_HZ(97);
    tape.fp += BLK_HZ(780);
    tape.fn += (((int32_t)(noise32(&tape.rnd) >> 16) - 32768) - tape.fn) >> 4;
    dw = (((amt >> 7) * (amt >> 7)) * 52) >> 8;         /* wow depth, Q8 samples: up to 52 */
    df = (amt * 307) >> 15;                             /* flutter depth, Q8: up to 1.2 */
    m = (sine_i(tape.w1) * 5 + sine_i(tape.w2) * 3) >> 3;
    f = (sine_i(tape.fp) * 3 + tape.fn) >> 2;
    dq = tape.dly << CTL_LOG2;                          /* the delay, Q8 << CTL_LOG2, a step a sample */
    tape.dly = ((dw * (32768 + m)) >> 15) + ((df * (32768 + f)) >> 15);
    dd = tape.dly - (dq >> CTL_LOG2);
    g0 = tape.wet;
    g1 = tape.wet = amt > 4096 ? 32767 : amt * 8;
    if (g0 < 32767 || g1 < 32767)                       /* the wet part fading: keep the dry block */
        for (i = 0; i < (int32_t)n; i++) {
            tape_dry[0][i] = l[i];
            tape_dry[1][i] = r[i];
        }
    for (i = 0; i < (int32_t)n; i++, dq += dd) {
        int32_t d = dq >> CTL_LOG2, fr = d & 255, a, c;
        uint32_t ri = (w - ((uint32_t)d >> 8)) & (TAPE_N - 1u), rj = (ri - 1u) & (TAPE_N - 1u);
        tape_buf[0][w & (TAPE_N - 1u)] = (int16_t)softclip((((clamp(l[i], -230000, 230000) >> 2) * drive) >> 10) + bias);
        tape_buf[1][w & (TAPE_N - 1u)] = (int16_t)softclip((((clamp(r[i], -230000, 230000) >> 2) * drive) >> 10) + bias);
        w++;
        a = tape_buf[0][ri] + (((tape_buf[0][rj] - tape_buf[0][ri]) * fr) >> 8) - b0;
        c = tape_buf[1][ri] + (((tape_buf[1][rj] - tape_buf[1][ri]) * fr) >> 8) - b0;
        tape.ll += mulq15(a - tape.ll, k);
        tape.lr += mulq15(c - tape.lr, k);
        l[i] = mulq15(tape.ll, mk);
        r[i] = mulq15(tape.lr, mk);
    }
    tape.w = w;
    if (g0 < 32767 || g1 < 32767)
        for (i = 0; i < (int32_t)n; i++) {
            int32_t g = (g0 + (((g1 - g0) * i) >> CTL_LOG2)) >> 3, x = tape_dry[0][i], y = tape_dry[1][i];
            l[i] = x + ((((l[i] - x) >> 3) * g) >> 9);
            r[i] = y + ((((r[i] - y) >> 3) * g) >> 9);
        }
}

/* -------------------------------------------------------------- HUM --- */
/* The floor of an old recording, under the music while it plays (and gone ~0.1 s after STOP, as DUST's
 * record: a stopped Jangada is silent). G_HUM 0..127, the level growing as the square of the knob:
 *   hum      60 Hz, a sine and the same sine clipped at a third (the odd harmonics: 180, 300, 420 Hz ..),
 *            the same on both sides (it comes from the wall), up to ~-27 dBFS;
 *   hiss     white noise through a one-pole (~5 kHz), different left and right, ~-46 dBFS;
 *   crackle  a tick now and then (up to ~2 a second), softened by the hiss filter.
 * ~25 operations a sample. */
#define HUM_INC 5843375u                                /* 60 Hz: 60 * 2^32 / FS */
static struct {
    uint32_t ph;
    int32_t bed;                                        /* level now, Q15 (0 stopped) */
    int32_t rnd, ll, lr, pop;                           /* noise, hiss low-passes, a crackle decaying */
} hum = {0, 0, 0x6A09E667, 0, 0, 0};

static __attribute__((noinline)) void hum_process(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t h = song.g[G_HUM], to = song.playing ? h * h * 2 : 0, bed = hum.bed, db, i;
    uint32_t pc = (uint32_t)h * 3u / 2u, ph = hum.ph;   /* crackle: chance per sample, x 2^-22 */
    if (!to && !bed)
        return;
    db = clamp(to - bed, -238, 238);
    hum.bed = bed + db;
    db = (db << 8) >> CTL_LOG2;                          /* the ramp a sample, Q8 */
    bed <<= 8;
    for (i = 0; i < (int32_t)n; i++, bed += db) {
        int32_t s = sine_i(ph), c = clamp(s * 3, -32767, 32767), x, y;
        uint32_t nz = noise32(&hum.rnd);
        ph += HUM_INC;
        if ((nz >> 10) < pc)
            hum.pop = (int32_t)(nz & 0x7FFu) - 1024;    /* a crackle */
        hum.ll += ((((int32_t)(nz >> 16) - 32768) >> 6) + hum.pop - hum.ll) >> 1;   /* hiss, ~5 kHz */
        hum.lr += ((((int32_t)(nz & 0xFFFFu) - 32768) >> 6) + hum.pop - hum.lr) >> 1;
        hum.pop -= hum.pop >> 3;
        s = ((s + c * 3) * 11) >> 10;                   /* the hum: +-1400 at full */
        x = s + hum.ll;
        y = s + hum.lr;
        l[i] += (x * (bed >> 8)) >> 15;
        r[i] += (y * (bed >> 8)) >> 15;
    }
    hum.ph = ph;
}
