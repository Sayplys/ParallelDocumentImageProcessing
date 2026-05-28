#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

typedef struct {
    uint8_t *data;  /* row-major, 1 byte per pixel (grayscale) */
    int      width;
    int      height;
} GrayImage;

GrayImage *image_alloc(int width, int height);
void       image_free(GrayImage *img);

GrayImage *image_load(const char *path);
int        image_save(const char *path, const GrayImage *img);

#endif /* IMAGE_H */
