#!/bin/bash
# Troca qual SARndbox está no projetor (só um programa usa o Kinect por vez).
#   sandbox.sh topografia     a topografia (relevo + efeito do tema) na versão com Foto; se ela
#                             não abrir, o SARndbox original (sem efeitos) entra no lugar (diz "original")
#   sandbox.sh original       o SARndbox original, sem mudanças (reserva)
#   sandbox.sh rosto          a versão com Foto/Esculpir/brincadeiras (aceita comandos)
#   sandbox.sh parar          fecha o que estiver aberto (nada no projetor)
#   sandbox.sh reabrir        fecha o que estiver aberto (mesmo travado) e abre a topografia, sem passar
#                             pelo original; religa o vigia se ele estava pausado (quem reabre quer a areia)
#   sandbox.sh estado         qual está rodando: original, rosto ou nenhum
#   sandbox.sh cmd "<texto>"  manda um comando para a versão com Foto (ex.: "relevo")
#   sandbox.sh tema <arq.cpt> troca as cores do relevo na hora (no que estiver rodando)
# Se a versão com Foto não subir, a topografia volta sozinha (o original, a "reserva"): fica anotado em
# ./reserva (desde=, ultima=, motivo=) e o registro da tentativa em ./sandbox.falhou.log; o vigia tenta
# de novo a versão completa de tempos em tempos. Abrir a versão com Foto apaga ./reserva.
# Uma troca de programa por vez (trava em ./.troca.lock); estado não espera, cmd e tema esperam a troca acabar.
# SARNDBOX_ORIG muda onde está o original (padrão ~/SARndbox-2.8).
set -uo pipefail
D=$(dirname "$(readlink -f "$0")")
ORIG=${SARNDBOX_ORIG:-$HOME/SARndbox-2.8}/bin/SARndbox
ROSTO=$D/bin/SARndbox
FIFO=$D/controle.fifo
FIFO_TOPO=$D/topografia.fifo   # comandos para a topografia (só "colorMap"; o programa é o original)
TEMA=$D/temas/atual.cpt         # cores do relevo escolhidas com areia-tema
LOG=$D/sandbox.log
RESERVA=$D/reserva              # existe = a versão com Foto não subiu na última vez (o original ficou no lugar)
TRAVA=$D/.troca.lock
U=$(id -u)
export DISPLAY=${DISPLAY:-:0} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$U}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$U/bus}
[ -n "${XAUTHORITY:-}" ] || { [ -f "/run/user/$U/gdm/Xauthority" ] && export XAUTHORITY=/run/user/$U/gdm/Xauthority; }

registra() { echo "$(date '+%F %T') sandbox: $*" >> "$D/inicio.log"; }

# Uma troca por vez (23/11/2026): painel, vigia, areia-topografia e terminal podiam trocar a areia ao mesmo
# tempo (dois SARndbox brigando pelo Kinect, comandos perdidos no cano enquanto o programa ainda abria).
# Trocas (rosto, original, topografia, parar, reabrir) pegam a trava só para si; cmd e tema a pegam
# compartilhada (vários ao mesmo tempo, mas esperam a troca em andamento acabar). Com tempo limite: se
# alguém segurar a trava, a resposta é "outra troca em andamento", nunca uma espera sem fim. A trava solta
# sozinha quando o processo sai (é do descritor 9, que o programa da areia NÃO herda: 9>&- ao abrir).
# AREIA_TROCA: o "$0" rosto/original chamado de dentro de uma troca já está coberto por ela.
trava() {  # $1 = segundos de espera (padrão 25: o painel desiste em 60 s e uma troca leva até ~30 s)
  [ -n "${AREIA_TROCA:-}" ] && return 0
  command -v flock >/dev/null 2>&1 || return 0
  exec 9>>"$TRAVA"
  flock -w "${1:-25}" 9 || { echo "outra troca da areia em andamento; tente de novo"; exit 1; }
  export AREIA_TROCA=1
}
trava_comando() {  # espera curta: o painel desiste de um comando em 10 s
  [ -n "${AREIA_TROCA:-}" ] && return 0
  command -v flock >/dev/null 2>&1 || return 0
  exec 9>>"$TRAVA"
  flock -s -w 6 9 || { echo "a areia está trocando de programa; tente de novo em alguns segundos"; exit 1; }
}

# janela em tela cheia no projetor (sem isso ela abre no monitor); só se o projetor estiver ligado.
# Só é consultado quando um programa vai abrir: "estado" e "cmd" são chamados o tempo todo pelo
# painel e não precisam do xrandr. --current lê o que o X já sabe, sem sondar as saídas de vídeo
# (o xrandr puro faz o driver NVIDIA sondar tudo de novo e despeja umas 20 linhas no journal).
TELA=()
tela() {
  TELA=()
  if xrandr --current 2>/dev/null | grep -q "^HDMI-0 connected"; then
    TELA=(-mergeConfig "$D/projetor.cfg")
  fi
}

