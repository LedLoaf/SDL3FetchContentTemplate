# SDL3 + Dear ImGui Demo

A minimal C++ project demonstrating [SDL3](https://github.com/libsdl-org/SDL) and [Dear ImGui](https://github.com/ocornut/imgui) integrated via CMake's `FetchContent` — no manual dependency installation required.

## Features

- **Zero-setup dependencies** — all libraries are fetched and built automatically at configure time
- **SDL3** for windowing, rendering, and input
- **Dear ImGui** for immediate-mode GUI
- **Single demo file** showing a working render loop with ImGui widgets

## Requirements

- CMake ≥ 3.24
- A C++17 compiler (GCC, Clang, or MSVC)
- An internet connection (for `FetchContent` to download dependencies)

## Building

```bash
cmake -B build 
or
cmake -S . -B build -G "Visual Studio 18 2026" -A x64

cmake --build build
```

## Running

```bash
./build/demo
```

A window will open with an ImGui demo panel. Close the window to exit.

## Project Structure

```
├── CMakeLists.txt
├── src/
│   └── main.cpp        # Demo: SDL3 window + ImGui render loop
└── README.md
```

## How It Works

`CMakeLists.txt` uses `FetchContent` to pull:

| Dependency | Source |
|---|---|
| SDL3 | `https://github.com/libsdl-org/SDL` |
| Dear ImGui | `https://github.com/ocornut/imgui` |

Both are built as part of the project — nothing is installed system-wide.

## License

This project is in the public domain.   