#!/bin/bash
# Mede a areia no laob em cada modo (para o antes/depois da revisão): qps, tempo de quadro p50/p95/p99/máx,
# água em % do tempo real, CPU e GPU por passada (linha "Perfil:" do sandbox.log), CPU e memória dos
# processos, uso/temperatura/clock da GPU, e o tempo de cada troca com os quadros lentos logo depois.
# Usa o projetor por ~1 min por modo e termina na topografia, tema clássico, sem brincadeira.
#   bash testes/medir.sh [pasta-de-saída] [modo ...]     (padrão: ~/medidas/<data-hora>, todos os modos)
# Modos: topo-<tema> tesouro fossil labirinto corrida vila rota vulcao copia bichos mundo foto esculpir
set -uo pipefail
export LC_ALL=C
D=$HOME/SARndbox-rosto
export PATH=$HOME/.local/bin:$PATH DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority
SAIDA=${1:-$HOME/medidas/$(date +%Y%m%d-%H%M)}; shift || true
mkdir -p "$SAIDA"
LOG=$D/sandbox.log
TEMAS=(classico vulcao inverno marte deserto oceano arco-iris)
MODOS=("$@")
[ ${#MODOS[@]} -gt 0 ] || MODOS=(topo-classico topo-vulcao topo-inverno topo-marte topo-deserto topo-oceano topo-arco-iris
                                  tesouro fossil labirinto corrida vila rota vulcao copia bichos mundo foto esculpir)
TSV=$SAIDA/medidas.tsv
[ -s "$TSV" ] || printf 'modo\ttroca_s\tqps\tagua_pct\tp50\tp95\tp99\tmax\tacima50\tgpu_agua\tgpu_relevo\tcpu_leituras_max\tlentos_na_troca\tcpu_sandbox_pct\tcpu_painel_pct\tcpu_som_pct\tcpu_total_pct\trss_sandbox_mb\tgpu_util\tgpu_temp\tgpu_mhz\n' > "$TSV"
areia-vigia pausa >/dev/null
trap 'areia-vigia liga >/dev/null' EXIT

pid_de() { pgrep -f "$1" | head -1; }
ticks() { [ -n "$1" ] && [ -r "/proc/$1/stat" ] && awk '{print $14+$15}' "/proc/$1/stat" || echo 0; }
total() { awk '/^cpu /{print $2+$3+$4+$5+$6+$7+$8, $5+$6}' /proc/stat; }
perfis() { grep -c '^Perfil: quadro p50' "$LOG" 2>/dev/null || echo 0; }

liga() {  # inicia o modo; o tema clássico fica para as brincadeiras
  case "$1" in
    topo-*) areia-topografia >/dev/null; areia-tema "${1#topo-}" >/dev/null ;;
    tesouro) areia-tesouro normal >/dev/null ;;
    fossil) areia-fossil trex facil >/dev/null ;;
    labirinto) areia-labirinto normal >/dev/null ;;
    corrida|vila|rota|vulcao) areia-jogo "$1" normal >/dev/null ;;
    copia) areia-jogo copia sorteio normal >/dev/null ;;
    bichos) areia-animais mar peixe=8 tubarao=1 tartaruga=2 pato=4 barco=2 carro=4 pessoa=4 cachorro=3 passaro=4 >/dev/null ;;
    mundo) areia-animais mundo >/dev/null ;;
    foto) areia-foto 1 >/dev/null ;;
    esculpir) areia-foto 1 >/dev/null; areia-foto esculpir >/dev/null ;;
    *) echo "modo desconhecido: $1"; return 1 ;;
  esac
}

