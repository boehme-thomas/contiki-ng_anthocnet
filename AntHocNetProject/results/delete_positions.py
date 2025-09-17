import sys

def change_positions(input_path):
    try:
        with open(input_path, 'r', encoding='utf-8') as infile:
            lines = infile.readlines()

        with open(input_path, 'w', encoding='utf-8') as outfile:
            for index, line in enumerate(lines):
                if float(line.split(" ")[1]) >= 1:
                    if int(line.split(" ")[0]) in range(50, 100):
                        continue
                outfile.writelines(line)

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python delete_positions.py <input_path>")
        sys.exit(1)

    path = sys.argv[1]
    change_positions(path)