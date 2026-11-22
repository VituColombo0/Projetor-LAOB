#!/bin/bash
# Prints do projetor de cada tema × cada brincadeira (para a prancha de antes/depois da revisão).
# Usa o projetor por ~5 min e termina na topografia, tema clássico, sem brincadeira.
#   bash testes/prancha.sh [pasta-de-saída]        (padrão: ~/prancha/<data-hora>)
# Gera <brincadeira>__<tema>.png (1024×768). A grade é montada fora (testes/monta_prancha.py).
set -uo pipefail
export LC_ALL=C
D=$HOME/SARndbox-rosto
export PATH=$HOME/.local/bin:$PATH DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority
SAIDA=${1:-$HOME/prancha/$(date +%Y%m%d-%H%M)}
mkdir -p "$SAIDA"
TEMAS=(classico vulcao inverno marte deserto oceano arco-iris)
BRINC=(topografia tesouro fossil labirinto corrida vila rota vulcao copia bichos)
areia-vigia pausa >/dev/null
trap 'areia-vigia liga >/dev/null' EXIT
print() { ffmpeg -loglevel error -y -f x11grab -video_size 1024x768 -i :0.0+1280,0 -frames:v 1 "$SAIDA/$1.png"; }
cmd() { "$D/sandbox.sh" cmd "$@" >/dev/null; }

for B in "${BRINC[@]}"; do
  areia-tema classico >/dev/null
  case "$B" in
    topografia) areia-topografia >/dev/null ;;
    tesouro) areia-tesouro normal >/dev/null; cmd tesouroTeste ;;
    fossil) areia-fossil trex facil >/dev/null; cmd tesouroTeste ;;
    labirinto) areia-labirinto normal >/dev/null ;;
    corrida|vila|rota|vulcao) areia-jogo "$B" normal >/dev/null ;;
    copia) areia-jogo copia sorteio normal >/dev/null ;;
    bichos) areia-animais mar peixe=8 tubarao=1 tartaruga=2 pato=4 barco=2 carro=4 pessoa=4 cachorro=3 passaro=4 >/dev/null ;;
  esac
  sleep 6
  for T in "${TEMAS[@]}"; do
    areia-tema "$T" >/dev/null
    sleep 2
    print "${B}__${T}"
  done
  echo "$B ok"
done
areia-tema classico >/dev/null
areia-animais mundo >/dev/null; sleep 6; print "mundo__classico"
areia-foto 1 >/dev/null; sleep 4; print "foto__classico"
areia-foto esculpir >/dev/null; sleep 4; print "esculpir__classico"
areia-topografia >/dev/null; areia-tema classico >/dev/null
ls "$SAIDA"/*.png | wc -l
echo "pronto: $SAIDA"
