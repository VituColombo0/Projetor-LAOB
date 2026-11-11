#!/bin/bash
# Janela para escolher o dinossauro e a profundidade da escavação (ou ver o placar).
D=$(dirname "$(readlink -f "$0")")
DINO=$(zenity --list --title="Escavação de fósseis" --text="Escolha o fóssil a enterrar:" \
  --column="Código" --column="Fóssil" --column="Como é" --hide-column=1 --width=640 --height=340 \
  trex "Tiranossauro Rex" "Crânio com dentes afiados e costelas" \
  triceratops "Triceratops" "Escudo ósseo e 3 chifres" \
  pterodactilo "Pterodáctilo" "Bico longo, crista e asas abertas" \
  pegada "Pegada gigante" "Pegada de três dedos na rocha" \
  achou "Ver o placar" "Se está enterrado e quantos já escavaram" \
  parar "Parar escavação" "Encerra a escavação" 2>/dev/null) || exit 0
case "$DINO" in
  "") exit 0 ;;
  achou) zenity --info --title="Escavação de fósseis" --width=420 --text="$("$D/areia-fossil" achou 2>&1)" 2>/dev/null; exit 0 ;;
  parar) exec "$D/gui-rodar.sh" "Escavação de fósseis" process-stop areia-fossil parar ;;
esac
NIVEL=$(zenity --list --title="Profundidade" --text="Quão fundo enterrar?" \
  --column="Código" --column="Nível" --column="Profundidade" --hide-column=1 --width=460 --height=260 \
  normal "Normal" "3 cm" facil "Fácil (crianças)" "2 cm" dificil "Difícil" "5 cm" 2>/dev/null) || exit 0
exec "$D/gui-rodar.sh" "Escavação de fósseis" applications-science areia-fossil "$DINO" "${NIVEL:-normal}"
