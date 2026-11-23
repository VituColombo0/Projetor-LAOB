#!/bin/bash
# Testes automáticos (não tocam no projetor nem no Kinect): bash testes/rodar.sh [python]
# O python precisa de numpy, opencv e flask (no laob: ~/SARndbox-rosto/venv/bin/python).
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
PY=${1:-$HOME/SARndbox-rosto/venv/bin/python}
[ -x "$PY" ] || PY=python3
falhas=0
echo "== sintaxe dos scripts"
for f in "$R"/instalar.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-vigia "$R"/testes/*.sh "$R"/deploy/laob/*.sh; do
  bash -n "$f" || { echo "  ERRO em $f"; falhas=1; }
done
for w in areia-foto areia-tema areia-tesouro areia-animais areia-fossil areia-labirinto areia-jogo; do sh -n "$R/areia/$w" || falhas=1; done
if command -v shellcheck >/dev/null; then
  shellcheck -S warning "$R"/instalar.sh "$R"/deploy/laob/otimizar-projetor.sh "$R"/areia/sandbox.sh "$R"/areia/areia-topografia "$R"/areia/areia-foto "$R"/areia/areia-tema "$R"/areia/areia-tesouro "$R"/areia/areia-animais "$R"/areia/areia-fossil "$R"/areia/areia-labirinto "$R"/areia/areia-jogo "$R"/areia/areia-vigia || falhas=1
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
echo "== instalador: favoritos, cópia atômica, calibração e --verificar (numa pasta temporária)"
# (auditoria 23/11/2026) roda as funções do instalar.sh com set -e, como na instalação de verdade
INST=$(mktemp -d)
teste_inst() {  # descrição código (roda num bash com as funções do instalar.sh e set -Eeuo pipefail)
  if HOME="$INST/casa" INSTALAR_SO_FUNCOES=1 bash -c "source '$R/instalar.sh'; set -Eeuo pipefail; cd \"\$(mktemp -d -p '$INST')\"; $2" >"$INST/saida" 2>&1; then
    echo "  ok: $1"
  else
    echo "  FALHA: $1"; sed 's/^/    /' "$INST/saida"; falhas=1
  fi
}
mkdir -p "$INST/casa"
F='file:///x/Pictures/Fotos%20na%20areia Fotos na areia'
teste_inst "favoritos: arquivo ausente" "atualiza_favorito b1 /x/Desktop '/x/Pictures/Fotos na areia'; [ \"\$(cat b1)\" = '$F' ]"
teste_inst "favoritos: arquivo vazio" ": > b2; atualiza_favorito b2 /x/Desktop '/x/Pictures/Fotos na areia'; [ \"\$(cat b2)\" = '$F' ]"
teste_inst "favoritos: só a linha das fotos, 2x não duplica" "echo '$F' > b3; atualiza_favorito b3 /x/Desktop '/x/Pictures/Fotos na areia'; atualiza_favorito b3 /x/Desktop '/x/Pictures/Fotos na areia'; [ \"\$(cat b3)\" = '$F' ]"
teste_inst "favoritos: mantém os outros e tira o Areia antigo" "printf 'file:///x/Documents\nfile:///x/Desktop/Areia Areia\n' > b4; atualiza_favorito b4 /x/Desktop '/x/Pictures/Fotos na areia'; atualiza_favorito b4 /x/Desktop '/x/Pictures/Fotos na areia'; [ \"\$(wc -l < b4)\" = 2 ] && grep -q Documents b4 && ! grep -q 'Desktop/Areia' b4 && [ ! -e b4.tmp ]"
teste_inst "cópia atômica: inode novo, modo e conteúdo" "echo velho > s; echo novo > o; i=\$(stat -c %i s); copia_atomica o s 755; [ \"\$(cat s)\" = novo ] && [ -x s ] && [ \"\$(stat -c %i s)\" != \"\$i\" ] && [ ! -e s.novo ]"
teste_inst "calibração: cópia nova e registro" "mkdir -p c d; echo A > c/BoxLayout.txt; echo P > c/ProjectorMatrix.dat; instala_calibracao c d; cmp c/BoxLayout.txt d/BoxLayout.txt && grep -q ' ProjectorMatrix.dat\$' d/.calib-instalada.md5"
teste_inst "calibração: recalibração local mais nova fica" "mkdir -p c d; echo A > c/BoxLayout.txt; instala_calibracao c d; touch -d '1 hour ago' c/BoxLayout.txt; echo LOCAL > d/BoxLayout.txt; instala_calibracao c d; instala_calibracao c d; [ \"\$(cat d/BoxLayout.txt)\" = LOCAL ]"
teste_inst "calibração: origem nova entra se a cópia não foi mexida" "mkdir -p c d; echo A > c/BoxLayout.txt; instala_calibracao c d; sleep 1; echo B > c/BoxLayout.txt; instala_calibracao c d; [ \"\$(cat d/BoxLayout.txt)\" = B ] && ! ls d/BoxLayout.txt.antes-* >/dev/null 2>&1"
teste_inst "calibração: origem mais nova que a local mexida entra e guarda a local" "mkdir -p c d; echo A > c/BoxLayout.txt; instala_calibracao c d; echo LOCAL > d/BoxLayout.txt; touch -d '1 hour ago' d/BoxLayout.txt; echo C > c/BoxLayout.txt; instala_calibracao c d; [ \"\$(cat d/BoxLayout.txt)\" = C ] && grep -q LOCAL d/BoxLayout.txt.antes-*"
teste_inst "calibração: cópia nova do original (etc de fábrica mais novo) recebe a de CALIB" "mkdir -p c d f; echo CAL > c/BoxLayout.txt; touch -d '2 days ago' c/BoxLayout.txt; echo FAB > f/BoxLayout.txt; cp -p f/BoxLayout.txt d/; instala_calibracao c d f; [ \"\$(cat d/BoxLayout.txt)\" = CAL ] && ! ls d/BoxLayout.txt.antes-* >/dev/null 2>&1"
teste_inst "pré-requisitos: sem g++ e sem make param a compilação" "PATH=/nenhum; QUIETO=1; confere_requisitos compilar >/dev/null; [ \$FALTAS -ge 3 ]"
# --verificar numa casa vazia: só linhas OK/AVISO/FALTA (e títulos), sai com 1 e não cria nada na casa
mkdir -p "$INST/vazia" "$INST/run"; touch "$INST/marca"; sleep 1
HOME="$INST/vazia" XDG_RUNTIME_DIR="$INST/run" DBUS_SESSION_BUS_ADDRESS=unix:path=/nenhum PULSE_SERVER=unix:/nenhum DISPLAY=:99 XAUTHORITY=/nenhum bash "$R/instalar.sh" --verificar > "$INST/ver" 2>&1; rc=$?
if [ $rc = 1 ] && grep -q '^FALTA ' "$INST/ver" && ! grep -vE '^(OK |AVISO |FALTA |== |$|[0-9]+ FALTA, [0-9]+ AVISO$)' "$INST/ver" | grep -q .; then
  echo "  ok: --verificar numa casa vazia acusa FALTA e sai com 1"
else
  echo "  FALHA: --verificar (rc=$rc, esperava 1 e só linhas OK/AVISO/FALTA)"; grep -vE '^(OK |AVISO |FALTA |== )' "$INST/ver" | head -5 | sed 's/^/    /'; falhas=1
fi
CRIADOS=$(find "$INST/vazia" -mindepth 1 -newer "$INST/marca" | head -3)
[ -z "$CRIADOS" ] && echo "  ok: --verificar não criou nada" || { echo "  FALHA: --verificar criou $CRIADOS"; falhas=1; }
rm -rf "$INST"
echo "== serviços"
for u in "$R"/sistema/areia-vigia.service "$R"/sistema/sandbox-painel.service; do
  grep -q "^KillMode=process" "$u" || { echo "  falta KillMode=process em $u (o systemd mataria a areia)"; falhas=1; }
  grep -q "^Nice=" "$u" && { echo "  Nice em $u: a areia herdaria a prioridade menor"; falhas=1; }
done
echo "== dedo apontado: classificação com mãos sintéticas (sarndbox/Gesto.cpp, sem Kinect)"
if command -v g++ >/dev/null; then
  BIN=$(mktemp)
  AN=$(mktemp)
  GRAV=$(mktemp -d)
  if g++ -O2 -Wall -Wextra -Werror -I "$R/sarndbox" "$R/testes/teste_gesto.cpp" "$R/sarndbox/Gesto.cpp" -o "$BIN"; then
    "$BIN" | grep -vE '^        ponta' || falhas=1   # (as medidas de cada cena: rodar o teste_gesto sozinho)
    # gravação sintética (o formato do gestoGrava) + a análise: para cima e para baixo confirmam, a mão aberta não
    echo "== dedo apontado: gravação sintética e análise (testes/analisa_gesto.cpp)"
    if g++ -O2 -Wall -Wextra -Werror -I "$R/sarndbox" "$R/testes/analisa_gesto.cpp" "$R/sarndbox/Gesto.cpp" -o "$AN" \
       && "$BIN" --grava "$GRAV" >/dev/null && SAIDA=$("$AN" "$GRAV"/gesto-* --png 4); then
      echo "$SAIDA" | grep -E '^(Resumo|Motivos|PNGs)'
      echo "$SAIDA" | grep -q 'confirmou 2 vez' || { echo "  FALHA: a análise devia confirmar 2 vezes"; falhas=1; }
      [ "$(ls "$GRAV"/gesto-*/analise/*.png 2>/dev/null | wc -l)" = 4 ] || { echo "  FALHA: faltam os PNGs da análise"; falhas=1; }
    else
      echo "  ERRO na gravação sintética ou na análise"; falhas=1
    fi
  else
    echo "  ERRO ao compilar o teste do gesto"; falhas=1
  fi
  rm -rf "$BIN" "$AN" "$GRAV"
else
  echo "  (g++ não instalado: pulei)"
fi
echo "== Mundo LAOB: planta e rotas (sarndbox/MundoPlanta.h, sem OpenGL)"
if command -v g++ >/dev/null; then
  BIN=$(mktemp)
  if g++ -O2 -Wall -Wextra -Werror -I "$R/sarndbox" "$R/testes/teste_mundo.cpp" -o "$BIN"; then
    "$BIN" || falhas=1
  else
    echo "  ERRO ao compilar o teste do mundo"; falhas=1
  fi
  rm -f "$BIN"
else
  echo "  (g++ não instalado: pulei)"
fi
echo "== shader dos extras: variantes iguais pixel a pixel (testes/teste_shader.cpp, na GPU por EGL, sem Kinect)"
# Só onde há EGL e uma placa com OpenGL (o laob); pode rodar com a areia aberta. Sem isso, pula sem falhar.
if command -v g++ >/dev/null && printf '#include <EGL/egl.h>\n#include <GL/gl.h>\n' | g++ -E -x c++ - >/dev/null 2>&1; then
  BIN=$(mktemp)
  if g++ -O1 -Wall -Wextra -I "$R/sarndbox" "$R/testes/teste_shader.cpp" -o "$BIN" -lEGL -lGL; then
    SAIDA=$("$BIN" "$R/sarndbox/Extras.cpp" 2>&1); rc=$?
    if [ $rc = 2 ]; then echo "  (sem GPU/EGL utilizável nesta máquina: pulei)"
    else echo "$SAIDA" | tail -n 4; [ $rc = 0 ] || { echo "  FALHA no teste do shader"; falhas=1; }; fi
  else
    echo "  ERRO ao compilar o teste do shader"; falhas=1
  fi
  rm -f "$BIN"
else
  echo "  (sem g++ ou sem os cabeçalhos do EGL/GL: pulei)"
fi
echo "== testes Python ($PY)"
( cd "$R/testes" && "$PY" -m unittest -v test_rosto test_areia test_painel ) || falhas=1
"$PY" -c "import ast,sys; ast.parse(open(sys.argv[1]).read())" "$R/areia/atualizar_papel_parede.py" || falhas=1
[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
