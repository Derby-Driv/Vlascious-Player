#!/bin/bash
# =============================================================================
# Krona Player — ArkOS Launcher Script
# =============================================================================
# Place the entire "kronaplayer" folder in /roms/ports/
#
# ArkOS (EmulationStation) will scan subdirectories and show this script 
# as an entry in the "Ports" menu. It will also automatically find the cover 
# art in images/Krona_Player-image.png relative to this script.
# =============================================================================

# ---- Environment setup for the R36S hardware ----
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
export SDL_GAMECONTROLLERCONFIG="$(cat /roms/ports/gamecontrollerdb.txt 2>/dev/null)"

# ---- Navigate to this script's directory ----
cd "$(dirname "$0")" || exit 1

# ---- Launch the player (File Browser Mode) ----
./krona_player

# ---- Return control to ArkOS ----
exit 0
