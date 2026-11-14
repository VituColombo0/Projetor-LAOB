"""Monta o painel de rostos dos membros para as pessoinhas da areia (roda com o venv: OpenCV).

  python membros.py            lê membros/*.jpg|png (o nome do arquivo é o nome do membro) e grava
                               fotos/membros.rgba (até 8 rostos de 128x128, lado a lado, RGBA cru)
                               e fotos/membros.json (os nomes, na ordem do painel)

Cada rosto é achado pelo YuNet (modelos/), recortado quadrado com folga para cabelo e queixo e
mascarado em círculo. Sem rosto na foto, usa o centro da imagem. As fotos ficam só nesta máquina.
"""
import json
import os
import sys
import unicodedata

import cv2
import numpy as np

AQUI = os.path.dirname(os.path.realpath(__file__))
PASTA = os.environ.get("AREIA_MEMBROS") or os.path.join(AQUI, "membros")
FOTOS = os.path.join(os.environ.get("AREIA_DIR") or AQUI, "fotos")
YUNET = os.path.join(AQUI, "modelos", "face_detection_yunet_2023mar.onnx")
LADO, MAXIMO = 128, 8
EXT = (".jpg", ".jpeg", ".png", ".webp", ".bmp")


def nome_curto(arquivo):
    t = unicodedata.normalize("NFKD", os.path.splitext(os.path.basename(arquivo))[0])
    t = "".join(c for c in t.encode("ascii", "ignore").decode().upper() if c.isalpha() or c == " ").split()
    return t[0][:8] if t else ""


def rosto(img):
    """Recorte quadrado em volta do maior rosto (ou do centro da imagem), LADO x LADO."""
    h, w = img.shape[:2]
    cx, cy, lado = w / 2, h / 2, min(w, h)
    if os.path.exists(YUNET):
        esc = min(1.0, 960 / max(h, w))
        peq = cv2.resize(img, (int(w * esc), int(h * esc))) if esc < 1 else img
        det = cv2.FaceDetectorYN.create(YUNET, "", (peq.shape[1], peq.shape[0]), 0.6, 0.3, 50)
        _, faces = det.detect(peq)
        if faces is not None and len(faces):
            x, y, fw, fh = max(faces, key=lambda f: f[2] * f[3])[:4] / esc
            cx, cy, lado = x + fw / 2, y + fh * 0.45, max(fw, fh) * 1.75
    x0, y0 = int(round(cx - lado / 2)), int(round(cy - lado / 2))
    lado = int(round(lado))
    borda = max(0, -x0, -y0, x0 + lado - w, y0 + lado - h)
    if borda:
        img = cv2.copyMakeBorder(img, borda, borda, borda, borda, cv2.BORDER_REPLICATE)
        x0, y0 = x0 + borda, y0 + borda
    return cv2.resize(img[y0:y0 + lado, x0:x0 + lado], (LADO, LADO), interpolation=cv2.INTER_AREA)


def monta():
    os.makedirs(FOTOS, exist_ok=True)
    arquivos = sorted((f for f in os.listdir(PASTA) if f.lower().endswith(EXT)), key=str.casefold) \
        if os.path.isdir(PASTA) else []
    painel = np.zeros((LADO, LADO * MAXIMO, 4), np.uint8)
    yy, xx = np.mgrid[0:LADO, 0:LADO]
    dist = np.hypot(xx - (LADO - 1) / 2, yy - (LADO - 1) / 2)
    alfa = (np.clip((LADO / 2 - 1 - dist) / 2.0, 0, 1) * 255).astype(np.uint8)   # círculo com borda suave
    nomes = []
    for f in arquivos:
        n = nome_curto(f)
        img = cv2.imread(os.path.join(PASTA, f), cv2.IMREAD_COLOR)
        if not n or n in nomes or img is None or len(nomes) >= MAXIMO:
            continue
        r = rosto(img)
        k = len(nomes)
        painel[:, k * LADO:(k + 1) * LADO, :3] = r[:, :, ::-1]   # BGR -> RGB
        painel[:, k * LADO:(k + 1) * LADO, 3] = alfa
        nomes.append(n)
    tmp = os.path.join(FOTOS, "membros.rgba.tmp")
    painel.tofile(tmp)
    os.replace(tmp, os.path.join(FOTOS, "membros.rgba"))
    with open(os.path.join(FOTOS, "membros.json.tmp"), "w") as fj:
        json.dump({"nomes": nomes, "lado": LADO, "maximo": MAXIMO}, fj)
    os.replace(os.path.join(FOTOS, "membros.json.tmp"), os.path.join(FOTOS, "membros.json"))
    return nomes


if __name__ == "__main__":
    print(" ".join(monta()))
    sys.exit(0)
