import sys
import os
from math import sqrt
import copy
import matplotlib as mpl
import matplotlib.pyplot as plt

def create_topology(nodes, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    # Create a LaTeX TikZ diagram for the network topology
    tikz_code = [
        r"\documentclass{standalone}",
        r"\usepackage{tikz}",
        r"\begin{document}",
        r"\begin{tikzpicture}[scale=0.5]",
        #r"\draw[step=10,gray,very thin] (0,0) grid (30,30);",
    ]
    for node_id, x, y in nodes:
        if node_id == 1:
            tikz_code.append(
                rf"\node[circle,draw, fill=red, minimum height=0.7cm] (N{node_id}) at ({x/10},{y/10}) {{ }};"
            )
        else:
            tikz_code.append(
                rf"\node[circle,draw, minimum height=0.7cm] (N{node_id}) at ({x/10},{y/10}) {{ }};"
            )
    tikz_code.append(r"\end{tikzpicture}")
    tikz_code.append(r"\end{document}")

    tex_path = os.path.join(output_dir, "topology.tex")

    with open(tex_path, "w") as f:
        f.write("\n".join(tikz_code))

    # Compile LaTeX to PDF
    os.system(f"pdflatex -output-directory={output_dir} {tex_path} > /dev/null 2>&1")
    # Optionally, clean up auxiliary files
    for ext in [".aux", ".log", ".tex"]:
        try:
            os.remove(os.path.join(output_dir, f"topology{ext}"))
        except FileNotFoundError:
            pass

def create_plots(every_two_minutes_list, output_dir):

    # Create the output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)



    mpl.rcParams.update({
        "text.usetex": True,
        "font.family": "serif",
        "font.serif": ["Computer Modern Roman"],
    })

    timestamps = [item[0] / 1e6 for item in every_two_minutes_list]  # Convert to seconds
    packages_sent = [item[1]["Packages sent"] for item in every_two_minutes_list]
    packages_received = [item[1]["Number of packages"] for item in every_two_minutes_list]
    packages_lost = [sent - received for sent, received in zip(packages_sent, packages_received)]
    delivery_ratio = [
        (received / sent * 100 if sent > 0 else 0)
        for sent, received in zip(packages_sent, packages_received)
    ]
    ants_sent = [
        sum(
            v if not isinstance(v, dict) else v.get("Sum", 0)
            for v in item[1]["Ants Count"].values()
        )
        for item in every_two_minutes_list
    ]
    jitter = [
        sqrt(
            sum((d - (sum(item[1]["Time Between Packages"]) / len(item[1]["Time Between Packages"])) ) ** 2
                for d in item[1]["Time Between Packages"]) / len(item[1]["Time Between Packages"])
        ) if item[1]["Time Between Packages"] else 0
        for item in every_two_minutes_list
    ]

    average_delay = [
        sum(item[1]["Time Between Packages"]) / len(item[1]["Time Between Packages"])
        if item[1]["Time Between Packages"] else 0
        for item in every_two_minutes_list
    ]
# Plot 1: Packages sent vs received, lost, delivery ratio
    plt.figure(figsize=(8, 5))
    plt.plot(timestamps, packages_sent, label="Packages Sent")
    plt.plot(timestamps, packages_received, label="Packages Received")
    plt.plot(timestamps, packages_lost, label="Packages Lost")
    plt.xlabel("Time (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.ylabel("Count", fontsize=1.5 * plt.rcParams["font.size"])
    plt.title("Packages Sent, Received, and Lost Over Time", fontsize=1.5 * plt.rcParams["font.size"])
    plt.xticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.yticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.legend(fontsize=1.5 * plt.rcParams["font.size"])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "packages_sent_received_lost.pdf"))
    plt.close()

    plt.figure(figsize=(8, 5))
    plt.plot(timestamps, delivery_ratio, marker="o")
    plt.xlabel("Time (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.ylabel("Delivery Ratio (\\%)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.title("UDP Packet Delivery Ratio Over Time", fontsize=1.5 * plt.rcParams["font.size"])
    plt.xticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.yticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "delivery_ratio.pdf"))
    plt.close()

    # Plot 2: Ratio of sent packages vs ants needed
    plt.figure(figsize=(8, 5))
    ratio_ants = [
        (ants / sent if sent > 0 else 0)
        for ants, sent in zip(ants_sent, packages_sent)
    ]
    plt.plot(timestamps, ratio_ants, marker="s")
    plt.xlabel("Time (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.ylabel("Ants Sent per Package", fontsize=1.5 * plt.rcParams["font.size"])
    plt.title("Ants Sent per Package Over Time", fontsize=1.5 * plt.rcParams["font.size"])
    plt.xticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.yticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "ants_per_package.pdf"))
    plt.close()

    # Plot 3: Jitter
    plt.figure(figsize=(8, 5))
    plt.plot(timestamps, jitter, marker="^")
    plt.xlabel("Time (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.ylabel("Jitter (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.title("Jitter Over Time", fontsize=1.5 * plt.rcParams["font.size"])
    plt.xticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.yticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "jitter.pdf"))
    plt.close()

    plt.figure(figsize=(8, 5))
    plt.plot(timestamps, average_delay, marker="*")
    plt.xlabel("Time (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.ylabel("Average Packet Delay (s)", fontsize=1.5 * plt.rcParams["font.size"])
    plt.title("Average Packet Delay Over Time", fontsize=1.5 * plt.rcParams["font.size"])
    plt.xticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.yticks(fontsize=1.5 * plt.rcParams["font.size"])
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "average_packet_delay.pdf"))
    plt.close()

