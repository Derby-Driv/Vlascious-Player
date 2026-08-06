# =============================================================================
# Krona Player — ARM64 Build Environment
# =============================================================================
# This image emulates the R36S target (aarch64) via QEMU so the host stays
# completely free of cross-compilation toolchains and ARM libraries.
# Base: Ubuntu 20.04 (Focal) to match ArkOS 2.0 glibc compatibility.
# =============================================================================

FROM --platform=linux/arm64 ubuntu:20.04

# Prevent interactive prompts during apt-get
ENV DEBIAN_FRONTEND=noninteractive

# ---------- System dependencies ----------
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        # SDL2
        libsdl2-dev \
        # FFmpeg
        libavcodec-dev \
        libavformat-dev \
        libswscale-dev \
        libavutil-dev \
        libswresample-dev \
        # Google Test
        libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

# ---------- Build Google Test static libs ----------
# Ubuntu 20.04 ships gtest sources only; we must compile them ourselves.
RUN cd /usr/src/gtest && cmake . && make && \
    cp lib/*.a /usr/lib/ 2>/dev/null || \
    cp *.a /usr/lib/ 2>/dev/null || true

# ---------- Working directory ----------
WORKDIR /project
