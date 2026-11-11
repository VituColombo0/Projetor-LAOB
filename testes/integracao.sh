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
confere "comandos no PATH" "for c in areia-foto areia-topografia areia-tema areia-tesouro areia-animais areia-fossil areia-desenho; do command -v \$c >/dev/null || exit 1; done"
confere "ao ligar abre a topografia" "grep -q '^Exec=.*areia-topografia' '$HOME/.config/autostart/SARndbox.desktop'"
confere "modelos presentes" "[ -s '$D/modelos/u2net_human_seg.onnx' ] && [ -s '$D/modelos/face_detection_yunet_2023mar.onnx' ]"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop")
for pasta in "1 - Topografia (ligar e consertar)" "2 - Fotos na areia" "3 - Cores do relevo" "4 - Caça ao tesouro" "5 - Peixes e carrinhos"; do
  confere "Área de Trabalho: '$pasta' com o manual" "[ -s \"$MESA/$pasta/COMO USAR.txt\" ]"
done
confere "pasta '7 - Fósseis de dinossauro' com o manual" "[ -s \"$MESA/7 - Fósseis de dinossauro/COMO USAR.txt\" ]"
confere "pasta '8 - Desenho na areia' com o manual" "[ -s \"$MESA/8 - Desenho na areia/COMO USAR.txt\" ]"
confere "atalhos de dois cliques: executáveis e liberados" "n=0; for a in \"$MESA\"/[1-7]\\ -\\ */*.desktop; do [ -x \"\$a\" ] && gio info -a metadata::trusted \"\$a\" | grep -q 'trusted: true' || exit 1; n=\$((n+1)); done; [ \$n -ge 20 ]"
confere "atalhos apontam para programas que existem" "for a in \"$MESA\"/[1-7]\\ -\\ */*.desktop; do e=\$(sed -n 's/^Exec=//p' \"\$a\" | cut -d' ' -f1); [ \"\${e#xdg-open}\" != \"\$e\" ] || [ -x \"\$e\" ] || exit 1; done"
confere "sem sobras da organização antiga" "[ ! -e '$MESA/Areia' ] && [ ! -e '$MESA/SE A AREIA NÃO ABRIR.txt' ]"

echo "== painel do celular"
confere "serviço ativo" "systemctl --user is-active -q sandbox-painel"
confere "responde na própria máquina" "curl -sf -m 5 http://127.0.0.1:8090/estado | grep -q '\"sandbox\"'"
LAN=$(ip -4 -o addr show scope global | awk '!/tailscale/ {sub(/\/.*/,"",$4); print $4; exit}')
confere "recusa a rede local ($LAN)" "[ \"\$(curl -s -o /dev/null -w '%{http_code}' -m 5 http://$LAN:8090/estado)\" = 403 ]"
TS=$(tailscale ip -4 2>/dev/null | head -1)
[ -n "$TS" ] && confere "responde no Tailscale ($TS)" "curl -sf -m 5 http://$TS:8090/estado >/dev/null"

echo "== projetor"
confere "topografia abre (na versão com efeitos)" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ]"
confere "topografia no projetor, tela cheia" "no_projetor"
TEMA0=$(readlink "$D/temas/atual.cpt" 2>/dev/null | sed 's/\.cpt$//'); TEMA0=${TEMA0:-classico}
confere "topografia recebe o efeito do tema" "grep -q '^Efeito: ' '$D/sandbox.log'"
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
confere "caça ao tesouro esconde o baú" "areia-tesouro facil >/dev/null && sleep 2 && grep -q 'Tesouro escondido' '$D/sandbox.log'"
confere "tesouro: situação" "areia-tesouro achou | grep -qiE 'escondido|acharam'"
confere "escavação de fóssil" "areia-fossil triceratops facil >/dev/null && sleep 2 && grep -q 'Fossil escondido' '$D/sandbox.log' && areia-fossil achou | grep -qiE 'enterrado|escavado'"
confere "desenho: folha branca liga" "areia-desenho >/dev/null && sleep 2 && grep -q '^Desenho: ligado' '$D/sandbox.log'"
DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority ffmpeg -loglevel error -y -f x11grab \
  -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 -vf scale=64:48 -f rawvideo -pix_fmt gray /tmp/integracao-folha.raw 2>/dev/null
confere "desenho: projetor mostra a folha clara" "[ \"\$(od -An -tu1 -v /tmp/integracao-folha.raw | tr -s ' ' '\\n' | grep -v '^$' | awk '{s+=\$1} END {print int(s/NR)}')\" -gt 200 ]"
confere "desenho: cor, traço e limpar" "areia-desenho azul >/dev/null && areia-desenho grosso >/dev/null && areia-desenho limpar >/dev/null && areia-desenho parar >/dev/null"
confere "atalho com janela (gui-rodar) roda o comando" "DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/\$(id -u)/bus '$D/gui-rodar.sh' Teste dialog-information areia-fossil parar >/dev/null"
confere "peixes e carrinhos" "areia-animais todos 12 >/dev/null && grep -q 'Animais: todos (12)' '$D/sandbox.log'"
sleep 3
DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority ffmpeg -loglevel error -y -f x11grab \
  -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 /tmp/integracao-brincadeiras.png 2>/dev/null \
  && ok "print das brincadeiras em /tmp/integracao-brincadeiras.png"
confere "sem erro de shader" "! grep -qiE 'shader.*(error|erro)|GLSL|compile error' '$D/sandbox.log'"
confere "parar brincadeiras" "areia-animais parar >/dev/null && areia-tesouro parar >/dev/null"
for t in vulcao inverno marte deserto oceano arco-iris mapa classico; do
  confere "efeito do tema $t" "areia-tema $t >/dev/null && sleep 0.5 && [ \"\$(grep '^Efeito: ' '$D/sandbox.log' | tail -n 1)\" = 'Efeito: $t' ]"
done
confere "sem erro de shader com os efeitos" "! grep -qiE 'shader.*(error|erro)|GLSL|compile error' '$D/sandbox.log'"
confere "reserva: o SARndbox original abre" "'$D/sandbox.sh' original >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = original ] && no_projetor"
confere "volta à topografia" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ] && no_projetor"
areia-tema "$TEMA0" >/dev/null

[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
