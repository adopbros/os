/*
 * Arrowix OS - Interactive shell + presentation layer.
 *
 * Renders a boot splash, a framed VGA UI (cyan status bar with a live PIT
 * clock, central scrolling console, blue shortcut bar), and a small command
 * interpreter fed by the PS/2 keyboard ring buffer. The command line uses a
 * fixed static buffer (no heap dependency), so the shell stays rock solid for
 * the demo even if dynamic allocation were unavailable.
 */

#include <arrowix/shell.h>
#include <arrowix/console.h>
#include <arrowix/keyboard.h>
#include <arrowix/pit.h>
#include <arrowix/string.h>
#include <arrowix/multiboot2.h>
#include <arrowix/pmm.h>
#include <arrowix/types.h>

/* --- UI geometry ----------------------------------------------------------- */
#define STATUS_ROW    0
#define REGION_TOP    1
#define REGION_BOTTOM 23
#define SHORTCUT_ROW  24

#define CH_BLOCK      ((char) 0xDB) /* CP437 full block */
#define CH_SHADE      ((char) 0xB0) /* CP437 light shade */

/* --- Tiny formatting helpers ----------------------------------------------- */
static int u_to_dec(u32 v, char *out)
{
    char tmp[12];
    int n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    }
    while (v != 0) {
        tmp[n++] = (char) ('0' + v % 10);
        v /= 10;
    }
    for (int i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    out[n] = '\0';
    return n;
}

static void put2(char *d, u32 v)
{
    d[0] = (char) ('0' + (v / 10) % 10);
    d[1] = (char) ('0' + v % 10);
}

/* --- 5x5 block font for the boot logo -------------------------------------- */
static const char *FONT_A[5] = {" ### ", "#   #", "#####", "#   #", "#   #"};
static const char *FONT_R[5] = {"#### ", "#   #", "#### ", "#  # ", "#   #"};
static const char *FONT_O[5] = {" ### ", "#   #", "#   #", "#   #", " ### "};
static const char *FONT_W[5] = {"#   #", "#   #", "# # #", "## ##", "#   #"};
static const char *FONT_I[5] = {" ### ", "  #  ", "  #  ", "  #  ", " ### "};
static const char *FONT_X[5] = {"#   #", " # # ", "  #  ", " # # ", "#   #"};
static const char *FONT_S[5] = {" ####", "#    ", " ### ", "    #", "#### "};
static const char *FONT_G[5] = {" ####", "#    ", "# ###", "#   #", " ### "};
static const char *FONT_L[5] = {"#    ", "#    ", "#    ", "#    ", "#####"};
static const char *FONT_E[5] = {"#####", "#    ", "#### ", "#    ", "#####"};

static const char *const *glyph_for(char c)
{
    switch (c) {
    case 'A': return FONT_A;
    case 'R': return FONT_R;
    case 'O': return FONT_O;
    case 'W': return FONT_W;
    case 'I': return FONT_I;
    case 'X': return FONT_X;
    case 'S': return FONT_S;
    case 'G': return FONT_G;
    case 'L': return FONT_L;
    case 'E': return FONT_E;
    default:  return NULL; /* space / unknown -> blank glyph */
    }
}

/* --- CP437 box-drawing chars ----------------------------------------------- */
#define BX_H  ((char) 0xC4) /* horizontal line  - */
#define BX_V  ((char) 0xB3) /* vertical line    | */
#define BX_TL ((char) 0xDA) /* top-left corner */
#define BX_TR ((char) 0xBF) /* top-right corner */
#define BX_BL ((char) 0xC0) /* bottom-left corner */
#define BX_BR ((char) 0xD9) /* bottom-right corner */

static void draw_box(int x, int y, int w, int h, enum vga_color fg, enum vga_color bg,
                     bool fill_interior)
{
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            char c;
            if (j == 0 && i == 0) {
                c = BX_TL;
            } else if (j == 0 && i == w - 1) {
                c = BX_TR;
            } else if (j == h - 1 && i == 0) {
                c = BX_BL;
            } else if (j == h - 1 && i == w - 1) {
                c = BX_BR;
            } else if (j == 0 || j == h - 1) {
                c = BX_H;
            } else if (i == 0 || i == w - 1) {
                c = BX_V;
            } else if (fill_interior) {
                c = ' ';
            } else {
                continue; /* leave the interior untouched */
            }
            console_put_at(x + i, y + j, c, fg, bg);
        }
    }
}

static void draw_banner(const char *word, int top_row, enum vga_color fg)
{
    int len = (int) strlen(word);
    int width = len * 6 - 1; /* 5-wide glyphs + 1 spacing column */
    int start_x = (VGA_WIDTH - width) / 2;

    for (int row = 0; row < 5; ++row) {
        int x = start_x;
        for (int i = 0; word[i] != '\0'; ++i) {
            const char *const *g = glyph_for(word[i]);
            for (int k = 0; k < 5; ++k) {
                char cell = (g != NULL && g[row][k] == '#') ? CH_BLOCK : ' ';
                console_put_at(x + k, top_row + row, cell, fg, VGA_BLACK);
            }
            x += 6;
        }
    }
}

