#!/bin/bash
# Instala o Projetor-LAOB num PC que já roda o AR Sandbox (SARndbox 2.8) calibrado, como o laob.
#   bash instalar.sh              instala/atualiza tudo em ~/SARndbox-rosto (sem sudo)
#   bash instalar.sh --compilar DIR   só monta e compila a versão com modo Foto em DIR (teste)
# Variáveis: SARNDBOX_ORIG (padrão ~/SARndbox-2.8) e SARNDBOX_CALIB (pasta com BoxLayout.txt e
# ProjectorMatrix.dat; padrão /root/etc/SARndbox-2.8, a que o SARndbox do laob usa).
# O SARndbox original nunca é alterado. Pode rodar de novo para atualizar.
set -euo pipefail
REPO=$(cd "$(dirname "$0")" && pwd)
ORIG=${SARNDBOX_ORIG:-$HOME/SARndbox-2.8}
CALIB=${SARNDBOX_CALIB:-/root/etc/SARndbox-2.8}
DEST=$HOME/SARndbox-rosto
SO_COMPILAR=0
if [ "${1:-}" = "--compilar" ]; then SO_COMPILAR=1; DEST=${2:?uso: instalar.sh --compilar DIR}; fi

[ -x "$ORIG/bin/SARndbox" ] || { echo "não achei o SARndbox original em $ORIG"; exit 1; }
echo "[1/8] cópia do SARndbox em $DEST (o original em $ORIG fica intacto)"
if [ ! -d "$DEST" ]; then cp -a "$ORIG" "$DEST"; fi
# fontes sempre limpas do original antes dos patches
for f in SurfaceRenderer.h SurfaceRenderer.cpp Sandbox.h Sandbox.cpp WaterTable2.h WaterTable2.cpp makefile; do cp "$ORIG/$f" "$DEST/$f"; done
mkdir -p "$DEST/etc/SARndbox-2.8"
for f in BoxLayout.txt ProjectorMatrix.dat HeightColorMap.cpt SARndbox.cfg; do
  if [ -r "$CALIB/$f" ]; then cp "$CALIB/$f" "$DEST/etc/SARndbox-2.8/"
  elif [ ! -f "$DEST/etc/SARndbox-2.8/$f" ]; then echo "  aviso: sem $CALIB/$f (calibre o SARndbox ou defina SARNDBOX_CALIB)"; fi
done

echo "[2/8] modo Foto: patches e compilação"
PROGRAMA_ANTES=$(md5sum "$DEST/bin/SARndbox" 2>/dev/null | cut -d" " -f1 || true)
cp "$REPO"/sarndbox/{FaceImage.h,FaceImage.cpp,Extras.h,Extras.cpp,Jogos.cpp,Mundo.cpp,patch_surface.py,patch_sandbox.py,patch_agua.py,projetor.cfg} "$DEST/"
for S in share/SARndbox-2.8/Shaders/SurfaceAddWaterColor.fs share/SARndbox-2.8/Shaders/Water2WaterUpdateShader.fs; do
  mkdir -p "$DEST/$(dirname "$S")" && cp "$ORIG/$S" "$DEST/$S"   # sempre do original, antes do patch
done
( cd "$DEST" && python3 patch_surface.py >/dev/null && python3 patch_sandbox.py >/dev/null && python3 patch_agua.py >/dev/null )
if ! make -C "$DEST" INSTALLDIR="$DEST" > "$DEST/compilacao.log" 2>&1 \
   || ! [ "$DEST/bin/SARndbox" -nt "$DEST/FaceImage.cpp" ]; then
  echo "a compilação falhou; veja $DEST/compilacao.log"; exit 1
fi
[ $SO_COMPILAR = 1 ] && { echo "compilado em $DEST/bin/SARndbox"; exit 0; }

