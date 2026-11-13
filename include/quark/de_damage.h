
#ifndef QUARK_DE_DAMAGE_H
#define QUARK_DE_DAMAGE_H

#include <quark/kernel.h>

#define DE_MAX_DAMAGE 64

typedef struct {
    int x, y;
    int w, h;
} de_rect_t;

/* Adds a rect to the damage list. Intersects against screen bounds.
 * Coalesces with existing rects if they overlap (union). */
void de_damage_add(int x, int y, int w, int h);

/* Adds a full-screen rect. Use sparingly. */
void de_damage_add_all(void);

/* Iterate: returns 1 and fills out_rect on each call, 0 when exhausted. */
int de_damage_next(de_rect_t* out_rect);

/* Clear the list. Call after finishing a frame's redraws. */
void de_damage_clear(void);

/* Number of rects pending. */
int de_damage_count(void);

/* Clip rect interface. All drawing primitives honour this. */
void de_clip_set(int x, int y, int w, int h);
void de_clip_reset(void);

#endif