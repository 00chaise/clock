#include "core.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
    printf(
"clock - widget horloge / pomodoro / alarme\n"
"\n"
"Usage : %s [options]\n"
"\n"
"Mode :\n"
"  -t, --tui            affiche dans le terminal au lieu du widget X11\n"
"  -p, --pomodoro       demarre directement une session pomodoro\n"
"\n"
"Pomodoro :\n"
"      --work MIN       duree de travail          (defaut 25)\n"
"      --break MIN      pause courte              (defaut 5)\n"
"      --long MIN       pause longue              (defaut 15)\n"
"      --rounds N       sessions avant la longue  (defaut 4)\n"
"\n"
"Alarmes :\n"
"  -a, --alarm HH:MM[=texte]   repetable, jusqu'a %d alarmes\n"
"\n"
"Affichage :\n"
"      --size WxH       taille du widget          (defaut 360x170)\n"
"      --pos X,Y        position a l'ecran\n"
"      --decorated      garde la barre de titre du gestionnaire de fenetres\n"
"      --no-above       n'impose pas la fenetre au premier plan\n"
"      --opacity F      opacite entre 0.1 et 1.0  (necessite un compositeur)\n"
"      --12h            affichage 12 heures\n"
"      --no-seconds     masque les secondes\n"
"  -h, --help           cette aide\n"
"\n"
"Touches : espace demarrer/pause, n suivant, r reset, m mode,\n"
"          s secondes, q ou Echap quitter.\n"
"Souris (widget) : clic gauche pour deplacer, clic droit pour changer de mode.\n",
        prog, MAX_ALARMS);
}

static bool parse_int_arg(const char *v, int *out, int lo, int hi)
{
    char *end = NULL;
    long n = strtol(v, &end, 10);
    if (!end || *end != '\0' || n < lo || n > hi)
        return false;
    *out = (int)n;
    return true;
}

int main(int argc, char **argv)
{
    App app;
    app_init(&app);

    UiOptions ui = {
        .width = 360, .height = 170,
        .x = 0, .y = 0, .has_pos = false,
        .decorated = false, .above = true, .opacity = 1.0
    };
    bool tui = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;

        #define NEED_VALUE() do { \
            if (!next) { \
                fprintf(stderr, "clock: %s attend une valeur\n", arg); \
                return 2; \
            } \
        } while (0)

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(arg, "-t") || !strcmp(arg, "--tui")) {
            tui = true;
        } else if (!strcmp(arg, "-p") || !strcmp(arg, "--pomodoro")) {
            app.mode = MODE_POMODORO;
        } else if (!strcmp(arg, "--work")) {
            NEED_VALUE();
            int m;
            if (!parse_int_arg(next, &m, 1, 600)) {
                fprintf(stderr, "clock: --work attend des minutes (1-600)\n");
                return 2;
            }
            app.work_len = m * 60;
            i++;
        } else if (!strcmp(arg, "--break")) {
            NEED_VALUE();
            int m;
            if (!parse_int_arg(next, &m, 1, 600)) {
                fprintf(stderr, "clock: --break attend des minutes (1-600)\n");
                return 2;
            }
            app.short_len = m * 60;
            i++;
        } else if (!strcmp(arg, "--long")) {
            NEED_VALUE();
            int m;
            if (!parse_int_arg(next, &m, 1, 600)) {
                fprintf(stderr, "clock: --long attend des minutes (1-600)\n");
                return 2;
            }
            app.long_len = m * 60;
            i++;
        } else if (!strcmp(arg, "--rounds")) {
            NEED_VALUE();
            int n;
            if (!parse_int_arg(next, &n, 1, 99)) {
                fprintf(stderr, "clock: --rounds attend un nombre (1-99)\n");
                return 2;
            }
            app.rounds_before_long = n;
            i++;
        } else if (!strcmp(arg, "-a") || !strcmp(arg, "--alarm")) {
            NEED_VALUE();
            if (!app_add_alarm(&app, next)) {
                fprintf(stderr, "clock: alarme invalide '%s' "
                                "(attendu HH:MM ou HH:MM=texte)\n", next);
                return 2;
            }
            i++;
        } else if (!strcmp(arg, "--size")) {
            NEED_VALUE();
            int w, h;
            if (sscanf(next, "%dx%d", &w, &h) != 2 || w < 140 || h < 70) {
                fprintf(stderr, "clock: --size attend WxH (minimum 140x70)\n");
                return 2;
            }
            ui.width = w;
            ui.height = h;
            i++;
        } else if (!strcmp(arg, "--pos")) {
            NEED_VALUE();
            int x, y;
            if (sscanf(next, "%d,%d", &x, &y) != 2) {
                fprintf(stderr, "clock: --pos attend X,Y\n");
                return 2;
            }
            ui.x = x;
            ui.y = y;
            ui.has_pos = true;
            i++;
        } else if (!strcmp(arg, "--decorated")) {
            ui.decorated = true;
        } else if (!strcmp(arg, "--no-above")) {
            ui.above = false;
        } else if (!strcmp(arg, "--opacity")) {
            NEED_VALUE();
            double f = atof(next);
            if (f < 0.1 || f > 1.0) {
                fprintf(stderr, "clock: --opacity attend une valeur entre 0.1 et 1.0\n");
                return 2;
            }
            ui.opacity = f;
            i++;
        } else if (!strcmp(arg, "--12h")) {
            app.use_24h = false;
        } else if (!strcmp(arg, "--no-seconds")) {
            app.show_seconds = false;
        } else {
            fprintf(stderr, "clock: option inconnue '%s' (--help pour l'aide)\n", arg);
            return 2;
        }
        #undef NEED_VALUE
    }

    // Les durees peuvent avoir change apres app_init. 
    if (app.phase == PH_IDLE) {
        app.total = app.work_len;
        app.remaining = app.work_len;
    }
    if (app.mode == MODE_POMODORO)
        app_start_phase(&app, PH_WORK);

    return tui ? ui_tui_run(&app) : ui_x11_run(&app, &ui);
}
