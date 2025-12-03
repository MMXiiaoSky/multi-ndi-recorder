# Multi NDI Recorder

A Qt 6 desktop utility for Windows that records multiple NDI sources in parallel with continuous or segmented MP4 output and a built-in recordings browser.

## Features at a glance
- Configure 1–10 NDI inputs, each with preview, start/stop/pause controls, and a per-source timer; global Start/Pause/Stop manage every recorder at once.
- Per-source settings dialog to pick NDI source, output folder, labeling, and continuous vs. segmented recording durations.
- Native-resolution H.264 MP4 writing with optional time-based segment rollover handled by the FFmpeg pipeline.
- Recording library tab lists completed files with open/reveal actions, plus simple metadata scanning.
- Lightweight logging to `logs/app.log` for capture and muxing events.

## Prerequisites (install first)
- **Windows 10/11 64-bit** with the **Desktop development with C++** workload from Visual Studio 2019/2022 (MSVC, Windows SDK, CMake, and Ninja if desired).
- **Qt 6 (Widgets)**: install a matching MSVC build (e.g., 6.5+). Note the `CMAKE_PREFIX_PATH` to its `lib/cmake` directory.
- **NDI 5 SDK**: install and record the `Include` and `Lib/x64` directories.
- **FFmpeg dev libraries** built for MSVC with import libraries (`avformat`, `avcodec`, `avutil`, `swscale`) and headers available.

## Configure and build
1. Open a **x64 Native Tools** developer prompt for your Visual Studio version.
2. Clone this repository and enter it:
   ```powershell
   git clone <repo-url>
   cd multi-ndi-recorder
   ```
3. Configure CMake with your dependency paths (adjust as needed):
   ```powershell
   cmake -S . -B build -G "Ninja" \
     -DNDI_SDK_INCLUDE="C:/Program Files/NDI SDK/Include" \
     -DNDI_SDK_LIB="C:/Program Files/NDI SDK/Lib/x64" \
     -DFFMPEG_INCLUDE="C:/ffmpeg/include" \
     -DFFMPEG_LIB="C:/ffmpeg/lib" \
     -DCMAKE_PREFIX_PATH="C:/Qt/6.5.2/msvc2019_64/lib/cmake"
   ```
   > You can replace `Ninja` with `"Visual Studio 17 2022" -A x64` if you prefer an IDE solution. The placeholder paths correspond to the cache variables exposed in `CMakeLists.txt`.
4. Build the application:
   ```powershell
   cmake --build build --config Release
   ```
5. Run the app (from the build tree or after installation):
   ```powershell
   build/Release/MultiNdiRecorder.exe
   ```

## Using the application
1. **Set source count**: Use the spin box at the top to choose how many NDI tiles to display (1–10). Tiles show preview, status, and an elapsed timer.
2. **Configure each source**: Click **Settings** on a tile to pick the NDI source, output folder, label, and continuous vs. segmented duration. Press **Refresh** to rescan sources.
3. **Start recording**: Hit **Start** on a tile or **Start All** for every source. Pause/Resume keeps the file active; Stop finalizes it. Segmented mode automatically rolls over files at the chosen minute interval.
4. **Library tab**: Switch to the Recordings tab to see captured files. Double-click **Open** to launch in the default player or **Reveal** to highlight in Explorer.
5. **Logs**: Review `logs/app.log` for capture, NDI, and FFmpeg events when diagnosing issues.

## Notes and tips
- Ensure output folders exist and are writable before starting a session.
- NDI and FFmpeg binaries must be discoverable at run time (e.g., via PATH or next to the executable) so their dependent DLLs load correctly.
- For best disk stability, record to fast local storage rather than network shares.
