#include "sauvola.h"
#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/* -------------------------------------------------- integral image helpers */

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

static inline long long rect_sum(const long long *tbl, int W,
                                  int x1, int y1, int x2, int y2) {
    long long v = tbl[y2 * W + x2];
    if (x1 > 0) v -= tbl[y2 * W + x1 - 1];
    if (y1 > 0) v -= tbl[(y1 - 1) * W + x2];
    if (x1 > 0 && y1 > 0) v += tbl[(y1 - 1) * W + x1 - 1];
    return v;
}

/* -------------------------------------------- core binarization (one row) */

static void process_row(const uint8_t *src, uint8_t *dst_row,
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
        dst_row[x] = ((double)src[y * W + x] < threshold) ? 0 : 255;
    }
}

/* ---------------------------------------------------- public API: parallel */

GrayImage *sauvola_binarize(const GrayImage *src, const SauvolaParams *p) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int W = 0, H = 0;
    if (rank == 0) {
        W = src->width;
        H = src->height;
    }

    MPI_Bcast(&W, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&H, 1, MPI_INT, 0, MPI_COMM_WORLD);

    uint8_t *raw_data = NULL;
    if (rank == 0) {
        raw_data = src->data;
    } else {
        raw_data = malloc((size_t)W * H);
        if (!raw_data) MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_Bcast(raw_data, W * H, MPI_BYTE, 0, MPI_COMM_WORLD);

    int half = p->window_size / 2;

    long long *isum = NULL, *isq = NULL;
    if (build_integral(raw_data, W, H, &isum, &isq) != 0) {
        if (rank == 0) fprintf(stderr, "sauvola: out of memory (integral tables)\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rows_per_proc = H / size;
    int remainder = H % size;
    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int));
    int sum = 0;

    for (int i = 0; i < size; i++) {
        int r = rows_per_proc + (i < remainder ? 1 : 0);
        sendcounts[i] = r * W;
        displs[i] = sum;
        sum += sendcounts[i];
    }

    int my_rows = rows_per_proc + (rank < remainder ? 1 : 0);
    int my_offset = displs[rank] / W;

    uint8_t *local_dst = malloc((size_t)my_rows * W);
    if (!local_dst && my_rows > 0) MPI_Abort(MPI_COMM_WORLD, 2);

    double t0 = MPI_Wtime();

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < my_rows; i++) {
        int global_y = my_offset + i;
        process_row(raw_data, &local_dst[i * W], isum, isq, W, H, global_y, half, p->k, p->r);
    }

    GrayImage *dst = NULL;
    if (rank == 0) dst = image_alloc(W, H);

    MPI_Gatherv(local_dst, my_rows * W, MPI_BYTE,
                rank == 0 ? dst->data : NULL, sendcounts, displs,
                MPI_BYTE, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("  [MPI + OpenMP] %.4f s  (%d thread(s)/node)\n",
               MPI_Wtime() - t0, omp_get_max_threads());
    }

    free(local_dst);
    free(sendcounts);
    free(displs);
    free(isum);
    free(isq);
    if (rank != 0) free(raw_data);

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
        process_row(src->data, &dst->data[y * W], isum, isq,
                    W, H, y, half, p->k, p->r);

    printf("  [sequential] %.4f s\n", omp_get_wtime() - t0);

    free(isum);
    free(isq);
    return dst;
}
