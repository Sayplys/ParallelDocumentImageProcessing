#ifndef SAUVOLA_H
#define SAUVOLA_H

#include "image.h"

/*
 * Sauvola local thresholding for document image binarization.
 *
 * For each pixel (x, y) the threshold is:
 *
 *   T(x,y) = mean * (1 + k * (std / R - 1))
 *
 * where mean and std are computed over a square window of side `window_size`
 * centred on the pixel.  Pixels with intensity < T become foreground (0 /
 * black); the rest become background (255 / white).
 *
 */
typedef struct {
    int    window_size; /* side of the local window, should be odd (e.g. 15) */
    double k;           /* sensitivity – typical range [0.1, 0.5]            */
    double r;           /* dynamic range of std dev – 128 for 8-bit images   */
} SauvolaParams;

/*
 * Binarize a grayscale image.
 */
GrayImage *sauvola_binarize(const GrayImage *src, const SauvolaParams *p);

/*
 * Sauvola binarization with a sequential implementation
 */
GrayImage *sauvola_binarize_seq(const GrayImage *src, const SauvolaParams *p);

#endif /* SAUVOLA_H */
