#!/bin/bash
# Roda um comando da caixa de areia a partir de um atalho da Área de Trabalho e mostra o
# resultado: notificação quando dá certo, janela de erro (com o motivo) quando dá errado.
#   gui-rodar.sh "Título" ícone comando [argumentos...]
D=$(dirname "$(readlink -f "$0")")
titulo=${1:?título}; icone=${2:?ícone}; shift 2
cmd=${1:?comando}; shift
[ -x "$D/$cmd" ] && cmd=$D/$cmd
avisa() { notify-send -i "$icone" "$titulo" "$1" 2>/dev/null || true; }
avisa "Um instante..."
if saida=$("$cmd" "$@" 2>&1); then
  avisa "$(printf '%s\n' "$saida" | grep -v '^\s*$' | tail -n 3)"
else
  texto=$(printf '%s\n' "$saida" | tail -n 6 | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g')
  zenity --error --title="$titulo" --width=460 \
    --text="Não deu certo:\n\n$texto\n\nSe a areia não estiver aparecendo, veja a pasta \"1 - Topografia\"." 2>/dev/null \
    || avisa "Não deu certo: $saida"
  exit 1
fi
