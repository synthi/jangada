/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Menu (HOME held): COLOR, SPEAKER (the low cut for the small speaker), NEW PROJECT, ABOUT.
 * Jangada: ZOOM and HARDWARE CALIBRATION left the menu (calibration: OCT- + OCT+ held at power-on). */
/* ------------------------------------------------------------ menu --- */
enum { MI_COLOR, MI_SPEAKER, MI_NEW, MI_ABOUT, MI_BACK, MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "SPEAKER", "NEW PROJECT", "ABOUT", "BACK"};
static uint8_t menu_new_armed;                      /* NEW PROJECT: OCT+ once arms, again clears */
static void felucca_init(void);                     /* main.c */

static void draw_menu(void)
{
    uint32_t i, pass, sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                            menu_new_armed * 104729u;
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    if (ui.force)                                   /* head + rule + two bands cover rows 0..229 */
        lcd_fill(0, H_HEAD + 1 + 124 + 85, 240, 240 - (H_HEAD + 1 + 124 + 85), C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);
    cv_text(4, 1, &FONT_S, ui.menu == 2 ? "ABOUT" : "MENU", C_HI);
    cv_blit(0, Y_HEAD);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    for (pass = 0; pass < 2u; pass++) {             /* the canvas holds 124 rows: draw in two bands */
        cv_begin(240, pass ? 85u : 124u, C_BLACK);
        cv_oy = pass ? -124 : 0;
        if (ui.menu == 2) {
            cv_text(4, 4, &FONT_L, "JANGADA", C_HI);
            cv_text(4, 36, &FONT_S, FELUCCA_VERSION, C_HI);
            cv_text(236 - text_w(&FONT_S, FELUCCA_DATE), 36, &FONT_S, FELUCCA_DATE, C_GRAY);   /* build date (build.py) */
            cv_text(4, 52, &FONT_S, "GITHUB.COM/ZEDNAKED/JANGADA", C_AMB);
            cv_text(4, 70, &FONT_S, "FORK OF FELUCCA BY", C_GRAY);
            cv_text(cv_text(4, 84, &FONT_S, "LEO KUROSHITA", C_HI) + 8, 84, &FONT_S, "H\xDCGELTON", C_AMB);   /* Latin-1 U-umlaut */
            cv_text(cv_text(4, 100, &FONT_S, "+ SLOOP", C_HI) + 8, 100, &FONT_S, "ISOD89", C_AMB);
            cv_text(4, 116, &FONT_S, "GPL-3.0, NO WARRANTY", C_HI);
            cv_text(4, 131, &FONT_S, "FM6: MSFA / DEXED (APACHE)", C_DIM);
            cv_text(4, 143, &FONT_S, "PHASE: CRISPYZEBRA (GPL)", C_DIM);
            cv_text(4, 155, &FONT_S, "VOICE: REF. KLATTSCH (MIT)", C_DIM);
            cv_text(4, 167, &FONT_S, "SAMPLES: VERSILIAN (CC0)", C_DIM);
            cv_text(4, 179, &FONT_S, "+ H\xDCGELTON SAMPLE PACK", C_DIM);
            cv_text(4, 191, &FONT_S, "FONT: TERMINUS (OFL)", C_DIM);
        } else {
            for (i = 0; i < MI_COUNT; i++) {
                int32_t y = 4 + (int32_t)i * 24;
                int sel = i == ui.menu_sel;
                if (sel)
                    cv_rect(4, y + 6, 3, 3, C_WHITE);
                cv_text(14, y, &FONT_S, MI_NAME[i], sel ? C_WHITE : C_GRAY);
                if (i == MI_SPEAKER)
                    cv_text(110, y, &FONT_S, settings.lowcut ? "LOW CUT ON" : "OFF", C_HI);
                if (i == MI_COLOR) {
                    uint32_t k;
                    cv_text(90, y, &FONT_S, PALETTES[settings.palette].name, C_HI);
                    for (k = 0; k < 5u; k++)
                        cv_rect(160 + (int32_t)k * 14, y + 3, 10, 10, pal[k]);
                }
            }
            cv_text(4, 146, &FONT_S, ui.menu_sel == MI_SPEAKER ? "ON: LESS BASS (SPEAKER)" :
                                     ui.menu_sel != MI_NEW ? "" : menu_new_armed ? "OCT+ AGAIN: CLEAR ALL" :
                                     "EVERY TRACK BACK TO START", menu_new_armed ? C_WHITE : C_GRAY);
            cv_text(4, 170, &FONT_S, "PRESETS MOVE", C_DIM);
            cv_text(4, 188, &FONT_S, "OCT+ OK   OCT- BACK", C_DIM);
        }
        cv_oy = 0;
        cv_blit(0, H_HEAD + 1 + pass * 124u);
    }
}

static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

static void menu_close(void)
{
    settings_save();                                   /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* menu: PRESETS moves, OCT+ confirms, OCT- cancels (ABOUT -> list -> close) */
static void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u;
    if (back) {
        if (ui.menu == 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && ui.menu == 1) {
        ui.menu_sel = (uint8_t)((ui.menu_sel + (s > 0 ? 1u : MI_COUNT - 1u)) % MI_COUNT);
        menu_new_armed = 0;
    }
    s = panel_enc(EN_K1);
    if (s != 0 && ui.menu == 1 && ui.menu_sel == MI_COLOR) {
        settings.palette = (settings.palette + (s > 0 ? 1u : NPALETTES - 1u)) % NPALETTES;
        palette_set(settings.palette);              /* (the menu signature redraws) */
    }
    if ((s != 0 || ok) && ui.menu == 1 && ui.menu_sel == MI_SPEAKER) {
        /* KNOB 1: right = ON, left = OFF; OCT+ toggles */
        uint32_t *v = &settings.lowcut;
        *v = s > 0 ? 1u : s < 0 ? 0u : !*v;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        ok = 0;
    }
    if (ok && ui.menu == 1) {
        switch (ui.menu_sel) {
        case MI_COLOR:                                 /* OCT+ steps through the palettes too */
            settings.palette = (settings.palette + 1u) % NPALETTES;
            palette_set(settings.palette);
            break;
        case MI_NEW:                                   /* Jangada: OCT+ arms, OCT+ again: a new project */
            if (!menu_new_armed) {
                menu_new_armed = 1;
                break;
            }
            menu_new_armed = 0;
            transport_req = 2;
            panic_req = (uint8_t)((1u << NTRK) - 1u);
            felucca_init();                            /* the power-on tracks, empty patterns */
            sync_reload = 1;
            menu_close();
            ui_message("NEW PROJECT");
            break;
        case MI_ABOUT:
            ui.menu = 2;
            ui.force = 1;
            break;
        default:
            menu_close();
            break;
        }
    }
    enc_drop();                                        /* swallow the rest while the menu is up */
}

