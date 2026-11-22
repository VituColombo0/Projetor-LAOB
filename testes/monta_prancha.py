"""Monta a prancha (grade) com os prints do testes/prancha.sh: linhas = brincadeiras, colunas = temas.
   python3 testes/monta_prancha.py <pasta-dos-png> <saida.jpg> [título]
Só precisa do Pillow. Quadro sem print fica cinza com "—"."""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

TEMAS = ["classico", "vulcao", "inverno", "marte", "deserto", "oceano", "arco-iris"]
NOMES_T = ["Clássico", "Vulcão", "Inverno", "Marte", "Deserto", "Oceano", "Arco-íris"]
BRINC = ["topografia", "tesouro", "fossil", "labirinto", "corrida", "vila", "rota", "vulcao", "copia", "bichos"]
NOMES_B = ["Topografia", "Tesouro", "Fóssil", "Labirinto", "Corrida", "Vila", "Rota", "Vulcão", "Copie", "Vida"]
EXTRAS = [("mundo", "Mundo LAOB"), ("foto", "Foto"), ("esculpir", "Esculpir")]
W, H = 320, 240
ROT = 150  # coluna dos nomes
TOPO = 70


def fonte(t):
    for f in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, t)
    return ImageFont.load_default()


def quadro(pasta, nome):
    p = os.path.join(pasta, nome + ".png")
    if os.path.exists(p):
        return Image.open(p).convert("RGB").resize((W, H), Image.LANCZOS)
    im = Image.new("RGB", (W, H), (60, 60, 60))
    ImageDraw.Draw(im).text((W // 2 - 10, H // 2 - 14), "—", fill=(200, 200, 200), font=fonte(28))
    return im


def main():
    pasta, saida = sys.argv[1], sys.argv[2]
    titulo = sys.argv[3] if len(sys.argv) > 3 else os.path.basename(os.path.normpath(pasta))
    linhas = len(BRINC) + 1
    img = Image.new("RGB", (ROT + W * len(TEMAS), TOPO + 30 + H * linhas), (24, 24, 28))
    d = ImageDraw.Draw(img)
    d.text((16, 14), titulo, fill=(255, 255, 255), font=fonte(30))
    for j, n in enumerate(NOMES_T):
        d.text((ROT + j * W + 10, TOPO + 2), n, fill=(230, 230, 230), font=fonte(22))
    y0 = TOPO + 30
    for i, (b, nb) in enumerate(zip(BRINC, NOMES_B)):
        d.text((10, y0 + i * H + H // 2 - 12), nb, fill=(230, 230, 230), font=fonte(20))
        for j, t in enumerate(TEMAS):
            img.paste(quadro(pasta, f"{b}__{t}"), (ROT + j * W, y0 + i * H))
    i = len(BRINC)
    d.text((10, y0 + i * H + H // 2 - 12), "Outros", fill=(230, 230, 230), font=fonte(20))
    for j, (b, nb) in enumerate(EXTRAS):
        img.paste(quadro(pasta, f"{b}__classico"), (ROT + j * W, y0 + i * H))
        d.rectangle((ROT + j * W, y0 + i * H, ROT + j * W + 150, y0 + i * H + 30), fill=(0, 0, 0))
        d.text((ROT + j * W + 6, y0 + i * H + 4), nb, fill=(255, 255, 255), font=fonte(20))
    # grade fina entre os quadros
    for j in range(len(TEMAS) + 1):
        d.line((ROT + j * W, y0, ROT + j * W, y0 + H * linhas), fill=(24, 24, 28), width=3)
    for i in range(linhas + 1):
        d.line((ROT, y0 + i * H, ROT + W * len(TEMAS), y0 + i * H), fill=(24, 24, 28), width=3)
    img.save(saida, quality=85)
    print(saida, img.size)


if __name__ == "__main__":
    main()