/* --- Boot splash ----------------------------------------------------------- */
static void boot_splash(void)
{
    console_clear();

    draw_banner("ARROWIX OS", 4, VGA_LIGHT_CYAN);

    const char *subtitle = "Hybrid 64-bit Kernel  .  x86_64 Long Mode  .  v0.1";
    console_write_at((VGA_WIDTH - (int) strlen(subtitle)) / 2, 11, subtitle,
                     VGA_DARK_GRAY, VGA_BLACK);

    const char *loading = "Cargando el nucleo de Arrowix...";
    console_write_at((VGA_WIDTH - (int) strlen(loading)) / 2, 14, loading,
                     VGA_LIGHT_GRAY, VGA_BLACK);

    const int bar_w = 50;
    const int bx = (VGA_WIDTH - bar_w) / 2 - 1;
    const int by = 16;

    console_put_at(bx, by, '[', VGA_WHITE, VGA_BLACK);
    console_put_at(bx + bar_w + 1, by, ']', VGA_WHITE, VGA_BLACK);
    for (int k = 0; k < bar_w; ++k) {
        console_put_at(bx + 1 + k, by, CH_SHADE, VGA_DARK_GRAY, VGA_BLACK);
    }

    for (int step = 0; step <= bar_w; ++step) {
        for (int k = 0; k < step; ++k) {
            console_put_at(bx + 1 + k, by, CH_BLOCK, VGA_LIGHT_GREEN, VGA_BLACK);
        }
        char pct[6];
        int n = u_to_dec((u32) (step * 100 / bar_w), pct);
        pct[n] = '%';
        pct[n + 1] = '\0';
        console_write_at((VGA_WIDTH - (n + 1)) / 2, by + 2, pct, VGA_WHITE, VGA_BLACK);

        pit_sleep_ms(40); /* ~2.04 s total over 51 steps */
    }

    pit_sleep_ms(200);
}

/* --- Framed UI ------------------------------------------------------------- */
static u32 g_last_secs = 0xFFFFFFFFu;

static void draw_status_bar(void)
{
    console_fill_row(STATUS_ROW, ' ', VGA_BLACK, VGA_CYAN);
    console_write_at(1, STATUS_ROW, "Arrowix OS v0.1", VGA_BLACK, VGA_CYAN);
}

/* --- Persistent taskbar (row 24) ------------------------------------------- */
#define TB_BG       VGA_LIGHT_GRAY
#define TB_FG       VGA_BLACK
#define TB_CLOCK_X  (VGA_WIDTH - 9) /* "HH:MM:SS" + 1 padding */

#define ICON_GLOBE_TB ((char) 0x09) /* CP437 circle as a globe */
#define ICON_FILE_TB  ((char) 0xFE) /* CP437 small square as files */
#define ICON_GEAR_TB  ((char) 0x0F) /* CP437 sun/gear as settings */
#define TB_BTN_BG     VGA_DARK_GRAY  /* raised-button background on the bar */

static u32 uptime_secs(void);
static void fmt_hms(char *b, u32 secs);

static volatile bool g_taskbar_enabled;
static volatile u32 g_taskbar_secs = 0xFFFFFFFFu;

/* Start menu overlay state. */
#define SM_W 26
#define SM_H 12
#define SM_X 0
#define SM_Y (SHORTCUT_ROW - SM_H)
static bool g_startmenu_open;
static u16 g_startmenu_save[SM_W * SM_H];

static void taskbar_draw_clock(void)
{
    char hms[12];
    fmt_hms(hms, uptime_secs());
    console_write_at(TB_CLOCK_X, SHORTCUT_ROW, hms, TB_FG, VGA_WHITE);
}

/* Draw a "[ <icon> <label> ]" quick-access button; returns the next free col. */
static int tb_button(int x, char glyph, enum vga_color glyph_fg, const char *label)
{
    int len = (int) strlen(label);
    console_put_at(x, SHORTCUT_ROW, '[', VGA_WHITE, TB_BTN_BG);
    console_put_at(x + 1, SHORTCUT_ROW, ' ', VGA_WHITE, TB_BTN_BG);
    console_put_at(x + 2, SHORTCUT_ROW, glyph, glyph_fg, TB_BTN_BG);
    console_put_at(x + 3, SHORTCUT_ROW, ' ', VGA_WHITE, TB_BTN_BG);
    console_write_at(x + 4, SHORTCUT_ROW, label, VGA_WHITE, TB_BTN_BG);
    console_put_at(x + 4 + len, SHORTCUT_ROW, ' ', VGA_WHITE, TB_BTN_BG);
    console_put_at(x + 5 + len, SHORTCUT_ROW, ']', VGA_WHITE, TB_BTN_BG);
    return x + len + 6 + 1; /* +1 spacing column before the next button */
}

