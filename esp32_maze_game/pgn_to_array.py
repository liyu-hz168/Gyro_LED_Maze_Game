from PIL import Image

def load_maze(number):
    filename = f"mazes/maze{number}.png"
    img = Image.open(filename).convert("RGBA")
    width, height = img.size
    pixels = img.load()
    maze = []

    for y in range(height):
        row = []
        for x in range(width):
            r, g, b, alpha = pixels[x, y]
            # Transparent pixel alpha = 0
            row.append(0 if alpha == 0 else 1)
        maze.append(row)

    return maze


def print_as_arduino_array(maze, name="maze"):
    print(f"uint8_t {name}[32][32] = {{")
    for row in maze:
        row_str = ", ".join(str(v) for v in row)
        print(f"  {{ {row_str} }},")
    print("};")


if __name__ == "__main__":
    num = int(input("Enter maze number: "))
    maze = load_maze(num)
    print_as_arduino_array(maze, name=f"maze{num}")
