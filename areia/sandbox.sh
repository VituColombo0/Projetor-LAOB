#!/bin/bash
# Troca qual SARndbox está no projetor (só um programa usa o Kinect por vez).
#   sandbox.sh original       a topografia: o SARndbox original, sem mudanças
#   sandbox.sh rosto          a versão com os modos Foto/Esculpir (aceita comandos)
#   sandbox.sh estado         qual está rodando: original, rosto ou nenhum
#   sandbox.sh cmd "<texto>"  manda um comando para a versão com Foto (ex.: "relevo")
#   sandbox.sh tema <arq.cpt> troca as cores do relevo na hora (no que estiver rodando)
# Se a versão com Foto não subir, a topografia volta sozinha.
# SARNDBOX_ORIG muda onde está o original (padrão ~/SARndbox-2.8).
set -uo pipefail
D=$(dirname "$(readlink -f "$0")")
ORIG=${SARNDBOX_ORIG:-$HOME/SARndbox-2.8}/bin/SARndbox
ROSTO=$D/bin/SARndbox
FIFO=$D/controle.fifo
FIFO_TOPO=$D/topografia.fifo   # comandos para a topografia (só "colorMap"; o programa é o original)
TEMA=$D/temas/atual.cpt         # cores do relevo escolhidas com areia-tema
LOG=$D/sandbox.log
U=$(id -u)
export DISPLAY=${DISPLAY:-:0} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$U}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$U/bus}
[ -n "${XAUTHORITY:-}" ] || { [ -f "/run/user/$U/gdm/Xauthority" ] && export XAUTHORITY=/run/user/$U/gdm/Xauthority; }

# janela em tela cheia no projetor (sem isso ela abre no monitor); só se o projetor estiver ligado
TELA=()
if xrandr 2>/dev/null | grep -q "^HDMI-0 connected"; then
  TELA=(-mergeConfig "$D/projetor.cfg")
fi

CORES=(-uhm)
[ -f "$TEMA" ] && CORES=(-uhm "$(readlink -f "$TEMA")")

fifo() { [ -p "$1" ] || { rm -f "$1"; mkfifo -m 600 "$1"; }; }

manda() {  # $1 = cano, $2 = texto
  timeout 3 bash -c 'printf "%s\n" "$1" > "$2"' _ "$2" "$1"
}

qual() {
  if pgrep -f "^$ROSTO" >/dev/null; then echo rosto
  elif pgrep -f "^$ORIG" >/dev/null; then echo original
  else echo nenhum; fi
}

parar() {
  pkill -f "^($ORIG|$ROSTO)" 2>/dev/null
  for _ in $(seq 1 20); do [ "$(qual)" = nenhum ] && return 0; sleep 0.25; done
  pkill -9 -f "^($ORIG|$ROSTO)" 2>/dev/null; sleep 1
}

iniciar_original() {
  fifo "$FIFO_TOPO"
  ( cd "$HOME" && exec setsid nohup "$ORIG" "${TELA[@]}" "${CORES[@]}" -fpv -cp "$FIFO_TOPO" ) >/dev/null 2>&1 < /dev/null &
}

espera() {  # $1 = qual esperar, $2 = segundos
  for _ in $(seq 1 $(($2 * 4))); do [ "$(qual)" = "$1" ] && return 0; sleep 0.25; done
  return 1
}

case "${1:-estado}" in
  estado) qual ;;
  original)
    if [ "$(qual)" != original ]; then
      parar; iniciar_original
      espera original 5 && sleep 2  # e continua de pé
    fi
    [ "$(qual)" = original ] && { echo original; exit 0; }
    echo "a topografia não abriu"; exit 1 ;;
  rosto)
    [ "$(qual)" = rosto ] && { echo rosto; exit 0; }
    [ -x "$ROSTO" ] || { echo "falta $ROSTO (rode o instalar.sh)"; exit 1; }
    fifo "$FIFO"
    parar
    ( cd "$HOME" && exec setsid nohup "$ROSTO" "${TELA[@]}" "${CORES[@]}" -fpv -cp "$FIFO" ) > "$LOG" 2>&1 < /dev/null &
    sleep 8  # o Kinect e a janela levam uns segundos para ficar prontos para comandos
    if [ "$(qual)" != rosto ]; then
      echo "a versão com Foto não subiu (veja $LOG); voltando à topografia"
      iniciar_original; espera original 5; exit 1
    fi
    echo rosto ;;
  cmd)
    [ "$(qual)" = rosto ] || { echo "a versão com Foto não está rodando"; exit 1; }
    manda "$FIFO" "${2:?comando}" || { echo "sem resposta do SARndbox"; exit 1; }
    sleep 0.3; tail -n 2 "$LOG" ;;
  tema)
    arq=$(readlink -f "${2:?arquivo .cpt}")
    [ -f "$arq" ] || { echo "tema não encontrado: $2"; exit 1; }
    case "$(qual)" in
      original)
        if pgrep -af "^$ORIG" | grep -q -- " -cp "; then
          manda "$FIFO_TOPO" "colorMap $arq" || { echo "sem resposta da topografia"; exit 1; }
        else  # aberta sem o cano de comandos (versão antiga do autostart): reabre já com o tema
          parar; iniciar_original; espera original 5 || { echo "a topografia não abriu"; exit 1; }
        fi ;;
      rosto) manda "$FIFO" "colorMap $arq" || { echo "sem resposta do SARndbox"; exit 1; } ;;
    esac
    echo tema ;;
  *) echo "uso: sandbox.sh original|rosto|estado|cmd \"<comando>\"|tema <arq.cpt>"; exit 2 ;;
esac
