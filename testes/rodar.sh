#!/bin/bash
# Testes automáticos (não tocam no projetor nem no Kinect): bash testes/rodar.sh [python]
# O python precisa de numpy, opencv e flask (no laob: ~/SARndbox-rosto/venv/bin/python).
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
PY=${1:-$HOME/SARndbox-rosto/venv/bin/python}
[ -x "$PY" ] || PY=python3
falhas=0
echo "== sintaxe dos scripts"
for f in "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-vigia "$R"/testes/*.sh; do
  bash -n "$f" || { echo "  ERRO em $f"; falhas=1; }
done
for w in areia-foto areia-tema areia-tesouro areia-animais areia-fossil areia-labirinto areia-jogo; do sh -n "$R/areia/$w" || falhas=1; done
if command -v shellcheck >/dev/null; then
  shellcheck -S warning "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-foto "$R"/areia/areia-tema "$R"/areia/areia-tesouro "$R"/areia/areia-animais "$R"/areia/areia-fossil "$R"/areia/areia-labirinto "$R"/areia/areia-jogo "$R"/areia/areia-vigia || falhas=1
else
  echo "  (shellcheck não instalado: só a sintaxe)"
fi
echo "== Área de Trabalho: só o atalho do painel"
n=0
for at in "$R"/area-de-trabalho/*.desktop; do
  n=$((n+1))
  for k in Type Name Exec Icon; do grep -q "^$k=" "$at" || { echo "  falta $k em $at"; falhas=1; }; done
done
[ $n = 1 ] || { echo "  esperava 1 atalho (o do painel), achei $n"; falhas=1; }
ls -d "$R"/area-de-trabalho/*/ >/dev/null 2>&1 && { echo "  sobraram pastas em area-de-trabalho/"; falhas=1; }
echo "== serviços"
for u in "$R"/sistema/areia-vigia.service "$R"/sistema/sandbox-painel.service; do
  grep -q "^KillMode=process" "$u" || { echo "  falta KillMode=process em $u (o systemd mataria a areia)"; falhas=1; }
  grep -q "^Nice=" "$u" && { echo "  Nice em $u: a areia herdaria a prioridade menor"; falhas=1; }
done
echo "== dedo apontado: classificação com mãos sintéticas (sarndbox/Gesto.cpp, sem Kinect)"
if command -v g++ >/dev/null; then
  BIN=$(mktemp)
  if g++ -O2 -Wall -Wextra -Werror -I "$R/sarndbox" "$R/testes/teste_gesto.cpp" "$R/sarndbox/Gesto.cpp" -o "$BIN"; then
    "$BIN" | grep -vE '^        ponta' || falhas=1   # (as medidas de cada cena: rodar o teste_gesto sozinho)
  else
    echo "  ERRO ao compilar o teste do gesto"; falhas=1
  fi
  rm -f "$BIN"
else
  echo "  (g++ não instalado: pulei)"
fi
echo "== testes Python ($PY)"
( cd "$R/testes" && "$PY" -m unittest -v test_rosto test_areia test_painel ) || falhas=1
"$PY" -c "import ast,sys; ast.parse(open(sys.argv[1]).read())" "$R/areia/atualizar_papel_parede.py" || falhas=1
[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