/* Full repaint of the taskbar; call when (re)entering the shell UI. */
static void taskbar_render(void)
{
    console_fill_row(SHORTCUT_ROW, ' ', TB_FG, TB_BG);

    /* Start button. */
    console_write_at(0, SHORTCUT_ROW, "[ Inicio ]", VGA_WHITE, VGA_GREEN);

    /* Quick-access icon buttons (internet / files / settings). */
    int x = 11;
    x = tb_button(x, ICON_GLOBE_TB, VGA_LIGHT_CYAN, "Web");
    x = tb_button(x, ICON_FILE_TB, VGA_YELLOW, "Sys");
    x = tb_button(x, ICON_GEAR_TB, VGA_LIGHT_GREEN, "Conf");

    /* Kernel status indicator. */
    console_write_at(50, SHORTCUT_ROW, "[ ", VGA_BLACK, TB_BG);
    console_write_at(52, SHORTCUT_ROW, "KERNEL OK", VGA_GREEN, TB_BG);
    console_write_at(61, SHORTCUT_ROW, " ]", VGA_BLACK, TB_BG);

    /* System tray clock (white "tray" background). */
    console_put_at(TB_CLOCK_X - 1, SHORTCUT_ROW, ' ', TB_FG, VGA_WHITE);
    console_put_at(VGA_WIDTH - 1, SHORTCUT_ROW, ' ', TB_FG, VGA_WHITE);
    g_taskbar_secs = 0xFFFFFFFFu; /* force the clock to repaint */
    taskbar_draw_clock();
}

void taskbar_set_enabled(bool on)
{
    g_taskbar_enabled = on;
    if (on) {
        taskbar_render();
    }
}

/*
 * Tick-safe update: called from the shell loop AND the PIT IRQ. Only repaints
 * the clock when the second changes, so it is cheap enough for IRQ context and
 * never touches rows the shell text uses (it owns row 24 exclusively).
 */
void update_taskbar(void)
{
    if (!g_taskbar_enabled) {
        return;
    }
    u32 secs = uptime_secs();
    if (secs == g_taskbar_secs) {
        return;
    }
    g_taskbar_secs = secs;
    taskbar_draw_clock();
}

bool taskbar_start_menu_open(void)
{
    return g_startmenu_open;
}

void taskbar_close_start_menu(void)
{
    if (!g_startmenu_open) {
        return;
    }
    console_write_cells(SM_X, SM_Y, SM_W, SM_H, g_startmenu_save);
    g_startmenu_open = false;
}

void taskbar_toggle_start_menu(void)
{
    if (g_startmenu_open) {
        taskbar_close_start_menu();
        return;
    }

    /* Save what we are about to cover so we can restore it on close. */
    console_read_cells(SM_X, SM_Y, SM_W, SM_H, g_startmenu_save);

    draw_box(SM_X, SM_Y, SM_W, SM_H, VGA_WHITE, VGA_BLUE, true);
    console_write_at(SM_X + 2, SM_Y, " Inicio - Arrowix ", VGA_BLACK, VGA_CYAN);

    static const char *const items[] = {
        "help     Ayuda",     "about    Acerca de",  "sysinfo  Sistema",
        "clear    Limpiar",   "matrix   Easter egg", "browser  Web texto",
        "gui      Ventanas",  "chrome   Navegador",
    };
    for (int i = 0; i < 8; ++i) {
        console_write_at(SM_X + 2, SM_Y + 2 + i, items[i], VGA_WHITE, VGA_BLUE);
    }
    console_write_at(SM_X + 2, SM_Y + SM_H - 1, "F1/ESC: cerrar", VGA_LIGHT_CYAN, VGA_BLUE);

    g_startmenu_open = true;
}

static u32 uptime_secs(void)
{
    u32 freq = pit_frequency();
    if (freq == 0) {
        freq = 100;
    }
    return (u32) (pit_ticks() / freq);
}

/* Format `secs` as "HH:MM:SS" (8 chars + NUL). */
static void fmt_hms(char *b, u32 secs)
{
    put2(b, secs / 3600);
    b[2] = ':';
    put2(&b[3], (secs / 60) % 60);
    b[5] = ':';
    put2(&b[6], secs % 60);
    b[8] = '\0';
}

static void update_clock(bool force)
{
    u32 secs = uptime_secs();
    if (!force && secs == g_last_secs) {
        return;
    }
    g_last_secs = secs;

    char buf[20];
    const char *pfx = "Uptime ";
    int i = 0;
    while (pfx[i] != '\0') {
        buf[i] = pfx[i];
        ++i;
    }
    fmt_hms(&buf[i], secs);

    console_write_at(VGA_WIDTH - (i + 8) - 1, STATUS_ROW, buf, VGA_BLACK, VGA_CYAN);
}

static void draw_prompt(void)
{
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kputs("arrowix@kernel");
    console_set_color(VGA_WHITE, VGA_BLACK);
    kputs(":~# ");
    console_reset_color();
}

