#ifndef EYES_H
#define EYES_H

#include <stdbool.h>

/* De temps en temps, le widget se rappelle a toi : deux yeux s'ouvrent au
 * milieu de l'affichage, font quelque chose, puis se referment et l'heure
 * revient. Le moment est tire au sort pour que ca reste une surprise : une
 * apparition reguliere se ferait oublier au bout de quelques heures. */

typedef enum {
    EYE_NONE = 0,
    EYE_LOOK,    /* le regard balaie de gauche a droite */
    EYE_WINK,    /* clin d'oeil de l'oeil droit */
    EYE_SMILE,   /* les yeux se plissent en deux arcs */
    EYE_COUNT
} EyeAnim;

/* Intervalle entre deux apparitions, en millisecondes. */
#define EYES_MIN_GAP_MS (10L * 60L * 1000L)
#define EYES_MAX_GAP_MS (30L * 60L * 1000L)

typedef struct {
    EyeAnim  anim;        /* animation en cours, EYE_NONE si repos */
    EyeAnim  last;        /* la precedente, pour ne pas la rejouer aussitot */
    long     started_ms;
    long     next_ms;     /* date de la prochaine apparition */
    unsigned seed;
} Eyes;

/* Ce qu'il faut dessiner a un instant donne. */
typedef struct {
    double open_l, open_r;  /* ouverture de chaque oeil, 0 ferme .. 1 ouvert */
    double gaze;            /* direction du regard, -1 gauche .. +1 droite */
    double smile;           /* 0 oeil rond .. 1 arc souriant */
    double presence;        /* 0 pas d'yeux .. 1 yeux a taille reelle */
} EyeFrame;

void eyes_init(Eyes *e, long now_ms);

/* Fait avancer l'animation et declenche la suivante le moment venu. Quand
 * busy est vrai (alarme qui clignote, fenetre en cours de deplacement) on
 * repousse l'apparition de quelques secondes plutot que de la sauter. */
void eyes_update(Eyes *e, long now_ms, bool busy);

/* Declenche l'animation suivante immediatement, pour les essais. */
void eyes_trigger(Eyes *e, long now_ms);

bool eyes_active(const Eyes *e);

/* Remplit out pour l'instant courant, ou renvoie false s'il n'y a rien a
 * dessiner. */
bool eyes_frame(const Eyes *e, long now_ms, EyeFrame *out);

#endif
