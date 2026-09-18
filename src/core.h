#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define MAX_ALARMS 16

typedef enum {
    MODE_CLOCK,
    MODE_POMODORO
} AppMode;

typedef enum {
    PH_IDLE,
    PH_WORK,
    PH_SHORT,
    PH_LONG
} Phase;

typedef struct {
    int  hour, min;
    bool enabled;
    int  last_fired_yday;   /* -1 tant que l'alarme n'a pas sonne aujourd'hui */
    char label[32];
} Alarm;

typedef struct {
    AppMode mode;
    Phase   phase;
    bool    running;

    int    total;       /* duree de la phase courante, en secondes */
    int    remaining;   /* secondes restantes */
    time_t deadline;    /* echeance absolue quand running (evite la derive) */
    int    round;       /* sessions de travail terminees */

    int work_len, short_len, long_len, rounds_before_long;

    bool show_seconds;
    bool use_24h;

    Alarm alarms[MAX_ALARMS];
    int   n_alarms;

    /* Signalement d'evenement, consomme par l'interface. */
    bool   flash;
    time_t flash_until;
    char   status[80];

    bool quit;
} App;

void app_init(App *a);
void app_tick(App *a, time_t now);
void app_key(App *a, int key);

void app_start_phase(App *a, Phase p);
void app_toggle(App *a);
void app_reset(App *a);
void app_skip(App *a);

bool app_add_alarm(App *a, const char *spec);

/* Ecrit l'heure ou le decompte a afficher, p.ex. "14:07:33" ou "24:59". */
void app_display(const App *a, time_t now, char *out, size_t n);

/* Ligne de contexte sous les chiffres. */
void app_subtitle(const App *a, time_t now, char *out, size_t n);

const char *phase_name(Phase p);

/* Avancement de la phase entre 0.0 et 1.0 (0.0 en mode horloge). */
double app_progress(const App *a);

void app_notify(App *a, const char *title, const char *body);

#endif
