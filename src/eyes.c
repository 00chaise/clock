#include "eyes.h"

#include <unistd.h>

/* Duree totale de chaque animation, en millisecondes. */
static long anim_duration(EyeAnim a)
{
    switch (a) {
    case EYE_LOOK:  return 3200;
    case EYE_WINK:  return 2600;
    case EYE_SMILE: return 3000;
    default:        return 0;
    }
}

/* Interpolation adoucie de a vers b pendant que t va de t0 a t1. Le lissage
 * evite les departs et les arrets secs : un oeil ne s'ouvre pas a vitesse
 * constante. */
static double ramp(long t, long t0, long t1, double a, double b)
{
    if (t1 <= t0)
        return b;
    if (t <= t0)
        return a;
    if (t >= t1)
        return b;
    double u = (double)(t - t0) / (double)(t1 - t0);
    u = u * u * (3.0 - 2.0 * u);
    return a + (b - a) * u;
}

/* Generateur maison : on ne touche pas a rand(), qui appartient au reste du
 * programme. */
static unsigned rnd(unsigned *s)
{
    *s = *s * 1103515245u + 12345u;
    return *s >> 1;
}

static void schedule_next(Eyes *e, long now_ms)
{
    long span = EYES_MAX_GAP_MS - EYES_MIN_GAP_MS;
    e->next_ms = now_ms + EYES_MIN_GAP_MS +
                 (long)(rnd(&e->seed) % (unsigned long)(span + 1));
}

static void start_anim(Eyes *e, long now_ms, EyeAnim which)
{
    if (which == EYE_NONE) {
        int choices = EYE_COUNT - EYE_LOOK;
        EyeAnim pick;
        do {
            pick = (EyeAnim)(EYE_LOOK + (int)(rnd(&e->seed) % (unsigned)choices));
        } while (pick == e->last && choices > 1);
        which = pick;
    }
    e->anim = which;
    e->last = which;
    e->started_ms = now_ms;
}

void eyes_init(Eyes *e, long now_ms)
{
    e->anim = EYE_NONE;
    e->last = EYE_NONE;
    e->started_ms = 0;
    e->seed = (unsigned)(now_ms ^ ((long)getpid() << 11)) | 1u;
    schedule_next(e, now_ms);
}

void eyes_trigger(Eyes *e, long now_ms)
{
    /* On avance dans l'ordre plutot qu'au hasard : appuyer trois fois sur la
     * touche montre les trois animations. */
    EyeAnim next = (EyeAnim)(e->last + 1);
    if (next <= EYE_NONE || next >= EYE_COUNT)
        next = EYE_LOOK;
    start_anim(e, now_ms, next);
}

void eyes_update(Eyes *e, long now_ms, bool busy)
{
    if (e->anim != EYE_NONE) {
        if (now_ms - e->started_ms >= anim_duration(e->anim)) {
            e->anim = EYE_NONE;
            schedule_next(e, now_ms);
        }
        return;
    }

    if (now_ms < e->next_ms)
        return;

    if (busy) {
        e->next_ms = now_ms + 5000;
        return;
    }

    start_anim(e, now_ms, EYE_NONE);
}

bool eyes_active(const Eyes *e)
{
    return e->anim != EYE_NONE;
}

bool eyes_frame(const Eyes *e, long now_ms, EyeFrame *out)
{
    if (e->anim == EYE_NONE)
        return false;

    long d = anim_duration(e->anim);
    long t = now_ms - e->started_ms;
    if (t < 0)
        t = 0;
    if (t > d)
        t = d;

    out->open_l = 0.0;
    out->open_r = 0.0;
    out->gaze = 0.0;
    out->smile = 0.0;
    out->presence = 0.0;

    switch (e->anim) {
    case EYE_LOOK: {
        /* Les yeux apparaissent fermes, s'ouvrent, balaient a gauche puis a
         * droite, reviennent au centre et se referment. */
        out->presence = ramp(t, 0, 260, 0, 1) * ramp(t, 2940, 3200, 1, 0);
        double open = ramp(t, 180, 420, 0, 1) * ramp(t, 2700, 2940, 1, 0);
        out->open_l = open;
        out->open_r = open;
        if (t < 1150)
            out->gaze = ramp(t, 700, 1150, 0, -1);
        else if (t < 1450)
            out->gaze = -1.0;
        else if (t < 2050)
            out->gaze = ramp(t, 1450, 2050, -1, 1);
        else if (t < 2350)
            out->gaze = 1.0;
        else
            out->gaze = ramp(t, 2350, 2650, 1, 0);
        break;
    }
    case EYE_WINK: {
        out->presence = ramp(t, 0, 260, 0, 1) * ramp(t, 2340, 2600, 1, 0);
        double open = ramp(t, 180, 420, 0, 1) * ramp(t, 2100, 2340, 1, 0);
        double wink = 1.0;
        if (t >= 900 && t < 1100)
            wink = ramp(t, 900, 1100, 1, 0);
        else if (t >= 1100 && t < 1260)
            wink = 0.0;
        else if (t >= 1260 && t < 1460)
            wink = ramp(t, 1260, 1460, 0, 1);
        out->open_l = open;
        out->open_r = open * wink;
        break;
    }
    case EYE_SMILE: {
        /* Les yeux s'ouvrent, se ferment brievement, puis se rouvrent en
         * arcs : on sourit toujours un peu les yeux plisses. */
        out->presence = ramp(t, 0, 260, 0, 1) * ramp(t, 2740, 3000, 1, 0);
        double open = ramp(t, 180, 420, 0, 1) * ramp(t, 1000, 1200, 1, 0);
        out->open_l = open;
        out->open_r = open;
        out->smile = ramp(t, 1200, 1400, 0, 1) * ramp(t, 2300, 2500, 1, 0);
        break;
    }
    default:
        return false;
    }

    return out->presence > 0.001;
}