/* --- Commands -------------------------------------------------------------- */
static void cmd_help(void)
{
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("Comandos disponibles:\n");
    console_reset_color();

    static const char *const names[] = {"help",   "about",   "sysinfo", "clear", "matrix",
                                        "browser", "gui",     "chrome"};
    static const char *const descs[] = {
        "Muestra esta ayuda",
        "Acerca de Arrowix OS y su vision",
        "Informacion del sistema (CPU, RAM, uptime)",
        "Limpia el area central de la pantalla",
        "Easter egg: lluvia de codigo (ESC para salir)",
        "Navegador web de texto (ej. browser google.com)",
        "Entorno de ventanas simulado (ESC para salir)",
        "Arrowix Browser a pantalla completa (ESC para salir)",
    };
    for (int i = 0; i < 8; ++i) {
        kputs("  ");
        console_set_color(VGA_YELLOW, VGA_BLACK);
        kputs(names[i]);
        console_reset_color();
        for (int pad = (int) strlen(names[i]); pad < 10; ++pad) {
            kputc(' ');
        }
        kputs(descs[i]);
        kputc('\n');
    }
}

static void cmd_about(void)
{
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("Arrowix OS\n");
    console_reset_color();
    kputs("Un sistema operativo grande y amplio, con la vision de ser tan\n");
    kputs("completo como Windows: modular, robusto y de arquitectura hibrida\n");
    kputs("de 64 bits (x86_64). Optimizado con IA y desarrollado con Cursor.\n");
    console_set_color(VGA_DARK_GRAY, VGA_BLACK);
    kputs("\"Pequeno hoy, inmenso manana.\"\n");
    console_reset_color();
}

static void cmd_sysinfo(void)
{
    struct pmm_stats ps;
    pmm_get_stats(&ps);

    u32 freq = pit_frequency();
    if (freq == 0) {
        freq = 100;
    }
    u32 secs = (u32) (pit_ticks() / freq);

    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("Informacion del sistema\n");
    console_reset_color();

    kputs("  Arquitectura : x86_64 (Long Mode, paginacion de 4 niveles)\n");
    kprintf("  RAM total    : %u MiB (detectada por Multiboot2)\n",
            (u32) (mb2_total_available() >> 20));
    kprintf("  Frames libres: %u de %u (%u MiB libres)\n",
            (u32) ps.free_frames, (u32) ps.total_frames,
            (u32) ((ps.free_frames * 4096ull) >> 20));
    kprintf("  Uptime       : %u s (%u ticks @ %u Hz)\n",
            secs, (u32) pit_ticks(), freq);
    kputs("  Interrupcion : PIC 8259A + PIT + teclado PS/2 (IRQ1)\n");
}

static void cmd_matrix(void)
{
    static int heads[VGA_WIDTH];
    u32 rng = (u32) pit_ticks() * 2654435761u + 1u;

    const int top = REGION_TOP;
    const int bottom = REGION_BOTTOM;
    const int span = bottom - top + 1;

    for (int x = 0; x < VGA_WIDTH; ++x) {
        rng = rng * 1103515245u + 12345u;
        heads[x] = top + (int) ((rng >> 16) % (u32) span);
    }

    for (;;) {
        for (int x = 0; x < VGA_WIDTH; ++x) {
            int r = heads[x];

            rng = rng * 1103515245u + 12345u;
            char head_ch = (char) ('!' + (int) ((rng >> 16) % 94u));
            console_put_at(x, r, head_ch, VGA_WHITE, VGA_BLACK);

            if (r - 1 >= top) {
                rng = rng * 1103515245u + 12345u;
                char trail = (char) ('!' + (int) ((rng >> 16) % 94u));
                console_put_at(x, r - 1, trail, VGA_LIGHT_GREEN, VGA_BLACK);
            }

            int tail = r - 7;
            if (tail >= top) {
                console_put_at(x, tail, ' ', VGA_GREEN, VGA_BLACK);
            }

            heads[x] = r + 1;
            if (heads[x] > bottom) {
                rng = rng * 1103515245u + 12345u;
                heads[x] = ((rng >> 16) % 3u == 0) ? top : bottom;
            }
        }

        update_clock(false);
        pit_sleep_ms(55);

        bool stop = false;
        int k;
        while ((k = keyboard_getchar()) != 0) {
            if (k == KEY_ESC) {
                stop = true;
            }
        }
        if (stop) {
            break;
        }
    }

    console_clear_region();
}

/* --- Shared "press ESC to exit" wait loop ---------------------------------- */
/* Spin in hlt, keep the requested clock alive, and return when ESC is pressed.
 * If `gui_taskbar` is true, refresh the GUI taskbar clock instead of the status
 * bar clock. */
static void wait_for_esc(bool gui_taskbar);

