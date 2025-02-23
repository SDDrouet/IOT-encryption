import collections
import numpy as np
import matplotlib.pyplot as plt
from pymongo import MongoClient
import os

def get_data():
    client = MongoClient("mongodb://localhost:27017")
    db = client["sensores"]
    coll = db["mediciones2"]
    return list(coll.find())

"""
def convert_to_microseconds(time_val, unit):
    if unit.lower() == 'ms':
        return time_val * 1000
    elif unit.lower() == 'us':
        return time_val
    else:
        return time_val
"""

def main():
    data = get_data()
    if not data:
        print("No se encontraron datos en la base de datos.")
        return

    processed = []
    for doc in data:
        t = doc.get("time")
        unit = doc.get("UnitMessure")
        t_us = t
        doc["time_us"] = t_us
        processed.append(doc)

    grouped_micro = collections.defaultdict(list)
    grouped_micro_medition = collections.defaultdict(list)
    micro_set = set()
    medition_set = set()

    for doc in processed:
        micro = doc.get("microName")
        medition = doc.get("medition")
        micro_set.add(micro)
        medition_set.add(medition)
        grouped_micro[micro].append(doc["time_us"])
        grouped_micro_medition[(micro, medition)].append(doc["time_us"])

    microcontrollers = sorted(list(micro_set))
    medition_types = sorted(list(medition_set))

    print("\nMétricas por Medición:")
    metrics_medition = {}
    for key, times in grouped_micro_medition.items():
        micro, medition = key
        mean_val = np.mean(times)
        std_val = np.std(times)
        metrics_medition[key] = (mean_val, std_val)
        print(f"{micro} - {medition}: Media = {mean_val:.2f} cycles, Desviación Estándar = {std_val:.2f} cycles")

    if not os.path.exists("measuresImages"):
        os.makedirs("measuresImages")

    # Gráfico para Encriptación/Desencriptación
    enc_dec_meditions = {'Encryption Time', 'Decryption Time'}
    medition_types_enc_dec = sorted([m for m in medition_types if m in enc_dec_meditions])

    if medition_types_enc_dec:
        x = np.arange(len(medition_types_enc_dec))
        fig, ax = plt.subplots(figsize=(10, 6))
        bar_width = 0.8  # Una barra para cada tipo de medición
        
        # Promediar los tiempos de todos los microcontroladores para cada tipo de medición
        means = []
        for med in medition_types_enc_dec:
            times = []
            for micro in microcontrollers:
                times.extend(grouped_micro_medition.get((micro, med), []))
            means.append(np.mean(times) if times else 0)
        
        # Crear las barras para cada tipo de medición
        ax.bar(x, means, bar_width, label="Average Time")
        
        ax.set_xticks(x)
        ax.set_xticklabels(medition_types_enc_dec, rotation=45, ha="right")
        ax.set_ylabel("# Cycles")
        ax.set_title("Encryption/Decryption Number of Cycles")
        ax.legend()
        plt.tight_layout()
        plt.savefig("measuresImages/enc_dec_avg_chart_cycles.png")
        plt.show()

    # Gráfico para métricas de claves
    key_meditions = {'UECC Shared Key Time', 'Optimized Shared Key Time', 'Public Private Key Time'}
    medition_types_key = sorted([m for m in medition_types if m in key_meditions])
    
    if medition_types_key:
        group_names = ['Shared Key Time', 'Public Private Key Time']
        x_key = np.arange(len(group_names))
        fig_key, ax_key = plt.subplots(figsize=(10, 6))
        bar_width_key = 0.8 / len(microcontrollers)
        colors = {'UECC': '#7befd5', 'Optimized': '#ff7f0e', 'Public': '#2ca02c'}
        
        shared_means = {}
        public_means = {}
        temp_public = {}
        meanTime = 0;

        for micro in microcontrollers:
            temp_public = grouped_micro_medition.get((micro, 'Public Private Key Time'), [])
            meanTime = np.mean(temp_public) if temp_public else 0 + meanTime;

        
        for micro in microcontrollers:
            uecc_times = grouped_micro_medition.get((micro, 'UECC Shared Key Time'), [])
            optimized_times = grouped_micro_medition.get((micro, 'Optimized Shared Key Time'), [])
            
            shared_means[micro] = ((np.mean(uecc_times) if uecc_times else 0),
                                   (np.mean(optimized_times) if optimized_times else 0))
            public_means[micro] = meanTime

        
        for i, micro in enumerate(microcontrollers):
            offset = (i - len(microcontrollers)/2) * bar_width_key + bar_width_key/2
            uecc_mean, optimized_mean = shared_means[micro]
            public_mean = public_means[micro]
            
            # Barras apiladas para Shared Key
            ax_key.bar(x_key[0] + offset, uecc_mean, bar_width_key, color=colors['UECC'], 
                      label='Shared' if i == 0 else "")
            ax_key.bar(x_key[0] + offset, optimized_mean, bar_width_key, 
                      bottom=uecc_mean, color=colors['Optimized'], label='Optimized' if i == 0 else "")
            
            # Barra para Public/Private
            ax_key.bar(x_key[1] + offset, public_mean, bar_width_key, color=colors['Public'],
                      label='Public/Private' if i == 0 else "")
        
        ax_key.set_xticks(x_key)
        ax_key.set_xticklabels(group_names, rotation=45, ha="right")
        ax_key.set_ylabel("T# Cycles")
        ax_key.set_title("Keys Generation Number of Cycles")

        # Evitar notación científica en el eje Y
        ax_key.ticklabel_format(style='plain', axis='y')
        
        handles, labels = ax_key.get_legend_handles_labels()
        unique_labels = dict(zip(labels, handles))
        ax_key.legend(unique_labels.values(), unique_labels.keys(), loc='upper left', bbox_to_anchor=(1, 1))

        
        plt.tight_layout()
        plt.savefig("measuresImages/key_operations_chart_cycles.png")
        plt.show()


if __name__ == "__main__":
    main()