# O programa da areia num escopo próprio do systemd do usuário (23/11/2026): antes ele ficava no grupo de
# quem o abriu (o painel, o vigia, até uma sessão ssh já encerrada, presa aberta só por causa dele) e
# morreria junto se aquele grupo fosse encerrado. Sem o systemd do usuário, abre do jeito antigo.
# O escopo herda o ambiente e a prioridade de quem chama (os serviços não têm Nice).
ESCOPO=()
escopo() {  # $1 = rosto ou original (nome: areia-rosto-<pid>-<hora>.scope)
  ESCOPO=()
  command -v systemd-run >/dev/null 2>&1 || return 0
  systemd-run --user --scope --quiet --collect true >/dev/null 2>&1 || return 0
  ESCOPO=(systemd-run --user --scope --quiet --collect "--unit=areia-$1-$$-$(date +%s)" --)
}

# Água: grade da simulação e passos por quadro (AREIA_GRADE_AGUA e AREIA_PASSOS_AGUA trocam, para testes).
GRADE_AGUA=${AREIA_GRADE_AGUA:-320 240}
PASSOS_AGUA=${AREIA_PASSOS_AGUA:-30}

CORES=(-uhm)
[ -f "$TEMA" ] && CORES=(-uhm "$(readlink -f "$TEMA")")

