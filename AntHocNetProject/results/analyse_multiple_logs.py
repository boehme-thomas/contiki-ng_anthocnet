import math
import sys
import os
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import copy
import seaborn as sns

def create_plots(path, every_two_minutes_list):
    path = os.path.join(path, "final_plots")
    os.makedirs(path, exist_ok=True)
    output_dir = os.path.join(path, "plots")
    os.makedirs(output_dir, exist_ok=True)

    sns.set_theme(style="whitegrid", font="serif", rc={"text.usetex": True}, font_scale=1.5)

    timestamps = lambda y: [i*120 for i in range(1, len(y)+1)]
    get_array = lambda x, _run: [item[x] for item in _run[1:]]

    def plot_with_error_band(index, ylabel, title, filename):
        # Collect all runs for the metric
        all_runs = [get_array(index, run) for run in every_two_minutes_list]
        max_len = max(len(run) for run in all_runs)
        # Pad runs to same length with np.nan
        all_runs = [run + [np.nan]*(max_len-len(run)) for run in all_runs]
        arr = np.array(all_runs)
        mean = np.nanmean(arr, axis=0)
        std = np.nanstd(arr, axis=0)

        # Cap mean + std at 100 for delivery ratio and percentage of ants sent
        if index in [15]:  # 15: delivery ratio, 14: percentage of ants sent
            upper = np.minimum(mean + std, 100)
        else:
            upper = mean + std

        x_data = timestamps(mean)
        plt.figure(figsize=(8, 5))
        sns.lineplot(x=x_data, y=mean, label="Mean")

        mask = std > 0
        plt.fill_between(
            np.array(x_data)[mask],
            np.maximum(mean[mask] - std[mask], 0),
            upper[mask],
            alpha=0.3,
            label="Std Dev"
        )
        plt.fill_between(np.array(x_data)[mask], mean[mask] - std[mask], mean[mask] + std[mask], alpha=0.3, label="Std Dev")
        plt.xlabel("Time (s)")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.tight_layout()
        plt.legend()
        plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
        plt.close()

    plot_with_error_band(2, "Packet Lost", "Packets Lost Over Time", "packages_lost.pdf")
    plot_with_error_band(15, "Delivery Ratio (\\%)", "UDP Packet Delivery Ratio Over Time", "delivery_ratio.pdf")
    plot_with_error_band(13, "Number of Ants Sent", "Number of Ants Sent Over Time", "number_ants.pdf")
    plot_with_error_band(14, "Percentage of Ants Sent (\\%)", "Ants Sent per Packet Over Time", "ants_per_package.pdf")
    plot_with_error_band(16, "Average Packet Delay (s)", "Average Packet Delay Over Time", "average_packet_delay.pdf")
    plot_with_error_band(17, "Jitter (s)", "Jitter Over Time", "average_jitter.pdf")

    all_runs = [[a / b if b != 0 else np.nan for a, b in zip(get_array(13, run), get_array(0, run))] for run in every_two_minutes_list]
    max_len = max(len(run) for run in all_runs)
    # Pad runs to same length with np.nan
    all_runs = [run + [np.nan] * (max_len - len(run)) for run in all_runs]
    arr = np.array(all_runs)
    mean = np.nanmean(arr, axis=0)
    std = np.nanstd(arr, axis=0)
    x_data = timestamps(mean)
    plt.figure(figsize=(8, 5))
    sns.lineplot(x=x_data, y=mean, label="Mean")
    mask = std > 0
    plt.fill_between(
        np.array(x_data)[mask],
        np.maximum(mean[mask] - std[mask], 0),
        mean[mask] + std[mask],
        alpha=0.3,
        label="Std Dev"
    )
    plt.fill_between(x_data, mean - std, mean + std, alpha=0.3, label="Std Dev")
    plt.xlabel("Time (s)")
    plt.ylabel("Ants per Packet")
    plt.title("Number of Ants Sent per Packet Over Time")
    plt.tight_layout()
    plt.legend()
    plt.savefig(os.path.join(output_dir, "number_ants_per_package.pdf"), bbox_inches="tight")
    plt.close()

    plot_with_error_band(21, "Energest Listen (s)", "Energest Listen Over Time", "energest_listen.pdf")
    plot_with_error_band(22, "Energest Transmit (s)", "Energest Transmit Over Time", "energest_transmit.pdf")


