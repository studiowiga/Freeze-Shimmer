#!/bin/bash
# Installe Freeze Shimmer (VST3) sur macOS.
# Utilisation : double-clic sur ce fichier (ou : ./install-freezeshimmer-mac.command)
# Si macOS refuse de l'ouvrir : clic droit > Ouvrir, ou  xattr -dr com.apple.quarantine <dossier>
set -e
cd "$(dirname "$0")"
SRC="Freeze Shimmer.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
[ -d "$SRC" ] || { echo "Dossier '$SRC' introuvable a cote du script."; exit 1; }

mkdir -p "$DEST"
rm -rf "$DEST/$SRC"
cp -R "$SRC" "$DEST/"
# retire la quarantaine (fichier telecharge) et signe localement (necessaire sur Apple Silicon)
xattr -dr com.apple.quarantine "$DEST/$SRC" 2>/dev/null || true
codesign --force --deep --sign - "$DEST/$SRC" 2>/dev/null || true

echo "Freeze Shimmer installe dans : $DEST/$SRC"
echo "Relancez le scan des plugins de votre DAW."
