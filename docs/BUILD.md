# Building FPS Timer on Windows

This guide builds the portable Windows x64 application from source using Microsoft C++ Build Tools. The full Visual Studio IDE is optional.

**Windows only:** the project supports Windows 10/11 x64. Linux and macOS builds are not supported.

## 1. Install the tools

Download [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/). In Visual Studio Installer, select **Desktop development with C++** and include:

- MSVC C++ build tools for x64/x86.
- Windows 10 or Windows 11 SDK.
- C++ CMake tools for Windows.

Install Git if you want to clone the repository. The build script uses PowerShell and discovers MSVC through Visual Studio Installer's `vswhere.exe`. It uses the bundled CMake when available, otherwise CMake from `PATH`.

The project requires C++17 and CMake 3.20 or newer. Use a CMake version that supports your installed Visual Studio release; the bundled version is recommended. The verified setup is Visual Studio Build Tools 2026 with Windows SDK 10.0.26100.0.

## 2. Get the source

In PowerShell, navigate to the directory where you keep projects and run:

```powershell
git clone https://github.com/koolakoff/fps-timer.git
cd fps-timer
```

If you already have the project, open PowerShell in its root directory, alongside `build.ps1` and `CMakeLists.txt`.

## 3. Build the portable executable

Close any running copy of FPS Timer so its executable can be replaced, then run:

```powershell
.\build.ps1
```

The script automatically:

1. Locates the installed MSVC tools and CMake.
2. Configures an x64 build in `build/`, with tests enabled.
3. Compiles the application and tests in Release mode.
4. Runs the stopwatch tests using CTest.
5. Copies the executable to `dist/fps-timer.exe`.

Launch the result:

```powershell
.\dist\fps-timer.exe
```

The script does not require a Developer Command Prompt or the full Visual Studio IDE.

## 4. Optional graphics and display checks

Close the normal application before starting these checks. A graphics smoke test briefly opens a fullscreen window and exercises the actual input handlers and renderer:

```powershell
.\build.ps1 -SmokeTest
```

To also test switching to another available refresh rate and restoring the initial rate:

```powershell
.\build.ps1 -SmokeTest -TestRefresh
```

The refresh test can briefly blank the display while Windows changes modes. `-TestRefresh` is only used together with `-SmokeTest`.

Generated diagnostics are saved under `build/`:

| File | Contents |
|---|---|
| `smoke-test.txt` | Display information and test result |
| `smoke-dark.png` | Application back buffer in the dark theme |
| `smoke-light.png` | Application back buffer in the light theme |
| `smoke-alternate-rate.png` | Application at the alternate rate when refresh testing is enabled |

## 5. Build with CMake directly

If `cmake` and `ctest` are available in your terminal, use:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

With the Visual Studio generator, the application is written to `build/Release/fps-timer.exe`. Direct CMake commands do not copy it into `dist/`; run `build.ps1` or copy it yourself.

For a separate Debug build:

```powershell
cmake -S . -B build-debug -A x64 -DBUILD_TESTING=ON
cmake --build build-debug --config Debug --parallel
ctest --test-dir build-debug -C Debug --output-on-failure
```

Keep this optional `build-debug/` directory out of commits; the default `.gitignore` excludes `build/`, not every possible custom build directory.

## 6. Distribute the result

The Release executable statically links the MSVC runtime (`/MT`) and uses graphics components included with Windows 10/11. It does not require an installer, administrator rights, the Visual C++ Redistributable, or adjacent application DLLs.

For a distribution package, include:

- `dist/fps-timer.exe`.
- The repository's `LICENSE` file, preserving the MIT license and copyright notice.

End users do not need the compiler, SDK, source files, or `build/` directory. `dist/`, `build/`, and `.tools/` are ignored by Git, so cloning the repository does not provide a prebuilt executable.

## Troubleshooting

| Problem | What to check |
|---|---|
| PowerShell blocks `build.ps1` | If local policy permits, run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1` for this invocation. This does not change the saved execution policy. |
| MSVC tools are not found | Modify the installation and add Desktop development with C++ and MSVC x64/x86 tools. |
| CMake is not found | Add C++ CMake tools for Windows through Visual Studio Installer, or make a compatible CMake installation available in `PATH`. |
| Windows SDK or resource compiler is missing | Install a Windows SDK through Visual Studio Installer. |
| CMake reports an old or incompatible generator | Configure into a new build directory. A CMake build directory retains its original generator and toolchain. |
| The EXE cannot be linked or copied | Close the running FPS Timer process and retry. |
| Graphics smoke test fails | Ensure the normal app is closed and an interactive desktop session is available. Read `build/smoke-test.txt` if it was generated. |

See [VALIDATION.md](VALIDATION.md) for tested hardware and current verification limits.
