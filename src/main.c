#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <omp.h>
#include <mpi.h>

#include "image.h"
#include "sauvola.h"

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <input_file>  <output_file> [options]   # single image\n"
        "  %s <input_dir>   <output_dir>  [options]   # batch directory\n"
        "\n"
        "Supported formats: PNG, JPEG, BMP, TGA (colour converted to grey).\n"
        "Output is always saved as PNG.\n"
        "\n"
        "Options:\n"
        "  -w <int>     Window size, must be odd          (default: 15)\n"
        "  -k <float>   Sensitivity parameter k           (default: 0.20)\n"
        "  -r <float>   Dynamic range of std dev R        (default: 128.0)\n"
        "  -t <int>     OpenMP thread count               (default: all cores)\n"
        "  -b           Also run sequential baseline for speedup comparison\n"
        "  -h           Show this help\n"
        "\n"
        "Examples:\n"
        "  %s page.jpg page_bin.png -w 25 -k 0.3 -t 4\n"
        "  %s docs/ results/ -w 15 -k 0.2\n",
        prog, prog, prog, prog);
}

/* ---------------------------------------------------------------- helpers */

static int is_image_ext(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return !strcasecmp(ext, ".png")  ||
           !strcasecmp(ext, ".jpg")  ||
           !strcasecmp(ext, ".jpeg") ||
           !strcasecmp(ext, ".bmp")  ||
           !strcasecmp(ext, ".tga");
}

static void replace_ext_png(const char *name, char *out_name, size_t size) {
    strncpy(out_name, name, size - 1);
    out_name[size - 1] = '\0';
    char *dot = strrchr(out_name, '.');
    if (dot) *dot = '\0';
    strncat(out_name, ".png", size - strlen(out_name) - 1);
}

/* ----------------------------------------------------------- core routine */

static int process_one(const char *in_path, const char *out_path,
                        const SauvolaParams *p, int benchmark) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    double t0 = 0;
    if (rank == 0) t0 = MPI_Wtime();

    GrayImage *img = NULL;
    int valid = 0;
    if (rank == 0) {
        img = image_load(in_path);
        valid = (img != NULL);
    }

    MPI_Bcast(&valid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!valid) return -1;

    if (benchmark && rank == 0) {
        GrayImage *seq = sauvola_binarize_seq(img, p);
        image_free(seq);
    }

    GrayImage *out = sauvola_binarize(img, p);

    int ret = 0;
    if (rank == 0) {
        image_free(img);
        if (out) {
            ret = image_save(out_path, out);
            image_free(out);
        } else {
            ret = -1;
        }
        if (ret == 0) printf("  total: %.4f s\n", MPI_Wtime() - t0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    return ret;
}

/* ------------------------------------------------------- directory mode */

static int collect_images(const char *dir_path, char ***names_out) {
    DIR *d = opendir(dir_path);
    if (!d) { perror(dir_path); return -1; }

    char **names = NULL;
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL) {
        if (!is_image_ext(entry->d_name)) continue;
        char **tmp = realloc(names, (count + 1) * sizeof(char *));
        if (!tmp) { free(names); closedir(d); return -1; }
        names = tmp;
        names[count] = strdup(entry->d_name);
        if (!names[count]) { free(names); closedir(d); return -1; }
        count++;
    }
    closedir(d);
    *names_out = names;
    return count;
}

static int process_directory(const char *in_dir, const char *out_dir,
                              const SauvolaParams *p, int benchmark) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    int count = 0;
    char **names = NULL;

    if (rank == 0) {
        if (mkdir(out_dir, 0755) != 0 && errno != EEXIST) {
            perror(out_dir);
        }
        count = collect_images(in_dir, &names);
    }

    MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (count < 0) return -1;
    if (count == 0) {
        if (rank == 0) fprintf(stderr, "No supported image files found in: %s\n", in_dir);
        return 0;
    }

    if (rank == 0) printf("Found %d image(s) in %s\n\n", count, in_dir);

    int errors = 0;
    for (int i = 0; i < count; i++) {
        char in_path[4096], out_name[256], out_path[4096];

        if (rank == 0) {
            snprintf(in_path, sizeof(in_path), "%s/%s", in_dir, names[i]);
            replace_ext_png(names[i], out_name, sizeof(out_name));
            snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, out_name);
            printf("[%d/%d] %s  ->  %s\n", i + 1, count, names[i], out_name);
        }

        if (process_one(rank == 0 ? in_path : NULL, 
                        rank == 0 ? out_path : NULL, p, benchmark) != 0) {
            errors++;
        }

        if (rank == 0) {
            printf("\n");
            free(names[i]);
        }
    }

    if (rank == 0) {
        free(names);
        printf("Done: %d/%d succeeded.\n", count - errors, count);
    }
    return errors ? -1 : 0;
}

/* --------------------------------------------------------------- main */

int main(int argc, char *argv[]) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) { 
        if (rank == 0) usage(argv[0]); 
        MPI_Finalize();
        return EXIT_FAILURE; 
    }

    const char *input  = argv[1];
    const char *output = argv[2];

    SauvolaParams params = { .window_size = 15, .k = 0.20, .r = 128.0 };
    int benchmark = 0;

    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            params.window_size = atoi(argv[++i]);
            if (params.window_size % 2 == 0) params.window_size++;
        } else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            params.k = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            params.r = atof(argv[++i]);
        } else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            omp_set_num_threads(atoi(argv[++i]));
        } else if (!strcmp(argv[i], "-b")) {
            benchmark = 1;
        } else if (!strcmp(argv[i], "-h")) {
            if (rank == 0) usage(argv[0]);
            MPI_Finalize();
            return EXIT_SUCCESS;
        } else {
            if (rank == 0) fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
    }

    if (rank == 0) {
        printf("MPI: %d nodes | OpenMP: %d thread(s)/node | window=%d  k=%.3f  R=%.1f\n\n",
               size, omp_get_max_threads(), params.window_size, params.k, params.r);
    }

    struct stat st;
    int is_dir = 0;
    if (rank == 0) {
        if (stat(input, &st) != 0) { perror(input); is_dir = -1; }
        else if (S_ISDIR(st.st_mode)) is_dir = 1;
    }
    
    MPI_Bcast(&is_dir, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (is_dir == -1) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int ret = 0;
    if (is_dir == 1) {
        ret = process_directory(input, output, &params, benchmark);
    } else {
        if (rank == 0) printf("%s\n", input);
        ret = process_one(input, output, &params, benchmark);
    }

    MPI_Finalize();
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
