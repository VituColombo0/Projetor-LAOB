#!/bin/bash
# Janela para escolher a foto da pasta "2 - Fotos na areia" e projetar.
D=$(dirname "$(readlink -f "$0")")
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop")
FOTO=$(zenity --file-selection --title="Escolha a foto para projetar na areia" \
  --filename="$MESA/2 - Fotos na areia/" \
  --file-filter="Imagens | *.png *.jpg *.jpeg *.webp *.bmp *.PNG *.JPG *.JPEG *.WEBP *.BMP" 2>/dev/null) || exit 0
[ -f "$FOTO" ] && exec "$D/gui-rodar.sh" "Fotos na areia" camera-photo areia-foto "$FOTO"
