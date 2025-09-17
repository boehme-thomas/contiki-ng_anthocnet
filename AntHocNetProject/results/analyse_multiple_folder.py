import sys
import os
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from analyse_multiple_logs import compute_mean_variance_from_txt

def create_combined_plots(output_dir, all_every_two_minutes, folder_labels):
    os.makedirs(output_dir, exist_ok=True)
    sns.set_theme(style="whitegrid", font="serif", rc={"text.usetex": True}, font_scale=1.5)
    metric_indices = [
        (2, "Packets Lost", "Packets Lost Over Time", "packages_lost.pdf", "Packets Lost"),
        (15, "Delivery Ratio (\\%)", "UDP Packet Delivery Ratio Over Time", "delivery_ratio.pdf", "Delivery Ratio"),
        (13, "Number of Ants Sent", "Number of Ants Sent Over Time", "number_ants.pdf", "Ants Sent"),
        (14, "Percentage of Ants Sent (\\%)", "Ants Sent per Packets Over Time", "ants_per_package.pdf", "Ants per Packets"),
        (16, "Average Packet Delay (s)", "Average Packet Delay Over Time", "average_packet_delay.pdf", "Avg Packet Delay"),
        (17, "Jitter (s)", "Jitter Over Time", "jitter.pdf", "Jitter"),
        (21, "Energest Listen (s)", "Energest Listen Over Time", "energest_listen.pdf", "Energest Listen"),
        (22, "Energest Transmit (s)", "Energest Transmit Over Time", "energest_transmit.pdf", "Energest Transmit"),
    ]
    timestamps = lambda y: [i*120 for i in range(1, len(y)+1)]

    for idx, ylabel, title, filename, legend_label in metric_indices:
        plt.figure(figsize=(9, 7))
        beta_values = [1, 1.5, 2, 1.25]  # Corresponding beta values for the folders
        for folder_idx, runs in enumerate(all_every_two_minutes):
            # runs: list of runs for this folder
            get_array = lambda x, _run: [item[x] for item in _run[1:]]
            all_runs = [get_array(idx, run) for run in runs]
            if not all_runs:
                continue
            max_len = max(len(run) for run in all_runs)
            all_runs = [run + [np.nan] * (max_len - len(run)) for run in all_runs]
            arr = np.array(all_runs)
            mean = np.nanmean(arr, axis=0)
            std = np.nanstd(arr, axis=0)
            x_data = timestamps(mean)
            label = rf"{folder_labels[folder_idx]} Mean"
            plt.plot(x_data, mean, label=label)
            mask = std > 0
            plt.fill_between(np.array(x_data)[mask],np.maximum(mean[mask] - std[mask], 0),mean[mask] + std[mask],alpha=0.2,label=rf"{folder_labels[folder_idx]} Std Dev")
        plt.xlabel("Time (s)")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.tight_layout()
        plt.legend()
        plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
        plt.close()

def create_combined_boxplot(output_dir, all_final_values, folder_labels):
    os.makedirs(output_dir, exist_ok=True)
    sns.set_theme(style="whitegrid", font="serif", rc={"text.usetex": True}, font_scale=1.5)
    metric_indices = [
        (2, "Packets Lost"),
        (5, "Ratio Reactive Forward Ants"),
        (8, "Ratio Proactive Forward Ants"),
        (15, "Delivery Ratio (\\%)"),
        (12, "Backward Ants"),
        (13, "Number of Ants Sent"),
        (14, "Percentage of Ants Sent (\\%)"),
        (16, "Average Packet Delay (s)"),
        (17, "Jitter (s)"),
        (21, "Energest Listen (s)"),
        (22, "Energest Transmit (s)"),
        (10, "Link Failure Notifications"),  # Add correct index for link failure notifications
        (13, "Total Ants"),                 # Add correct index for total ants
    ]
    for idx, ylabel in metric_indices:
        if idx == 5:
            data = [vals[:, idx] / vals[:, 13] * 100 for vals in all_final_values]
        elif idx == 8:
            data = [vals[:, idx] / vals[:, 13] * 100 for vals in all_final_values]
        else:
            data = [vals[:, idx] for vals in all_final_values]

        plt.figure(figsize=(9, 7))
        sns.boxplot(data=data, width=0.4)
        plt.xticks(ticks=range(len(folder_labels)), labels=folder_labels, rotation=10)
        plt.title(f"{ylabel}")
        if idx == 5 or idx == 8:
            plt.ylabel(f"{ylabel} (\\%)")
        else:
            plt.ylabel(ylabel)
        plt.tight_layout()
        plt.grid(True)
        filename = f"combined_{ylabel.lower().replace(' ', '_').replace('(', '').replace(')', '').replace('%', 'percent').replace('\\', '')}_boxplot.pdf"
        plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
        plt.close()

