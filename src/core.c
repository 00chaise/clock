#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void app_init(App *a)
{
    memset(a, 0, sizeof(*a));
    a->mode = MODE_CLOCK;
    a->phase = PH_IDLE;
    a->running = false;
    a->work_len = 25 * 60;
    a->short_len = 5 * 60;
    a->long_len = 15 * 60;
    a->rounds_before_long = 4;
    a->show_seconds = true;
    a->use_24h = true;
    a->remaining = a->work_len;
    a->total = a->work_len;
    snprintf(a->status, sizeof(a->status), "pret");
}

const char *phase_name(Phase p)
{
    switch (p) {
    case PH_WORK:  return "TRAVAIL";
    case PH_SHORT: return "PAUSE";
    case PH_LONG:  return "GRANDE PAUSE";
    default:       return "POMODORO";
    }
}

static int phase_len(const App *a, Phase p)
{
    switch (p) {
    case PH_WORK:  return a->work_len;
    case PH_SHORT: return a->short_len;
    case PH_LONG:  return a->long_len;
    default:       return a->work_len;
    }
}

void app_notify(App *a, const char *title, const char *body)
{
    a->flash = true;
    a->flash_until = time(NULL) + 5;
    snprintf(a->status, sizeof(a->status), "%s", body);

    /* notify-send en fils detache : l'horloge ne doit jamais bloquer. */
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            execlp("notify-send", "notify-send", "-a", "clock", title, body, (char *)NULL);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }

    /* Cloche du terminal : audible meme sans serveur de son. */
    if (write(STDERR_FILENO, "\a", 1) < 0) {
        /* rien a faire, la notification visuelle suffit */
    }
}

void app_start_phase(App *a, Phase p)
{
    a->phase = p;
    a->total = phase_len(a, p);
    a->remaining = a->total;
    a->deadline = time(NULL) + a->total;
    a->running = true;
    snprintf(a->status, sizeof(a->status), "%s en cours", phase_name(p));
}

static void advance_phase(App *a)
{
    if (a->phase == PH_WORK) {
        a->round++;
        if (a->round % a->rounds_before_long == 0)
            app_start_phase(a, PH_LONG);
        else
            app_start_phase(a, PH_SHORT);
        app_notify(a, "Pomodoro", "Session terminee, c'est la pause");
    } else {
        app_start_phase(a, PH_WORK);
        app_notify(a, "Pomodoro", "Pause terminee, au boulot");
    }
}

void app_toggle(App *a)
{
    if (a->mode != MODE_POMODORO)
        return;

    if (a->phase == PH_IDLE) {
        app_start_phase(a, PH_WORK);
        return;
    }

    if (a->running) {
        a->remaining = (int)(a->deadline - time(NULL));
        if (a->remaining < 0)
            a->remaining = 0;
        a->running = false;
        snprintf(a->status, sizeof(a->status), "%s en pause", phase_name(a->phase));
    } else {
        a->deadline = time(NULL) + a->remaining;
        a->running = true;
        snprintf(a->status, sizeof(a->status), "%s en cours", phase_name(a->phase));
    }
}

void app_reset(App *a)
{
    a->phase = PH_IDLE;
    a->running = false;
    a->round = 0;
    a->total = a->work_len;
    a->remaining = a->work_len;
    snprintf(a->status, sizeof(a->status), "remis a zero");
}

void app_skip(App *a)
{
    if (a->mode != MODE_POMODORO)
        return;
    if (a->phase == PH_IDLE) {
        app_start_phase(a, PH_WORK);
        return;
    }
    advance_phase(a);
}

static void check_alarms(App *a, time_t now)
{
    struct tm tm;
    localtime_r(&now, &tm);

    for (int i = 0; i < a->n_alarms; i++) {
        Alarm *al = &a->alarms[i];
        if (!al->enabled)
            continue;
        if (al->last_fired_yday == tm.tm_yday)
            continue;
        if (tm.tm_hour != al->hour || tm.tm_min != al->min)
            continue;

        al->last_fired_yday = tm.tm_yday;

        char body[96];
        if (al->label[0])
            snprintf(body, sizeof(body), "%02d:%02d  %s", al->hour, al->min, al->label);
        else
            snprintf(body, sizeof(body), "Il est %02d:%02d", al->hour, al->min);
        app_notify(a, "Alarme", body);
    }
}

