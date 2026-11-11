#!/bin/bash
# Testes automáticos (não tocam no projetor nem no Kinect): bash testes/rodar.sh [python]
# O python precisa de numpy, opencv e flask (no lab2: ~/SARndbox-rosto/venv/bin/python).
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
PY=${1:-$HOME/SARndbox-rosto/venv/bin/python}
[ -x "$PY" ] || PY=python3
falhas=0
echo "== sintaxe dos scripts"
for f in "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/testes/*.sh; do
  bash -n "$f" || { echo "  ERRO em $f"; falhas=1; }
done
for w in areia-foto areia-tema areia-tesouro areia-animais areia-fossil; do sh -n "$R/areia/$w" || falhas=1; done
for g in "$R"/areia/gui-*.sh; do bash -n "$g" || falhas=1; done
if command -v shellcheck >/dev/null; then
  shellcheck -S warning "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-foto "$R"/areia/areia-tema "$R"/areia/areia-tesouro "$R"/areia/areia-animais "$R"/areia/areia-fossil "$R"/areia/gui-*.sh || falhas=1
else
  echo "  (shellcheck não instalado: só a sintaxe)"
fi
echo "== atalhos da Área de Trabalho"
for at in "$R"/area-de-trabalho/*/*.desktop; do
  exe=$(sed -n 's/^Exec=//p' "$at" | awk '{print $1}')
  [ -f "$R/areia/$(basename "$exe")" ] || { echo "  atalho aponta para algo que não existe: $at -> $exe"; falhas=1; }
  for k in Type Name Exec Icon; do grep -q "^$k=" "$at" || { echo "  falta $k em $at"; falhas=1; }; done
  for c in $(sed -n 's/^Exec=.*gui-rodar.sh "[^"]*" [^ ]* \([^ ]*\).*/\1/p' "$at"); do
    [ -f "$R/areia/$c" ] || { echo "  comando inexistente em $at: $c"; falhas=1; }
  done
done
for p in "$R"/area-de-trabalho/*/; do [ -s "$p/COMO USAR.txt" ] || { echo "  falta o manual em $p"; falhas=1; }; done
echo "== testes Python ($PY)"
( cd "$R/testes" && "$PY" -m unittest -v test_rosto test_areia test_painel ) || falhas=1
[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
