#include "segments.h"

unsigned char seg_mask(char c)
{
    switch (c) {
    case '0': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F;
    case '1': return SEG_B | SEG_C;
    case '2': return SEG_A | SEG_B | SEG_G | SEG_E | SEG_D;
    case '3': return SEG_A | SEG_B | SEG_G | SEG_C | SEG_D;
    case '4': return SEG_F | SEG_G | SEG_B | SEG_C;
    case '5': return SEG_A | SEG_F | SEG_G | SEG_C | SEG_D;
    case '6': return SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D;
    case '7': return SEG_A | SEG_B | SEG_C;
    case '8': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
    case '9': return SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G;
    case '-': return SEG_G;
    default:  return 0;
    }
}

bool seg_is_colon(char c)
{
    return c == ':';
}

void seg_rects(int x, int y, int w, int h, int t, SegRect out[7])
{
    int mid = y + (h - t) / 2;      /* haut du segment G */
    int upper = mid - (y + t);      /* hauteur des verticales du haut */
    int lower = (y + h - t) - (mid + t);

    if (upper < 0) upper = 0;
    if (lower < 0) lower = 0;

    out[0] = (SegRect){ x + t,     y,         w - 2 * t, t     }; /* A */
    out[1] = (SegRect){ x + w - t, y + t,     t,         upper }; /* B */
    out[2] = (SegRect){ x + w - t, mid + t,   t,         lower }; /* C */
    out[3] = (SegRect){ x + t,     y + h - t, w - 2 * t, t     }; /* D */
    out[4] = (SegRect){ x,         mid + t,   t,         lower }; /* E */
    out[5] = (SegRect){ x,         y + t,     t,         upper }; /* F */
    out[6] = (SegRect){ x + t,     mid,       w - 2 * t, t     }; /* G */
}

void seg_colon_rects(int x, int y, int w, int h, int t, SegRect out[2])
{
    int cx = x + (w - t) / 2;

    out[0] = (SegRect){ cx, y + h / 4 - t / 2,     t, t };
    out[1] = (SegRect){ cx, y + 3 * h / 4 - t / 2, t, t };
}