/* Re-draw the framed Arrowix Shell UI (used when a full-screen app exits). */
static void restore_shell_frame(void)
{
    g_startmenu_open = false; /* overlay is gone after a full-screen clear */
    console_clear();
    console_set_region(REGION_TOP, REGION_BOTTOM);
    console_reset_color();
    console_clear_region();
    draw_status_bar();
    taskbar_set_enabled(true); /* repaints the taskbar on row 24 */
    update_clock(true);
}

/* --- App 1: text web browser simulator ------------------------------------- */
static bool str_contains(const char *haystack, const char *needle)
{
    for (; *haystack != '\0'; ++haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*n != '\0' && *h == *n) {
            ++h;
            ++n;
        }
        if (*n == '\0') {
            return true;
        }
    }
    return false;
}

static void draw_google_logo(int top_row)
{
    static const char letters[] = "GOOGLE";
    static const enum vga_color colors[6] = {
        VGA_LIGHT_BLUE, VGA_LIGHT_RED, VGA_YELLOW,
        VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_RED,
    };
    int width = 6 * 6 - 1;
    int start_x = (VGA_WIDTH - width) / 2;

    for (int row = 0; row < 5; ++row) {
        int x = start_x;
        for (int i = 0; i < 6; ++i) {
            const char *const *g = glyph_for(letters[i]);
            for (int k = 0; k < 5; ++k) {
                char cell = (g != NULL && g[row][k] == '#') ? CH_BLOCK : ' ';
                console_put_at(x + k, top_row + row, cell, colors[i], VGA_BLACK);
            }
            x += 6;
        }
    }
}

static void render_google_page(void)
{
    draw_google_logo(REGION_TOP + 2);

    const char *search = "[   Escribe tu busqueda aqui                       ]";
    const char *button = "[ Buscar ]    [ Voy a tener suerte ]";
    console_write_at((VGA_WIDTH - (int) strlen(search)) / 2, REGION_TOP + 9, search,
                     VGA_BLACK, VGA_WHITE);
    console_write_at((VGA_WIDTH - (int) strlen(button)) / 2, REGION_TOP + 11, button,
                     VGA_LIGHT_GRAY, VGA_BLACK);
}

static void render_generic_page(const char *url)
{
    int x = 6;
    int y = REGION_TOP + 1;
    int w = VGA_WIDTH - 12;
    int h = 16;

    draw_box(x, y, w, h, VGA_LIGHT_GRAY, VGA_BLACK, false);

    /* Title bar inside the frame. */
    for (int i = 1; i < w - 1; ++i) {
        console_put_at(x + i, y + 1, ' ', VGA_WHITE, VGA_BLUE);
    }
    console_write_at(x + 2, y + 1, url, VGA_WHITE, VGA_BLUE);
    console_write_at(x + w - 9, y + 1, "[ _ ][X]", VGA_WHITE, VGA_BLUE);

    /* Simulated menu bar. */
    console_write_at(x + 2, y + 3, "| Inicio | Foros | Noticias | Acerca |",
                     VGA_LIGHT_CYAN, VGA_BLACK);
    for (int i = 1; i < w - 1; ++i) {
        console_put_at(x + i, y + 4, BX_H, VGA_DARK_GRAY, VGA_BLACK);
    }

    console_write_at(x + 2, y + 6, "Bienvenido a la red descentralizada de Arrowix OS",
                     VGA_WHITE, VGA_BLACK);
    console_write_at(x + 2, y + 8,
                     "Esta es una pagina renderizada por el navegador de texto",
                     VGA_LIGHT_GRAY, VGA_BLACK);
    console_write_at(x + 2, y + 9, "integrado en el kernel. Contenido 100% simulado.",
                     VGA_LIGHT_GRAY, VGA_BLACK);
    console_write_at(x + 2, y + 12, "> Articulo destacado: \"El futuro es hibrido\"",
                     VGA_YELLOW, VGA_BLACK);
    console_write_at(x + 2, y + 13, "> Comunidad: 1.337 nodos conectados",
                     VGA_YELLOW, VGA_BLACK);
}

static void cmd_browser(const char *url)
{
    if (*url == '\0') {
        console_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kputs("Uso: browser <url> (Ej. browser google.com)\n");
        console_reset_color();
        return;
    }

    /* Simulated network handshake driven by the PIT. */
    console_clear_region();
    console_set_color(VGA_YELLOW, VGA_BLACK);
    kputs("[CONNECTING] Buscando servidor en 192.168.1.254...\n");
    console_reset_color();
    pit_sleep_ms(500);

    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[RECEIVING] Descargando paquetes HTML (4.2 KB) desde %s...\n", url);
    console_reset_color();
    pit_sleep_ms(500);

    console_clear_region();
    if (str_contains(url, "google")) {
        render_google_page();
    } else {
        render_generic_page(url);
    }

    console_write_at(2, REGION_BOTTOM, "Pulsa ESC para volver a la shell",
                     VGA_DARK_GRAY, VGA_BLACK);

    wait_for_esc(false);
    console_clear_region();
}

