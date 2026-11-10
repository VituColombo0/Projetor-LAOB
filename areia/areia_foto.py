"""Modo FOTO da caixa de areia: projeta uma imagem da pasta Areia (na Área de Trabalho).

  areia-foto                 escolhe a imagem num menu (Enter = a última usada)
  areia-foto 3               a imagem número 3 da lista
  areia-foto gato            a imagem cujo nome contém "gato"
  areia-foto ~/x/foto.jpg    qualquer arquivo (é copiado para a pasta Areia)
  areia-foto lista           só mostra a lista
  areia-foto pasta           abre a pasta Areia
  areia-foto girar           gira 90° (se aparecer de lado para quem olha a caixa)
  areia-foto espelhar        espelha
  areia-foto esculpir        mostra onde pôr/tirar areia para formar a imagem
  areia-foto camera          tira a foto pela câmera do laboratório (precisa do Willbert)
Opções depois da imagem: --com-fundo (não recorta) / --sem-fundo (recorta a pessoa mesmo sem rosto).
Para voltar à topografia: areia-topografia
"""
import os
import subprocess
import sys

import areia


def mostra_lista(lista, ultima=None):
    print(f"Imagens em {areia.pasta()}:")
    if not lista:
        print("  (vazia: coloque fotos .jpg ou .png nessa pasta)")
    for i, c in enumerate(lista, 1):
        print(f"  {i:2d}  {os.path.basename(c)}" + ("   <- última" if c == ultima else ""))


def menu(e):
    """Pergunta a imagem no terminal; sem terminal, usa a última."""
    lista = areia.imagens()
    ultima = e.get("imagem") if e.get("imagem") in lista else None
    if not sys.stdin.isatty():
        if ultima:
            return ultima
        mostra_lista(lista)
        raise areia.Erro("diga qual: areia-foto <número>")
    mostra_lista(lista, ultima)
    if not lista:
        raise areia.Erro("a pasta está vazia")
    while True:
        r = input("Número da imagem" + (" (Enter = a última)" if ultima else "") + ": ").strip()
        if not r and ultima:
            return ultima
        if r:
            try:
                return areia.acha(r, lista)
            except areia.Erro as err:
                print(f"  {err}")


def projeta(img, fundo, modo="rosto"):
    print(f"Imagem: {os.path.basename(img)}")
    e = areia.estado()
    if not os.path.exists(os.path.join(areia.CACHE, areia.chave(img, e, e["fundo"] if fundo is None else fundo))):
        print("Preparando a imagem (recorte do fundo, uns 5 s)...")
    areia.mostra(img, fundo, modo)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    opcoes = [a for a in argv if a.startswith("--")]
    if any(o in ("-h", "--help") for o in opcoes) or args[:1] in (["ajuda"], ["-h"]):
        print(__doc__.strip())
        return 0
    desconhecidas = [o for o in opcoes if o not in ("--com-fundo", "--sem-fundo")]
    if desconhecidas:
        raise areia.Erro(f"opção desconhecida: {desconhecidas[0]} (veja: areia-foto ajuda)")
    fundo = opcoes[-1] if opcoes else None
    e = areia.estado()
    cmd = args[0] if args else None

    if cmd == "lista":
        mostra_lista(areia.imagens(), e.get("imagem"))
        return 0
    if cmd == "pasta":
        subprocess.Popen(["xdg-open", areia.pasta()], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(areia.pasta())
        return 0
    if cmd in ("girar", "espelhar"):
        e = areia.reorienta(giro=e["giro"] + 90) if cmd == "girar" else areia.reorienta(espelho=not e["espelho"])
        print(f"Giro {e['giro']}°, espelho {'sim' if e['espelho'] else 'não'}.")
        if not (e.get("imagem") and os.path.isfile(e["imagem"])):
            print("Ajuste guardado; escolha uma imagem: areia-foto")
        return 0
    if cmd == "esculpir":
        img = e.get("imagem")
        if not (img and os.path.isfile(img)):
            raise areia.Erro("escolha uma imagem antes: areia-foto")
        projeta(img, fundo, "esculpir")
        print("Modo ESCULPIR: vermelho = falta areia, azul = sobra. Voltar à foto: areia-foto")
        return 0
    if cmd in ("camera", "câmera"):
        img = areia.foto_da_camera()
    elif cmd is None:
        img = menu(e)
    else:
        caminho = os.path.expanduser(cmd)
        de_fora = os.path.isfile(caminho) and os.path.dirname(os.path.abspath(caminho)) != areia.pasta()
        img = areia.acha(cmd)
        if de_fora:
            print(f"Copiada para a pasta Areia: {os.path.basename(img)}")
    projeta(img, fundo)
    print("Modo FOTO no projetor. Outra imagem: areia-foto   Voltar: areia-topografia")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except areia.Erro as err:
        print(err, file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print()
        sys.exit(130)
