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
for w in areia-foto areia-tema areia-tesouro areia-animais; do sh -n "$R/areia/$w" || falhas=1; done
if command -v shellcheck >/dev/null; then
  shellcheck -S warning "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-foto "$R"/areia/areia-tema "$R"/areia/areia-tesouro "$R"/areia/areia-animais || falhas=1
else
  echo "  (shellcheck não instalado: só a sintaxe)"
fi
echo "== testes Python ($PY)"
( cd "$R/testes" && "$PY" -m unittest -v test_rosto test_areia test_painel ) || falhas=1
[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
