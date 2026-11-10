#!/bin/bash
# Teste no PC de verdade (lab2), depois do instalar.sh. Usa o projetor por ~1 minuto e termina
# na topografia; o estado (última imagem, giro, ajustes) volta a ser o de antes.
#   bash testes/integracao.sh
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
D=$HOME/SARndbox-rosto
export PATH=$HOME/.local/bin:$PATH
falhas=0
ok() { echo "OK    $*"; }
falha() { echo "FALHA $*"; falhas=1; }
confere() { if eval "$2"; then ok "$1"; else falha "$1"; fi; }
no_projetor() { DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority xwininfo -root -tree 2>/dev/null \
                  | grep '"SARndbox"' | grep -q "+1280+0"; }

TMP=$(mktemp -d)
cp -p "$D/estado.json" "$TMP/estado.json" 2>/dev/null || true
trap 'cp -p "$TMP/estado.json" "$D/estado.json" 2>/dev/null; rm -rf "$TMP"' EXIT

echo "== instalação"
confere "compila do zero a partir do repositório" "bash '$R/instalar.sh' --compilar '$TMP/build' >/dev/null 2>&1"
confere "binário instalado é o mesmo que o repositório gera" "cmp -s <(strings '$TMP/build/bin/SARndbox' | grep -c faceOut) <(strings '$D/bin/SARndbox' | grep -c faceOut)"
confere "comandos no PATH" "command -v areia-foto >/dev/null && command -v areia-topografia >/dev/null"
confere "ao ligar abre a topografia" "grep -q '^Exec=.*areia-topografia' '$HOME/.config/autostart/SARndbox.desktop'"
confere "modelos presentes" "[ -s '$D/modelos/u2net_human_seg.onnx' ] && [ -s '$D/modelos/face_detection_yunet_2023mar.onnx' ]"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop")
confere "Área de Trabalho: pasta Areia, ajuda e atalho" "[ -f '$MESA/Areia/COMO USAR.txt' ] && [ -f '$MESA/SE A AREIA NÃO ABRIR.txt' ] && [ -x '$MESA/Abrir a areia.desktop' ]"

echo "== painel do celular"
confere "serviço ativo" "systemctl --user is-active -q sandbox-painel"
confere "responde na própria máquina" "curl -sf -m 5 http://127.0.0.1:8090/estado | grep -q '\"sandbox\"'"
LAN=$(ip -4 -o addr show scope global | awk '!/tailscale/ {sub(/\/.*/,"",$4); print $4; exit}')
confere "recusa a rede local ($LAN)" "[ \"\$(curl -s -o /dev/null -w '%{http_code}' -m 5 http://$LAN:8090/estado)\" = 403 ]"
TS=$(tailscale ip -4 2>/dev/null | head -1)
[ -n "$TS" ] && confere "responde no Tailscale ($TS)" "curl -sf -m 5 http://$TS:8090/estado >/dev/null"

echo "== projetor"
confere "topografia abre" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = original ]"
confere "topografia no projetor, tela cheia" "no_projetor"
TEMA0=$(readlink "$D/temas/atual.cpt" 2>/dev/null | sed 's/\.cpt$//'); TEMA0=${TEMA0:-classico}
confere "topografia aceita troca de cores (cano de comandos)" "pgrep -af '^$HOME/SARndbox-2.8/bin/SARndbox' | grep -q -- ' -cp '"
confere "troca de tema na hora (< 2 s)" "s=\$(date +%s); areia-tema vulcao >/dev/null && areia-tema '$TEMA0' >/dev/null && [ \$((\$(date +%s) - s)) -lt 2 ]"
N=$(areia-foto lista | grep -cE '^ +[0-9]+  ')
if [ "$N" -gt 0 ]; then
  confere "modo foto abre com a imagem 1" "areia-foto 1 >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ]"
  confere "SARndbox recebeu a imagem" "grep -q 'Modo Rosto:' '$D/sandbox.log'"
  confere "foto no projetor, tela cheia" "no_projetor"
  DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority ffmpeg -loglevel error -y -f x11grab \
    -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 /tmp/integracao-foto.png 2>/dev/null \
    && ok "print do projetor em /tmp/integracao-foto.png"
  confere "girar 4 vezes volta ao começo" "for i in 1 2 3 4; do areia-foto girar >/dev/null || exit 1; done; grep -q '\"giro\": 0' '$D/estado.json'"
  confere "esculpir" "areia-foto esculpir >/dev/null && sleep 1 && grep -q 'Modo Esculpir:' '$D/sandbox.log'"
  confere "segunda vez sai do cache (< 15 s)" "s=\$(date +%s); areia-foto 1 >/dev/null; [ \$((\$(date +%s) - s)) -lt 15 ]"
else
  echo "(pasta Areia vazia: pulei o modo foto)"
fi
confere "volta à topografia" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = original ] && no_projetor"

[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
