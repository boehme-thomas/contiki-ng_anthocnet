import sys

def change_positions(input_path):
    try:
        with open(input_path, 'r', encoding='utf-8') as infile:
            lines = infile.readlines()
        for line_number in range(100, len(lines)):
            parts = lines[line_number].split(" ")
            if len(parts) > 1:
                try:
                    time_stamp = int(parts[1])
                    time_stamp += 108
                    parts[1] = str(time_stamp)
                    lines[line_number] = " ".join(parts)
                except ValueError:
                    pass
        with open(input_path, 'w', encoding='utf-8') as outfile:
            outfile.writelines(lines)

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python change_positions.py <input_path>")
        sys.exit(1)

    path = sys.argv[1]
    change_positions(path)