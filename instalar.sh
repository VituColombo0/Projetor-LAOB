#!/bin/bash
# Instala o Projetor-LAOB num PC que já roda o AR Sandbox (SARndbox 2.8) calibrado, como o lab2.
#   bash instalar.sh              instala/atualiza tudo em ~/SARndbox-rosto (sem sudo)
#   bash instalar.sh --compilar DIR   só monta e compila a versão com modo Foto em DIR (teste)
# Variáveis: SARNDBOX_ORIG (padrão ~/SARndbox-2.8) e SARNDBOX_CALIB (pasta com BoxLayout.txt e
# ProjectorMatrix.dat; padrão /root/etc/SARndbox-2.8, a que o SARndbox do lab2 usa).
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
for f in SurfaceRenderer.h SurfaceRenderer.cpp Sandbox.h Sandbox.cpp makefile; do cp "$ORIG/$f" "$DEST/$f"; done
mkdir -p "$DEST/etc/SARndbox-2.8"
for f in BoxLayout.txt ProjectorMatrix.dat HeightColorMap.cpt SARndbox.cfg; do
  if [ -r "$CALIB/$f" ]; then cp "$CALIB/$f" "$DEST/etc/SARndbox-2.8/"
  elif [ ! -f "$DEST/etc/SARndbox-2.8/$f" ]; then echo "  aviso: sem $CALIB/$f (calibre o SARndbox ou defina SARNDBOX_CALIB)"; fi
done

echo "[2/8] modo Foto: patches e compilação"
cp "$REPO"/sarndbox/{FaceImage.h,FaceImage.cpp,patch_surface.py,patch_sandbox.py,projetor.cfg} "$DEST/"
( cd "$DEST" && python3 patch_surface.py >/dev/null && python3 patch_sandbox.py >/dev/null )
if ! make -C "$DEST" INSTALLDIR="$DEST" > "$DEST/compilacao.log" 2>&1 \
   || ! [ "$DEST/bin/SARndbox" -nt "$DEST/FaceImage.cpp" ]; then
  echo "a compilação falhou; veja $DEST/compilacao.log"; exit 1
fi
[ $SO_COMPILAR = 1 ] && { echo "compilado em $DEST/bin/SARndbox"; exit 0; }

echo "[3/8] programas da pasta areia/"
cp "$REPO"/areia/{areia.py,areia_foto.py,rosto.py,painel.py,sandbox.sh,areia-foto,areia-topografia} "$DEST/"
chmod +x "$DEST"/{sandbox.sh,areia-foto,areia-topografia}
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

echo "[6/8] comandos areia-foto e areia-topografia"
mkdir -p "$HOME/.local/bin" "$DEST/fotos"
ln -sf "$DEST/areia-foto" "$HOME/.local/bin/areia-foto"
ln -sf "$DEST/areia-topografia" "$HOME/.local/bin/areia-topografia"

echo "[7/8] Área de Trabalho: pasta Areia, ajuda e atalho"
MESA=$(xdg-user-dir DESKTOP 2>/dev/null || echo "$HOME/Desktop"); [ "$MESA" = "$HOME" ] && MESA=$HOME/Desktop
mkdir -p "$MESA/Areia"
cp "$REPO/area-de-trabalho/COMO-USAR.txt" "$MESA/Areia/COMO USAR.txt"
cp "$REPO/area-de-trabalho/SE-A-AREIA-NAO-ABRIR.txt" "$MESA/SE A AREIA NÃO ABRIR.txt"
sed "s#/home/laob#$HOME#g" "$REPO/area-de-trabalho/abrir-a-areia.desktop" > "$MESA/Abrir a areia.desktop"
chmod +x "$MESA/Abrir a areia.desktop"
gio set "$MESA/Abrir a areia.desktop" metadata::trusted true 2>/dev/null || true
B=$HOME/.config/gtk-3.0/bookmarks; mkdir -p "$(dirname "$B")"
grep -q "file://$MESA/Areia " "$B" 2>/dev/null || echo "file://$MESA/Areia Areia" >> "$B"

echo "[8/8] topografia ao ligar e painel do celular"
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

echo
echo "Pronto. Topografia: areia-topografia   Foto: areia-foto   Ajuda: $MESA/Areia/COMO USAR.txt"
