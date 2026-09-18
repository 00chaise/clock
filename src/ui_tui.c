#include "ui.h"
#include "segments.h"

#include <ncurses.h>
#include <stdio.h>
#include <string.h>

/* La geometrie est calculee dans une grille virtuelle puis etiree x2 en
 * colonnes : une cellule terminal fait environ deux fois plus haut que large,
 * donc sans ca les chiffres seraient ecrases. */
#define XSCALE 2

enum { CP_ACCENT = 1, CP_WORK, CP_BREAK, CP_TEXT, CP_DIM };

static void fill_cells(const SegRect *r, int ox, int oy, int pair)
{
    if (r->w <= 0 || r->h <= 0)
        return;
    attron(COLOR_PAIR(pair) | A_REVERSE);
    for (int row = 0; row < r->h; row++) {
        for (int col = 0; col < r->w * XSCALE; col++)
            mvaddch(oy + r->y + row, ox + (r->x * XSCALE) + col, ' ');
    }
    attroff(COLOR_PAIR(pair) | A_REVERSE);
}

static void draw_digits(const char *s, int oy, int rows, int cols, int pair, int dim_pair)
{
    size_t len = strlen(s);
    if (len == 0)
        return;

    /* Largeur virtuelle, par unite d'echelle s. */
    int units = 0;
    for (size_t i = 0; i < len; i++)
        units += seg_is_colon(s[i]) ? 2 : 4;
    units += (int)(len - 1);   /* un espace d'une unite entre les caracteres */

    int s_by_w = (cols - 2) / (units * XSCALE);
    int s_by_h = rows / 7;
    int scale = s_by_w < s_by_h ? s_by_w : s_by_h;
    if (scale < 1)
        scale = 1;

    int dh = 7 * scale;
    int total_cols = units * scale * XSCALE;
    int ox = (cols - total_cols) / 2;
    if (ox < 0)
        ox = 0;
    int y = oy + (rows - dh) / 2;
    if (y < oy)
        y = oy;

    int x = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (seg_is_colon(c)) {
            SegRect dots[2];
            seg_colon_rects(x, 0, 2 * scale, dh, scale, dots);
            for (int k = 0; k < 2; k++)
                fill_cells(&dots[k], ox, y, pair);
            x += 2 * scale + scale;
            continue;
        }

        SegRect r[7];
        seg_rects(x, 0, 4 * scale, dh, scale, r);
        unsigned char mask = seg_mask(c);
        for (int k = 0; k < 7; k++)
            fill_cells(&r[k], ox, y, (mask & (1 << k)) ? pair : dim_pair);
        x += 4 * scale + scale;
    }
}

static int phase_pair(const App *a)
{
    if (a->mode == MODE_CLOCK)
        return CP_ACCENT;
    switch (a->phase) {
    case PH_WORK:  return CP_WORK;
    case PH_SHORT:
    case PH_LONG:  return CP_BREAK;
    default:       return CP_ACCENT;
    }
}

static void redraw(App *a, time_t now, int frame)
{
    erase();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int pair = phase_pair(a);
    if (a->flash && (frame / 3) % 2 == 0)
        pair = CP_TEXT;

    char text[32];
    app_display(a, now, text, sizeof(text));
    draw_digits(text, 1, rows - 5, cols, pair, CP_DIM);

    if (a->mode == MODE_POMODORO && a->phase != PH_IDLE) {
        int bw = cols - 4;
        int done = (int)(bw * app_progress(a));
        int by = rows - 4;
        attron(COLOR_PAIR(CP_DIM) | A_REVERSE);
        for (int i = 0; i < bw; i++)
            mvaddch(by, 2 + i, ' ');
        attroff(COLOR_PAIR(CP_DIM) | A_REVERSE);
        attron(COLOR_PAIR(pair) | A_REVERSE);
        for (int i = 0; i < done; i++)
            mvaddch(by, 2 + i, ' ');
        attroff(COLOR_PAIR(pair) | A_REVERSE);
    }

    char sub[96];
    app_subtitle(a, now, sub, sizeof(sub));
    attron(COLOR_PAIR(CP_TEXT) | A_BOLD);
    mvaddstr(rows - 2, (cols - (int)strlen(sub)) / 2, sub);
    attroff(COLOR_PAIR(CP_TEXT) | A_BOLD);

    const char *help = "espace demarrer/pause  n suivant  r reset  m mode  s secondes  q quitter";
    if ((int)strlen(help) < cols) {
        attron(COLOR_PAIR(CP_DIM));
        mvaddstr(rows - 1, (cols - (int)strlen(help)) / 2, help);
        attroff(COLOR_PAIR(CP_DIM));
    }

    refresh();
}

int ui_tui_run(App *a)
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(150);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_ACCENT, COLOR_CYAN, -1);
        init_pair(CP_WORK,   COLOR_RED, -1);
        init_pair(CP_BREAK,  COLOR_GREEN, -1);
        init_pair(CP_TEXT,   COLOR_YELLOW, -1);
        init_pair(CP_DIM,    COLOR_BLACK, -1);
    }

    int frame = 0;
    time_t last_tick = 0;

    while (!a->quit) {
        int ch = getch();
        if (ch == 27)              /* Echap */
            a->quit = true;
        else if (ch != ERR && ch < 128)
            app_key(a, ch);

        time_t now = time(NULL);
        if (now != last_tick) {
            app_tick(a, now);
            last_tick = now;
        }
        redraw(a, now, frame++);
    }

    endwin();
    return 0;
}