def create_boxplots(path, values):
    path = os.path.join(path, "final_plots")
    os.makedirs(path, exist_ok=True)
    box_path = os.path.join(path, "boxplots")
    os.makedirs(box_path, exist_ok=True)

    sns.set_theme(style="whitegrid", font="serif", rc={"text.usetex": True}, font_scale=1.5)

    def seaborn_boxplot(data, labels, title, ylabel, filename):
        plt.figure(figsize=None)
        sns.boxplot(data=data, width=0.4)
        if len(labels) >= 3:
            plt.xticks(ticks=range(len(labels)), labels=labels, rotation=20)
        else:
            plt.xticks(ticks=range(len(labels)), labels=labels)
        plt.title(title)
        plt.ylabel(ylabel)
        plt.tight_layout()
        plt.grid(True)
        plt.savefig(os.path.join(box_path, filename), bbox_inches="tight")
        plt.close()

    udp_packages = np.array([item[0:2] for item in values])
    seaborn_boxplot(udp_packages, ["Sent", "Received"], "UDP Packets", "Numer Packets", "packages_sent_received.pdf")

    seaborn_boxplot(np.array([item[2] for item in values]), ["Lost"], "UDP Packets", "Numer Packets", "number_packages_lost.pdf")
    seaborn_boxplot(np.array([item[3:5] for item in values]), ["Broadcast", "Unicast"],
                    "Reactive Forward Ants", "Number of Ants", "number_rfa.pdf")
    seaborn_boxplot(np.array([item[6:8] for item in values]), ["Broadcast", "Unicast"],
                    "Forward Ants Ratio", "Number of Ants", "number_pfa.pdf")
    seaborn_boxplot(np.array([item[9:13] for item in values]),
                    ["Path Repair Ants", "Link Failure Notifications", "Warning Messages", "Backward Ants"],
                    "Other Ants", "Number of Ants", "number_other_ants.pdf")
    seaborn_boxplot(np.array([item[10] for item in values]), ["Link Failure Notifications"],
                    "Link Failure Notifications", "Number of Ants", "number_link_failure_notifications.pdf")


    seaborn_boxplot(np.array([item[3:5] / item[13] * 100 for item in values]), ["Broadcast", "Unicast"], "Reactive Forward Ants Ratio", "Ratio to All Ants (\\%)", "rfa.pdf")
    seaborn_boxplot(np.array([[item[3] / item[13] * 100, item[4] / item[13] * 100, item[10] / item [13] *100] for item in values]), ["RFA Broadcast", "RFA Unicast", "Link Failure Notifications"], "Ratios", "Ratio to All Ants (\\%)", "rfa_linkfailure.pdf")
    seaborn_boxplot(np.array([item[6:8] / item[13] * 100 for item in values]), ["Broadcast", "Unicast"], "Proactive Forward Ants Ratio", "Ratio to All Ants (\\%)", "pfa.pdf")
    seaborn_boxplot(np.array([item[9:13] / item[13] * 100 for item in values]), ["Path Repair Ants", "Link Failure Notifications", "Warning Messages", "Backward Ants"], "Ratios of Other Ants", "Ratio to All Ants (\\%)", "other_ants.pdf")
    seaborn_boxplot(np.array([item[10] / item[13] * 100 for item in values]), ["Link Failure Notifications"], "Link Failure Notifications Ratio", "Ratio to All Ants (\\%)", "link_failure_notifications.pdf")
    seaborn_boxplot(np.array([item[13] for item in values]), ["Total"], "Sum of Ants Sent", "Numer Ants", "total_ants.pdf")
    seaborn_boxplot(np.array([item[14] for item in values]), ["Percentage of Ants Sent"], "Percentage of Ants sent", "Percentage (\\%)", "percentage_ants.pdf")
    seaborn_boxplot(np.array([item[15] for item in values]), ["Packet Delivery Ratio"], "Packet Delivery Ratio", "Percentage (\\%)", "packet_delivery_ratio.pdf")
    seaborn_boxplot(np.array([item[16] for item in values if not math.isnan(item[16])]), ["Average Packet Delay"], "Average Packet Delay", "Time (s)", "average_delay.pdf")
    seaborn_boxplot(np.array([item[17] for item in values if not math.isnan(item[17])]), ["Jitter"], "Jitter", "Time (s)", "jitter.pdf")
    seaborn_boxplot(np.array([item[18:] for item in values]), ["CPU", "LPM", "Deep LPM", "Listen", "Transmit", "Off"], "Energest", "Time (s)", "energest.pdf")
    seaborn_boxplot(np.array([item[21] for item in values]), ["Listen"], "Energest", "Time (s)", "energest_listen.pdf")
    seaborn_boxplot(np.array([item[22] for item in values]), ["Transmit"], "Energest", "Time (s)", "energest_transmit.pdf")
    seaborn_boxplot(np.array([item[23] for item in values]), ["Off"], "Energest", "Time (s)", "energest_off.pdf")

    violin_path = os.path.join(path, "violinplots")
    os.makedirs(violin_path, exist_ok=True)

    def seaborn_violinplot(data, labels, title, ylabel, filename):
        plt.figure()
        sns.violinplot(data=data, width=0.4)
        plt.xticks(ticks=range(len(labels)), labels=labels, rotation=20)
        plt.title(title)
        plt.ylabel(ylabel)
        plt.tight_layout()
        plt.grid(True)
        plt.savefig(os.path.join(violin_path, filename), bbox_inches="tight")
        plt.close()

    # Replace all seaborn_boxplot calls with seaborn_violinplot
    udp_packages = np.array([item[:2] for item in values])
    seaborn_violinplot(udp_packages, ["Sent", "Received"], "UDP Packets", "Numer Packets", "violin_packages_sent_received.pdf")

    seaborn_violinplot(np.array([item[2] for item in values]), ["Lost"], "UDP Packets", "Numer Packets", "violin_packages_lost.pdf")
    seaborn_violinplot(np.array([item[3:5] for item in values]), ["Broadcast", "Unicast"], "Reactive Forward Ants", "Numer Ants", "violin_rfa.pdf")
    seaborn_violinplot(np.array([item[6:8] for item in values]), ["Broadcast", "Unicast"], "Proactive Forward Ants", "Numer Ants", "pfa.pdf")
    seaborn_violinplot(np.array([item[9:13] for item in values]), ["Path Repair Ants", "Link Failure Notifications", "Warning Messages", "Backward Ants"], "Other Ants", "Numer Ants", "violin_other_ants.pdf")
    seaborn_violinplot(np.array([item[13] for item in values]), ["Total"], "Sum of Ants Sent", "Numer Ants", "total_ants.pdf")
    seaborn_violinplot(np.array([item[14] for item in values]), ["Percentage of Ants Sent"], "Percentage of Ants sent", "Percentage (\\%)", "violin_percentage_ants.pdf")
    seaborn_violinplot(np.array([item[15] for item in values]), ["Packet Delivery Ratio"], "Packet Delivery Ratio", "Percentage (\\%)", "violin_packet_delivery_ratio.pdf")
    seaborn_violinplot(np.array([item[16] for item in values if not math.isnan(item[16])]), ["Average Packet Delay"], "Average Packet Delay", "Time (s)", "violin_average_delay.pdf")
    seaborn_violinplot(np.array([item[17] for item in values if not math.isnan(item[17])]), ["Jitter"], "Jitter", "Time (s)", "violin_jitter.pdf")
    seaborn_violinplot(np.array([item[18:] for item in values]), ["CPU", "LPM", "Deep LPM", "Listen", "Transmit", "Off"], "Energest", "Time (s)", "violin_energest.pdf")
    seaborn_violinplot(np.array([item[21] for item in values]), ["Listen"], "Energest", "Time (s)", "violin_energest_listen.pdf")
    seaborn_violinplot(np.array([item[22] for item in values]), ["Transmit"], "Energest", "Time (s)", "violin_energest_transmit.pdf")

