#!/bin/bash
# =============================================================================
# Krona Player — ArkOS Launcher Script
# =============================================================================
# Place this file at: /roms/ports/Krona_Player.sh
# Place the binary at: /roms/ports/kronaplayer/krona_player
# Place videos at:     /roms/ports/kronaplayer/videos/
#
# ArkOS will show this script as an entry in the "Ports" menu.
# =============================================================================

# ---- Environment setup for the R36S hardware ----
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
export SDL_GAMECONTROLLERCONFIG="$(cat /roms/ports/gamecontrollerdb.txt 2>/dev/null)"

# ---- Paths ----
PLAYER_DIR="/roms/ports/kronaplayer"
BINARY="${PLAYER_DIR}/krona_player"
VIDEO_DIR="${PLAYER_DIR}/videos"

# ---- Navigate to player directory ----
cd "${PLAYER_DIR}" || exit 1

# ---- Find the first .mp4 file ----
VIDEO_FILE=$(find "${VIDEO_DIR}" -maxdepth 1 -iname "*.mp4" -type f | head -n 1)

if [ -z "${VIDEO_FILE}" ]; then
    echo "ERROR: No .mp4 files found in ${VIDEO_DIR}" >&2
    sleep 3
    exit 1
fi

# ---- Launch the player ----
echo "Playing: ${VIDEO_FILE}"
"${BINARY}" "${VIDEO_FILE}"

# ---- Return control to ArkOS ----
exit 0
