# OpticSketch

A lightweight desktop application for creating publication-quality optical bench diagrams.

## Phase 1: Foundation

This phase includes:
- GLFW window with OpenGL 3.3 context
- ImGui integration with Moonlight theme
- Basic project structure

## Building

### Prerequisites

- **CMake** 3.15 or higher
- **C++17** compatible compiler (GCC, Clang, or MSVC)
- **OpenGL 3.3** compatible graphics driver
- **Python 3** (for GLAD loader generation)
- **GLAD** Python module: `pip install glad`

#### Linux (Ubuntu/Debian)

Install system packages:
```bash
sudo apt-get install libgl1-mesa-dev libglfw3-dev
```

#### Windows

- **Visual Studio 2022** (or Visual Studio Build Tools 2022) with the "Desktop development with C++" workload, or another C++17 compiler.
- **Python 3** in your PATH (for GLAD). Install the GLAD module:
  ```powershell
  pip install glad
  ```
- **GLFW**: If not installed (e.g. via vcpkg), CMake will download it automatically via FetchContent.
- **OpenGL**: Provided by your graphics drivers; no extra install needed.

Optional – use vcpkg for GLFW instead of FetchContent:
1. Install [vcpkg](https://github.com/Microsoft/vcpkg).
2. Run: `vcpkg install glfw3`
3. Configure with: `cmake -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake -B build -S .`

### Build Steps

#### Linux

```bash
mkdir build
cd build
cmake ..
make
```

#### Windows (PowerShell) – recommended

From the project root (e.g. `C:\Work\DamnVFX\opticsketch`):

```powershell
mkdir build -ErrorAction SilentlyContinue
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

The executable is written to `build\Release\OpticSketch.exe`.

#### Windows (Command Prompt)

```cmd
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Run: `build\Release\OpticSketch.exe`

#### Windows (Visual Studio generator)

To use a specific Visual Studio version (from project root):

```powershell
mkdir build -ErrorAction SilentlyContinue
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Executable: `build\Release\OpticSketch.exe` (or `build\Debug\OpticSketch.exe` for Debug).

#### Clean rebuild (Windows)

If the app behavior doesn’t match your code changes, do a full clean rebuild:

```powershell
# From project root
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Then run `.\Release\OpticSketch.exe` from the `build` folder, or `.\build\Release\OpticSketch.exe` from the project root.

#### CMake / GLM note (Windows)

If CMake reports an error about "Compatibility with CMake < 3.5 has been removed" when configuring GLM, pass the policy minimum:

```powershell
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

The project’s `CMakeLists.txt` sets this for the GLM subproject when needed.

### GLAD Setup

GLAD requires Python to generate the OpenGL loader. If Python is not found during CMake configuration:

1. Install Python 3 and ensure it's in your PATH
2. Install the GLAD Python module:
   ```bash
   pip install glad
   ```
3. CMake will automatically generate GLAD files during the build process

If you need to manually generate GLAD files:
```bash
pip install glad
glad --generator=c --spec=gl --api=gl=3.3 --profile=core --out-path=glad
```

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

### Linux

From the project root:
```bash
./build/OpticSketch
```

### Windows

From the project root (PowerShell):
```powershell
.\build\Release\OpticSketch.exe
```

From the project root (Command Prompt):
```cmd
build\Release\OpticSketch.exe
```

Or from inside `build`:
```powershell
.\Release\OpticSketch.exe
```

You should see a window with the OpticSketch interface and the Moonlight theme applied.