void app_tick(App *a, time_t now)
{
    if (a->flash && now >= a->flash_until)
        a->flash = false;

    check_alarms(a, now);

    if (a->mode != MODE_POMODORO || !a->running)
        return;

    a->remaining = (int)(a->deadline - now);
    if (a->remaining <= 0) {
        a->remaining = 0;
        advance_phase(a);
    }
}

void app_key(App *a, int key)
{
    switch (key) {
    case ' ':
        if (a->mode == MODE_CLOCK) {
            a->mode = MODE_POMODORO;
            app_start_phase(a, PH_WORK);
        } else {
            app_toggle(a);
        }
        break;
    case 'm': case 'M':
        a->mode = (a->mode == MODE_CLOCK) ? MODE_POMODORO : MODE_CLOCK;
        snprintf(a->status, sizeof(a->status), "%s",
                 a->mode == MODE_CLOCK ? "horloge" : "pomodoro");
        break;
    case 'n': case 'N':
        app_skip(a);
        break;
    case 'r': case 'R':
        app_reset(a);
        break;
    case 's': case 'S':
        a->show_seconds = !a->show_seconds;
        break;
    case 'q': case 'Q':
        a->quit = true;
        break;
    default:
        break;
    }
}

bool app_add_alarm(App *a, const char *spec)
{
    if (a->n_alarms >= MAX_ALARMS)
        return false;

    int h = -1, m = -1;
    char label[32] = { 0 };

    /* Formats acceptes : "7:30", "07:30", "07:30=cafe" */
    const char *eq = strchr(spec, '=');
    char timepart[16];
    size_t len = eq ? (size_t)(eq - spec) : strlen(spec);
    if (len >= sizeof(timepart))
        return false;
    memcpy(timepart, spec, len);
    timepart[len] = '\0';

    if (sscanf(timepart, "%d:%d", &h, &m) != 2)
        return false;
    if (h < 0 || h > 23 || m < 0 || m > 59)
        return false;
    if (eq)
        snprintf(label, sizeof(label), "%s", eq + 1);

    Alarm *al = &a->alarms[a->n_alarms++];
    al->hour = h;
    al->min = m;
    al->enabled = true;
    al->last_fired_yday = -1;
    snprintf(al->label, sizeof(al->label), "%s", label);
    return true;
}

double app_progress(const App *a)
{
    if (a->mode != MODE_POMODORO || a->total <= 0 || a->phase == PH_IDLE)
        return 0.0;
    double p = 1.0 - (double)a->remaining / (double)a->total;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}

void app_display(const App *a, time_t now, char *out, size_t n)
{
    if (a->mode == MODE_CLOCK) {
        struct tm tm;
        localtime_r(&now, &tm);
        int h = tm.tm_hour;
        if (!a->use_24h) {
            h = h % 12;
            if (h == 0) h = 12;
        }
        if (a->show_seconds)
            snprintf(out, n, "%02d:%02d:%02d", h, tm.tm_min, tm.tm_sec);
        else
            snprintf(out, n, "%02d:%02d", h, tm.tm_min);
        return;
    }

    int s = a->remaining;
    if (s < 0) s = 0;
    if (s >= 3600)
        snprintf(out, n, "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    else
        snprintf(out, n, "%02d:%02d", s / 60, s % 60);
}

void app_subtitle(const App *a, time_t now, char *out, size_t n)
{
    if (a->mode == MODE_CLOCK) {
        struct tm tm;
        localtime_r(&now, &tm);
        char date[64];
        strftime(date, sizeof(date), "%a %d %b", &tm);
        if (a->n_alarms > 0) {
            snprintf(out, n, "%s  -  %d alarme%s",
                     date, a->n_alarms, a->n_alarms > 1 ? "s" : "");
        } else {
            snprintf(out, n, "%s", date);
        }
        return;
    }

    if (a->phase == PH_IDLE) {
        snprintf(out, n, "POMODORO  -  espace pour demarrer");
        return;
    }
    snprintf(out, n, "%s  -  serie %d  -  %s",
             phase_name(a->phase), a->round + 1,
             a->running ? "en cours" : "PAUSE");
}
