#!/bin/bash
# =============================================================================
# Krona Player — ArkOS Launcher Script
# =============================================================================

# ---- Environment setup for the R36S hardware ----
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
export SDL_GAMECONTROLLERCONFIG="$(cat /roms/ports/gamecontrollerdb.txt 2>/dev/null)"

# ---- Navigate to this script's directory ----
cd "$(dirname "$0")" || exit 1

# ---- Auto-Inject gamelist.xml metadata ----
GAMELIST="/roms/ports/gamelist.xml"
if [ -f "$GAMELIST" ]; then
    if ! grep -q "<name>Krona Player</name>" "$GAMELIST"; then
        # Backup the original gamelist
        cp "$GAMELIST" "${GAMELIST}.backup"
        
        # Inject the metadata right before </gameList>
        sed -i 's|</gameList>|	<game>\n		<path>./Krona_Player.sh</path>\n		<name>Krona Player</name>\n		<image>./kronaplayer/images/cover.png</image>\n		<desc>Native hardware-accelerated MP4 Video Player for the R36S console.</desc>\n	</game>\n</gameList>|g' "$GAMELIST"
    fi
else
    # Create new gamelist.xml if it doesn't exist
    echo '<?xml version="1.0"?>' > "$GAMELIST"
    echo '<gameList>' >> "$GAMELIST"
    echo '	<game>' >> "$GAMELIST"
    echo '		<path>./Krona_Player.sh</path>' >> "$GAMELIST"
    echo '		<name>Krona Player</name>' >> "$GAMELIST"
    echo '		<image>./kronaplayer/images/cover.png</image>' >> "$GAMELIST"
    echo '		<desc>Native hardware-accelerated MP4 Video Player for the R36S console.</desc>' >> "$GAMELIST"
    echo '	</game>' >> "$GAMELIST"
    echo '</gameList>' >> "$GAMELIST"
fi

# ---- Launch the player (File Browser Mode) ----
./kronaplayer/krona_player

# ---- Return control to ArkOS ----
exit 0
