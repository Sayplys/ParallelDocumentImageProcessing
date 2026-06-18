# Parallel Document Image Processing (Hybrid MPI + OpenMP)

A high-performance, distributed implementation of the **Sauvola local thresholding** algorithm for document image binarization, written in C using a **Hybrid MPI + OpenMP** architecture.

## What it does

Sauvola binarization converts a grayscale document image to pure black-and-white by computing a local threshold for each pixel based on the mean and standard deviation of its surrounding window:


```

T(x,y) = mean * (1 + k * (std / R - 1))

```

Pixels with intensity below `T` become foreground (black); the rest become background (white). This adapts to local illumination variations, making it well-suited for scanned documents, handwriting, and degraded text.

### The Hybrid Architecture
To achieve massive speedups, this implementation distributes the workload across multiple machines/nodes and multiple CPU cores:
1. **MPI (Message Passing Interface):** The Master node reads the image and broadcasts it to all available worker nodes over the network. The image rows are then logically divided among the nodes. Once processed, the master gathers the chunks to save the final image.
2. **OpenMP:** Within each individual node, the assigned chunk of rows is further parallelized across the physical CPU cores.
3. **Integral Images:** The implementation uses summed-area tables so that each pixel's local statistics are computed in O(1) regardless of window size.

## Dependencies

- **OpenMPI** (`openmpi-bin`, `libopenmpi-dev`)
- GCC with OpenMP support (`-fopenmp`)
- `libm`
- [`stb_image`](https://github.com/nothings/stb) and `stb_image_write` (downloaded automatically)

## Build

```sh
make

```

The `Makefile` uses the `mpicc` compiler wrapper. It downloads the `stb` headers into `vendor/` if they are missing, then compiles the `sauvola` binary.

```sh
make clean   # remove objects and binary
make deps    # download vendor headers only

```

## Usage

Because this is an MPI application, you must run it using `mpirun` or `mpiexec`:

```sh
mpirun -n <nodes> ./sauvola <input_file>  <output_file> [options]   # single image
mpirun -n <nodes> ./sauvola <input_dir>   <output_dir>  [options]   # batch directory

```

Supported input formats: PNG, JPEG, BMP, TGA. Output is always PNG.

### Options

| Flag | Description | Default |
| --- | --- | --- |
| `-w <int>` | Window size (must be odd) | `15` |
| `-k <float>` | Sensitivity parameter k | `0.20` |
| `-r <float>` | Dynamic range of std dev R | `128.0` |
| `-t <int>` | Number of OpenMP threads **per node** | all available cores |
| `-b` | Also run sequential baseline for speedup comparison | off |
| `-h` | Show help |  |

### Examples

```sh
# Single image on a local machine (simulating 4 MPI processes, letting OpenMP auto-detect threads)
mpirun -n 4 ./sauvola page.jpg page_bin.png -w 25 -k 0.3

# Batch process a directory across a real network cluster
mpirun -n 4 -hostfile my_cluster_hosts.txt ./sauvola docs/ results/ -w 15 -k 0.2 -b

```

## 🐳 Simulating a Cluster with Docker

Don't have a multi-node cluster? You can simulate one locally using the provided Docker Compose configuration! It sets up a virtual network with 1 Master node and 3 Worker nodes.

**1. Start the cluster:**

```sh
docker-compose up -d --build

```

**2. Enter the Master node:**

```sh
docker exec -it mpi_master bash

```

**3. Compile and run across the virtual cluster:**

```sh
# Inside the container:
make clean && make
mpirun --host master,worker1,worker2,worker3 -n 4 ./sauvola docs/ results/ -w 25 -b

```

**4. Shut down the cluster:**

```sh
# Back on your host machine:
docker-compose down

```

## Running benchmarks

You can automate the testing of different combinations of MPI Nodes and OpenMP Threads to find your hardware's "sweet spot" by using the benchmark script:

```sh
./run_benchmark.sh

```

This will run multiple combinations and output a `benchmark_results.csv` file, which you can then visualize using the included Python script:

```sh
python plot_benchmark.py

```

```

### O que mudou e porquê?
1. **Nova secção de Arquitetura:** Explica claramente a quem for ver o seu código no GitHub que não é apenas um script banal em C, mas sim um pipeline HPC completo (Broadcast -> Computação -> Gather).
2. **Atualização das Dependências:** Adição do requisito vital do OpenMPI para que outros utilizadores consigam compilar.
3. **Novos Comandos de Execução:** Atualização de `./sauvola` para `mpirun -n X ./sauvola`, ensinando como usar com um *hostfile* real.
4. **Secção Docker:** Um belo destaque ensinando como usar o laboratório virtual que criámos. Isto valoriza imenso o seu repositório num portefólio!
5. **Secção de Benchmark:** Atualizada para refletir os scripts bash e python automáticos que gerámos para a recolha de dados e geração de gráficos.

```
