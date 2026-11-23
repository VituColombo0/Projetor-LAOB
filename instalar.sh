#!/bin/bash
# Instala o Projetor-LAOB num PC que já roda o AR Sandbox (SARndbox 2.8) calibrado, como o laob.
#   bash instalar.sh                  instala/atualiza tudo em ~/SARndbox-rosto (sem sudo)
#   bash instalar.sh --compilar DIR   só monta e compila a versão com modo Foto em DIR (teste)
#   bash instalar.sh --verificar      só confere a máquina e a instalação (não muda nada): uma linha
#                                     OK / AVISO / FALTA por item; sai com 1 se houver FALTA
# Variáveis: SARNDBOX_ORIG (padrão ~/SARndbox-2.8) e SARNDBOX_CALIB (pasta com BoxLayout.txt e
# ProjectorMatrix.dat; padrão /root/etc/SARndbox-2.8, a que o SARndbox do laob usa). Recuperação:
# testes/dados/ guarda a calibração do laob (SARNDBOX_CALIB=testes/dados), que só vale para aquela
# caixa, aquele Kinect e aquele projetor na posição de hoje (ver docs/RECUPERACAO.md).
# O SARndbox original nunca é alterado. Pode rodar de novo para atualizar (rodar 2x não duplica nada).
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ORIG=${SARNDBOX_ORIG:-$HOME/SARndbox-2.8}
CALIB=${SARNDBOX_CALIB:-/root/etc/SARndbox-2.8}
DEST=$HOME/SARndbox-rosto
VRUI_MAKE=/usr/local/share/Vrui-8.0/make
KINECT_ETC=/usr/local/etc/Vrui-8.0/Kinect-3.10
# sha256 dos modelos que o laob usa desde 10/11/2026 (o YuNet vem do "main" do opencv_zoo, que pode mudar)
SHA_YUNET=8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4
SHA_U2NET=01eb6a29a5c4d8edb30b56adad9bb3a2a0535338e480724a213e0acfd2d1c73c
URL_YUNET=https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
URL_U2NET=https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net_human_seg.onnx

# ---------------------------------------------------------------- funções (testadas em testes/rodar.sh)
PROBLEMAS=()
FALTAS=0
AVISOS=0
QUIETO=0   # 1 = mostra só AVISO/FALTA (na instalação); 0 = mostra tudo (no --verificar)
problema() { PROBLEMAS+=("$*"); echo "  !!! $*" >&2; }
aviso() { echo "  aviso: $*"; }
mostra() {  # OK|AVISO|FALTA texto
  case $1 in FALTA) FALTAS=$((FALTAS+1)) ;; AVISO) AVISOS=$((AVISOS+1)) ;; esac
  [ "$1" = OK ] && [ "$QUIETO" = 1 ] && return 0
  printf '%-6s %s\n' "$1" "$2"
}
tem_cmd() { command -v "$1" >/dev/null 2>&1; }
exige_cmd() {  # FALTA|AVISO comando pacote para-que
  if tem_cmd "$2"; then mostra OK "comando $2"; else mostra "$1" "falta o comando $2: sudo apt install $3 ($4)"; fi
}
md5de() { md5sum < "$1" | cut -d" " -f1; }

# Pré-requisitos (auditoria 23/11/2026: antes só o SARndbox era conferido, e o resto falhava no meio
# ou em silêncio). Os obrigatórios param a instalação; os outros só avisam o que deixa de funcionar.
confere_requisitos() {  # compilar|instalar|verificar
  if [ -x "$ORIG/bin/SARndbox" ]; then mostra OK "SARndbox original em $ORIG"
  else mostra FALTA "não achei o SARndbox compilado em $ORIG/bin/SARndbox (compile o SARndbox-2.8 ou defina SARNDBOX_ORIG)"; fi
  if [ -f "$VRUI_MAKE/SystemDefinitions" ] && [ -f "$VRUI_MAKE/Configuration.Kinect" ]; then mostra OK "Vrui-8.0 e Kinect-3.10 em /usr/local"
  else mostra FALTA "falta o Vrui-8.0/Kinect-3.10 em /usr/local ($VRUI_MAKE): compile e instale os dois antes (docs/RECUPERACAO.md)"; fi
  exige_cmd FALTA g++ build-essential "compilar a areia"
  exige_cmd FALTA make build-essential "compilar a areia"
  exige_cmd FALTA python3 python3 "patches e painel"
  [ "$1" = compilar ] && return 0
  if python3 -c 'import ensurepip' >/dev/null 2>&1; then mostra OK "python3-venv"
  else mostra FALTA "falta o python3-venv: sudo apt install python3-venv (o painel roda num Python próprio)"; fi
  exige_cmd FALTA wget wget "baixar os modelos do rosto e do recorte"
  exige_cmd AVISO ffmpeg ffmpeg "sem ele não há imagem ao vivo do projetor no painel nem prints dos testes"
  exige_cmd AVISO pactl pulseaudio-utils "sem ele a areia fica em silêncio"
  exige_cmd AVISO paplay pulseaudio-utils "sem ele a areia fica em silêncio"
  exige_cmd AVISO pacat pulseaudio-utils "sem ele não toca o som de fundo"
  exige_cmd AVISO xrandr x11-xserver-utils "sem ele a areia não acha o projetor e abre na tela principal"
  exige_cmd AVISO xwininfo x11-utils "o testes/integracao.sh usa para ver a areia no projetor"
  exige_cmd AVISO gnome-session-inhibit gnome-session-bin "sem ele a tela pode apagar com a areia aberta"
  exige_cmd AVISO xdg-user-dir xdg-user-dirs "sem ele usa ~/Desktop e ~/Pictures"
  exige_cmd AVISO curl curl "o testes/integracao.sh e o --verificar usam para falar com o painel"
}

