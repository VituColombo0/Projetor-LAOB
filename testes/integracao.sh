#!/bin/bash
# Teste no PC de verdade (laob), depois do instalar.sh. Usa o projetor por ~1 minuto e termina
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
# Só o que o SARndbox escreveu desde a marca: o sandbox.log só zera quando a areia abre do zero, e as linhas
# antigas (de horas antes) faziam os testes passarem mesmo com o comando de agora falhando.
LOG=$D/sandbox.log
marca() { L0=$(stat -c%s "$LOG" 2>/dev/null || echo 0); }
novo() { tail -c +$((L0+1)) "$LOG"; }
tem() { grep -q "$@" <(novo); }   # sem pipe: com pipefail, o grep -q saindo cedo mataria o tail (SIGPIPE) e daria falha
L0=0
no_projetor() { DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority xwininfo -root -tree 2>/dev/null \
                  | grep '"SARndbox"' | grep -q "+1280+0"; }

TMP=$(mktemp -d)
cp -p "$D/estado.json" "$TMP/estado.json" 2>/dev/null || true
trap 'cp -p "$TMP/estado.json" "$D/estado.json" 2>/dev/null; rm -rf "$TMP"' EXIT

echo "== instalação"
confere "compila do zero a partir do repositório" "bash '$R/instalar.sh' --compilar '$TMP/build' >/dev/null 2>&1"
# textos de recursos recentes nos dois binários (antes só a contagem de "faceOut", igual em toda versão desde
# o modo foto: um binário velho passava)
tem_texto() { grep -qF "$1" <(strings "$2"); }
confere "binário instalado é o mesmo que o repositório gera" "(for t in 'Comida em x' 'Copia: forma' 'Chuva: nuvem por' 'Mundo: hora' 'extrasComida'; do tem_texto \"\$t\" '$TMP/build/bin/SARndbox' && tem_texto \"\$t\" '$D/bin/SARndbox' || exit 1; done) && ! tem_texto 'extrasMaos' '$D/bin/SARndbox' && ! tem_texto 'JogoInfo: correnteza' '$D/bin/SARndbox' && cmp -s <(strings '$TMP/build/bin/SARndbox' | grep -c faceOut) <(strings '$D/bin/SARndbox' | grep -c faceOut)"
confere "comandos no PATH" "(for c in areia-foto areia-topografia areia-tema areia-tesouro areia-animais areia-fossil areia-labirinto areia-jogo areia-vigia; do command -v \$c >/dev/null || exit 1; done)"
confere "ao ligar abre a topografia" "grep -q '^Exec=.*areia-topografia' '$HOME/.config/autostart/SARndbox.desktop'"
confere "modelos presentes" "[ -s '$D/modelos/u2net_human_seg.onnx' ] && [ -s '$D/modelos/face_detection_yunet_2023mar.onnx' ]"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop")
confere "Área de Trabalho sem as pastas antigas (tudo pelo painel)" "! ls -d \"$MESA\"/[1-8]\\ -\\ * >/dev/null 2>&1"
confere "atalho do painel na Área de Trabalho" "[ -x \"$MESA/Abrir painel da caixa de areia.desktop\" ]"
confere "fotos em Imagens/Fotos na areia" "[ -d \"\$(xdg-user-dir PICTURES)/Fotos na areia\" ]"
confere "sem sobras da organização antiga" "[ ! -e '$MESA/Areia' ] && [ ! -e '$MESA/SE A AREIA NÃO ABRIR.txt' ]"

echo "== painel do celular"
confere "serviço ativo" "systemctl --user is-active -q sandbox-painel"
confere "vigia da areia agendado" "systemctl --user is-active -q areia-vigia.timer && [ \"\$(areia-vigia estado)\" = vigiando ]"
confere "responde na própria máquina" "curl -sf -m 5 http://127.0.0.1:8090/estado | grep -q '\"sandbox\"'"
LAN=$(ip -4 -o addr show scope global | awk '!/tailscale/ {sub(/\/.*/,"",$4); print $4; exit}')
confere "responde na rede local do laboratório ($LAN)" "curl -sf -m 5 http://$LAN:8090/estado >/dev/null"
TS=$(tailscale ip -4 2>/dev/null | head -1)
[ -n "$TS" ] && confere "responde no Tailscale ($TS)" "curl -sf -m 5 http://$TS:8090/estado >/dev/null"

