#!/bin/bash
# Janela do labirinto de água: escolher o nível, rodada nova, placar ou parar.
D=$(dirname "$(readlink -f "$0")")
OP=$(zenity --list --title="Labirinto de água" --text="O que fazer?" \
  --column="Código" --column="Opção" --column="Como é" --hide-column=1 --width=600 --height=330 \
  normal "Começar: normal" "Nascente e alvo em lados opostos" \
  facil "Começar: fácil" "Alvo mais perto (crianças menores)" \
  dificil "Começar: difícil" "Cantos opostos e menos água" \
  nova "Rodada nova" "Outra nascente e outro alvo agora" \
  placar "Ver o placar" "Se a água chegou e quantas rodadas venceram" \
  parar "Parar o labirinto" "Encerra e a água vai embora" 2>/dev/null) || exit 0
case "$OP" in
  "") exit 0 ;;
  placar) zenity --info --title="Labirinto de água" --width=420 --text="$("$D/areia-labirinto" placar 2>&1)" 2>/dev/null; exit 0 ;;
  parar) exec "$D/gui-rodar.sh" "Labirinto de água" process-stop areia-labirinto parar ;;
  *) exec "$D/gui-rodar.sh" "Labirinto de água" weather-showers areia-labirinto "$OP" ;;
esac
