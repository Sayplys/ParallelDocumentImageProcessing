#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ alloc */

GrayImage *image_alloc(int width, int height) {
    GrayImage *img = malloc(sizeof(GrayImage));
    if (!img) return NULL;
    img->data = malloc((size_t)width * height);
    if (!img->data) { free(img); return NULL; }
    img->width  = width;
    img->height = height;
    return img;
}

void image_free(GrayImage *img) {
    if (!img) return;
    free(img->data);
    free(img);
}

/* ------------------------------------------------------------------ load */

/* Supports PNG, JPEG, BMP, TGA, GIF and more via stb_image.
   Colour images are converted to grayscale automatically. */
GrayImage *image_load(const char *path) {
    int w, h, src_channels;
    uint8_t *raw = stbi_load(path, &w, &h, &src_channels, 1);
    if (!raw) {
        fprintf(stderr, "%s: %s\n", path, stbi_failure_reason());
        return NULL;
    }

    GrayImage *img = malloc(sizeof(GrayImage));
    if (!img) { stbi_image_free(raw); return NULL; }
    img->data   = raw;
    img->width  = w;
    img->height = h;
    return img;
}

/* ------------------------------------------------------------------ save */

/* Output format is chosen by file extension:
     .jpg / .jpeg  →  JPEG (quality 95)
     anything else →  PNG  (lossless, recommended for binary images) */
int image_save(const char *path, const GrayImage *img) {
    const char *ext = strrchr(path, '.');
    int ok;

    if (ext && (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))) {
        ok = stbi_write_jpg(path, img->width, img->height, 1, img->data, 95);
    } else {
        ok = stbi_write_png(path, img->width, img->height, 1, img->data,
                            img->width /* stride = width * 1 channel */);
    }

    if (!ok) {
        fprintf(stderr, "%s: failed to write image\n", path);
        return -1;
    }
    return 0;
}
