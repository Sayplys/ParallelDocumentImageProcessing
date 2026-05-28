# Parallel Document Image Processing

A parallel implementation of the **Sauvola local thresholding** algorithm for document image binarization, written in C with OpenMP.

## What it does

Sauvola binarization converts a grayscale document image to pure black-and-white by computing a local threshold for each pixel based on the mean and standard deviation of its surrounding window:

```
T(x,y) = mean * (1 + k * (std / R - 1))
```

Pixels with intensity below `T` become foreground (black); the rest become background (white). This adapts to local illumination variations, making it well-suited for scanned documents, handwriting, and degraded text.

The implementation uses **summed-area tables (integral images)** so that each pixel's local statistics are computed in O(1) regardless of window size. The per-row loop is then parallelized with OpenMP.

## Performance

On a 12-core machine (`window=35`, `k=0.05`):

| Mode       | Time   | Speedup |
|------------|--------|---------|
| Sequential | 9.8 ms |   1×    |
| Parallel   | 2.1 ms | ~4.7×   |

## Dependencies

- GCC with OpenMP support (`-fopenmp`)
- `libm`
- [`stb_image`](https://github.com/nothings/stb) and `stb_image_write` (downloaded automatically)

## Build

```sh
make
```

This downloads the `stb` headers into `vendor/` if they are missing, then compiles the `sauvola` binary.

```sh
make clean   # remove objects and binary
make deps    # download vendor headers only
```

## Usage

```
./sauvola <input_file>  <output_file> [options]   # single image
./sauvola <input_dir>   <output_dir>  [options]   # batch directory
```

Supported input formats: PNG, JPEG, BMP, TGA. Output is always PNG.

### Options

| Flag | Description | Default |
|------|-------------|---------|
| `-w <int>` | Window size (must be odd) | `15` |
| `-k <float>` | Sensitivity parameter k | `0.20` |
| `-r <float>` | Dynamic range of std dev R | `128.0` |
| `-t <int>` | Number of OpenMP threads | all cores |
| `-b` | Also run sequential baseline for speedup comparison | off |
| `-h` | Show help | |

### Examples

```sh
# Single image with custom parameters
./sauvola page.jpg page_bin.png -w 25 -k 0.3 -t 4

# Batch process a directory, comparing parallel vs sequential
./sauvola docs/ results/ -w 15 -k 0.2 -b
```

## Running benchmarks

```sh
bash generate_tests.sh
```

This runs both a single-image and a directory benchmark, saving the output to `single_image_banchmark.txt` and `directory_banchmark.txt`.
