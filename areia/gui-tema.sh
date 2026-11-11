#!/bin/bash
# Janela para escolher o tema de cores (e a animação) do relevo.
D=$(dirname "$(readlink -f "$0")")
TEMA=$(zenity --list --title="Cores do relevo" --text="Escolha o tema da caixa de areia:" \
  --column="Código" --column="Tema" --column="Animação" --hide-column=1 --width=640 --height=390 \
  classico "Clássico" "Água, verde, amarelo e marrom + chuva fina" \
  vulcao "Vulcão" "Lava escorrendo dos morros" \
  inverno "Inverno" "Neve caindo, picos nevados" \
  marte "Marte" "Ferrugem + tempestade de poeira" \
  deserto "Deserto" "Dunas e oásis + areia ao vento" \
  oceano "Oceano" "Mar com luz dançando; picos viram ilhas" \
  arco-iris "Arco-íris" "Uma cor por altura + estrelinhas" \
  mapa "Mapa de papel" "Papel claro com curvas de nível" 2>/dev/null) || exit 0
[ -n "$TEMA" ] && exec "$D/gui-rodar.sh" "Cores do relevo" preferences-desktop-theme areia-tema "$TEMA"
