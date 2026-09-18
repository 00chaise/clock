#ifndef SEGMENTS_H
#define SEGMENTS_H

#include <stdbool.h>

/* Affichage 7 segments :
 *
 *    AAA
 *   F   B
 *    GGG
 *   E   C
 *    DDD
 */
enum {
    SEG_A = 1 << 0,
    SEG_B = 1 << 1,
    SEG_C = 1 << 2,
    SEG_D = 1 << 3,
    SEG_E = 1 << 4,
    SEG_F = 1 << 5,
    SEG_G = 1 << 6
};

typedef struct {
    int x, y, w, h;
} SegRect;

/* Masque des segments allumes pour un caractere ('0'-'9', ' ', '-'). */
unsigned char seg_mask(char c);

/* Un ':' se dessine comme deux points, pas comme des segments. */
bool seg_is_colon(char c);

/* Remplit out[7] avec la geometrie des segments A..G d'un chiffre de taille
 * w x h place en (x, y), avec une epaisseur de trait t. */
void seg_rects(int x, int y, int w, int h, int t, SegRect out[7]);

/* Les deux points d'un ':' dans une cellule de taille w x h. */
void seg_colon_rects(int x, int y, int w, int h, int t, SegRect out[2]);

#endif
