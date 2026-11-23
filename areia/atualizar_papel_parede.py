#!/usr/bin/env python3
"""Atualiza o papel de parede da Área de Trabalho com o IP e porta atuais do painel.
Roda pelo timer de usuário atualizar-papel-parede (a cada 5 minutos) e quando o painel abre;
só redesenha (e só escreve no journal) quando o endereço muda. Base: ~/Pictures/logo-original.png (logo do LAOB)."""
import os
import socket
import subprocess
from PIL import Image, ImageDraw, ImageFont

PORTA = int(os.environ.get("AREIA_PORTA", "8090"))
FOTOS = os.path.join(os.path.expanduser("~"), "Pictures")
BASE_IMG = os.path.join(FOTOS, "logo-original.png")
IMG_A = os.path.join(FOTOS, "wallpaper-a.png")
IMG_B = os.path.join(FOTOS, "wallpaper-b.png")
LAST_IP_FILE = os.path.join(FOTOS, ".last_wallpaper_ip")

def obter_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

def atualizar(forcar=False):
    ip = obter_ip()
    if ip.startswith("127."):
        return ip  # sem rede agora: mantém o endereço que já está no papel de parede
    url = f"http://{ip}:{PORTA}"

    if not forcar and os.path.exists(LAST_IP_FILE):
        try:
            with open(LAST_IP_FILE) as f:
                if f.read().strip() == url:
                    return ip
        except Exception:
            pass

    if not os.path.exists(BASE_IMG):
        return ip

    im = Image.open(BASE_IMG).convert("RGBA")
    w, h = im.size

    try:
        font_titulo = ImageFont.truetype("/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf", 15)
        font_url = ImageFont.truetype("/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf", 22)
        font_sub = ImageFont.truetype("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf", 12)
    except Exception:
        font_titulo = font_url = font_sub = ImageFont.load_default()

    t_titulo = "PAINEL DA CAIXA DE AREIA"
    t_url = url
    t_sub = "Acesse pelo celular ou PC na mesma rede"

    draw = ImageDraw.Draw(im)
    b1 = draw.textbbox((0, 0), t_titulo, font=font_titulo)
    b2 = draw.textbbox((0, 0), t_url, font=font_url)
    b3 = draw.textbbox((0, 0), t_sub, font=font_sub)

    pad_x = 22
    pad_y = 16
    box_w = max(b1[2]-b1[0], b2[2]-b2[0], b3[2]-b3[0]) + pad_x * 2
    box_h = (b1[3]-b1[1]) + (b2[3]-b2[1]) + (b3[3]-b3[1]) + pad_y * 2 + 16

    # Canto inferior esquerdo
    margin_x = 32
    margin_y = 32
    x0 = margin_x
    y0 = h - box_h - margin_y
    x1 = x0 + box_w
    y1 = y0 + box_h

    overlay = Image.new("RGBA", im.size, (0, 0, 0, 0))
    odraw = ImageDraw.Draw(overlay)

    # Caixa com cantos arredondados, fundo escuro semitransparente e borda fina
    odraw.rounded_rectangle([x0, y0, x1, y1], radius=10, fill=(18, 20, 24, 225), outline=(255, 255, 255, 75), width=1)

    cy = y0 + pad_y
    odraw.text((x0 + pad_x, cy), t_titulo, font=font_titulo, fill=(200, 205, 215, 255))
    cy += (b1[3]-b1[1]) + 6
    odraw.text((x0 + pad_x, cy), t_url, font=font_url, fill=(255, 210, 0, 255))
    cy += (b2[3]-b2[1]) + 8
    odraw.text((x0 + pad_x, cy), t_sub, font=font_sub, fill=(160, 165, 175, 255))

    final_im = Image.alpha_composite(im, overlay).convert("RGB")

    # Alterna entre IMG_A e IMG_B para o GNOME detectar a troca de URI (grava na mais velha;
    # antes, com as duas existindo, gravava sempre na A e o GNOME não redesenhava)
    def idade(c):
        return os.path.getmtime(c) if os.path.exists(c) else 0.0
    alvo = IMG_A if idade(IMG_A) <= idade(IMG_B) else IMG_B
    final_im.save(alvo)

    uri = f"file://{alvo}"
    env = dict(os.environ)
    u = os.getuid()
    env["DISPLAY"] = env.get("DISPLAY", ":0")
    env["DBUS_SESSION_BUS_ADDRESS"] = env.get("DBUS_SESSION_BUS_ADDRESS", f"unix:path=/run/user/{u}/bus")

    subprocess.run(["gsettings", "set", "org.gnome.desktop.background", "picture-uri", uri],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", uri],
                   env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    with open(LAST_IP_FILE, "w") as f:
        f.write(url)

    return ip

if __name__ == "__main__":
    import sys
    # sem --forcar: só redesenha se o endereço mudou. Antes forçava a cada minuto, e o GNOME
    # recarregava a imagem de fundo o tempo todo (em 13/11 o gnome-shell travou)
    def _anotado():
        try:
            with open(LAST_IP_FILE) as f:
                return f.read().strip()
        except OSError:
            return None
    antes = _anotado()
    ip = atualizar(forcar="--forcar" in sys.argv[1:])
    # 23/11/2026: pelo timer, só diz algo quando o endereço mudou (antes eram ~770 linhas por dia no journal
    # repetindo o mesmo endereço); no terminal, sempre
    if sys.stdout.isatty() or _anotado() != antes:
        print(f"Endereço do painel: http://{ip}:{PORTA}")
