#ifndef QUARK_DE_IMAGE_H
#define QUARK_DE_IMAGE_H

#include <quark/kernel.h>

/* Pixels are 0xAARRGGBB, premultiplied is NOT used. Alpha is applied at
 * blit time against whatever is in the backbuffer. */
typedef struct {
    uint16_t w, h;
    const uint32_t* pixels;
} de_image_t;

/* Blit at native size. alpha is a global multiplier (0..255) on top of
 * per-pixel alpha, useful for fades. */
void de_image_blit(const de_image_t* img, int x, int y, uint8_t alpha);

/* Blit scaled to (dw, dh) using nearest-neighbor. Cheap but not smooth. */
void de_image_blit_scaled(const de_image_t* img, int x, int y,
                          int dw, int dh, uint8_t alpha);

/* Blit with rounded corners (radius in destination pixels). Uses
 * per-pixel coverage on the corners so edges are antialiased. */
void de_image_blit_rounded(const de_image_t* img, int x, int y,
                           int dw, int dh, int radius, uint8_t alpha);

#endif