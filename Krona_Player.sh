#!/bin/bash
# =============================================================================
# Krona Player — ArkOS Launcher Script
# =============================================================================
# Place this file at: /roms/ports/Krona_Player.sh
# Place the binary at: /roms/ports/kronaplayer/krona_player
# Place videos at:     /roms/ports/kronaplayer/videos/
# Place font at:       /roms/ports/kronaplayer/assets/font.ttf
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

# ---- Navigate to player directory ----
cd "${PLAYER_DIR}" || exit 1

# ---- Launch the player (File Browser Mode) ----
"${BINARY}"

# ---- Return control to ArkOS ----
exit 0
