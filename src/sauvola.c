#include "sauvola.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/* -------------------------------------------------- integral image helpers */

/*
 * Build summed-area tables so that mean and variance over any rectangular
 * window can be computed in O(1) (independent of window size).
 *
 * isum[y*W+x] = Σ src[r*W+c]   for 0≤r≤y, 0≤c≤x
 * isq [y*W+x] = Σ src²          (same bounds)
 */
static int build_integral(const uint8_t *src, int W, int H,
                           long long **isum_out, long long **isq_out) {
    size_t n     = (size_t)W * H;
    long long *s = malloc(n * sizeof(long long));
    long long *q = malloc(n * sizeof(long long));
    if (!s || !q) { free(s); free(q); return -1; }

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            long long v  = src[y * W + x];
            long long sv = v, qv = v * v;
            if (x > 0) { sv += s[y*W + x-1]; qv += q[y*W + x-1]; }
            if (y > 0) { sv += s[(y-1)*W + x]; qv += q[(y-1)*W + x]; }
            if (x > 0 && y > 0) { sv -= s[(y-1)*W + x-1]; qv -= q[(y-1)*W + x-1]; }
            s[y*W + x] = sv;
            q[y*W + x] = qv;
        }
    }
    *isum_out = s;
    *isq_out  = q;
    return 0;
}

/* Sum of pixels in rectangle [x1,x2] × [y1,y2] (inclusive). */
static inline long long rect_sum(const long long *tbl, int W,
                                  int x1, int y1, int x2, int y2) {
    long long v = tbl[y2 * W + x2];
    if (x1 > 0) v -= tbl[y2 * W + x1 - 1];
    if (y1 > 0) v -= tbl[(y1 - 1) * W + x2];
    if (x1 > 0 && y1 > 0) v += tbl[(y1 - 1) * W + x1 - 1];
    return v;
}

/* -------------------------------------------- core binarization (one row) */

/*
 * Classify all pixels in row `y` and store results in dst.
 */
static void process_row(const uint8_t *src, uint8_t *dst,
                         const long long *isum, const long long *isq,
                         int W, int H, int y,
                         int half, double k, double r) {
    int y1 = (y - half < 0)  ? 0     : y - half;
    int y2 = (y + half >= H) ? H - 1 : y + half;

    for (int x = 0; x < W; x++) {
        int x1 = (x - half < 0)  ? 0     : x - half;
        int x2 = (x + half >= W) ? W - 1 : x + half;

        long long n   = (long long)(x2 - x1 + 1) * (y2 - y1 + 1);
        long long sum = rect_sum(isum, W, x1, y1, x2, y2);
        long long sq  = rect_sum(isq,  W, x1, y1, x2, y2);

        double mean = (double)sum / n;
        double var  = (double)sq  / n - mean * mean;
        double std  = (var > 0.0) ? sqrt(var) : 0.0;

        double threshold = mean * (1.0 + k * (std / r - 1.0));
        dst[y * W + x] = ((double)src[y * W + x] < threshold) ? 0 : 255;
    }
}

/* ---------------------------------------------------- public API: parallel */

GrayImage *sauvola_binarize(const GrayImage *src, const SauvolaParams *p) {
    int W    = src->width;
    int H    = src->height;
    int half = p->window_size / 2;

    long long *isum = NULL, *isq = NULL;
    if (build_integral(src->data, W, H, &isum, &isq) != 0) {
        fprintf(stderr, "sauvola: out of memory (integral tables)\n");
        return NULL;
    }

    GrayImage *dst = image_alloc(W, H);
    if (!dst) { free(isum); free(isq); return NULL; }

    double t0 = omp_get_wtime();

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < H; y++)
        process_row(src->data, dst->data, isum, isq,
                    W, H, y, half, p->k, p->r);

    printf("  [parallel] %.4f s  (%d thread(s))\n",
           omp_get_wtime() - t0, omp_get_max_threads());

    free(isum);
    free(isq);
    return dst;
}

/* -------------------------------------------------- public API: sequential */

GrayImage *sauvola_binarize_seq(const GrayImage *src, const SauvolaParams *p) {
    int W    = src->width;
    int H    = src->height;
    int half = p->window_size / 2;

    long long *isum = NULL, *isq = NULL;
    if (build_integral(src->data, W, H, &isum, &isq) != 0) {
        fprintf(stderr, "sauvola: out of memory (integral tables)\n");
        return NULL;
    }

    GrayImage *dst = image_alloc(W, H);
    if (!dst) { free(isum); free(isq); return NULL; }

    double t0 = omp_get_wtime();

    for (int y = 0; y < H; y++)
        process_row(src->data, dst->data, isum, isq,
                    W, H, y, half, p->k, p->r);

    printf("  [sequential] %.4f s\n", omp_get_wtime() - t0);

    free(isum);
    free(isq);
    return dst;
}