def compute_mean_variance_from_txt(path):
    values_every_two_minutes_per_run = []
    final_values = []
    die_count = 0.0
    for file in os.listdir(path):
        if file.endswith('.txt'):
            print(f"Open file: {os.path.join(path, file)}")
            run = []
            two_min_run = []

            with (open(os.path.join(path, file), 'r') as f):
                final = False
                for line in f:

                    if "Node will die" in line:
                        die_count += 1.0

                    if "Final Results" in line:
                        final = True

                    if "Number of UDP packages sent" in line:
                        run.append(int(line.split(":")[1].strip()))
                    if "Number of UDP packages received" in line:
                        run.append(int(line.split(":")[1].strip()))
                    if "Number of packages lost" in line:
                        run.append(int(line.split(":")[1].strip()))

                    if "Ants sent:" in line:
                        ants_data = []

                        for _ in range(12):  # There are 10 lines of data after "Ants sent:"
                            ants_data.append(next(f).strip())
                        # Example: extract values from ants_data
                        #broadcast_reactive
                        run.append(int(ants_data[1].split(":")[1].strip()))
                        #unicast_reactive
                        run.append(int(ants_data[2].split(":")[1].strip()))
                        #sum_reactive
                        run.append(int(ants_data[3].split(":")[1].strip()))
                        #broadcast_proactive
                        run.append(int(ants_data[5].split(":")[1].strip()))
                        #unicast_proactive
                        run.append(int(ants_data[6].split(":")[1].strip()))
                        #sum_proactive
                        run.append(int(ants_data[7].split(":")[1].strip()))
                        #path_repair_ant
                        run.append(int(ants_data[8].split(":")[1].strip()))
                        #link_failure_notification
                        run.append(int(ants_data[9].split(":")[1].strip()))
                        #warning_message
                        run.append(int(ants_data[10].split(":")[1].strip()))
                        #backward_ant
                        run.append(int(ants_data[11].split(":")[1].strip()))

                    if "Total number of ants sent" in line:
                        #total_ants_sent
                        run.append(int(line.split(":")[1].strip()))
                    if "Percentage of ants sent" in line:
                        #percentage_of_ants_sent
                        run.append(float(line.split(":")[1].split("%")[0].strip()))
                    if "UDP Packet delivery ratio" in line:
                        #udp_packet_delivery_ratio
                        run.append(float(line.split(":")[1].split("%")[0].strip()))
                    if "No packets received" in line:
                        #no_packets_received
                        run.append(math.nan)
                        run.append(math.nan)
                    if "Average packet delay" in line:
                        #average_packet_delay
                        run.append(float(line.split(":")[1].split(" ")[1].strip()))
                    if "Jitter" in line:
                        #jitter
                        run.append(float(line.split(":")[1].split(" ")[1].strip()))
                    if "Energest:" in line:
                        _sum = float(next(f).strip().split(":")[1].strip())
                        if _sum != 0:
                            #cup
                            run.append(float(next(f).strip().split(":")[1].strip()))
                            #lpm
                            run.append(float(next(f).strip().split(":")[1].strip()))
                            #deep lpm
                            run.append(float(next(f).strip().split(":")[1].strip()))
                            #listen
                            run.append(float(next(f).strip().split(":")[1].strip()))
                            #transmit
                            run.append(float(next(f).strip().split(":")[1].strip()))
                            #off
                            run.append(float(next(f).strip().split(":")[1].strip()))
                        else:
                            for _ in range(7):
                                run.append(0.0)

                    if "------------------------------------------" in line:
                        if run:
                            two_min_run.append(copy.deepcopy(run))
                            run= []
                if final and run:
                    values_every_two_minutes_per_run.append(copy.deepcopy(two_min_run))
                    final_values.append(copy.deepcopy(run))

    die_count = die_count / max(1.0, len(final_values))
    print(f"Avg die count: {die_count}")
    if final_values:
        print(f"finale_values: {[len(item) for item in final_values]}")
        print(f"every_two_minutes: {[len(item) for item in values_every_two_minutes_per_run]}")
        final_values = np.array(final_values)
        _mean = np.mean(final_values, axis=0)
        _variance = np.var(final_values, axis=0)
        with(open(os.path.join(path, "final_result.txt"), 'w') as f):
            f.write(f"{final_values}\n")
            f.write(f"Mean: {_mean}\n")
            f.write(f"Variance: {_variance}\n")

        return _mean, _variance, final_values, values_every_two_minutes_per_run
    else:
        return None, None, None, None


if __name__ == "__main__":
    #file_name = "bust_p_2_min_07_15"
    #file_name = "loglistener_kite_07_14_1916"
    #file_name = "loglistener_line_07_07_1729"
    '''
    for file in os.listdir("logfiles")[6:]:
        file_name = file.split(".")[0]
        get_files(file_name)
    '''

    if len(sys.argv) != 2:
        print("Usage: python analyse_multiple_logs.py <path_to_output_files_folder>")
    else:
        input_dir = sys.argv[1]
        mean, var, vals, vals_every_two_min = compute_mean_variance_from_txt(input_dir)
        create_boxplots(input_dir, vals)
        create_plots(input_dir, vals_every_two_min)
        print(f"Mean: {mean}")
        print(f"Variance: {var}")