tema_nome() { local t; t=$(readlink "$TEMA" 2>/dev/null); t=${t##*/}; t=${t%.cpt}; echo "${t:-classico}"; }

fifo() { [ -p "$1" ] || { rm -f "$1"; mkfifo -m 600 "$1"; }; }

manda() {  # $1 = cano, $2 = texto
  timeout 3 bash -c 'printf "%s\n" "$1" > "$2"' _ "$2" "$1"
}

qual() {
  if pgrep -f "^$ROSTO" >/dev/null; then echo rosto
  elif pgrep -f "^$ORIG" >/dev/null; then echo original
  else echo nenhum; fi
}

# Sem protetor de tela enquanto a areia está aberta: o GNOME do laob apagava as telas após 15 min sem
# mexer no mouse (idle-delay, que alguém volta a ligar nas Configurações; 19/11/2026) e a areia caía para
# 1 quadro/s. O SARndbox roda "segurando" o ocioso da sessão, como um player de vídeo; ao fechar, solta.
ACORDADO=()
if command -v gnome-session-inhibit >/dev/null 2>&1; then
  [ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ] || { [ -S "/run/user/$(id -u)/bus" ] && export DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus"; }
  [ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ] && ACORDADO=(gnome-session-inhibit --inhibit idle --reason "caixa de areia no projetor")
fi

parar() {
  pkill -f "^($ORIG|$ROSTO)" 2>/dev/null
  for _ in $(seq 1 20); do [ "$(qual)" = nenhum ] && return 0; sleep 0.25; done
  pkill -9 -f "^($ORIG|$ROSTO)" 2>/dev/null; sleep 1
}

iniciar_original() {
  tela
  escopo original
  fifo "$FIFO_TOPO"
  ( cd "$HOME" && exec setsid nohup "${ESCOPO[@]}" "${ACORDADO[@]}" "$ORIG" "${TELA[@]}" "${CORES[@]}" -fpv -cp "$FIFO_TOPO" ) >/dev/null 2>&1 < /dev/null 9>&- &
}

espera() {  # $1 = qual esperar, $2 = segundos
  for _ in $(seq 1 $(($2 * 4))); do [ "$(qual)" = "$1" ] && return 0; sleep 0.25; done
  return 1
}

marca_reserva() {  # $1 = motivo; "desde" continua o da primeira queda, "ultima" é agora
  local agora desde
  agora=$(date +%s)
  desde=$(sed -n 's/^desde=//p' "$RESERVA" 2>/dev/null | head -n 1)
  printf 'desde=%s\nultima=%s\nmotivo=%s\n' "${desde:-$agora}" "$agora" "$1" > "$RESERVA.tmp" && mv -f "$RESERVA.tmp" "$RESERVA"
}

case "${1:-estado}" in
  estado) qual ;;
  original)
    # pedido de propósito (do terminal, do instalar.sh), não a queda do rosto dentro de uma troca: não é
    # "reserva", e o vigia não tenta trocá-lo pela versão completa
    [ -n "${AREIA_TROCA:-}" ] || rm -f "$RESERVA"
    trava
    if [ "$(qual)" != original ]; then
      parar; iniciar_original
      espera original 5 && sleep 2  # e continua de pé
    fi
    [ "$(qual)" = original ] && { echo original; exit 0; }
    echo "a topografia não abriu"; exit 1 ;;
  rosto)
    trava
    [ "$(qual)" = rosto ] && { echo rosto; exit 0; }
    [ -x "$ROSTO" ] || { echo "falta $ROSTO (rode o instalar.sh)"; exit 1; }
    fifo "$FIFO"
    tela
    escopo rosto
    parar
    # O registro da vez anterior fica em sandbox.log.1 (antes era apagado: sumia a pista de uma queda).
    # O programa escreve com >> (O_APPEND): assim o vigia pode esvaziar o registro grande sem fechar a areia.
    [ -s "$LOG" ] && mv -f "$LOG" "$LOG.1"
    : > "$LOG"
    ( cd "$HOME" && exec setsid nohup "${ESCOPO[@]}" "${ACORDADO[@]}" "$ROSTO" "${TELA[@]}" "${CORES[@]}" -wts $GRADE_AGUA -ws 1.0 "$PASSOS_AGUA" -fpv -cp "$FIFO" ) >> "$LOG" 2>&1 < /dev/null 9>&- &
    sleep 8  # o Kinect e a janela levam uns segundos para ficar prontos para comandos
    if [ "$(qual)" != rosto ]; then
      echo "a versão com Foto não subiu (veja $D/sandbox.falhou.log); voltando à topografia"
      cp -f "$LOG" "$D/sandbox.falhou.log" 2>/dev/null
      marca_reserva "a versão com Foto não subiu"
      registra "a versão com Foto não subiu; o SARndbox original (reserva) vai para o projetor (registro em sandbox.falhou.log)"
      iniciar_original; espera original 5 && sleep 2; exit 1
    fi
    rm -f "$RESERVA"
    # animação do tema (lava, neve...) e o brilho escolhido no painel, numa escrita só
    luz=(); [ -r "$D/luz.txt" ] && luz=("luz $(head -c 40 "$D/luz.txt")")
    manda "$FIFO" "$(printf '%s\n' "efeito $(tema_nome)" "${luz[@]}")" || true
    echo rosto ;;
  topografia)
    trava
    "$0" rosto >/dev/null 2>&1
    if [ "$(qual)" = rosto ]; then
      # tudo numa escrita só (antes eram 8, de até 3 s cada se o programa não lesse o cano, e com "|| true"
      # dizia "topografia" sem ter mandado nada)
      manda "$FIFO" "$(printf '%s\n' relevo tesouroFim fossilFim labirintoFim jogoFim copiaFim "animais nada" "efeito $(tema_nome)")" \
        || { echo "sem resposta do SARndbox"; exit 1; }
      echo topografia; exit 0
    fi
    "$0" original ;;
  parar)
    trava
    parar
    [ "$(qual)" = nenhum ] && { echo nenhum; exit 0; }
    echo "a areia não fechou"; exit 1 ;;
  reabrir)
    trava
    rm -f "$D/sem-vigia"   # quem reabre quer a areia: o vigia volta a cuidar dela
    parar
    exec "$0" topografia ;;   # a trava continua (o descritor 9 passa para o exec)
  cmd)
    trava_comando
    [ "$(qual)" = rosto ] || { echo "a versão com Foto não está rodando"; exit 1; }
    shift; [ $# -gt 0 ] || { echo "uso: sandbox.sh cmd <comando> [mais comandos]"; exit 2; }
    # vários comandos numa escrita só (uma linha cada): trocar de brincadeira manda 3 de uma vez
    manda "$FIFO" "$(printf '%s\n' "$@")" || { echo "sem resposta do SARndbox"; exit 1; }
    sleep 0.1; tail -n 2 "$LOG" ;;
  tema)
    arq=$(readlink -f "${2:?arquivo .cpt}")
    [ -f "$arq" ] || { echo "tema não encontrado: $2"; exit 1; }
    case "$(qual)" in
      original)
        if pgrep -af "^$ORIG" | grep -q -- " -cp "; then
          trava_comando
          manda "$FIFO_TOPO" "colorMap $arq" || { echo "sem resposta da topografia"; exit 1; }
        else  # aberta sem o cano de comandos (versão antiga do autostart): reabre já com o tema
          trava
          parar; iniciar_original; espera original 5 || { echo "a topografia não abriu"; exit 1; }
        fi ;;
      rosto) trava_comando
             t=${arq##*/}
             manda "$FIFO" "$(printf '%s\n' "colorMap $arq" "efeito ${t%.cpt}")" || { echo "sem resposta do SARndbox"; exit 1; } ;;
    esac
    echo tema ;;
  *) echo "uso: sandbox.sh topografia|original|rosto|parar|reabrir|estado|cmd \"<comando>\"|tema <arq.cpt>"; exit 2 ;;
esac
