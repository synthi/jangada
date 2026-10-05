/* SPDX-License-Identifier: GPL-3.0-only */
/* Jangada: the layers (firmware/src/ui_layers.c) on the host, with the UI code and a framebuffer in
 * place of the LCD: the SEQ layer's step keys, pattern tools and undo / redo, the ENGINE keys, and
 * every layer's screen draws; and the page columns (ui_draw.c draw_column, Inter Tight on cards): no
 * label, value or unit of any page, engine or value is cut to fit its card.
 * Build: cc -Ibuild/gen -Ifirmware/src tests/layers_test.c -lm */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static int ncut;
#define UI_HOOK_CUT(src, maxw) (ncut++ < 20 ? printf("cut to %d px: '%s'\n", (int)(maxw), (src)) : 0)
#include <math.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
static volatile uint32_t fm1_ms;
static uint16_t FB[240*240];
static void lcd_sync(void){}
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c){for(uint32_t j=y;j<y+h&&j<240;j++)for(uint32_t i=x;i<x+w&&i<240;i++)FB[j*240+i]=(uint16_t)((c>>8)|(c<<8));}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t*p){for(uint32_t j=0;j<h;j++)for(uint32_t i=0;i<w;i++)if(y+j<240&&x+i<240)FB[(y+j)*240+x+i]=p[j*w+i];}
static void fm1_delay_ms(uint32_t ms){(void)ms;}
static uint32_t fm1_ticks(void){return fm1_ms*1000;}
static void fm1_wdt_feed(void){}
static void fm1_irq_off(void){} static void fm1_irq_on(void){}
static uint8_t ledstate[64];
static void fm1_led_key(uint32_t k,int on){if(k<64)ledstate[k]=on;}
static int32_t fm1_enc_take(uint32_t i){(void)i;return 0;}
static uint32_t fm1_input_edges(int x){(void)x;return 0;}
static uint32_t fm1_input_note_edges(void){return 0;}
#include "gfx.c"
#include "core.h"
#include "engines.c"
#include "mod.c"
#include "drums.c"
#include "params.c"
#include "voice.c"
#include "slicer.c"
#include "fx.c"
#include "usb.c"
#include "midi_uart.c"
#include "seq.c"
struct felucca_dbg { uint32_t in_audio, late, halves, last_us, stage; } felucca_dbg;
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#include "panel.c"
#include "ui.c"
#include "icons.c"
#include "ui_draw.c"
#include "ui_menu.c"
#define FM1_NCOL 8
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
#define FM1_TICKS_PER_US 1
static uint8_t fm1_led[FM1_NCOL];
#include "ui_input.c"
#include "upreset.c"
#include "project.c"

/* power-on: three parts with their default sounds (TRK_DEF), the drum track, empty patterns */
static void felucca_init(void)
{
    uint32_t i;
    fm6_init();                                 /* Jangada: every track the FM6 init voice */
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);   /* with its sends */
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);              /* the sequencers start empty */
    }
    song.sel = 0;
    song.master_q12 = 2048;
    ui.home = 1;
    ui.force = 1;
}
#include <assert.h>

static int notes(track_t*t,int i){return t->step[i].n?t->step[i].note[0]:0;}
int main(void){
 felucca_init(); track_t*t=&trk[0]; t->p[P_SLEN]=4;
 for(int i=0;i<4;i++){t->step[i].note[0]=60+i;t->step[i].n=1;t->step[i].time=ST_NOTE;}
 ly.snap=0; pattern_tool(1);  /* shift > */
 assert(notes(t,0)==63&&notes(t,1)==60);
 pattern_tool(3); assert(t->p[P_SLEN]==8&&notes(t,4)==63); /* same hold: same snapshot */
 undo_swap(0); assert(t->p[P_SLEN]==4&&notes(t,0)==60&&notes(t,3)==63);
 undo_swap(1); assert(t->p[P_SLEN]==8&&notes(t,0)==63);
 undo_swap(1); /* nothing to redo */
 ly.snap=0; pattern_tool(5); assert(notes(t,0)==64);
 pattern_tool(2); assert(t->p[P_SLEN]==4);
 undo_swap(0); assert(notes(t,0)==63&&t->p[P_SLEN]==8);
 /* step keys: down on empty sets, down+up on set clears */
 ly.snap=0; ly.page=0; last_note=50; t->step[6].n=0; t->step[6].time=ST_REST;
 layer_key(LY_STEP,key_of_white(6),1,0); assert(notes(t,6)==50);
 layer_key(LY_STEP,key_of_white(6),0,0); assert(notes(t,6)==50);  /* just set: stays */
 layer_key(LY_STEP,key_of_white(6),1,0); layer_key(LY_STEP,key_of_white(6),0,0); assert(!t->step[6].n);
 /* held + knob: edited, kept */
 layer_key(LY_STEP,key_of_white(1),1,0); steps_held_edit(1,1); layer_key(LY_STEP,key_of_white(1),0,0);
 assert(t->step[1].n && ((t->step[1].flags&SF_RATCH)>>SF_RATCH_SH)==1);
 /* engine layer */
 layer_key(LY_ENGINE,key_of_white(6),1,0); assert(t->eng_req==6);
 printf("%-46s ok\n", "layers: SEQ tools, undo / redo, step keys");
 printf("%-46s ok\n", "layers: ENGINE keys pick the engine");
 { /* every layer's screen draws, in every palette, with something on it; leaving it restores the page */
   uint32_t l, p, lit;
   for (p = 0; p < NPALETTES; p++) for (l = LY_FX; l < LY_COUNT; l++) {
     palette_set(p); ly.btn = (uint8_t)l; ly.lock = 0; ly.t0 = 0; fm1_ms = 1000; ly.shown = 0;
     memset(FB, 0, sizeof FB); ui.force = 1; ui_draw();
     for (lit = 0; lit < 240u * 240u && !FB[lit]; lit++) ;
     assert(lit < 240u * 240u && ly.shown);
   }
   ly.btn = 0; ui_draw(); assert(!ly.shown);
 }
 printf("%-46s ok\n", "layers: every screen draws, in every palette");
 { /* every page of every engine, every value (sampled) of every column: nothing cut */
   uint32_t e, pi, c;
   palette_set(5); ui.home = 0; song.sel = 0;
   for (e = 0; e < NENGINES; e++) {
     set_engine_of(&trk[0], e); trk[0].engine = trk[0].eng_req;
     for (pi = 0; pi < NPAGES; pi++) {
       const page_t *pg = &PAGES[pi];
       if (e && pg->scope != SC_ENGINE && pg->graph != GR_BROWSE) continue;
       ui.page = (uint8_t)pi;
       for (c = 0; c < 4u; c++) {
         int16_t *vp = 0, keep;
         const param_desc_t *d = pg->scope == SC_STEP || pg->scope == SC_TRK ? 0 : page_desc(pg, c, &vp);
         int32_t v, step;
         if (!d || !vp) { ui.force = 1; draw_columns(); continue; }
         keep = *vp; step = (d->max - d->min) / 300 + 1;
         for (v = d->min; v <= d->max; v += step) { *vp = (int16_t)v; ui.force = 1; draw_columns(); }
         *vp = keep;
       }
     }
   }
   ui.home = 1; ui.force = 1; draw_columns();
   assert(!ncut);
 }
 printf("%-46s ok\n", "columns: no label / value / unit cut (Inter Tight)");
 return 0;}
