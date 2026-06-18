import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

# Configurações de estilo para deixar o gráfico mais bonito
sns.set_theme(style="whitegrid")
plt.rcParams.update({'font.size': 12})

CSV_FILE = "benchmark_results.csv"

def main():
    if not os.path.exists(CSV_FILE):
        print(f"Erro: Arquivo '{CSV_FILE}' não encontrado.")
        print("Rode o script bash run_benchmark.sh primeiro!")
        return

    # Lê o CSV
    print("Lendo os dados...")
    df = pd.read_csv(CSV_FILE)

    # Converte colunas de tempo para numérico (força erros a virarem NaN para ignorar falhas)
    df['Calc_Time_s'] = pd.to_numeric(df['Calc_Time_s'], errors='coerce')
    df['Total_Time_s'] = pd.to_numeric(df['Total_Time_s'], errors='coerce')

    # Remove linhas que deram erro (NaN)
    df = df.dropna()

    # Calcula a média do tempo para cada combinação de (Nodes, Threads)
    df_mean = df.groupby(['Nodes', 'Threads']).mean().reset_index()

    print("Gerando gráficos...")
    
    # Cria uma figura com 2 subgráficos lado a lado
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))

    # ---------------------------------------------------------
    # Gráfico 1: Tempo de Cálculo (Algoritmo Sauvola)
    # ---------------------------------------------------------
    sns.lineplot(
        data=df_mean, x='Threads', y='Calc_Time_s', hue='Nodes', 
        marker='o', palette='tab10', ax=axes[0], linewidth=2.5, markersize=8
    )
    axes[0].set_title('Tempo de Cálculo vs OpenMP Threads\n(Foco na CPU e Rede)', fontsize=14, fontweight='bold')
    axes[0].set_xlabel('Número de Threads OpenMP (por Nó)')
    axes[0].set_ylabel('Tempo Médio (Segundos)')
    axes[0].set_xticks(df_mean['Threads'].unique())
    
    # ---------------------------------------------------------
    # Gráfico 2: Tempo Total (Incluindo I/O de Disco)
    # ---------------------------------------------------------
    sns.lineplot(
        data=df_mean, x='Threads', y='Total_Time_s', hue='Nodes', 
        marker='s', palette='tab10', ax=axes[1], linewidth=2.5, markersize=8
    )
    axes[1].set_title('Tempo Total vs OpenMP Threads\n(Foco no Gargalo de I/O)', fontsize=14, fontweight='bold')
    axes[1].set_xlabel('Número de Threads OpenMP (por Nó)')
    axes[1].set_ylabel('Tempo Médio (Segundos)')
    axes[1].set_xticks(df_mean['Threads'].unique())

    plt.tight_layout()
    output_img = "benchmark_plot.png"
    plt.savefig(output_img, dpi=300)
    print(f"Sucesso! Gráfico salvo como '{output_img}'")

if __name__ == "__main__":
    main()