echo "== projetor"
ANTES=$("$D/sandbox.sh" estado)
marca
confere "topografia abre (na versão com efeitos)" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ]"
[ "$ANTES" = rosto ] || L0=0   # a areia abriu agora: o registro recomeçou do zero
confere "topografia no projetor, tela cheia" "no_projetor"
TEMA0=$(readlink "$D/temas/atual.cpt" 2>/dev/null | sed 's/\.cpt$//'); TEMA0=${TEMA0:-classico}
confere "topografia recebe o efeito do tema" "tem '^Efeito: '"
confere "troca de tema na hora (< 2 s)" "s=\$(date +%s); areia-tema vulcao >/dev/null && areia-tema '$TEMA0' >/dev/null && [ \$((\$(date +%s) - s)) -lt 2 ]"
N=$(areia-foto lista | grep -cE '^ +[0-9]+  ')
if [ "$N" -gt 0 ]; then
  confere "modo foto abre com a imagem 1" "areia-foto 1 >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ]"
  confere "SARndbox recebeu a imagem" "tem 'Modo Rosto:'"
  confere "foto no projetor, tela cheia" "no_projetor"
  DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority ffmpeg -loglevel error -y -f x11grab \
    -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 /tmp/integracao-foto.png 2>/dev/null \
    && ok "print do projetor em /tmp/integracao-foto.png"
  GIRO0=$(grep -o '"giro": [0-9]*' "$D/estado.json")
  confere "girar 4 vezes volta ao começo ($GIRO0)" "for i in 1 2 3 4; do areia-foto girar >/dev/null || exit 1; done; grep -q '$GIRO0' '$D/estado.json'"
  confere "esculpir" "areia-foto esculpir >/dev/null && sleep 1 && tem 'Modo Esculpir:'"
  confere "segunda vez sai do cache (< 15 s)" "s=\$(date +%s); areia-foto 1 >/dev/null; [ \$((\$(date +%s) - s)) -lt 15 ]"
else
  echo "(pasta Areia vazia: pulei o modo foto)"
fi
confere "caça ao tesouro esconde o baú" "areia-tesouro facil >/dev/null && sleep 2 && tem 'Tesouro escondido'"
confere "tesouro: situação" "areia-tesouro achou | grep -qiE 'escondido|acharam'"
confere "escavação de fóssil" "areia-fossil triceratops facil >/dev/null && sleep 2 && tem 'Fossil escondido' && areia-fossil achou | grep -qiE 'enterrado|escavado'"
confere "peixes e carrinhos" "areia-animais todos 12 >/dev/null && tem 'Animais: todos (12)'"
confere "baú: animação de achado (tesouroTeste)" "areia-tesouro facil >/dev/null && sleep 2 && '$D/sandbox.sh' cmd tesouroTeste >/dev/null && sleep 1 && tem '^Tesouro: teste, mostrando' && areia-tesouro parar >/dev/null"
confere "mundo LAOB: laboratório montado" "areia-animais mundo >/dev/null && for _ in \$(seq 1 30); do tem '^Mundo: laboratório montado' && break; sleep 0.5; done; tem '^Animais: mundo' && tem '^Mundo: laboratório montado' && curl -s -m 5 localhost:8090/estado | grep -q '\"animais\":\"mundo\"'"
confere "mundo LAOB: hora de teste (mundoHora)" "'$D/sandbox.sh' cmd 'mundoHora 22' >/dev/null && sleep 1 && tem '^Mundo: hora 22' && '$D/sandbox.sh' cmd 'mundoHora auto' >/dev/null"
confere "animais marinhos à escolha" "areia-animais mar peixe=6 tubarao=1 tartaruga=1 aguaviva=1 polvo=1 arraia=1 carro=2 pessoa=3 >/dev/null && tem 'Animais: mar (16)'"
confere "comida sem pássaros nem cachorros: o painel avisa" "curl -s -m 5 -X POST -H 'Content-Type: application/json' -d '{\"x\":0.5,\"y\":0.5}' localhost:8090/comida | python3 -c 'import json,sys; r=json.load(sys.stdin); sys.exit(0 if r.get(\"ok\") is False and \"nem cachorros\" in r.get(\"msg\",\"\") else 1)'"
confere "comida pelo painel: pássaros e cachorros vêm" "areia-animais mar peixe=2 passaro=3 cachorro=2 >/dev/null && tem 'Animais: mar (7)' && curl -s -m 5 -X POST -H 'Content-Type: application/json' -d '{\"x\":0.5,\"y\":0.5}' localhost:8090/comida | grep -q '\"ok\":true' && sleep 1 && tem '^Comida em x'"
confere "chuva pelo painel e secar" "curl -s -m 5 -X POST -H 'Content-Type: application/json' -d '{\"x\":0.5,\"y\":0.5}' localhost:8090/chuva | grep -q '\"ok\":true' && sleep 1 && tem '^Chuva: nuvem' && curl -s -m 5 -X POST -H 'Content-Type: application/json' -d '{\"secar\":true}' localhost:8090/chuva | grep -q '\"ok\":true' && sleep 1 && tem '^Chuva: a água foi seca'"
confere "copie o relevo: alvo sorteado e nota medida" "areia-jogo copia montanha facil >/dev/null && for _ in \$(seq 1 30); do tem '^Copia: [0-9]*%' && break; sleep 0.5; done; tem '^Copia: forma montanha' && curl -s -m 5 localhost:8090/estado | grep -q '\"forma\":\"montanha\"' && areia-jogo copia parar >/dev/null"
confere "pessoas andando" "areia-animais pessoas 8 >/dev/null && tem 'Animais: pessoas (8)'"
confere "labirinto de água liga" "areia-labirinto facil >/dev/null && sleep 1 && tem '^Labirinto: nascente' && areia-labirinto placar | grep -qiE 'correndo|chegou'"
confere "labirinto: a água chega ao alvo e é medida" "'$D/sandbox.sh' cmd labirintoTeste >/dev/null && for _ in \$(seq 1 40); do tem '^Labirinto: a água chegou' && break; sleep 0.5; done; tem '^Labirinto: a água chegou' && areia-labirinto placar | grep -q 'chegou'"
confere "labirinto: rodada nova" "areia-labirinto nova >/dev/null && sleep 1 && tail -n 20 '$D/sandbox.log' | grep -q '^Labirinto: rodada nova'"
sleep 3
DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority ffmpeg -loglevel error -y -f x11grab \
  -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 /tmp/integracao-brincadeiras.png 2>/dev/null \
  && ok "print das brincadeiras em /tmp/integracao-brincadeiras.png"
