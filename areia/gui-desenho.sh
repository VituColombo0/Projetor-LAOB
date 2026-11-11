#!/bin/bash
# Janela para escolher a cor e o traço do desenho na areia.
D=$(dirname "$(readlink -f "$0")")
ESCOLHA=$(zenity --list --title="Desenho na areia" --text="Escolha a cor ou o traço:" \
  --column="Código" --column="Opção" --hide-column=1 --width=380 --height=470 \
  preto "Preto" azul "Azul" vermelho "Vermelho" verde "Verde" laranja "Laranja" roxo "Roxo" \
  rosa "Rosa" amarelo "Amarelo" arco-iris "Arco-íris (muda sozinho)" borracha "Borracha" \
  fino "Traço fino" medio "Traço médio" grosso "Traço grosso" 2>/dev/null) || exit 0
[ -n "$ESCOLHA" ] && exec "$D/gui-rodar.sh" "Desenho na areia" applications-graphics areia-desenho "$ESCOLHA"
