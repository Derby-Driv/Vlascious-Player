# Architecture — Krona Player

## Overview

```
┌─────────────────────────────────────────────────────┐
│                   Host (x86_64)                     │
│                                                     │
│  ┌───────────────────────────────────────────────┐  │
│  │     Docker (ARM64 via QEMU user-static)       │  │
│  │                                               │  │
│  │  Ubuntu 20.04 aarch64                         │  │
│  │  ├─ FFmpeg dev libs                           │  │
│  │  ├─ SDL2 dev libs                             │  │
│  │  ├─ GTest                                     │  │
│  │  └─ CMake + GCC (aarch64-native)              │  │
│  │                                               │  │
│  │  Volume mount: host project ↔ /project        │  │
│  └───────────────────────────────────────────────┘  │
│                                                     │
│  Output: build/krona_player (ELF aarch64)             │
└─────────────────────────────────────────────────────┘
```

## Why Docker Multiarch?

The R36S runs ArkOS 2.0 on an **aarch64** (ARM64) SoC. The developer's machine is typically **x86_64**. We have two options:

1. **Cross-compilation toolchain** — Install an ARM64 cross-compiler, ARM64 versions of FFmpeg/SDL2 headers and `.so` files, and configure CMake to target a foreign architecture. This pollutes the host with architecture-specific packages and is fragile to maintain.

2. **Docker multiarch + QEMU** ✅ — Run a native ARM64 Ubuntu container under QEMU user-space emulation. Inside the container, `gcc` and all libraries are genuine ARM64 packages. No cross-compilation flags are needed; CMake runs as if on real ARM64 hardware. The host only needs Docker and `qemu-user-static`.

We chose option 2. The trade-off is build speed (QEMU emulation is ~5-10× slower than native), but correctness and maintainability are far superior.

---

## Runtime Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        main thread                           │
│                                                              │
│   SDL_Init(VIDEO | AUDIO)                                    │
│          │                                                   │
│          ├──→ Create SDL_Window + SDL_Renderer               │
│          │    (fullscreen, DRM/KMS on R36S)                   │
│          │                                                   │
│          ├──→ Create SDL_Texture (YUV420P / IYUV)            │
│          │                                                   │
│          ├──→ Open audio device (SDL_OpenAudioDevice)         │
│          │                                                   │
│          └──→ Event loop ─────────────────────────────┐      │
│                  │                                     │      │
│                  ├─ SDL_PollEvent() → key/gamepad      │      │
│                  │   SPACE / A  → toggle pause         │      │
│                  │   ESCAPE / START → quit              │      │
│                  │                                     │      │
│                  ├─ decoder.readFrame()                 │      │
│                  │   └─ av_read_frame + decode          │      │
│                  │                                     │      │
│                  ├─ SDL_UpdateYUVTexture(frame)         │      │
│                  │                                     │      │
│                  └─ SDL_RenderPresent()                 │      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Decoding Thread (FFmpeg)

The `Decoder` class encapsulates all FFmpeg interaction:

| Step | FFmpeg Call | Purpose |
|------|-----------|---------|
| 1 | `avformat_open_input` | Open the MP4 container |
| 2 | `avformat_find_stream_info` | Probe stream metadata |
| 3 | `avcodec_find_decoder` | Locate H.264 / AAC decoder |
| 4 | `avcodec_open2` | Initialise the codec context |
| 5 | `av_read_frame` | Demux one packet |
| 6 | `avcodec_send_packet` | Feed packet to decoder |
| 7 | `avcodec_receive_frame` | Retrieve decoded frame (YUV420P) |

A `SwsContext` is available to convert pixel formats if the source is not `AV_PIX_FMT_YUV420P`, but most MP4/H.264 content is already in this format.

### Rendering Thread (SDL2)

SDL2 on the R36S uses the **DRM/KMS** video backend (`SDL_VIDEODRIVER=kmsdrm`). On the development host inside Docker, there is no display — rendering is only meaningful on the actual device.

Key rendering flow:
1. `SDL_UpdateYUVTexture` — zero-copy upload of YUV planes from the decoded `AVFrame`.
2. `SDL_RenderCopy` — blit texture to the back buffer.
3. `SDL_RenderPresent` — flip buffers (V-Sync is handled by the DRM backend on R36S).

### Audio Pipeline

Audio is handled via SDL2's audio callback mechanism:
1. FFmpeg decodes audio packets into PCM samples.
2. `SwrContext` resamples to the format expected by SDL2 (signed 16-bit, stereo, 44100 Hz).
3. SDL2 pulls samples via its audio callback.

> **Note:** Audio/video synchronisation relies on a combination of frame-pacing and an audio buffer capacity threshold. If the audio buffer exceeds a certain size (e.g. 1 second of audio), the video loop temporarily pauses decoding to allow the SDL audio thread to drain the buffer, keeping playback smoothly in sync.

---

## Project Layout

```
MPlayer/
├── Dockerfile              # ARM64 build environment
├── Makefile                 # Host-side orchestration
├── CMakeLists.txt           # Build system
├── Krona_Player.sh           # ArkOS launcher script
│
├── include/
│   ├── app.h                # App lifecycle and SDL context
│   ├── decoder.h            # FFmpeg wrapper API
│   ├── menu.h               # UI and filesystem scanner
│   └── video_player.h       # A/V playback and sync logic
├── src/
│   ├── app.cpp
│   ├── decoder.cpp
│   ├── main.cpp             # Minimal entry point
│   ├── menu.cpp
│   └── video_player.cpp
├── tests/
│   └── test_core.cpp        # Google Test suite
│
├── docs/
│   └── ARCHITECTURE.md      # This file
│
└── .github/
    └── workflows/
        └── ci.yml           # GitHub Actions CI
```

---

## Component Dependency Graph

```
┌──────────┐     ┌──────────────┐     ┌───────────┐
│  main()  │────→│ App (Window) │────→│   Menu    │
│  (exe)   │     └──────┬───────┘     └───────────┘
└──────────┘            │
                        ▼             ┌───────────┐
                 ┌──────────────┐     │  FFmpeg   │
                 │ VideoPlayer  │────→│ (Decoder) │
                 └──────────────┘     └───────────┘
```

The application is highly modular. `App` manages global state and SDL initialization. It orchestrates `Menu` (for UI) and `VideoPlayer` (for media playback). `VideoPlayer` delegates low-level video decoding to the `Decoder` class.