def print_every_two_minutes(every_two_minutes_list, out_file):

    out_file.write(f"\nEvery two minutes:\n")
    for item in every_two_minutes_list:
        out_file.write(f"Time: {item[0]}\n")
        out_file.write(f"\nNumber of UDP packages sent: {item[1]['Packages sent']}\n")
        out_file.write(f"Number of UDP packages received: {item[1]['Number of packages']}\n")
        out_file.write(f"Number of packages lost: {item[1]['Packages sent'] - item[1]['Number of packages']}\n")

        out_file.write(f"\nAnts sent:\n")
        all_ants_sent = 0
        for key, value in item[1]["Ants Count"].items():
            if isinstance(value, dict):
                out_file.write(f"\t{key}:\n")
                for k, v in value.items():
                    out_file.write(f"\t\t{k}: {v}\n")
                    if k == "Sum":
                        all_ants_sent += v
            else:
                out_file.write(f"\t{key}: {value}\n")
                all_ants_sent += value

        out_file.write(f"\nTotal number of ants sent: {all_ants_sent}\n")

        if item[1]["Packages sent"] > 0:
            out_file.write(f"Percentage of ants sent: {all_ants_sent / item[1]['Packages sent'] * 100}%\n")
            out_file.write(f"\nUDP Packet delivery ratio: {item[1]['Number of packages'] / item[1]['Packages sent'] * 100}%\n")
        else:
            out_file.write("No packets sent\n")

        if item[1]["Number of packages"] > 0:
            len_time_between_packages = len(item[1]["Time Between Packages"])
            average_delay = sum(item[1]["Time Between Packages"]) / len_time_between_packages
            out_file.write(f"Average packet delay: {average_delay} seconds\n")
            jitter = sqrt(1 / len_time_between_packages * sum([(d - average_delay) ** 2 for d in item[1]["Time Between Packages"]]))
            out_file.write(f"Jitter: {jitter} seconds\n")
        else:
            out_file.write("No packets received\n")

        out_file.write(f"\nEnergest:\n")
        out_file.write(f"\tSum: {item[1]["Energest"]['Sum']}\n")
        _energest = item[1]["Energest"]
        energest_sum = _energest['Sum']
        if energest_sum != 0:
            out_file.write(f"\tAverage CPU: {_energest['CPU'] / energest_sum}\n")
            out_file.write(f"\tAverage LPM: {_energest['LPM'] / energest_sum}\n")
            out_file.write(f"\tAverage Deep LPM: {_energest['Deep LPM'] / energest_sum}\n")
            out_file.write(f"\tAverage Listen: {_energest['Listen'] / energest_sum}\n")
            out_file.write(f"\tAverage Transmit: {_energest['Transmit'] / energest_sum}\n")
            out_file.write(f"\tAverage Off: {_energest['Off'] / energest_sum}\n")

        out_file.write("------------------------------------------\n\n")


