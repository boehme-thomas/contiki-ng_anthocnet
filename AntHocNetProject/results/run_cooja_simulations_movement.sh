#!/bin/bash

# Usage: ./run_cooja_simulations.sh <csc_file> <num_runs> <movement_type> (to be executed in AntHocNetProject/results)
# Example: ./run_cooja_simulations.sh my_simulation.csc 5 rw

CSC_FILE="$1"
NUM_RUNS="$2"
MOVEMENT_TYPE="$3"
LOG_DIR="logfiles"
OUTPUT_DIR="outputs"
ANALYSIS_SCRIPT="analyse_log.py"
ANALYSIS_ALL_SCRIPT="analyse_multiple_logs.py"
CHANGE_POSITIONS_SCRIPT="change_positions.py"

BONNMOTION_BIN=~/Applications/bonnmotion-3.0.1/bin
WML2DAT_SCRIPT=~/Applications/wml2dat
RUNS_DIR=../runs/dat

if [ ! -f "../simulations/$CSC_FILE" ]; then
    echo "CSC simulation file $CSC_FILE not found."
    exit 1
fi

if ! [[ "$NUM_RUNS" =~ ^[0-9]+$ ]]; then
    echo "Number of runs must be an integer."
    exit 1
fi

SIM_NAME=$(basename "$CSC_FILE" .csc)

TS=$(date +'%m_%d_%H_%M')

LOG_DIR="${LOG_DIR}/${TS}_${SIM_NAME}"
OUTPUT_DIR="${OUTPUT_DIR}/${TS}_${SIM_NAME}"
mkdir -p "$LOG_DIR"

for (( i=1; i<=NUM_RUNS; i++ )); do
    echo "Starting simulation run $i..."

    echo "Create new positions"
    if [ "$MOVEMENT_TYPE" == "rw" ]; then
        echo "Using RandomWaypoint model"
        $BONNMOTION_BIN/bm -f scenario RandomWaypoint -n 100 -x 300 -y 300 -d 7200 -h 1.0 -l 0.1
    elif [ "$MOVEMENT_TYPE" == "gm" ]; then
        echo "Using GaussMarkov model"
        $BONNMOTION_BIN/bm -f scenario GaussMarkov -n 100 -x 300 -y 300 -d 7200 -h 1.0 -b
    else
        echo "Unknown movement type: $MOVEMENT_TYPE. Use 'rw' for RandomWaypoint or 'gm' for GaussMarkov."
        exit 1
    fi

    $BONNMOTION_BIN/bm WiseML -f scenario -L 1 -I
    $WML2DAT_SCRIPT scenario.wml $RUNS_DIR/positions.dat

    echo "Change position in dat file"
    python3 $CHANGE_POSITIONS_SCRIPT $RUNS_DIR/positions.dat

    echo "Start simulation"
    # Run COOJA headless, assuming COOJA.jar is in the current directory.
    # Set your JAVA path and COOJA path as needed.
    #java -jar COOJA.jar -nogui="$CSC_FILE" > "$LOG_DIR/COOJA.logfile" 2>&1
    docker run --privileged --sysctl net.ipv6.conf.all.disable_ipv6=0 --mount type=bind,source=/home/thomas/Git/ieee-802.15.4-antnet,destination=/home/user/ieee-802.15.4-antnet --mount type=bind,source=$CNG_PATH,destination=/home/user/contiki-ng --mount type=bind,source=/home/thomas/Git/contiki-ng-projektmodul,destination=/home/user/contiki-ng-projektmodul -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v /dev/bus/usb:/dev/bus/usb -v $XAUTHORITY:/home/user/.Xauthority --workdir /home/user/ieee-802.15.4-antnet -ti --rm contiker/contiki-ng_cooja cooja --args="--no-gui AntHocNetProject/simulations/$CSC_FILE --logdir=AntHocNetProject/results/$LOG_DIR"
    # Wait for simulation to finish (if needed, add checks here).

    # Generate timestamp
    TS=$(date +'%m_%d_%H_%M')

    # Rename logfile
    NEW_LOG="$LOG_DIR/${TS}_${SIM_NAME}_run${i}.txt"
    mv "$LOG_DIR/COOJA.testlog" "$NEW_LOG"

    echo "Simulation run $i complete. Log saved to $NEW_LOG"
    echo "Start analyse of log"
    # Run analysis
    python3 "$ANALYSIS_SCRIPT" "$NEW_LOG"
done

echo "Start analysis of all logs"
python3 "$ANALYSIS_ALL_SCRIPT" "$OUTPUT_DIR"

echo "All simulations complete."