# Favoritos do Arquivos: tira as linhas antigas e põe a das fotos uma vez só. Com set -e, o grep -v sem
# nenhuma linha que sobrasse (ou sem o arquivo, antes do 1º login gráfico) dava status 1 e o instalar.sh
# parava calado no passo 7 (auditoria 23/11/2026): por isso o "|| true" dentro do bloco.
atualiza_favorito() {  # arquivo-de-favoritos pasta-da-mesa pasta-das-fotos
  local b=$1 mesa=$2 fotos=$3
  mkdir -p "$(dirname "$b")"
  { grep -vF "Fotos%20na%20areia" "$b" 2>/dev/null | grep -vF "file://${mesa// /%20}/Areia " || true; } > "$b.tmp"
  echo "file://${fotos// /%20} Fotos na areia" >> "$b.tmp"
  mv -f "$b.tmp" "$b"
}

# Cópia que troca o arquivo de uma vez (inode novo, 23/11/2026): o bash lê o script aos poucos enquanto
# roda, e o vigia ou o sandbox.sh podem estar no meio de um sleep durante a instalação; com cp por cima
# eles continuariam lendo o arquivo NOVO a partir do byte em que estavam no antigo.
copia_atomica() {  # origem destino [modo]
  cp -- "$1" "$2.novo"
  [ -z "${3:-}" ] || chmod "$3" "$2.novo"
  mv -f -- "$2.novo" "$2"
}

# Calibração (caixa e projetor), regra de 23/11/2026: o md5 do que veio de $CALIB fica anotado em
# .calib-instalada.md5. Arquivo da cópia que ninguém mexeu é atualizado quando $CALIB muda. Arquivo mudado
# aqui (recalibração pelo bin/CalibrateProjector da cópia, que grava em $DEST/etc) e mais novo que o de
# $CALIB fica, com aviso; se o de $CALIB for o mais novo, ele entra e o local vira <arquivo>.antes-<data>.
# Sem registro, um arquivo igual ao da pasta etc do SARndbox original (o que o "cp -a" de uma cópia nova
# traz) não conta como mudado aqui: no laob o ~/SARndbox-2.8/etc tem um BoxLayout.txt de 17/07/2024 que
# nunca foi usado (o original lê /root/etc) e, por ser mais novo, era mantido numa cópia nova (23/11/2026).
instala_calibracao() {  # pasta-de-origem pasta-destino [pasta-etc-do-original]
  local de=$1 para=$2 fab=${3:-} f md5o md5d md5i reg novo mexido
  mkdir -p "$para"; reg=$para/.calib-instalada.md5; novo=$reg.tmp
  : > "$novo"
  for f in BoxLayout.txt ProjectorMatrix.dat HeightColorMap.cpt SARndbox.cfg; do
    md5i=$(awk -v f="$f" '$2 == f {print $1}' "$reg" 2>/dev/null || true)
    if [ ! -r "$de/$f" ]; then
      [ -z "$md5i" ] || echo "$md5i $f" >> "$novo"
      continue
    fi
    md5o=$(md5de "$de/$f")
    if [ -f "$para/$f" ]; then
      md5d=$(md5de "$para/$f")
      mexido=0
      [ "$md5d" = "$md5i" ] || mexido=1
      if [ -z "$md5i" ] && [ -n "$fab" ] && [ -f "$fab/$f" ] && [ "$md5d" = "$(md5de "$fab/$f")" ]; then mexido=0; fi
      if [ "$md5d" = "$md5o" ]; then
        :   # igual: nada a fazer
      elif [ $mexido = 1 ] && [ "$para/$f" -nt "$de/$f" ]; then
        aviso "calibração: $f desta cópia foi mudado aqui e é mais novo que o de $de: mantido (apague $para/$f para voltar ao de $de)"
        [ -z "$md5i" ] || echo "$md5i $f" >> "$novo"
        continue
      else
        [ $mexido = 0 ] || cp -p "$para/$f" "$para/$f.antes-$(date +%Y%m%d-%H%M%S)"
        cp "$de/$f" "$para/$f"; echo "  calibração: $f atualizado a partir de $de"
      fi
    else
      cp "$de/$f" "$para/$f"
    fi
    echo "$md5o $f" >> "$novo"
  done
  mv -f "$novo" "$reg"
}

confere_sha() {  # arquivo sha256
  [ "$(sha256sum < "$1" | cut -d" " -f1)" = "$2" ]
}

# Modelo baixado só se faltar. Antes (wget -q, sem trap) uma falha de rede derrubava a instalação no
# passo 5 sem nenhuma mensagem; agora diz o que faltou e a instalação segue (o resto funciona sem a foto).
baixa() {  # arquivo url sha256
  local f=$DEST/modelos/$1 mostra_prog=()
  if [ -s "$f" ]; then
    confere_sha "$f" "$3" || aviso "$1 é diferente do que o laob usa (sha256): mantido"
    return 0
  fi
  [ -t 2 ] && mostra_prog=(--show-progress)
  echo "  baixando $1"
  if wget -q "${mostra_prog[@]}" --tries=3 --timeout=60 -O "$f.part" "$2" && confere_sha "$f.part" "$3"; then
    mv -f "$f.part" "$f"
  else
    rm -f "$f.part"
    problema "não consegui baixar $1 de $2 (sem rede? o endereço mudou?). Sem ele a foto na areia não funciona. Rode o instalar.sh de novo com rede, ou copie o arquivo do backup para $DEST/modelos/"
  fi
}