/* --- App 2: text-mode windowing environment -------------------------------- */
#define ICON_GEAR  ((char) 0x0F) /* CP437 sun/gear glyph */
#define ICON_FILE  ((char) 0xFE) /* CP437 small filled square */
#define ICON_GLOBE ((char) 0x09) /* CP437 circle */

static u32 g_gui_last_secs = 0xFFFFFFFFu;

static void gui_update_clock(bool force)
{
    u32 secs = uptime_secs();
    if (!force && secs == g_gui_last_secs) {
        return;
    }
    g_gui_last_secs = secs;

    char hms[12];
    fmt_hms(hms, secs);
    console_write_at(VGA_WIDTH - 10, SHORTCUT_ROW, hms, VGA_BLACK, VGA_LIGHT_GRAY);
}

static void gui_draw(void)
{
    /* Wallpaper: subtle shaded blue desktop across rows 0..23. */
    for (int y = 0; y < SHORTCUT_ROW; ++y) {
        console_fill_row(y, CH_SHADE, VGA_LIGHT_BLUE, VGA_BLUE);
    }

    /* Centered window. */
    int w = 40;
    int h = 13;
    int x = (VGA_WIDTH - w) / 2;
    int y = 4;
    draw_box(x, y, w, h, VGA_WHITE, VGA_LIGHT_GRAY, true);

    /* Title bar. */
    for (int i = 1; i < w - 1; ++i) {
        console_put_at(x + i, y + 1, ' ', VGA_WHITE, VGA_BLUE);
    }
    console_write_at(x + 2, y + 1, "[ X ] Mi Computadora", VGA_WHITE, VGA_BLUE);

    /* Icon list. */
    console_put_at(x + 4, y + 4, '(', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 6, y + 4, ')', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 5, y + 4, ICON_GEAR, VGA_BROWN, VGA_LIGHT_GRAY);
    console_write_at(x + 8, y + 4, "Sistema", VGA_BLACK, VGA_LIGHT_GRAY);

    console_put_at(x + 4, y + 6, '(', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 6, y + 6, ')', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 5, y + 6, ICON_FILE, VGA_BLUE, VGA_LIGHT_GRAY);
    console_write_at(x + 8, y + 6, "Archivos", VGA_BLACK, VGA_LIGHT_GRAY);

    console_put_at(x + 4, y + 8, '(', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 6, y + 8, ')', VGA_BLACK, VGA_LIGHT_GRAY);
    console_put_at(x + 5, y + 8, ICON_GLOBE, VGA_GREEN, VGA_LIGHT_GRAY);
    console_write_at(x + 8, y + 8, "Internet", VGA_BLACK, VGA_LIGHT_GRAY);

    console_write_at(x + 2, y + h - 2, "Arrowix Desktop (simulado)",
                     VGA_DARK_GRAY, VGA_LIGHT_GRAY);

    /* Taskbar (row 24). */
    console_fill_row(SHORTCUT_ROW, ' ', VGA_BLACK, VGA_LIGHT_GRAY);
    console_write_at(1, SHORTCUT_ROW, " Inicio ", VGA_WHITE, VGA_CYAN);
    console_write_at(11, SHORTCUT_ROW, "ESC: cerrar el entorno grafico",
                     VGA_BLACK, VGA_LIGHT_GRAY);
    gui_update_clock(true);
}

static void cmd_gui(void)
{
    taskbar_set_enabled(false); /* gui owns row 24 with its own taskbar */
    console_clear();
    gui_draw();
    wait_for_esc(true);
    restore_shell_frame();
}

static void wait_for_esc(bool gui_taskbar)
{
    for (;;) {
        __asm__ volatile("hlt");
        if (gui_taskbar) {
            gui_update_clock(false);
        } else {
            update_clock(false);
        }

        bool stop = false;
        int k;
        while ((k = keyboard_getchar()) != 0) {
            if (k == KEY_ESC) {
                stop = true;
            }
        }
        if (stop) {
            return;
        }
    }
}

/* --- App 3: full-screen "Arrowix Browser" (chrome) ------------------------- */
#define CH_REFRESH ((char) 0x12) /* CP437 up/down arrow, used as reload glyph */

/* Search field geometry (an input box rendered with a white background). */
#define CHROME_SEARCH_X 13
#define CHROME_SEARCH_Y 14
#define CHROME_SEARCH_W 54
#define CHROME_SEARCH_INNER (CHROME_SEARCH_W - 4)

static char g_chrome_search[CHROME_SEARCH_INNER + 1];
static int g_chrome_search_len;