confere "sem erro de shader" "! grep -qiE 'shader.*(error|erro)|GLSL|compile error' '$D/sandbox.log'"
for j in corrida vila rota vulcao; do
  confere "jogo de água: $j liga" "areia-jogo $j >/dev/null && sleep 1.5 && tail -n 30 '$D/sandbox.log' | grep -q '^Jogo pedido: $j' && areia-jogo placar | grep -q 'rodada'"
done
confere "vulcão: não troca o tema e mostra o placar de casas" "antes=\$(readlink '$D/temas/atual.cpt'); areia-jogo vulcao >/dev/null && sleep 1.5 && [ \"\$(readlink '$D/temas/atual.cpt')\" = \"\$antes\" ] && tail -n 5 '$D/sandbox.log' | grep -q 'casas)' && curl -s -m 5 localhost:8090/estado | grep -q '\"casas\":[1-9]'"
confere "corrida: a água chega à bandeira e há vencedor" "areia-jogo corrida >/dev/null && sleep 2 && '$D/sandbox.sh' cmd jogoTeste >/dev/null && for _ in \$(seq 1 40); do tail -n 5 '$D/sandbox.log' | grep -q 'fim da rodada; venceu' && break; sleep 0.5; done; tail -n 5 '$D/sandbox.log' | grep -q 'fim da rodada; venceu' && areia-jogo placar | grep -q 'terminou'"
confere "jogos retirados não ligam" "! areia-jogo barco >/dev/null 2>&1 && ! areia-jogo fazenda >/dev/null 2>&1"
confere "jogo: parar" "areia-jogo parar >/dev/null && areia-jogo placar | grep -q 'Nenhum jogo'"
confere "parar brincadeiras" "areia-animais parar >/dev/null && areia-tesouro parar >/dev/null && areia-labirinto parar >/dev/null && areia-labirinto placar | grep -q 'não está ligado'"
confere "desempenho: mede quanto a água acompanha o tempo real" "tem 'a água andou [0-9]*% do tempo real' || { sleep 31; tem 'a água andou [0-9]*% do tempo real'; }"
confere "desempenho medido no log" "tem '^Desempenho: ' || { sleep 31; tem '^Desempenho: '; }"
confere "log sem a enxurrada de 'Ran out of time'" "! grep -q 'Ran out of time' '$D/sandbox.log'"
for t in vulcao inverno marte deserto oceano arco-iris classico; do
  confere "efeito do tema $t" "areia-tema $t >/dev/null && sleep 0.5 && [ \"\$(grep '^Efeito: ' '$D/sandbox.log' | tail -n 1)\" = 'Efeito: $t' ]"
done
confere "sem erro de shader com os efeitos" "! grep -qiE 'shader.*(error|erro)|GLSL|compile error' '$D/sandbox.log'"
confere "vigia não mexe com a areia aberta" "timeout 5 areia-vigia && [ \"\$('$D/sandbox.sh' estado)\" = rosto ]"
confere "reserva: o SARndbox original abre" "'$D/sandbox.sh' original >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = original ] && no_projetor"
confere "volta à topografia" "areia-topografia >/dev/null && [ \"\$('$D/sandbox.sh' estado)\" = rosto ] && no_projetor"
areia-tema "$TEMA0" >/dev/null

[ $falhas = 0 ] && echo "TUDO OK" || { echo "HÁ FALHAS"; exit 1; }
