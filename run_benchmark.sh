#!/bin/bash

# Define a imagem mais pesada para o teste de stress
INPUT="imageDirectory/pexels-alex-ning-523843601-34945594.jpg"
OUTPUT="results/benchmark_out.png"

# Configurações do Benchmark
RUNS=5                 # Quantas vezes cada configuração vai rodar
NODES=(1 2 3 4 6)      # Quantidade de processos MPI (-n)
THREADS=(1 2 4 6 12)   # Quantidade de threads OpenMP (-t)

# Nome do arquivo de saída
CSV_FILE="benchmark_results.csv"

# Cria o cabeçalho do arquivo CSV
echo "Nodes,Threads,Run,Calc_Time_s,Total_Time_s" > $CSV_FILE

echo "======================================================"
echo " INICIANDO BENCHMARK DISTRIBUÍDO"
echo " Imagem: $INPUT"
echo " Execuções por config: $RUNS"
echo "======================================================"

for n in "${NODES[@]}"; do
    for t in "${THREADS[@]}"; do
        echo -n "Testando Nós (MPI): $n | Threads (OpenMP): $t ... "
        
        for ((i=1; i<=RUNS; i++)); do
            OUT=$(mpirun -n $n ./sauvola $INPUT $OUTPUT -t $t -w 25)
            
            CALC_TIME=$(echo "$OUT" | grep "\[MPI + OpenMP\]" | awk '{print $4}')
            TOTAL_TIME=$(echo "$OUT" | grep "total:" | awk '{print $2}')
            
            if [ -z "$CALC_TIME" ]; then
                CALC_TIME="Erro"
                TOTAL_TIME="Erro"
            fi

            echo "$n,$t,$i,$CALC_TIME,$TOTAL_TIME" >> $CSV_FILE
        done
        echo "OK"
    done
done

echo "======================================================"
echo " Benchmark finalizado com sucesso!"
echo " Os dados foram salvos em: $CSV_FILE"
echo "======================================================"
