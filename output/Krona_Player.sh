#!/bin/bash
# =============================================================================
# Krona Player — ArkOS Launcher Script
# =============================================================================

# ---- Environment setup for the R36S hardware ----
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
export SDL_GAMECONTROLLERCONFIG="$(cat /roms/ports/gamecontrollerdb.txt 2>/dev/null)"

# ---- Navigate to player directory ----
cd "$(dirname "$0")/kronaplayer" || exit 1

# ---- Launch the player (File Browser Mode) ----
./krona_player

# ---- Return control to ArkOS ----
exit 0