for M in "${MODOS[@]}"; do
  case "$M" in topo-*) ;; *) [ "$(readlink "$D/temas/atual.cpt")" = classico.cpt ] || areia-tema classico >/dev/null ;; esac
  L0=$(stat -c%s "$LOG")
  t0=$(date +%s.%N); liga "$M" || continue; t1=$(date +%s.%N)
  troca=$(echo "$t1 - $t0" | bc)
  sleep 5
  [ "$(stat -c%s "$LOG")" -lt "$L0" ] && L0=0   # a areia reabriu (log novo)
  lentos=$(tail -c +$((L0+1)) "$LOG" | grep -o 'quadro lento de [0-9]* ms' | grep -o '[0-9]*' | paste -sd, -)
  # uma janela inteira de 30 s dentro do modo: espera a próxima linha Perfil e mede até a seguinte
  n=$(perfis); for _ in $(seq 1 80); do [ "$(perfis)" -gt "$n" ] && break; sleep 0.5; done
  SB=$(pid_de "^$D/bin/SARndbox"); PN=$(pid_de "painel.py"); SO=$(pid_de "som.py motor")
  a1=$(ticks "$SB"); b1=$(ticks "$PN"); c1=$(ticks "$SO"); read -r T1 I1 < <(total)
  gu=(); n=$(perfis)
  for _ in $(seq 1 80); do
    [ "$(perfis)" -gt "$n" ] && break
    gu+=("$(nvidia-smi --query-gpu=utilization.gpu,temperature.gpu,clocks.gr --format=csv,noheader,nounits)")
    sleep 1
  done
  a2=$(ticks "$SB"); b2=$(ticks "$PN"); c2=$(ticks "$SO"); read -r T2 I2 < <(total)
  HZ=$(getconf CLK_TCK); NC=$(nproc)
  dt=$(echo "($T2 - $T1) / $NC" | bc -l)   # em ticks de um núcleo
  pct() { printf '%.1f' "$(echo "100 * ($2 - $1) / $dt" | bc -l)"; }
  # CPU da máquina toda (os 4 núcleos) = 1 - ocioso/total na janela; conferido em 23/11/2026 contra a soma
  # dos processos. Arredonda com printf (o "scale=1" do bc truncava: 8,9% saía 9,0 e 17,96% saía 17,9).
  ctot=$(printf '%.1f' "$(echo "100 - 100 * ($I2 - $I1) / ($T2 - $T1)" | bc -l)")
  rss=$(awk '/VmRSS/{printf "%.0f", $2/1024}' "/proc/$SB/status" 2>/dev/null)
  des=$(grep '^Desempenho' "$LOG" | tail -1); per=$(grep '^Perfil: quadro p50' "$LOG" | tail -1)
  qps=$(echo "$des" | grep -o '^Desempenho: [0-9]*' | grep -o '[0-9]*$')
  agua=$(echo "$des" | grep -o 'andou [0-9]*%' | grep -o '[0-9]*')
  read -r p50 p95 p99 mx ac < <(echo "$per" | sed -E 's/.*p50 ([0-9.]+) p95 ([0-9.]+) p99 ([0-9.]+) max ([0-9.]+) ms, ([0-9]+) acima.*/\1 \2 \3 \4 \5/')
  ga=$(echo "$per" | sed -nE 's/.*GPU agua ([0-9.]+).*/\1/p'); gr=$(echo "$per" | sed -nE 's/.*relevo ([0-9.]+) \(max [0-9.]+\) ms$/\1/p')
  lm=$(echo "$per" | sed -nE 's/.*leituras [0-9.]+ \(max ([0-9.]+)\).*/\1/p')
  gstat=$(printf '%s\n' "${gu[@]}" | awk -F', ' '{u+=$1; if($2>t)t=$2; c+=$3; n++} END {if(n) printf "%.0f\t%d\t%.0f", u/n, t, c/n}')
  printf '%s\t%.2f\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$M" "$troca" "$qps" "$agua" \
    "$p50" "$p95" "$p99" "$mx" "$ac" "$ga" "$gr" "$lm" "${lentos:--}" \
    "$(pct "$a1" "$a2")" "$(pct "$b1" "$b2")" "$(pct "$c1" "$c2")" "$ctot" "$rss" "$gstat" >> "$TSV"
  echo "$per" >> "$SAIDA/perfil-bruto.txt"; echo "$M $des" >> "$SAIDA/perfil-bruto.txt"
  tail -1 "$TSV"
  [ "$HZ" = 100 ] || echo "  (CLK_TCK=$HZ)"
done
areia-topografia >/dev/null; areia-tema classico >/dev/null
echo "pronto: $TSV"
