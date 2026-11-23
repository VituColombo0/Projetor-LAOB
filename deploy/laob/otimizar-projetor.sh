#!/bin/bash
# Ajustes da máquina laob para a caixa de areia (revisão final, 23/11/2026). Tudo reversível; complementa o
# otimizar.sh do Willbert-LAOB (processador, prioridades, driver fora das atualizações, kernel fixado).
#   sudo bash deploy/laob/otimizar-projetor.sh              parte do SISTEMA: limite do journal e apt no boot
#   bash deploy/laob/otimizar-projetor.sh                   parte do USUÁRIO (sem sudo): tela do GNOME nunca apaga
#   bash deploy/laob/otimizar-projetor.sh --simular         mostra o que cada parte faria, sem mudar nada
#   [sudo] bash deploy/laob/otimizar-projetor.sh --desfazer volta ao que estava antes (a mesma parte)
# O "antes" fica guardado na 1ª vez (rodar de novo não perde o original): sistema em /etc/projetor-otimizacao,
# usuário em ~/.local/state/projetor-otimizacao.
# NÃO mexe em: kernel, driver NVIDIA, calibração, pacotes (nada de apt purge/remove), interface gráfica.
set -euo pipefail
ACAO=aplicar
case "${1:-}" in
  "") ;;
  --simular) ACAO=simular ;;
  --desfazer) ACAO=desfazer ;;
  -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "uso: [sudo] bash $0 [--simular|--desfazer]"; exit 2 ;;
esac
EST=/etc/projetor-otimizacao
JDROP=/etc/systemd/journald.conf.d/projetor.conf
TDROP=/etc/systemd/system/apt-daily-upgrade.timer.d/projetor.conf
UEST=${XDG_STATE_HOME:-$HOME/.local/state}/projetor-otimizacao
# chaves do GNOME (esquema chave valor-desejado)
GNOME=(
  "org.gnome.desktop.session idle-delay uint32 0"
  "org.gnome.settings-daemon.plugins.power idle-dim false"
  "org.gnome.desktop.screensaver lock-enabled false"
  "org.gnome.settings-daemon.plugins.power sleep-inactive-ac-type 'nothing'"
)

# ------------------------------------------------------------------ parte do sistema (sudo)
# 1) journald com teto de 500 MB. Em 22/11/2026 o journal do laob estava com 4,0 GB (sem limite: o padrão
#    é 10% do disco, até 4 GB). Um drop-in em journald.conf.d não toca no journald.conf do Ubuntu. Depois
#    de aplicar, o journald é reiniciado (é como ele relê a configuração) e os arquivos ARQUIVADOS além de
#    500 MB são apagados (journalctl --vacuum-size): esses registros antigos não voltam com --desfazer.
# 2) apt-daily-upgrade sem rodar "atrasado" no boot. O timer de fábrica tem Persistent=true: se o PC estava
#    desligado às 6h, a atualização roda ~10 s depois do boot, junto com a areia abrindo (medido no boot do
#    laob: 10 s). Com Persistent=false ela espera o próximo horário (6h, todo dia); o laob fica ligado, então
#    as atualizações de segurança continuam diárias. Drop-in no timer, sem mexer no arquivo do pacote.
#    (Não mexe no apt-daily.timer, que só baixa a lista, nem no que o otimizar.sh do Willbert já fez.)
sistema_mostra() {
  echo "== parte do sistema (precisa de sudo)"
  echo "journal: $(journalctl --disk-usage 2>/dev/null | grep -o '[0-9.]*[KMGT]' | head -1 || echo '?') hoje; limite: $(
    [ -f "$JDROP" ] && grep -h '^SystemMaxUse' "$JDROP" || echo 'nenhum (padrão do systemd)')"
  echo "apt-daily-upgrade.timer: Persistent=$(systemctl show apt-daily-upgrade.timer -p Persistent --value 2>/dev/null || echo '?')$(
    [ -f "$TDROP" ] && echo ' (drop-in do projetor presente)')"
  [ -d "$EST" ] && echo "estado guardado em $EST: $(ls "$EST" 2>/dev/null | tr '\n' ' ')"
  return 0
}

sistema_aplicar() {
  mkdir -p "$EST"
  # guarda o "antes" só na primeira vez
  if [ ! -e "$EST/journald.antes" ]; then
    if [ -f "$JDROP" ]; then cp -p "$JDROP" "$EST/journald.antes"; else echo ausente > "$EST/journald.antes"; fi
  fi
  if [ ! -e "$EST/apt-timer.antes" ]; then
    if [ -f "$TDROP" ]; then cp -p "$TDROP" "$EST/apt-timer.antes"; else echo ausente > "$EST/apt-timer.antes"; fi
  fi
  echo "[1/2] journal com limite de 500 MB"
  mkdir -p "$(dirname "$JDROP")"
  cat > "$JDROP" <<'CONF'
# projetor-otimizacao (Projetor-LAOB, deploy/laob/otimizar-projetor.sh; desfazer: --desfazer)
# O journal do laob chegou a 4 GB sem limite; 500 MB guardam semanas de registro.
[Journal]
SystemMaxUse=500M
CONF
  systemctl restart systemd-journald
  journalctl --vacuum-size=500M 2>&1 | tail -1 | sed 's/^/  /'
  echo "[2/2] apt-daily-upgrade não roda atrasado logo depois do boot"
  mkdir -p "$(dirname "$TDROP")"
  cat > "$TDROP" <<'CONF'
# projetor-otimizacao (Projetor-LAOB, deploy/laob/otimizar-projetor.sh; desfazer: --desfazer)
# Sem rodar a atualização perdida ~10 s depois do boot, junto com a areia abrindo; segue às 6h todo dia.
[Timer]
Persistent=false
CONF
  systemctl daemon-reload
  echo "pronto. Desfazer: sudo bash $0 --desfazer"
}