# systemctl --user sem derrubar a instalação: por ssh sem sessão gráfica (e sem linger) não há o systemd do usuário
sysu() {
  systemctl --user "$@" >/dev/null 2>&1 || problema "systemctl --user $* falhou (sem a sessão do usuário? rode de novo depois do login gráfico)"
}

# ---------------------------------------------------------------- --verificar (só lê)
verificar() {
  local x n f d i lista
  export DISPLAY=${DISPLAY:-:0} XAUTHORITY=${XAUTHORITY:-/run/user/$(id -u)/gdm/Xauthority}
  echo "== base"
  confere_requisitos verificar
  x=$(ls "$KINECT_ETC"/IntrinsicParameters-*.dat 2>/dev/null | head -1)
  if [ -n "$x" ]; then mostra OK "calibração interna do Kinect ($(basename "$x"))"
  else mostra AVISO "sem $KINECT_ETC/IntrinsicParameters-*.dat: sudo /usr/local/bin/KinectUtil getCalib 0"; fi
  if [ -f /etc/udev/rules.d/69-Kinect.rules ]; then mostra OK "regra udev do Kinect"
  else mostra AVISO "sem /etc/udev/rules.d/69-Kinect.rules (vem do make install do Kinect-3.10)"; fi
  if tem_cmd lsusb; then
    if lsusb -d 045e:02ae >/dev/null 2>&1; then mostra OK "Kinect ligado no USB"; else mostra FALTA "Kinect não aparece no USB (lsusb -d 045e:02ae): confira o cabo e a fonte"; fi
  fi
  exige_cmd AVISO gdbus libglib2.0-bin "o vigia usa para destravar o GNOME"
  exige_cmd AVISO gio libglib2.0-bin "marca o atalho da Área de Trabalho como confiável"
  exige_cmd AVISO lsusb usbutils "a areia confere o Kinect antes de abrir"
  exige_cmd AVISO nvidia-settings nvidia-settings "desempenho máximo da placa, PowerMizer"

  echo "== calibração"
  d=$DEST/etc/SARndbox-2.8
  for f in BoxLayout.txt ProjectorMatrix.dat; do
    if [ ! -s "$d/$f" ]; then mostra FALTA "sem $d/$f: a areia abre descalibrada (rode o instalar.sh com SARNDBOX_CALIB)"
    elif [ ! -r "$CALIB/$f" ]; then mostra AVISO "$f instalado, mas sem $CALIB/$f para comparar"
    elif [ "$(md5de "$d/$f")" = "$(md5de "$CALIB/$f")" ]; then mostra OK "$f igual ao de $CALIB"
    else mostra AVISO "$f da cópia é diferente do de $CALIB (recalibrado aqui? o instalar.sh mantém o mais novo)"; fi
  done

  echo "== programa da areia"
  if [ -x "$DEST/bin/SARndbox" ]; then
    mostra OK "areia compilada em $DEST/bin/SARndbox"
    lista=""
    for f in "$DEST"/{FaceImage.cpp,Extras.cpp,Gesto.cpp,Jogos.cpp,Mundo.cpp}; do
      [ -f "$f" ] && [ "$f" -nt "$DEST/bin/SARndbox" ] && lista="$lista $(basename "$f")"
    done
    if [ -z "$lista" ]; then mostra OK "compilação mais nova que as fontes"
    else mostra AVISO "fontes mais novas que o programa compilado:$lista (rode o instalar.sh)"; fi
  else
    mostra FALTA "sem $DEST/bin/SARndbox: rode o instalar.sh"
  fi
  # o que está instalado é o que está no repositório? (antes de reinstalar no laob: mudanças feitas à mão)
  lista=""; n=0
  [ -d "$DEST" ] && for f in "$REPO"/sarndbox/* "$REPO"/areia/*.py "$REPO"/areia/sandbox.sh "$REPO"/areia/areia-*; do
    [ -f "$f" ] || continue
    if ! cmp -s "$f" "$DEST/$(basename "$f")"; then n=$((n+1)); [ $n -le 6 ] && lista="$lista $(basename "$f")"; fi
  done
  [ -d "$DEST" ] && for f in "$REPO"/sistema/*.service "$REPO"/sistema/*.timer; do
    cmp -s "$f" "$HOME/.config/systemd/user/$(basename "$f")" || { n=$((n+1)); [ $n -le 6 ] && lista="$lista $(basename "$f")"; }
  done
  if [ ! -d "$DEST" ]; then :
  elif [ $n = 0 ]; then mostra OK "arquivos instalados iguais aos do repositório"
  else mostra AVISO "$n arquivo(s) instalado(s) diferentes do repositório:$lista$([ $n -gt 6 ] && echo " ...") (rode o instalar.sh, ou traga ao repo o que foi mudado à mão)"; fi

  echo "== Python próprio (venv) e modelos"
  if [ -x "$DEST/venv/bin/pip" ]; then
    mostra OK "venv com pip"
    if "$DEST/venv/bin/python" -B -c 'import cv2, numpy, flask, waitress, PIL' >/dev/null 2>&1; then mostra OK "venv importa cv2, numpy, flask, waitress e PIL"
    else mostra FALTA "o venv não importa cv2/numpy/flask/waitress/PIL: rode o instalar.sh"; fi
    lista=$("$DEST/venv/bin/pip" freeze 2>/dev/null | tr 'A-Z' 'a-z' | sort || true)
    x=""
    while read -r i; do
      i=${i%%#*}; i=$(echo "$i" | tr -d ' ' | tr 'A-Z' 'a-z'); [ -n "$i" ] || continue
      grep -qxF "$i" <<< "$lista" || x="$x $i"
    done < "$REPO/requirements.txt"
    if [ -z "$x" ]; then mostra OK "versões iguais às do requirements.txt"; else mostra AVISO "venv com versões diferentes de:$x (rode o instalar.sh)"; fi
  else
    mostra FALTA "sem $DEST/venv/bin/pip: rode o instalar.sh (ele refaz o venv)"
  fi
  for x in "face_detection_yunet_2023mar.onnx $SHA_YUNET" "u2net_human_seg.onnx $SHA_U2NET"; do
    f=${x%% *}
    if [ ! -s "$DEST/modelos/$f" ]; then mostra FALTA "sem o modelo $DEST/modelos/$f: rode o instalar.sh com rede"
    elif confere_sha "$DEST/modelos/$f" "${x##* }"; then mostra OK "modelo $f ($(du -h "$DEST/modelos/$f" | cut -f1))"
    else mostra AVISO "modelo $f diferente do que o laob usa (sha256)"; fi
  done
  n=$(ls "$DEST"/membros/*.{jpg,jpeg,png,JPG,JPEG,PNG} 2>/dev/null | wc -l)
  if [ "$n" -gt 0 ]; then mostra OK "$n foto(s) de membros em $DEST/membros"
  else mostra AVISO "sem fotos em $DEST/membros (Nome.jpg; os bonecos do Mundo LAOB ficam sem rosto)"; fi
  if [ -s "$DEST/fotos/membros.rgba" ]; then mostra OK "rostos dos membros montados (fotos/membros.rgba)"
  else mostra AVISO "sem $DEST/fotos/membros.rgba (o painel monta ao abrir o Mundo LAOB)"; fi

  echo "== comandos, Área de Trabalho e ao ligar"
  lista=""
  for f in "$REPO"/areia/areia-*; do
    x=$(basename "$f")
    [ "$(readlink "$HOME/.local/bin/$x" 2>/dev/null)" = "$DEST/$x" ] || lista="$lista $x"
  done
  if [ -z "$lista" ]; then mostra OK "comandos areia-* em ~/.local/bin"; else mostra FALTA "comandos fora de ~/.local/bin:$lista"; fi
  if [[ ":$PATH:" == *":$HOME/.local/bin:"* ]] || grep -qs '\.local/bin' "$HOME/.profile"; then mostra OK "~/.local/bin no PATH"
  else mostra AVISO "~/.local/bin não está no PATH (os comandos areia-* não acham)"; fi
  if grep -qs '^Exec=.*areia-topografia' "$HOME/.config/autostart/SARndbox.desktop"; then mostra OK "ao ligar abre a topografia"
  else mostra FALTA "o autostart não abre a areia-topografia: rode o instalar.sh"; fi
  if [ -f "$HOME/.config/autostart/nvidia-desempenho.desktop" ]; then mostra OK "placa em desempenho máximo ao ligar (Willbert)"
  else mostra AVISO "sem ~/.config/autostart/nvidia-desempenho.desktop (vem do otimizar.sh do Willbert-LAOB)"; fi
  x=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop"); [ "$x" = "$HOME" ] && x=$HOME/Desktop
  if [ -x "$x/Abrir painel da caixa de areia.desktop" ]; then mostra OK "atalho do painel na Área de Trabalho"
  else mostra AVISO "sem o atalho do painel em $x"; fi
  x=$(xdg-user-dir PICTURES 2>/dev/null || echo "$HOME/Pictures"); [ "$x" = "$HOME" ] && x=$HOME/Pictures
  if [ -d "$x/Fotos na areia" ]; then mostra OK "pasta de fotos ($x/Fotos na areia)"; else mostra AVISO "sem a pasta $x/Fotos na areia"; fi
  if grep -qsF "Fotos%20na%20areia" "$HOME/.config/gtk-3.0/bookmarks"; then mostra OK "favorito das fotos no Arquivos"
  else mostra AVISO "sem o favorito das fotos no Arquivos"; fi

  echo "== serviços do usuário"
  if systemctl --user show-environment >/dev/null 2>&1; then
    for x in sandbox-painel.service areia-vigia.timer atualizar-papel-parede.timer; do
      if systemctl --user is-enabled -q "$x" 2>/dev/null && systemctl --user is-active -q "$x" 2>/dev/null; then mostra OK "$x ligado"
      elif [ "$x" = atualizar-papel-parede.timer ]; then mostra AVISO "$x desligado (papel de parede com o endereço do painel)"
      else mostra FALTA "$x não está ligado: systemctl --user enable --now $x"; fi
    done
    for x in sandbox-painel areia-vigia; do
      if [ "$(systemctl --user show "$x" -p KillMode --value 2>/dev/null)" = process ] && [ "$(systemctl --user show "$x" -p Nice --value 2>/dev/null)" = 0 ]; then
        mostra OK "$x com KillMode=process e sem Nice"
      else mostra FALTA "$x sem KillMode=process ou com Nice (a areia abriria e fecharia; regra de 13/11)"; fi
    done
  else
    mostra AVISO "sem acesso ao systemd do usuário (ssh sem sessão?): não deu para conferir painel e vigia"
  fi
  if [ -e "$DEST/sem-vigia" ]; then mostra AVISO "vigia pausado (areia-vigia liga)"; else mostra OK "vigia não pausado"; fi
  if [ "$(loginctl show-user "$(id -un)" -p Linger --value 2>/dev/null)" = yes ]; then mostra OK "linger ligado (serviços sobem no boot)"
  else mostra AVISO "linger desligado: loginctl enable-linger $(id -un)"; fi
  if grep -qs "^AutomaticLogin=$(id -un)" /etc/gdm3/custom.conf && grep -qs '^AutomaticLoginEnable=[Tt]rue' /etc/gdm3/custom.conf; then
    mostra OK "login automático"
  else mostra AVISO "login automático desligado: sem ele a areia não abre sozinha depois de reiniciar (Configurações > Usuários)"; fi

  echo "== tela e GNOME"
  if tem_cmd gsettings; then
    x=$(gsettings get org.gnome.desktop.session idle-delay 2>/dev/null || true)
    if [ "${x##* }" = 0 ]; then mostra OK "tela não apaga sozinha (idle-delay 0)"
    elif tem_cmd gnome-session-inhibit; then mostra AVISO "idle-delay ${x##* } (a areia segura com gnome-session-inhibit; fora dela a tela apaga)"
    else mostra FALTA "idle-delay ${x##* } e sem gnome-session-inhibit: a tela apaga e a areia cai para 1 qps"; fi
    [ "$(gsettings get org.gnome.desktop.screensaver lock-enabled 2>/dev/null)" = false ] && mostra OK "sem bloqueio de tela" \
      || mostra AVISO "bloqueio de tela ligado (gsettings set org.gnome.desktop.screensaver lock-enabled false)"
    [ "$(gsettings get org.gnome.settings-daemon.plugins.power sleep-inactive-ac-type 2>/dev/null)" = "'nothing'" ] && mostra OK "não suspende na tomada" \
      || mostra AVISO "o PC pode suspender sozinho (gsettings set org.gnome.settings-daemon.plugins.power sleep-inactive-ac-type nothing)"
  fi
  if tem_cmd xrandr && x=$(xrandr --current 2>/dev/null); then
    if grep -qE ' connected .*1024x768\+1280\+0' <<< "$x"; then mostra OK "projetor em 1024x768 à direita do monitor (+1280+0)"
    else mostra AVISO "projetor não está em 1024x768+1280+0 (desligado? Configurações > Telas)"; fi
  else
    mostra AVISO "sem acesso à tela :0 (xrandr): não deu para conferir o projetor"
  fi

  echo "== som"
  if tem_cmd pactl && x=$(pactl list short sinks 2>/dev/null); then
    if grep -qw combinado <<< "$x"; then mostra OK "saída combinado (projetor + caixas)"
    else mostra AVISO "sem a saída combinado: o painel recria ao abrir (systemctl --user restart sandbox-painel)"; fi
    grep -q hdmi <<< "$x" && mostra OK "saída HDMI (projetor)" || mostra AVISO "sem saída HDMI (projetor desligado?)"
    grep -q analog <<< "$x" && mostra OK "saída analógica (caixas)" || mostra AVISO "sem saída analógica"
  else
    mostra AVISO "sem acesso ao PulseAudio: não deu para conferir o som"
  fi
  if grep -qs '"mudo": *true' "$DEST/estado_som.json"; then mostra AVISO "som no mudo pelo painel (botão 🔇 no cabeçalho)"; fi

  echo "== placa de vídeo"
  if tem_cmd nvidia-smi && nvidia-smi -L >/dev/null 2>&1; then mostra OK "NVIDIA responde ($(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>/dev/null | head -1))"
  else mostra FALTA "a placa NVIDIA não responde (nvidia-smi): kernel sem o driver? ver kernel-com-driver.sh do Willbert-LAOB"; fi
  if [ -d /sys/module/nvidia ]; then mostra OK "módulo nvidia carregado no kernel $(uname -r)"; else mostra FALTA "módulo nvidia não carregado no kernel $(uname -r)"; fi
  f=/etc/default/grub.d/99-willbert-kernel.cfg
  if [ -f "$f" ]; then
    x=$(grep -o 'gnulinux-[0-9][^-]*-[0-9]*-[a-z]*' "$f" | head -1 | sed 's/^gnulinux-//')
    if [ -z "$x" ] || [ "$x" = "$(uname -r)" ]; then mostra OK "kernel fixado com o driver ($(uname -r))"
    else mostra AVISO "GRUB fixado em $x, mas o kernel de agora é $(uname -r)"; fi
  else
    mostra AVISO "kernel não fixado ($f): um kernel novo sem o driver NVIDIA deixa a areia a 1 qps"
  fi

  echo "== painel e Willbert"
  if tem_cmd curl && curl -sf -m 5 http://127.0.0.1:8090/estado >/dev/null 2>&1; then mostra OK "painel responde em :8090"
  else mostra FALTA "o painel não responde em http://127.0.0.1:8090 (systemctl --user status sandbox-painel)"; fi
  if [ -x /usr/local/sbin/willbert-controle ]; then mostra OK "willbert-controle instalado"
  else mostra AVISO "sem /usr/local/sbin/willbert-controle: o painel não liga/desliga o Willbert (otimizar.sh do Willbert-LAOB)"; fi

  echo "== máquina"
  x=$(df -BG --output=avail / 2>/dev/null | tail -1 | tr -dc 0-9)
  if [ -z "$x" ]; then :; elif [ "$x" -lt 2 ]; then mostra FALTA "só ${x} GB livres em /"
  elif [ "$x" -lt 10 ]; then mostra AVISO "só ${x} GB livres em /"; else mostra OK "${x} GB livres em /"; fi
  x=$(df -BM --output=avail /dev/shm 2>/dev/null | tail -1 | tr -dc 0-9)
  if [ -n "$x" ] && [ "$x" -lt 1000 ]; then mostra AVISO "só ${x} MB livres em /dev/shm (gestoGrava usa ~400 MB)"
  elif [ -n "$x" ]; then mostra OK "${x} MB livres em /dev/shm"; fi
  if [ -f /etc/systemd/journald.conf.d/projetor.conf ]; then mostra OK "journal com limite (otimizar-projetor.sh)"
  else mostra AVISO "journal sem limite: sudo bash deploy/laob/otimizar-projetor.sh"; fi

  echo
  echo "$FALTAS FALTA, $AVISOS AVISO"
  [ "$FALTAS" = 0 ]
}

# testes/rodar.sh: "source" só das funções (rodado direto com a variável, sai sem instalar nada)
[ "${INSTALAR_SO_FUNCOES:-}" = 1 ] && { return 0 2>/dev/null || exit 0; }

# ---------------------------------------------------------------- instalação
MODO=instalar
case "${1:-}" in
  --compilar) MODO=compilar; DEST=${2:?uso: instalar.sh --compilar DIR} ;;
  --verificar) verificar; exit $? ;;
  "") ;;
  -h|--help) sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  *) echo "uso: bash instalar.sh [--compilar DIR | --verificar]"; exit 2 ;;
esac

set -Eeuo pipefail
ETAPA="pré-requisitos"
trap 'echo "ERRO: o instalar.sh parou na etapa \"$ETAPA\" (linha $LINENO: $BASH_COMMAND). Nada do que já estava instalado foi apagado; corrija e rode de novo." >&2' ERR

echo "[0/8] pré-requisitos"
QUIETO=1
confere_requisitos "$MODO"
if [ "$FALTAS" != 0 ]; then echo "faltam $FALTAS pré-requisito(s) acima: instale e rode de novo (nada foi mudado)"; exit 1; fi
QUIETO=0

ETAPA="1/8 cópia do SARndbox"
echo "[1/8] cópia do SARndbox em $DEST (o original em $ORIG fica intacto)"
if [ ! -d "$DEST" ]; then cp -a "$ORIG" "$DEST"; fi
# fontes sempre limpas do original antes dos patches
for f in SurfaceRenderer.h SurfaceRenderer.cpp Sandbox.h Sandbox.cpp WaterTable2.h WaterTable2.cpp FrameFilter.h FrameFilter.cpp makefile; do cp "$ORIG/$f" "$DEST/$f"; done
instala_calibracao "$CALIB" "$DEST/etc/SARndbox-2.8" "$ORIG/etc/SARndbox-2.8"
if [ $MODO = instalar ]; then
  for f in BoxLayout.txt ProjectorMatrix.dat; do
    [ -s "$DEST/etc/SARndbox-2.8/$f" ] || problema "SEM CALIBRAÇÃO: falta $f (nem em $CALIB nem na cópia). A areia abre com o relevo e as cores fora do lugar. Calibre o SARndbox, ou, se for a mesma caixa/Kinect/projetor do laob: SARNDBOX_CALIB=$REPO/testes/dados bash instalar.sh"
  done
fi

ETAPA="2/8 patches e compilação"
echo "[2/8] modo Foto: patches e compilação"
PROGRAMA_ANTES=$(md5sum "$DEST/bin/SARndbox" 2>/dev/null | cut -d" " -f1 || true)
cp "$REPO"/sarndbox/{FaceImage.h,FaceImage.cpp,Perfil.h,Gesto.h,Gesto.cpp,Extras.h,Extras.cpp,Jogos.cpp,Mundo.cpp,MundoPlanta.h,patch_surface.py,patch_sandbox.py,patch_agua.py,projetor.cfg} "$DEST/"
for S in share/SARndbox-2.8/Shaders/SurfaceAddWaterColor.fs share/SARndbox-2.8/Shaders/Water2WaterUpdateShader.fs share/SARndbox-2.8/Shaders/SurfaceAddContourLines.fs; do
  mkdir -p "$DEST/$(dirname "$S")" && cp "$ORIG/$S" "$DEST/$S"   # sempre do original, antes do patch
done
( cd "$DEST" && python3 patch_surface.py >/dev/null && python3 patch_sandbox.py >/dev/null && python3 patch_agua.py >/dev/null )
if ! make -C "$DEST" INSTALLDIR="$DEST" > "$DEST/compilacao.log" 2>&1 \
   || ! [ "$DEST/bin/SARndbox" -nt "$DEST/FaceImage.cpp" ]; then
  echo "a compilação falhou; veja $DEST/compilacao.log (últimas linhas:)" >&2
  tail -5 "$DEST/compilacao.log" >&2 || true
  exit 1
fi
[ $MODO = compilar ] && { echo "compilado em $DEST/bin/SARndbox"; exit 0; }

ETAPA="3/8 programas da pasta areia"
echo "[3/8] programas da pasta areia/"
for f in "$REPO"/areia/*.py; do copia_atomica "$f" "$DEST/$(basename "$f")"; done
copia_atomica "$REPO/areia/sandbox.sh" "$DEST/sandbox.sh" 755
COMANDOS=()
for f in "$REPO"/areia/areia-*; do COMANDOS+=("$(basename "$f")"); copia_atomica "$f" "$DEST/$(basename "$f")" 755; done
mkdir -p "$DEST/sons" && cp "$REPO"/areia/sons/*.wav "$DEST/sons/"
# sons retirados do repositório em 23/11/2026 (o cp não apaga o que sobrou de antes)
rm -f "$DEST"/sons/{cigarra_1,cigarra_2,eco_marte_1,eco_marte_2,gaivota_1,gaivota_2,neve_galho,passarinho_1,passarinho_2,passarinho_3,sino_neve_1,sino_neve_2,sino_neve_3,trovao_1,trovao_2}.wav
# Som combinado (HDMI do projetor + caixas). Quem cuida dele de verdade é o som.py (recria quando falta);
# aqui é só uma ajuda, que nunca derruba a instalação.
if tem_cmd pactl; then
  SINKS=$(pactl list short sinks 2>/dev/null || true)
  if ! grep -qw combinado <<< "$SINKS"; then
    pactl load-module module-combine-sink sink_name=combinado slaves=alsa_output.pci-0000_01_00.1.hdmi-stereo,alsa_output.pci-0000_00_1b.0.analog-stereo >/dev/null 2>&1 || true
  fi
  pactl set-sink-mute alsa_output.pci-0000_00_1b.0.analog-stereo 0 2>/dev/null || true
else
  aviso "sem pactl: a areia fica em silêncio (sudo apt install pulseaudio-utils)"
fi
mkdir -p "$DEST/membros"   # fotos dos membros para as pessoinhas (Nome.jpg); ficam só nesta máquina
rm -f "$DEST"/gui-*.sh   # janelas dos atalhos da Área de Trabalho (substituídos pelo painel em 13/11)
rm -f "$DEST"/*.bak "$DEST/patch_brincar.py" "$DEST/gui-willbert-estado.sh" "$DEST"/*.novo   # sobras de edições manuais
mkdir -p "$DEST/temas" && cp "$REPO"/areia/temas/*.cpt "$DEST/temas/"   # o tema escolhido (temas/atual.cpt) fica
# o tema "mapa de papel" saiu em 16/11/2026: se era o escolhido, volta o clássico
rm -f "$DEST/temas/mapa.cpt"
[ "$(readlink "$DEST/temas/atual.cpt" 2>/dev/null)" = mapa.cpt ] && ln -sfn classico.cpt "$DEST/temas/atual.cpt"
rm -f "$DEST/Sandbox.cpp.antes-area"  # sobra de versões antigas

ETAPA="4/8 venv"
echo "[4/8] Python próprio (venv) com OpenCV e Flask"
# A guarda antiga só olhava o bin/python: sem o python3-venv, o "python3 -m venv" cria a pasta e o link e
# falha no ensurepip, e a instalação seguinte caía em "venv/bin/pip: No such file" (auditoria 23/11/2026).
if [ ! -x "$DEST/venv/bin/pip" ] || ! "$DEST/venv/bin/python" -c 'import sys' >/dev/null 2>&1; then
  [ -d "$DEST/venv" ] && echo "  o venv estava pela metade: refazendo"
  rm -rf "$DEST/venv"
  python3 -m venv "$DEST/venv"
fi
"$DEST/venv/bin/pip" install -q -r "$REPO/requirements.txt" \
  || problema "o pip não instalou o requirements.txt (sem rede?): o painel pode não abrir. Rode o instalar.sh de novo com rede"

ETAPA="5/8 modelos"
echo "[5/8] modelos (rosto e recorte do fundo)"
mkdir -p "$DEST/modelos"
baixa face_detection_yunet_2023mar.onnx "$URL_YUNET" "$SHA_YUNET"
baixa u2net_human_seg.onnx "$URL_U2NET" "$SHA_U2NET"

ETAPA="6/8 comandos"
echo "[6/8] comandos: ${COMANDOS[*]}"
mkdir -p "$HOME/.local/bin" "$DEST/fotos"
for c in "${COMANDOS[@]}"; do ln -sfn "$DEST/$c" "$HOME/.local/bin/$c"; done

ETAPA="7/8 Área de Trabalho e fotos"
echo "[7/8] tudo pelo painel: fotos em Imagens/Fotos na areia, Área de Trabalho só com o atalho do painel"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop"); [ "$MESA" = "$HOME" ] && MESA=$HOME/Desktop
IMGS=$(xdg-user-dir PICTURES 2>/dev/null || echo "$HOME/Pictures"); [ "$IMGS" = "$HOME" ] && IMGS=$HOME/Pictures
FOTOS_DIR="$IMGS/Fotos na areia"
mkdir -p "$FOTOS_DIR" "$MESA"
# fotos das versões antigas (pasta "2 - Fotos na areia" ou "Areia" na Área de Trabalho) vão para o lugar novo
for velha in "$MESA/2 - Fotos na areia" "$MESA/Areia"; do
  [ -d "$velha" ] || continue
  find "$velha" -mindepth 1 -maxdepth 1 ! -name "*.desktop" ! -name "COMO USAR.txt*" -print0 |
    while IFS= read -r -d "" f; do
      alvo="$FOTOS_DIR/$(basename "$f")"
      if [ -d "$f" ] && [ -d "$alvo" ]; then cp -an "$f/." "$alvo/" && rm -rf "$f"
      elif [ ! -e "$alvo" ]; then mv "$f" "$alvo"; fi
    done
  [ -f "$DEST/estado.json" ] && sed -i "s#$velha/#$FOTOS_DIR/#g" "$DEST/estado.json"   # última foto no lugar novo
done
# as pastas 1 a 8 (manual e atalhos, que eram do repositório) saem; se sobrar algo de outra pessoa, a pasta fica
for velha in "$MESA"/[1-8]\ -\ *; do
  [ -d "$velha" ] || continue
  rm -f "$velha"/*.desktop "$velha/COMO USAR.txt" "$velha/COMO USAR.txt.old"
  rmdir "$velha" 2>/dev/null || echo "  ficou $velha (tem arquivos que não são do Projetor)"
done
rm -f "$MESA/SE A AREIA NÃO ABRIR.txt" "$MESA/Abrir a areia.desktop"
for at in "$REPO"/area-de-trabalho/*.desktop; do   # só o atalho do painel
  [ -e "$at" ] || continue
  destino="$MESA/$(basename "$at")"
  sed "s#/home/laob#$HOME#g" "$at" > "$destino.novo"; chmod +x "$destino.novo"; mv -f "$destino.novo" "$destino"
  gio set "$destino" metadata::trusted true 2>/dev/null || true
done
atualiza_favorito "$HOME/.config/gtk-3.0/bookmarks" "$MESA" "$FOTOS_DIR"

ETAPA="8/8 autostart, painel e vigia"
echo "[8/8] topografia ao ligar, painel do celular e vigia"
A=$HOME/.config/autostart/SARndbox.desktop
mkdir -p "$(dirname "$A")"
if [ -f "$A" ]; then
  # guarda o original só se ele ainda é o original (numa máquina recuperada o autostart já pode ser o nosso)
  if [ ! -f "$DEST/SARndbox.desktop.original" ] && ! grep -q 'areia-topografia' "$A"; then cp "$A" "$DEST/SARndbox.desktop.original"; fi
  sed -i "s#^Exec=.*#Exec=$DEST/areia-topografia#" "$A"
else
  printf '[Desktop Entry]\nType=Application\nName=SANDBOX\nExec=%s/areia-topografia\nX-GNOME-Autostart-enabled=true\n' "$DEST" > "$A"
fi
U=$HOME/.config/systemd/user
mkdir -p "$U"
for f in "$REPO"/sistema/*.service "$REPO"/sistema/*.timer; do copia_atomica "$f" "$U/$(basename "$f")"; done
sysu daemon-reload
# painel: reinicia de propósito (pega o painel.py novo)
sysu enable --now sandbox-painel
sysu restart sandbox-painel
# vigia: se a areia fechar sozinha, reabre a topografia (areia-vigia pausa / liga)
sysu enable --now areia-vigia.timer
# papel de parede com o endereço do painel (muda sozinho se o IP mudar)
mkdir -p "$HOME/Pictures"
[ -s "$HOME/Pictures/logo-original.png" ] || cp "$REPO/sistema/logo-laob.png" "$HOME/Pictures/logo-original.png"
sysu enable --now atualizar-papel-parede.timer

# A areia aberta continua sendo o programa antigo até fechar (em 14/11/2026 os jogos novos
# "não existiam" depois de instalar): se o programa mudou e ela está aberta, reabre agora.
ETAPA="reabrir a areia"
if [ "$PROGRAMA_ANTES" != "$(md5sum "$DEST/bin/SARndbox" | cut -d" " -f1)" ] \
   && [ "$("$DEST/sandbox.sh" estado 2>/dev/null || true)" = rosto ]; then
  echo "o programa da areia mudou: reabrindo a areia (uns 15 s)"
  # "reabrir": fecha e abre a topografia direto, sem passar pelo SARndbox original (e religa o vigia).
  # Abrir de novo também liga a rotação do sandbox.log (o programa novo escreve com O_APPEND).
  "$DEST/sandbox.sh" reabrir >/dev/null 2>&1 || echo "  a areia não reabriu sozinha: use o botão 'Reabrir a areia' do painel"
fi

echo
if [ ${#PROBLEMAS[@]} -gt 0 ]; then
  echo "==================================================================="
  echo " A instalação terminou, mas ${#PROBLEMAS[@]} coisa(s) precisam de atenção:"
  for p in "${PROBLEMAS[@]}"; do echo "  - $p"; done
  echo " Depois de corrigir: bash instalar.sh de novo (e bash instalar.sh --verificar)"
  echo "==================================================================="
  exit 1
fi
echo "Pronto. Tudo pelo painel: http://localhost:8090 (atalho na Área de Trabalho)."
echo "Conferir a máquina: bash instalar.sh --verificar   Teste completo: bash testes/integracao.sh"