def read_file_lines(input_path, output_path):
    os.makedirs(output_path.split('.')[0], exist_ok=True)
    if not os.path.isfile(input_path):
        print(f"Error: '{input_path}' does not exist or is not a file.")
        return

    positions = []
    packages_sent = 0
    packages_sent_in_two_minutes = 0
    number_of_packages = 0
    number_of_packages_in_two_minutes = 0
    time_between_packages = []
    time_between_packages_two_minutes = []
    ants_count = {"Reactive forward ant":{"Broadcast":0, "Unicast":0, "Sum":0}, "Proactive forward ant":{"Broadcast":0, "Unicast":0, "Sum":0}, "Path repair ant":0, "Link failure notification":0, "Warning message":0, "Backward ant":0}
    ants_count_two_minutes = {"Reactive forward ant": {"Broadcast": 0, "Unicast": 0, "Sum": 0},
                  "Proactive forward ant": {"Broadcast": 0, "Unicast": 0, "Sum": 0}, "Path repair ant": 0,
                  "Link failure notification": 0, "Warning message": 0, "Backward ant": 0}
    energest = {"Sum":0.0, "CPU":0.0, "LPM":0.0, "Deep LPM":0.0, "Listen":0.0, "Transmit":0.0, "Off":0.0}
    energest_two_minutes = {"Sum": 0.0, "CPU": 0.0, "LPM": 0.0, "Deep LPM": 0.0, "Listen": 0.0, "Transmit": 0.0, "Off": 0.0}
    energest_old = {"Sum":0.0, "CPU":0.0, "LPM":0.0, "Deep LPM":0.0, "Listen":0.0, "Transmit":0.0, "Off":0.0}
    time_thresh = 120000000
    every_two_minutes = []
    end = False

    try:
        with open(input_path, 'r', encoding='utf-8') as infile, \
                open(output_path, 'w', encoding='utf-8') as outfile:
            for line_number, line in enumerate(infile, start=1):
                if "x:" and "y:" in line:
                    sp = line.split("\t")
                    positions.append((int(sp[1].split(":")[1]), float(sp[2].split(":")[1]), float(sp[3].split(":")[1].strip())))

                if not end:
                    if "Simulation-End" in line:
                        end = True
                        continue

                    if "Send package with content" in line:
                        packages_sent += 1
                        packages_sent_in_two_minutes += 1
                        st = line.split('\t')[0]
                        outfile.write(f"Package sent at: {st} \n")

                    if "UDP Package received" in line:
                        number_of_packages += 1
                        number_of_packages_in_two_minutes += 1

                    if "Time difference was" in line:
                        time_of_package = float(line.split(" ")[-2])
                        outfile.write(f"Package received and time difference was {time_of_package} seconds\n")
                        time_between_packages.append(time_of_package)
                        time_between_packages_two_minutes.append(time_of_package)

                    if "Reactive Forward Ant" in line:
                        ants_count["Reactive forward ant"]["Sum"] += 1
                        ants_count_two_minutes["Reactive forward ant"]["Sum"] += 1
                        if "broadcast" in line:
                            ants_count["Reactive forward ant"]["Broadcast"] += 1
                            ants_count_two_minutes["Reactive forward ant"]["Broadcast"] += 1
                        if "unicast" in line:
                            ants_count["Reactive forward ant"]["Unicast"] += 1
                            ants_count_two_minutes["Reactive forward ant"]["Unicast"] += 1

                    if "Proactive forward ant" in line:
                        ants_count["Proactive forward ant"]["Sum"] += 1
                        ants_count_two_minutes["Proactive forward ant"]["Sum"] += 1
                        if "broadcast" in line:
                            ants_count["Proactive forward ant"]["Broadcast"] += 1
                            ants_count_two_minutes["Proactive forward ant"]["Broadcast"] += 1
                        if "unicast" in line:
                            ants_count["Proactive forward ant"]["Unicast"] += 1
                            ants_count_two_minutes["Proactive forward ant"]["Unicast"] += 1

                    if "Path repair ant" in line:
                        ants_count["Path repair ant"] += 1
                        ants_count_two_minutes["Path repair ant"] += 1
                    if "Link failure notification" in line:
                        ants_count["Link failure notification"] += 1
                        ants_count_two_minutes["Link failure notification"] += 1
                    if "Warning message" in line:
                        ants_count["Warning message"] += 1
                        ants_count_two_minutes["Warning message"] += 1
                    if "Backward ant" in line:
                        ants_count["Backward ant"] += 1
                        ants_count_two_minutes["Backward ant"] += 1

                    if "CPU" in line:
                        energest_two_minutes["Sum"] += 1.0
                        energest_two_minutes["CPU"] += float(line.split(" ")[-2])
                    if "LPM" in line:
                        energest_two_minutes["LPM"] += float(line.split(" ")[-2])
                    if "DEEP LPM" in line:
                        energest_two_minutes["Deep LPM"] += float(line.split(" ")[-2])
                    if "Radio LISTEN" in line:
                        energest_two_minutes["Listen"] += float(line.split(" ")[-2])
                    if "Radio TRANSMIT" in line:
                        energest_two_minutes["Transmit"] += float(line.split(" ")[-2])
                    if "Radio OFF" in line:
                        energest_two_minutes["Off"] += float(line.split(" ")[-2])
                if end:
                    if "CPU" in line:
                        energest["Sum"] += 1.0
                        energest["CPU"] += float(line.split(" ")[-2])
                    if "LPM" in line:
                        energest["LPM"] += float(line.split(" ")[-2])
                    if "DEEP LPM" in line:
                        energest["Deep LPM"] += float(line.split(" ")[-2])
                    if "Radio LISTEN" in line:
                        energest["Listen"] += float(line.split(" ")[-2])
                    if "Radio TRANSMIT" in line:
                        energest["Transmit"] += float(line.split(" ")[-2])
                    if "Radio OFF" in line:
                        energest["Off"] += float(line.split(" ")[-2])

                try:
                    time = int(line.split("\t")[0])
                    if time >= time_thresh:
                        if energest_old["Sum"] != 0.0:
                            energest_difference = {"Sum": energest_two_minutes["Sum"],
                                                  "CPU": energest_two_minutes["CPU"] - energest_old["CPU"],
                                                  "LPM": energest_two_minutes["LPM"] - energest_old["LPM"],
                                                  "Deep LPM": energest_two_minutes["Deep LPM"] - energest_old["Deep LPM"],
                                                  "Listen": energest_two_minutes["Listen"] - energest_old["Listen"],
                                                  "Transmit": energest_two_minutes["Transmit"] - energest_old["Transmit"],
                                                  "Off": energest_two_minutes["Off"] - energest_old["Off"]}
                        else:
                            energest_difference = copy.deepcopy(energest_two_minutes)
                        every_two_minutes.append((copy.deepcopy(time), {"Packages sent": copy.deepcopy(packages_sent_in_two_minutes),
                                                                        "Number of packages": copy.deepcopy(
                                                                            number_of_packages_in_two_minutes),
                                                                        "Time Between Packages": copy.deepcopy(
                                                                            time_between_packages_two_minutes),
                                                                        "Ants Count": copy.deepcopy(ants_count_two_minutes),
                                                                        "Energest": copy.deepcopy(energest_difference)}))
                        time_thresh += 120000000
                        time_between_packages_two_minutes = []
                        number_of_packages_in_two_minutes = 0
                        packages_sent_in_two_minutes = 0
                        ants_count_two_minutes = {"Reactive forward ant": {"Broadcast": 0, "Unicast": 0, "Sum": 0},
                                                 "Proactive forward ant": {"Broadcast": 0, "Unicast": 0, "Sum": 0},
                                                 "Path repair ant": 0,
                                                 "Link failure notification": 0,
                                                 "Warning message": 0,
                                                 "Backward ant": 0}
                        energest_old = copy.deepcopy(energest_two_minutes)
                        energest_two_minutes = {"Sum": 0.0, "CPU": 0.0, "LPM": 0.0, "Deep LPM": 0.0, "Listen": 0.0,
                                                "Transmit": 0.0, "Off": 0.0}

                except:
                    continue

            print_every_two_minutes(every_two_minutes, outfile)

            outfile.write(f"\nFinal Results:\n")
            outfile.write(f"\nNumber of UDP packages sent: {packages_sent}\n")
            outfile.write(f"Number of UDP packages received: {number_of_packages}\n")
            outfile.write(f"Number of packages lost: {packages_sent - number_of_packages}\n")

            outfile.write(f"\nAnts sent:\n")
            all_ants_sent = 0
            for key, value in ants_count.items():
                if isinstance(value, dict):
                    outfile.write(f"\t{key}:\n")
                    for k, v in value.items():
                        outfile.write(f"\t\t{k}: {v}\n")
                        if k == "Sum":
                            all_ants_sent += v
                else:
                    outfile.write(f"\t{key}: {value}\n")
                    all_ants_sent += value


            outfile.write(f"\nTotal number of ants sent: {all_ants_sent}\n")

            if packages_sent > 0:
                outfile.write(f"Percentage of ants sent: {all_ants_sent / packages_sent * 100}%\n")
                outfile.write(f"\nUDP Packet delivery ratio: {number_of_packages / packages_sent * 100}%\n")
            else:
                outfile.write("No packets sent\n")

            if number_of_packages > 0:
                len_time_between_packages = len(time_between_packages)
                average_delay = sum(time_between_packages) / len_time_between_packages
                outfile.write(f"Average packet delay: {average_delay} seconds\n")
                jitter = sqrt(1/len_time_between_packages * sum([(d-average_delay)**2 for d in time_between_packages]))
                outfile.write(f"Jitter: {jitter} seconds\n")
            else:
                outfile.write("No packets received\n")

            outfile.write(f"\nEnergest:\n")
            outfile.write(f"\tSum: {energest['Sum']}\n")
            if energest["Sum"] != 0:
                outfile.write(f"\tAverage CPU: {energest['CPU'] / energest['Sum']}\n")
                outfile.write(f"\tAverage LPM: {energest['LPM'] / energest['Sum']}\n")
                outfile.write(f"\tAverage Deep LPM: {energest['Deep LPM'] / energest['Sum']}\n")
                outfile.write(f"\tAverage Listen: {energest['Listen'] / energest['Sum']}\n")
                outfile.write(f"\tAverage Transmit: {energest['Transmit'] / energest['Sum']}\n")
                outfile.write(f"\tAverage Off: {energest['Off'] / energest['Sum']}\n")

    except Exception as e:
        print(f"An error occurred while reading the file: {e}")

    create_plots(every_two_minutes, output_path.split('.')[0])
    create_topology(positions, output_path.split('.')[0])

def start_analysis(input_path):
    input_file = input_path
    path = input_path.removeprefix("logfiles/")
    output_file = "outputs/" + path.split('.')[0] + "_result.txt"
    read_file_lines(input_file, output_file)

if __name__ == "__main__":

    if len(sys.argv) == 3:
        for file in os.listdir(sys.argv[1]):
            if file.endswith('.txt'):
                print(f"Open file: {os.path.join(sys.argv[1], file)}")
                start_analysis(os.path.join(sys.argv[1], file))

    elif len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Usage for one file: python analyse_log.py <input_file>")
        print("Usage for multiple files: python analyse_log.py <input_directory> all")
    else:
        start_analysis(sys.argv[1])