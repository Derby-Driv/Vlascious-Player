# 🎬 Krona Player

A native C++ MP4 video player built specifically for the **R36S** handheld console running **ArkOS 2.0** (aarch64/ARM64).

Uses **FFmpeg** for decoding and **SDL2** for rendering and audio output — no heavy GUI frameworks, just fast native playback optimised for the R36S hardware. It features full A/V sync, hardware scaling, and video seeking.

---

## ⚡ Quick Start

> **Prerequisite:** Docker must be installed on your development machine.

```bash
# 1. (One-time) Install QEMU user-static for ARM64 emulation
make setup

# 2. Build the project (compiles inside an ARM64 Docker container)
make build
```

That's it. The compiled ARM64 binary will appear at `build/krona_player`.

### Other Make targets

| Command       | Description                                    |
|---------------|------------------------------------------------|
| `make setup`     | Install QEMU user-static on the host (once)     |
| `make build-env` | Build the ARM64 Docker image only               |
| `make build`     | Compile the project inside the ARM64 container   |
| `make test`      | Run unit tests inside the ARM64 container        |
| `make clean`     | Remove the local `build/` directory              |

---

## 🎮 Deploy to R36S

Transfer the contents of the `output/` folder (or the extracted `KronaPlayer.zip` release) into the `/roms/ports/` directory on your R36S (the `EASYROMS` partition).

The final structure on your R36S should look exactly like this:
- `/roms/ports/Krona_Player.sh`
- `/roms/ports/images/`
- `/roms/ports/kronaplayer/`

Place your `.mp4` video files inside the `/roms/ports/kronaplayer/videos/` directory.

Once the files are in place, restart EmulationStation or reboot the console. The player will appear under the **Ports** menu with its cover art!

### Running

Launch **"Krona Player"** from the **Ports** menu in ArkOS. 
It will open a video selection menu listing all `.mp4` files inside `/roms/ports/kronaplayer/videos/`.

**Controls:**

| R36S Button | Action                                |
|-------------|---------------------------------------|
| D-Pad UP/DN | Navigate video list                   |
| D-Pad L/R   | Seek backward/forward 10 seconds      |
| A           | Select video / Play / Pause           |
| B           | Return to menu                        |
| START       | Quit                                  |

---

## 🏗 Architecture

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for a detailed technical overview.

**Key decisions:**
- **Docker multiarch build** — the host machine is never polluted with ARM64 libraries.
- **Two-thread architecture** — FFmpeg decodes in a worker thread; SDL2 renders in the main thread.
- **Static core library** — business logic (`core_lib`) is separated from `main()` so it can be tested with Google Test without initialising SDL2.

---

## 🧪 Testing

```bash
make test
```

Tests run inside the same ARM64 Docker container using Google Test and CTest.

---

## 📄 License

MIT