static void chrome_draw_frame(void)
{
    console_clear();

    /* Full-screen window border. */
    draw_box(0, 0, VGA_WIDTH, VGA_HEIGHT, VGA_LIGHT_CYAN, VGA_BLACK, false);
    console_write_at(31, 0, " Arrowix Browser ", VGA_BLACK, VGA_LIGHT_CYAN);

    /* Navigation buttons. */
    console_write_at(2, 2, "[<] [>] [", VGA_WHITE, VGA_BLACK);
    console_put_at(11, 2, CH_REFRESH, VGA_LIGHT_GREEN, VGA_BLACK);
    console_put_at(12, 2, ']', VGA_WHITE, VGA_BLACK);

    /* Address bar drawn with box characters. */
    draw_box(15, 1, 63, 3, VGA_WHITE, VGA_BLACK, true);
    console_write_at(17, 2, "https://google.com", VGA_LIGHT_CYAN, VGA_BLACK);

    console_write_at(2, VGA_HEIGHT - 2, "ENTER: navegar      ESC: salir",
                     VGA_DARK_GRAY, VGA_BLACK);
}

static void chrome_loading(void)
{
    const char *msg = "[Conectando a los servidores de Google...]";
    int x = 2;
    int y = VGA_HEIGHT - 2;
    int len = (int) strlen(msg);

    /* Blink ~5 times (~1 s total) to simulate network latency via the PIT. */
    for (int cycle = 0; cycle < 5; ++cycle) {
        console_write_at(x, y, msg, VGA_YELLOW, VGA_BLACK);
        pit_sleep_ms(100);
        for (int i = 0; i < len; ++i) {
            console_put_at(x + i, y, ' ', VGA_BLACK, VGA_BLACK);
        }
        pit_sleep_ms(100);
    }
}

static void chrome_render_search(void)
{
    int x = CHROME_SEARCH_X;
    int y = CHROME_SEARCH_Y;

    /* Clear the input line (white "field" background). */
    for (int i = 1; i < CHROME_SEARCH_W - 1; ++i) {
        console_put_at(x + i, y + 1, ' ', VGA_BLACK, VGA_WHITE);
    }

    if (g_chrome_search_len == 0) {
        console_write_at(x + 2, y + 1, "Buscar en internet o escribir URL...",
                         VGA_DARK_GRAY, VGA_WHITE);
    } else {
        for (int i = 0; i < g_chrome_search_len; ++i) {
            console_put_at(x + 2 + i, y + 1, g_chrome_search[i], VGA_BLACK, VGA_WHITE);
        }
    }

    /* Fake text cursor (only once the user starts typing, to keep the
     * placeholder readable). */
    if (g_chrome_search_len > 0 && g_chrome_search_len < CHROME_SEARCH_INNER) {
        console_put_at(x + 2 + g_chrome_search_len, y + 1, '_', VGA_LIGHT_RED, VGA_WHITE);
    }
}

static void chrome_render_page(void)
{
    /* Clear the content area, preserving the window frame and address bar. */
    for (int y = 5; y <= VGA_HEIGHT - 3; ++y) {
        for (int x = 1; x < VGA_WIDTH - 1; ++x) {
            console_put_at(x, y, ' ', VGA_WHITE, VGA_BLACK);
        }
    }

    draw_google_logo(6);

    /* Search bar (interactive input field): gray frame, white "paper" interior. */
    draw_box(CHROME_SEARCH_X, CHROME_SEARCH_Y, CHROME_SEARCH_W, 3, VGA_DARK_GRAY, VGA_WHITE, true);
    chrome_render_search();

    /* Bottom status bar. */
    for (int x = 1; x < VGA_WIDTH - 1; ++x) {
        console_put_at(x, VGA_HEIGHT - 2, ' ', VGA_WHITE, VGA_BLUE);
    }
    console_write_at(2, VGA_HEIGHT - 2,
                     "Arrowix Browser v0.1 - Conectado mediante emulacion de nucleo seguro",
                     VGA_WHITE, VGA_BLUE);
}

/* The demo fallback: a fully self-contained, simulated Chrome window. */
static void display_simulated_chrome_browser(void)
{
    taskbar_set_enabled(false); /* chrome takes over the whole screen */
    g_chrome_search_len = 0;
    g_chrome_search[0] = '\0';

    chrome_draw_frame();
    bool loaded = false;

    for (;;) {
        __asm__ volatile("hlt");

        int k;
        while ((k = keyboard_getchar()) != 0) {
            if (k == KEY_ESC) {
                restore_shell_frame();
                return;
            }

            if (!loaded) {
                if (k == '\n') {
                    chrome_loading();
                    chrome_render_page();
                    loaded = true;
                }
                continue;
            }

            /* Loaded: the search field is interactive. */
            if (k == '\n') {
                g_chrome_search_len = 0;
                chrome_loading();
                chrome_render_page();
            } else if (k == '\b') {
                if (g_chrome_search_len > 0) {
                    --g_chrome_search_len;
                    g_chrome_search[g_chrome_search_len] = '\0';
                    chrome_render_search();
                }
            } else if (k >= 32 && k < 127 && g_chrome_search_len < CHROME_SEARCH_INNER) {
                g_chrome_search[g_chrome_search_len++] = (char) k;
                g_chrome_search[g_chrome_search_len] = '\0';
                chrome_render_search();
            }
        }
    }
}

