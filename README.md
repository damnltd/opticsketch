# OpticSketch

A lightweight desktop application for creating publication-quality optical bench diagrams.

## Phase 1: Foundation

This phase includes:
- GLFW window with OpenGL 3.3 context
- ImGui integration with Moonlight theme
- Basic project structure

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler
- OpenGL 3.3 compatible graphics driver
- Python 3 (for GLAD loader generation)
- GLAD Python module: `pip install glad`
- System packages:
  - Linux (Ubuntu/Debian): 
    ```bash
    sudo apt-get install libgl1-mesa-dev libglfw3-dev
    ```
  - Windows: Install GLFW via vcpkg or download pre-built binaries

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

### GLAD Setup

GLAD requires Python to generate the OpenGL loader. If Python is not found during CMake configuration, you can:

1. Install Python 3 and ensure it's in your PATH
2. Or manually generate GLAD files:
   ```bash
   pip install glad
   glad --generator=c --spec=gl --api=gl=3.3 --profile=core --out-path=glad
   ```
   Then update CMakeLists.txt to use the generated files.

## Project Structure

```
opticsketch/
├── CMakeLists.txt       # Build configuration
├── DESIGN.md            # Complete design document
├── README.md            # This file
└── src/
    ├── main.cpp         # Application entry point
    └── ui/
        ├── theme.h      # Theme header
        └── theme.cpp    # Moonlight theme implementation
```

## Running

After building, run:
```bash
./build/OpticSketch
```

You should see a window with ImGui demo and the Moonlight theme applied.