sistema_desfazer() {
  # journald: volta o drop-in que havia antes (se havia) ou tira o nosso
  if [ -f "$EST/journald.antes" ] && [ "$(cat "$EST/journald.antes")" != ausente ]; then cp -p "$EST/journald.antes" "$JDROP"
  else rm -f "$JDROP"; rmdir "$(dirname "$JDROP")" 2>/dev/null || true; fi
  if [ -f "$EST/apt-timer.antes" ] && [ "$(cat "$EST/apt-timer.antes")" != ausente ]; then cp -p "$EST/apt-timer.antes" "$TDROP"
  else rm -f "$TDROP"; rmdir "$(dirname "$TDROP")" 2>/dev/null || true; fi
  systemctl daemon-reload
  systemctl restart systemd-journald
  rm -f "$EST"/*.antes; rmdir "$EST" 2>/dev/null || true
  echo "desfeito (os registros antigos que o limite apagou não voltam)"
}

# ------------------------------------------------------------------ parte do usuário (sem sudo)
# 3) Tela do GNOME nunca apaga nem bloqueia, e o PC não suspende na tomada. Em 19/11/2026 o GNOME apagou o
#    projetor após 15 min ocioso (DPMS) e a areia caiu para 1 qps; depois alguém voltou o idle-delay para 900.
#    A areia já se protege com gnome-session-inhibit enquanto está aberta (sandbox.sh); isto cobre o resto do
#    tempo e uma máquina recuperada. Os valores de antes ficam em $UEST/gnome.antes.
bus_do_usuario() {  # por ssh não há DBUS_SESSION_BUS_ADDRESS; o gsettings set precisa do barramento da sessão
  if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ] && [ -S "/run/user/$(id -u)/bus" ]; then
    export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u)/bus
  fi
}

usuario_mostra() {
  bus_do_usuario
  echo "== parte do usuário $(id -un) (sem sudo)"
  local l esq ch quer agora
  for l in "${GNOME[@]}"; do
    read -r esq ch quer <<< "$l"
    agora=$(gsettings get "$esq" "$ch" 2>/dev/null || echo '?')
    if [ "${agora#uint32 }" = "${quer#uint32 }" ]; then
      echo "ok     $esq $ch = $agora"
    else
      echo "mudaria $esq $ch: $agora -> $quer"
    fi
  done
  [ -f "$UEST/gnome.antes" ] && echo "estado guardado em $UEST/gnome.antes"
  return 0
}

usuario_aplicar() {
  bus_do_usuario
  local l esq ch quer
  mkdir -p "$UEST"
  if [ ! -e "$UEST/gnome.antes" ]; then
    for l in "${GNOME[@]}"; do
      read -r esq ch quer <<< "$l"
      echo "$esq $ch $(gsettings get "$esq" "$ch")"
    done > "$UEST/gnome.antes.tmp"
    mv -f "$UEST/gnome.antes.tmp" "$UEST/gnome.antes"
  fi
  echo "[1/1] tela do GNOME sem apagar, sem bloquear, sem suspender na tomada"
  for l in "${GNOME[@]}"; do
    read -r esq ch quer <<< "$l"
    gsettings set "$esq" "$ch" "$quer"
    echo "  $esq $ch = $(gsettings get "$esq" "$ch")"
  done
  echo "pronto. Desfazer: bash $0 --desfazer"
}

usuario_desfazer() {
  bus_do_usuario
  local esq ch antes
  [ -f "$UEST/gnome.antes" ] || { echo "nada guardado em $UEST: nada a desfazer"; return 0; }
  while read -r esq ch antes; do
    [ -n "$esq" ] && gsettings set "$esq" "$ch" "$antes" && echo "  $esq $ch = $antes"
  done < "$UEST/gnome.antes"
  rm -f "$UEST/gnome.antes"; rmdir "$UEST" 2>/dev/null || true
  echo "desfeito"
}

# ------------------------------------------------------------------
if [ "$ACAO" = simular ]; then
  echo "(simulação: nada é mudado)"
  sistema_mostra
  if [ ! -f "$JDROP" ]; then
    echo "faria: criar $JDROP (SystemMaxUse=500M), reiniciar o systemd-journald, journalctl --vacuum-size=500M"
  else echo "journal: já aplicado"; fi
  if [ ! -f "$TDROP" ]; then
    echo "faria: criar $TDROP (Persistent=false) e systemctl daemon-reload"
  else echo "apt-daily-upgrade: já aplicado"; fi
  echo "faria: guardar o antes em $EST (journald.antes, apt-timer.antes)"
  echo
  usuario_mostra
  echo "faria: guardar o antes em $UEST/gnome.antes e gsettings set nas chaves marcadas 'mudaria'"
  exit 0
fi

if [ "$(id -u)" = 0 ]; then
  sistema_mostra
  if [ "$ACAO" = desfazer ]; then sistema_desfazer; else sistema_aplicar; fi
  echo "(a parte da tela do GNOME é do usuário: rode sem sudo, bash $0${1:+ $1})"
else
  usuario_mostra
  if [ "$ACAO" = desfazer ]; then usuario_desfazer; else usuario_aplicar; fi
  echo "(a parte do sistema, journal e apt: sudo bash $0${1:+ $1})"
fi
