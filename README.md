# 🎬 Krona Player

A native C++ MP4 video player built specifically for the **R36S** handheld console running **ArkOS 2.0** (aarch64/ARM64).

Uses **FFmpeg** for decoding and **SDL2** for rendering and audio output — no heavy GUI frameworks, just fast native playback optimised for the R36S hardware.

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

## 🎮 Deploy to R36S (Offline)

The R36S **does not have WiFi**. All file transfers are done via USB OTG or by removing the SD card.

### Method 1 — USB OTG (recommended)

1. Copy the following files to a USB flash drive:
   ```
   kronaplayer/
   ├── krona_player          ← the compiled ARM64 binary
   ├── assets/
   │   └── font.ttf          ← required font for menu
   └── videos/
       └── myvideo.mp4     ← your video file(s)
   ```
2. Copy `Krona_Player.sh` to the root of the flash drive.
3. Plug the USB flash drive into the R36S **OTG port** (the port closest to the shoulder buttons).
4. On the R36S, open **File Manager** (under the Tools section in ArkOS).
5. Copy `kronaplayer/` → `/roms/ports/kronaplayer/`
6. Copy `Krona_Player.sh` → `/roms/ports/`
7. Restart EmulationStation or reboot. The player will appear under **Ports**.

### Method 2 — SD Card

1. Power off the R36S and remove the SD card.
2. Mount the SD card on your PC.
3. Copy the same files to the equivalent paths on the SD card's `EASYROMS` partition:
   - `ports/kronaplayer/krona_player`
   - `ports/kronaplayer/assets/font.ttf`
   - `ports/kronaplayer/videos/`
   - `ports/Krona_Player.sh`
4. Unmount, reinsert, and boot.

### Running

Launch **"Krona Player"** from the **Ports** menu in ArkOS. 
It will open a video selection menu listing all `.mp4` files inside `/roms/ports/kronaplayer/videos/`.

**Controls:**

| R36S Button | Action                                |
|-------------|---------------------------------------|
| D-Pad UP/DN | Navigate video list                   |
| A           | Select video / Play / Pause           |
| B           | Return to menu (when playing)         |
| START       | Quit (or return to menu when playing) |

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