echo "[3/8] programas da pasta areia/"
cp "$REPO"/areia/{areia.py,areia_foto.py,areia_tema.py,areia_brincar.py,rosto.py,membros.py,painel.py,som.py,sandbox.sh,atualizar_papel_parede.py} "$DEST/"
mkdir -p "$DEST/sons" && cp "$REPO"/areia/sons/*.wav "$DEST/sons/"
# Som combinado (HDMI do projetor + caixas de som) e desmuta saída analógica
if ! pactl list short sinks 2>/dev/null | grep -q '\bcombinado\b'; then
  pactl load-module module-combine-sink sink_name=combinado slaves=alsa_output.pci-0000_01_00.1.hdmi-stereo,alsa_output.pci-0000_00_1b.0.analog-stereo 2>/dev/null || true
fi
pactl set-sink-mute alsa_output.pci-0000_00_1b.0.analog-stereo 0 2>/dev/null || true
mkdir -p "$DEST/membros"   # fotos dos membros para as pessoinhas (Nome.jpg); ficam só nesta máquina
COMANDOS=(areia-foto areia-tema areia-topografia areia-tesouro areia-animais areia-fossil areia-labirinto areia-jogo areia-vigia)
for c in "${COMANDOS[@]}"; do cp "$REPO/areia/$c" "$DEST/"; chmod +x "$DEST/$c"; done
rm -f "$DEST"/gui-*.sh   # janelas dos atalhos da Área de Trabalho (substituídos pelo painel em 13/11)
rm -f "$DEST"/*.bak "$DEST/patch_brincar.py" "$DEST/gui-willbert-estado.sh"   # sobras de edições manuais
chmod +x "$DEST/sandbox.sh"
mkdir -p "$DEST/temas" && cp "$REPO"/areia/temas/*.cpt "$DEST/temas/"   # o tema escolhido (temas/atual.cpt) fica
# o tema "mapa de papel" saiu em 16/11/2026: se era o escolhido, volta o clássico
rm -f "$DEST/temas/mapa.cpt"
[ "$(readlink "$DEST/temas/atual.cpt" 2>/dev/null)" = mapa.cpt ] && ln -sfn classico.cpt "$DEST/temas/atual.cpt"
rm -f "$DEST/Sandbox.cpp.antes-area"  # sobra de versões antigas

echo "[4/8] Python próprio (venv) com OpenCV e Flask"
[ -x "$DEST/venv/bin/python" ] || python3 -m venv "$DEST/venv"
"$DEST/venv/bin/pip" install -q -r "$REPO/requirements.txt"

echo "[5/8] modelos (rosto e recorte do fundo)"
mkdir -p "$DEST/modelos"
baixa() { [ -s "$DEST/modelos/$1" ] || { wget -q -O "$DEST/modelos/$1.part" "$2" && mv "$DEST/modelos/$1.part" "$DEST/modelos/$1"; }; }
baixa face_detection_yunet_2023mar.onnx \
  https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
baixa u2net_human_seg.onnx https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net_human_seg.onnx

echo "[6/8] comandos: ${COMANDOS[*]}"
mkdir -p "$HOME/.local/bin" "$DEST/fotos"
for c in "${COMANDOS[@]}"; do ln -sf "$DEST/$c" "$HOME/.local/bin/$c"; done

echo "[7/8] tudo pelo painel: fotos em Imagens/Fotos na areia, Área de Trabalho só com o atalho do painel"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop"); [ "$MESA" = "$HOME" ] && MESA=$HOME/Desktop
IMGS=$(xdg-user-dir PICTURES 2>/dev/null || echo "$HOME/Pictures"); [ "$IMGS" = "$HOME" ] && IMGS=$HOME/Pictures
FOTOS_DIR="$IMGS/Fotos na areia"
mkdir -p "$FOTOS_DIR"
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
  sed "s#/home/laob#$HOME#g" "$at" > "$destino"; chmod +x "$destino"
  gio set "$destino" metadata::trusted true 2>/dev/null || true
done
B=$HOME/.config/gtk-3.0/bookmarks; mkdir -p "$(dirname "$B")"
grep -v "Fotos%20na%20areia" "$B" 2>/dev/null | grep -v "file://$MESA/Areia " > "$B.tmp"; mv "$B.tmp" "$B"
echo "file://${FOTOS_DIR// /%20} Fotos na areia" >> "$B"

echo "[8/8] topografia ao ligar, painel do celular e vigia"
A=$HOME/.config/autostart/SARndbox.desktop
mkdir -p "$(dirname "$A")"
if [ -f "$A" ]; then
  [ -f "$DEST/SARndbox.desktop.original" ] || cp "$A" "$DEST/SARndbox.desktop.original"
  sed -i "s#^Exec=.*#Exec=$DEST/areia-topografia#" "$A"
else
  printf '[Desktop Entry]\nType=Application\nName=SANDBOX\nExec=%s/areia-topografia\nX-GNOME-Autostart-enabled=true\n' "$DEST" > "$A"
fi
mkdir -p "$HOME/.config/systemd/user"
cp "$REPO/sistema/sandbox-painel.service" "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
systemctl --user enable --now sandbox-painel >/dev/null 2>&1 && systemctl --user restart sandbox-painel || true
# vigia: se a areia fechar sozinha, reabre a topografia (areia-vigia pausa / liga)
cp "$REPO/sistema/areia-vigia.service" "$REPO/sistema/areia-vigia.timer" "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
systemctl --user enable --now areia-vigia.timer >/dev/null 2>&1 || true
# papel de parede com o endereço do painel (muda sozinho se o IP mudar)
mkdir -p "$HOME/Pictures"
[ -s "$HOME/Pictures/logo-original.png" ] || cp "$REPO/sistema/logo-laob.png" "$HOME/Pictures/logo-original.png"
cp "$REPO/sistema/atualizar-papel-parede.service" "$REPO/sistema/atualizar-papel-parede.timer" "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
systemctl --user enable --now atualizar-papel-parede.timer >/dev/null 2>&1 || true

# A areia aberta continua sendo o programa antigo até fechar (em 14/11/2026 os jogos novos
# "não existiam" depois de instalar): se o programa mudou e ela está aberta, reabre agora.
if [ "$PROGRAMA_ANTES" != "$(md5sum "$DEST/bin/SARndbox" | cut -d" " -f1)" ] \
   && [ "$("$DEST/sandbox.sh" estado 2>/dev/null)" = rosto ]; then
  echo "o programa da areia mudou: reabrindo a areia (uns 15 s)"
  "$DEST/sandbox.sh" original >/dev/null 2>&1 || true
  "$DEST/areia-topografia" >/dev/null 2>&1 || echo "  a areia não reabriu sozinha: use o botão 'Reabrir a areia' do painel"
fi

echo
echo "Pronto. Tudo pelo painel: http://localhost:8090 (atalho na Área de Trabalho). Teste completo: bash testes/integracao.sh"