/*
 * Capability probes. Arrowix has no NIC/GPU stack or ELF loader yet, so these
 * report false today and launch_chrome() falls back to the simulated browser.
 * They are the seams where the real drivers/ABI will plug in later.
 */
static bool has_network_driver(void)
{
    return false;
}

static bool has_graphics_driver(void)
{
    return false;
}

static void execute_external_elf(const char *path)
{
    /* TODO(phase 7+): map the ELF, set up the user ABI, and jump to it. */
    console_set_color(VGA_LIGHT_RED, VGA_BLACK);
    kprintf("[exec] '%s' no soportado todavia (sin cargador ELF/ABI).\n", path);
    console_reset_color();
}

static void launch_chrome(void)
{
    if (has_network_driver() && has_graphics_driver()) {
        /* Real path (future): load and run the actual Chrome binary. */
        execute_external_elf("/usr/bin/chrome");
    } else {
        /* Immediate fallback for the current demo. */
        display_simulated_chrome_browser();
    }
}

/* --- Command line ---------------------------------------------------------- */
#define LINE_MAX 256
static char g_line[LINE_MAX];
static int g_len;

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    return s;
}

static void process_command(const char *raw)
{
    const char *p = skip_spaces(raw);

    if (*p == '\0') {
        return; /* empty input: just show a fresh prompt */
    }

    /* Split the first whitespace-delimited token (the verb) from its arguments. */
    static char verb[32];
    int n = 0;
    while (p[n] != '\0' && p[n] != ' ' && p[n] != '\t' && n < (int) sizeof(verb) - 1) {
        verb[n] = p[n];
        ++n;
    }
    verb[n] = '\0';
    const char *args = skip_spaces(p + n);

    if (strcmp(verb, "help") == 0) {
        cmd_help();
    } else if (strcmp(verb, "about") == 0) {
        cmd_about();
    } else if (strcmp(verb, "sysinfo") == 0) {
        cmd_sysinfo();
    } else if (strcmp(verb, "clear") == 0) {
        console_clear_region();
    } else if (strcmp(verb, "matrix") == 0) {
        cmd_matrix();
    } else if (strcmp(verb, "browser") == 0) {
        cmd_browser(args);
    } else if (strcmp(verb, "gui") == 0) {
        cmd_gui();
    } else if (strcmp(verb, "chrome") == 0) {
        launch_chrome();
    } else {
        console_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("Comando no reconocido: '%s'.", verb);
        console_reset_color();
        kputs(" Escribe 'help'.\n");
    }
}

static void run_builtin_sysinfo(void)
{
    kputc('\n');
    cmd_sysinfo();
    g_len = 0;
    draw_prompt();
}

static void handle_key(int k)
{
    /* F1 acts as the "Windows key": toggle the Start Menu over the taskbar. */
    if (k == KEY_F1) {
        taskbar_toggle_start_menu();
        return;
    }

    /* Any other key dismisses an open Start Menu before being processed. */
    if (taskbar_start_menu_open()) {
        taskbar_close_start_menu();
        if (k == KEY_ESC) {
            return;
        }
    }

    switch (k) {
    case KEY_F2:
        console_clear_region();
        g_len = 0;
        draw_prompt();
        return;
    case KEY_F3:
        run_builtin_sysinfo();
        return;
    case KEY_ESC:
        return; /* nothing to cancel at the prompt */
    case '\n':
        kputc('\n');
        g_line[g_len] = '\0';
        process_command(g_line);
        g_len = 0;
        draw_prompt();
        return;
    case '\b':
        if (g_len > 0) {
            --g_len;
            kputc('\b');
        }
        return;
    case '\t':
        return;
    default:
        if (k >= 32 && k < 127 && g_len < LINE_MAX - 1) {
            g_line[g_len++] = (char) k;
            kputc((char) k);
        }
        return;
    }
}

static void print_welcome(void)
{
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kputs("Bienvenido a Arrowix Shell.\n");
    console_reset_color();
    kputs("Escribe ");
    console_set_color(VGA_YELLOW, VGA_BLACK);
    kputs("help");
    console_reset_color();
    kputs(" para ver los comandos, o pulsa F1 (menu Inicio).\n\n");
}

void shell_run(void)
{
    boot_splash();

    console_set_region(REGION_TOP, REGION_BOTTOM);
    console_reset_color();
    console_clear_region();
    draw_status_bar();
    taskbar_set_enabled(true); /* draw the persistent taskbar on row 24 */
    update_clock(true);

    print_welcome();

    /* Discard any keystrokes captured during the splash animation. */
    while (keyboard_getchar() != 0) {
    }

    draw_prompt();

    for (;;) {
        __asm__ volatile("hlt"); /* sleep until the next IRQ (timer/keyboard) */
        update_clock(false);
        update_taskbar(); /* also refreshed from the PIT tick for accuracy */

        int k;
        while ((k = keyboard_getchar()) != 0) {
            handle_key(k);
        }
    }
}
