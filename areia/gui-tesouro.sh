#!/bin/bash
# Janela de opções da caça ao tesouro.
D=$(dirname "$(readlink -f "$0")")
OPCAO=$(zenity --list --title="Caça ao tesouro" --text="Escolha:" \
  --column="Código" --column="Opção" --column="Detalhes" --hide-column=1 --width=560 --height=330 \
  normal "Começar (normal)" "Tesouro a 3 cm de profundidade" \
  facil "Começar (fácil)" "Tesouro a 2 cm, bom para crianças" \
  dificil "Começar (difícil)" "Tesouro a 5 cm" \
  achou "Ver o placar" "Se está escondido e quantos já acharam" \
  parar "Parar" "Tira o tesouro da areia" 2>/dev/null) || exit 0
if [ "$OPCAO" = achou ]; then
  zenity --info --title="Caça ao tesouro" --width=420 --text="$("$D/areia-tesouro" achou 2>&1)" 2>/dev/null
elif [ -n "$OPCAO" ]; then
  exec "$D/gui-rodar.sh" "Caça ao tesouro" games-highscores areia-tesouro "$OPCAO"
fi