def plot_ants_per_package_all_runs(output_dir, all_every_two_minutes, folder_labels):
    os.makedirs(output_dir, exist_ok=True)
    sns.set_theme(style="whitegrid", font="serif", rc={"text.usetex": True}, font_scale=1.5)
    timestamps = lambda y: [i*120 for i in range(1, len(y)+1)]
    beta_values = [1, 1.5, 2, 1.25]  # Adjust if needed

    plt.figure(figsize=(9, 7))
    for folder_idx, runs in enumerate(all_every_two_minutes):
        get_array = lambda x, _run: [item[x] for item in _run[1:]]
        # Compute ants per package for each run
        all_runs = [
            [a / b if b != 0 else np.nan for a, b in zip(get_array(13, run), get_array(0, run))]
            for run in runs
        ]
        if not all_runs:
            continue
        max_len = max(len(run) for run in all_runs)
        all_runs = [run + [np.nan] * (max_len - len(run)) for run in all_runs]
        arr = np.array(all_runs)
        mean = np.nanmean(arr, axis=0)
        std = np.nanstd(arr, axis=0)
        x_data = timestamps(mean)
        label = f"{folder_labels[folder_idx]} Mean"
        plt.plot(x_data, mean, label=label)
        mask = std > 0
        plt.fill_between(np.array(x_data)[mask],np.maximum(mean[mask] - std[mask], 0),mean[mask] + std[mask],alpha=0.2,label=f"{folder_labels[folder_idx]} Std Dev")
    plt.xlabel("Time (s)")
    plt.ylabel("Ants per Packet")
    plt.title("Number of Ants Sent per Packet Over Time")
    plt.tight_layout()
    plt.legend()
    plt.savefig(os.path.join(output_dir, "number_ants_per_package.pdf"), bbox_inches="tight")
    plt.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python analyse_multiple_folders.py <output_path> <folder1> <folder2> ...")
        sys.exit(1)
    output_path = sys.argv[1]
    input_folders = sys.argv[2:]
    all_final_values = []
    all_every_two_minutes = []
    for folder in input_folders:
        _, _, final_values, every_two_minutes = compute_mean_variance_from_txt(folder)
        if final_values is not None:
            all_final_values.append(final_values)
        if every_two_minutes is not None:
            all_every_two_minutes.append(every_two_minutes)

    input_folders = [folder.split("movement_", 1)[-1].replace("_a22", "").replace("/", "") if "movement_" in folder else folder.replace("_a22", "").replace("/", "") for folder in input_folders]
    if all_final_values and all_every_two_minutes:
        # You can still call create_boxplots on combined data if you want
        # create_boxplots(output_path, np.vstack(all_final_values))
        create_combined_boxplot(os.path.join(output_path, "final_plots", "boxplots"), all_final_values, input_folders)
        create_combined_plots(os.path.join(output_path, "final_plots", "plots"), all_every_two_minutes, input_folders)
        plot_ants_per_package_all_runs(os.path.join(output_path, "final_plots", "plots"), all_every_two_minutes, input_folders)
        print("Combined plots with one line per folder created.")
    else:
        print("No valid data found in the provided folders.")