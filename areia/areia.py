"""Núcleo do Projetor-LAOB: pasta de imagens, estado, preparo (com cache) e controle do projetor.

Usado pelo comando areia-foto, pelo painel do celular e pelo areia-topografia. Só usa a
biblioteca padrão (o areia-topografia roda com o python3 do sistema, até sem o venv).
Caminhos a partir da pasta deste arquivo; para testes: AREIA_DIR, AREIA_PASTA, AREIA_SANDBOX.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
import unicodedata

D = os.environ.get("AREIA_DIR") or os.path.dirname(os.path.realpath(__file__))
FOTOS = os.path.join(D, "fotos")
CACHE = os.path.join(FOTOS, "cache")
ESTADO = os.path.join(D, "estado.json")
SANDBOX = os.environ.get("AREIA_SANDBOX") or os.path.join(D, "sandbox.sh")
PY = os.path.join(D, "venv", "bin", "python")
ROSTO_PY = os.path.join(D, "rosto.py")
EXT = (".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff")
MODOS = ("relevo", "rosto", "esculpir", "original")
FUNDOS = ("", "--com-fundo", "--sem-fundo")
AJUSTE = {"zoom": 0.03, "desloc": 0.0, "mistura": 0.0, "sombra": 1.0}
PADRAO = {"modo": "original", "giro": 0, "espelho": False, "imagem": None, "fundo": "", "ajuste": AJUSTE}
MANTER_NO_CACHE = 40
PASTA_FOTOS = "2 - Fotos na areia"  # na Área de Trabalho (antes se chamava "Areia")
DIFICULDADE = {"facil": 2.0, "normal": 3.0, "dificil": 5.0}  # cm de areia em cima do tesouro
ANIMAIS = ("peixes", "carros", "todos")
TEMAS = os.path.join(D, "temas")
TEMA_ATUAL = os.path.join(TEMAS, "atual.cpt")
NOMES_TEMAS = {"classico": "Clássico", "vulcao": "Vulcão", "inverno": "Inverno", "marte": "Marte",
               "deserto": "Deserto", "oceano": "Oceano", "arco-iris": "Arco-íris", "mapa": "Mapa de papel"}


class Erro(Exception):
    """Falha com mensagem para mostrar ao usuário."""


# ---------- pasta Areia ----------

def pasta():
    """A pasta das fotos, na Área de Trabalho (criada se não existir)."""
    p = os.environ.get("AREIA_PASTA")
    if not p:
        try:
            mesa = subprocess.run(["xdg-user-dir", "DESKTOP"], capture_output=True, text=True,
                                  timeout=5).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            mesa = ""
        casa = os.path.expanduser("~")
        p = os.path.join(mesa if mesa and mesa != casa else os.path.join(casa, "Desktop"), PASTA_FOTOS)
    os.makedirs(p, exist_ok=True)
    return p


def imagens():
    """Imagens da pasta Areia em ordem alfabética (sem diferenciar maiúsculas)."""
    p = pasta()
    nomes = [n for n in os.listdir(p)
             if n.lower().endswith(EXT) and not n.startswith(".") and os.path.isfile(os.path.join(p, n))]
    return [os.path.join(p, n) for n in sorted(nomes, key=_ordem)]


def _ordem(nome):
    """Ordem natural: "foto2" antes de "foto10", "ana" antes de "ana-2"; sem diferenciar maiúsculas."""
    base, ext = os.path.splitext(nome.casefold())
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", base)], ext


def acha(pedido, lista=None):
    """Imagem pelo número (1..n), arquivo (copiado para a pasta) ou parte do nome."""
    lista = imagens() if lista is None else lista
    pedido = pedido.strip()
    if pedido.isdigit():
        n = int(pedido)
        if 1 <= n <= len(lista):
            return lista[n - 1]
        raise Erro(f"não há imagem número {n}")
    caminho = os.path.expanduser(pedido)
    if os.path.isfile(caminho):
        return guarda(caminho)
    achadas = [c for c in lista if pedido.casefold() in os.path.basename(c).casefold()]
    if len(achadas) == 1:
        return achadas[0]
    if achadas:
        raise Erro(f'mais de uma imagem com "{pedido}": use o número')
    raise Erro(f'nenhuma imagem com "{pedido}"')


def guarda(caminho, nome=None):
    """Copia um arquivo para a pasta Areia (sem sobrescrever outro de mesmo nome)."""
    if not caminho.lower().endswith(EXT) and not (nome or "").lower().endswith(EXT):
        raise Erro("formato não suportado: use .jpg, .png, .webp ou .bmp (HEIC do iPhone: converta antes)")
    p = pasta()
    nome = nome or os.path.basename(caminho)
    destino = os.path.join(p, nome)
    if os.path.abspath(caminho) == os.path.abspath(destino):
        return destino
    base, ext = os.path.splitext(nome)
    n = 2
    while os.path.exists(destino):
        if _mesmo_conteudo(caminho, destino):
            return destino
        destino = os.path.join(p, f"{base}-{n}{ext}")
        n += 1
    shutil.copy2(caminho, destino)
    return destino


def _mesmo_conteudo(a, b):
    return os.path.getsize(a) == os.path.getsize(b) and _hash_arquivo(a) == _hash_arquivo(b)


def _hash_arquivo(caminho):
    h = hashlib.sha1()
    with open(caminho, "rb") as f:
        for bloco in iter(lambda: f.read(1 << 20), b""):
            h.update(bloco)
    return h.hexdigest()


# ---------- estado (o que está no projetor) ----------

def estado():
    try:
        with open(ESTADO) as f:
            e = json.load(f)
        if not isinstance(e, dict):
            e = {}
    except (OSError, ValueError):
        e = {}
    e = {**PADRAO, **e}
    e["ajuste"] = {**AJUSTE, **(e.get("ajuste") or {})}
    e["giro"] = int(e["giro"]) % 360
    e["espelho"] = bool(e["espelho"])
    if e["modo"] not in MODOS:
        e["modo"] = "original"
    if e["fundo"] not in FUNDOS:
        e["fundo"] = ""
    return e


def salva(e):
    tmp = ESTADO + ".tmp"
    with open(tmp, "w") as f:
        json.dump(e, f, ensure_ascii=False)
    os.replace(tmp, ESTADO)


# ---------- preparo da imagem ----------

def chave(img, e, fundo):
    """Muda se a imagem, o rosto.py, o giro, o espelho ou o recorte mudarem."""
    h = hashlib.sha1(_hash_arquivo(img).encode())
    h.update(_hash_arquivo(ROSTO_PY).encode())
    h.update(f"{e['giro']}|{int(e['espelho'])}|{fundo}".encode())
    return h.hexdigest()[:16]


def prepara(img, e, fundo=""):
    """Gera fotos/rosto.png e rosto.dem (uns 5 s) ou reaproveita do cache. Devolve (pasta, novo)."""
    if not os.path.isfile(img):
        raise Erro(f"imagem não encontrada: {img}")
    pronto = os.path.join(CACHE, chave(img, e, fundo))
    novo = not os.path.exists(os.path.join(pronto, "rosto.dem"))
    if novo:
        args = [PY, ROSTO_PY, img, "--giro", str(e["giro"]), "--saida", pronto]
        if e["espelho"]:
            args.append("--espelho")
        if fundo:
            args.append(fundo)
        r = subprocess.run(args, capture_output=True, text=True, timeout=120)
        if r.returncode != 0 or not os.path.exists(os.path.join(pronto, "rosto.dem")):
            shutil.rmtree(pronto, ignore_errors=True)
            linhas = [l for l in (r.stderr or r.stdout).splitlines() if "WARN" not in l]
            raise Erro("não consegui preparar a imagem: " + (linhas[-1] if linhas else "erro desconhecido"))
    os.makedirs(FOTOS, exist_ok=True)
    for n in ("rosto.png", "rosto.dem"):  # o painel e o SARndbox usam fotos/rosto.*
        shutil.copy2(os.path.join(pronto, n), os.path.join(FOTOS, n + ".tmp"))
        os.replace(os.path.join(FOTOS, n + ".tmp"), os.path.join(FOTOS, n))
    os.utime(pronto)
    limpa_cache()
    return pronto, novo


def limpa_cache(manter=MANTER_NO_CACHE):
    if not os.path.isdir(CACHE):
        return
    dirs = sorted((os.path.join(CACHE, d) for d in os.listdir(CACHE)), key=os.path.getmtime, reverse=True)
    for d in dirs[manter:]:
        shutil.rmtree(d, ignore_errors=True)


# ---------- projetor ----------

def _sandbox(*args, timeout=60):
    r = subprocess.run([SANDBOX, *args], capture_output=True, text=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).strip()


def qual():
    """original, rosto ou nenhum."""
    return _sandbox("estado", timeout=10)[1] or "nenhum"


def _comando(texto):
    rc, out = _sandbox("cmd", texto, timeout=10)
    if rc != 0:
        raise Erro(out or "o SARndbox não aceitou o comando")


def aplica(e):
    """Põe o projetor no modo de e (sobe a versão com Foto se precisar) e salva o estado."""
    if e["modo"] == "original":
        rc, out = _sandbox("original")
        if rc != 0 or qual() != "original":
            raise Erro("a topografia não abriu: " + out)
        salva(e)
        return
    if e["modo"] in ("rosto", "esculpir") and not os.path.exists(os.path.join(FOTOS, "rosto.png")):
        raise Erro("ainda não há imagem: escolha uma")
    if qual() != "rosto":
        rc, out = _sandbox("rosto")
        if rc != 0:
            raise Erro("o modo foto não abriu; a topografia voltou")
    a = e["ajuste"]
    if e["modo"] == "rosto":
        _comando(f"rosto {os.path.join(FOTOS, 'rosto.png')} {e['giro']} {int(e['espelho'])}")
        _comando(f"rostoAjuste {a['zoom']} {a['desloc']} {a['mistura']} {a['sombra']}")
    elif e["modo"] == "esculpir":
        _comando(f"esculpir {os.path.join(FOTOS, 'rosto.dem')}")
    else:
        _comando("relevo")
    salva(e)


def ajusta(e):
    """Só os controles deslizantes (sem recarregar a imagem)."""
    a = e["ajuste"]
    if qual() == "rosto" and e["modo"] == "rosto":
        _comando(f"rostoAjuste {a['zoom']} {a['desloc']} {a['mistura']} {a['sombra']}")
    salva(e)


def mostra(img, fundo=None, modo="rosto"):
    """Prepara img e projeta. Devolve True se precisou preparar (não estava no cache)."""
    e = estado()
    fundo = e["fundo"] if fundo is None else fundo
    _, novo = prepara(img, e, fundo)
    e.update(imagem=img, fundo=fundo, modo=modo)
    aplica(e)
    return novo


def reorienta(giro=None, espelho=None):
    """Muda giro/espelho e refaz a imagem atual (se houver)."""
    e = estado()
    if giro is not None:
        e["giro"] = int(giro) % 360
    if espelho is not None:
        e["espelho"] = bool(espelho)
    img = e.get("imagem")
    if img and os.path.isfile(img):
        prepara(img, e, e["fundo"])
        if e["modo"] in ("rosto", "esculpir") and qual() == "rosto":
            aplica(e)
            return e
    salva(e)
    return e


def foto_da_camera():
    """Foto pela câmera do laboratório (comando liberado do Willbert), guardada na pasta Areia."""
    r = subprocess.run(["sudo", "-n", "/usr/local/sbin/willbert-controle", "foto"],
                       capture_output=True, text=True, timeout=90)
    origem = os.path.join(FOTOS, "camera.jpg")
    if r.returncode != 0 or not os.path.isfile(origem):
        raise Erro("não consegui a foto da câmera (o Willbert está configurado?)")
    return guarda(origem, time.strftime("camera-%Y-%m-%d-%H%M%S.jpg"))


# ---------- temas de cor do relevo ----------

def temas():
    """Temas disponíveis (arquivos .cpt da pasta temas/), na ordem de NOMES_TEMAS."""
    try:
        achados = {f[:-4] for f in os.listdir(TEMAS) if f.endswith(".cpt") and f != "atual.cpt"}
    except OSError:
        achados = set()
    return [t for t in NOMES_TEMAS if t in achados] + sorted(achados - set(NOMES_TEMAS))


def tema_atual():
    try:
        return os.path.basename(os.readlink(TEMA_ATUAL))[:-4]
    except OSError:
        return "classico"


def _chave_tema(nome):
    t = unicodedata.normalize("NFKD", nome).encode("ascii", "ignore").decode().lower().strip()
    return "-".join(t.replace("_", " ").replace("-", " ").split())


def escolhe_tema(nome):
    """Troca as cores do relevo (na hora, no que estiver no projetor) e guarda para as próximas aberturas."""
    t = _chave_tema(nome)
    disponiveis = temas()
    if t not in disponiveis:
        achados = [x for x in disponiveis if x.startswith(t)] if t else []
        if len(achados) != 1:
            raise Erro(f'tema "{nome}" não existe; temas: {", ".join(disponiveis)}')
        t = achados[0]
    tmp = TEMA_ATUAL + ".tmp"
    if os.path.lexists(tmp):
        os.remove(tmp)
    os.symlink(t + ".cpt", tmp)
    os.replace(tmp, TEMA_ATUAL)
    rc, out = _sandbox("tema", os.path.join(TEMAS, t + ".cpt"), timeout=30)
    if rc != 0:
        raise Erro("não consegui trocar as cores agora (" + out + "); valem na próxima abertura")
    e = estado()
    e["tema"] = t
    salva(e)
    return t


# ---------- brincadeiras: caça ao tesouro, peixes e carrinhos ----------

def _brincadeira(comando):
    """Brincadeiras rodam na versão com Foto (sobre o relevo colorido); sobe ela se precisar."""
    if qual() != "rosto":
        rc, _ = _sandbox("rosto")
        if rc != 0:
            raise Erro("a caixa de areia não abriu o modo brincadeira; a topografia voltou")
    _comando(comando)
    e = estado()
    e["modo"] = "relevo"
    salva(e)


def tesouro(dificuldade="normal"):
    """Esconde um tesouro num lugar sorteado, embaixo da areia. Devolve a profundidade (cm)."""
    d = _chave_tema(str(dificuldade))
    prof = DIFICULDADE.get(d)
    if prof is None:
        try:
            prof = float(d.replace(",", "."))
        except ValueError:
            raise Erro("dificuldade: facil, normal ou dificil (ou a profundidade em cm)")
    if not 1.0 <= prof <= 10.0:
        raise Erro("a profundidade tem que ser entre 1 e 10 cm")
    _brincadeira(f"tesouro {prof:g}")
    return prof


def tesouro_situacao():
    """escondido, achado ou nenhum (lido do registro do programa da areia)."""
    if qual() != "rosto":
        return "nenhum"
    try:
        with open(os.path.join(D, "sandbox.log"), errors="replace") as f:
            linhas = [l for l in f if l.startswith(("Tesouro", "Modo Rosto:"))]
    except OSError:
        return "nenhum"
    if not linhas or linhas[-1].startswith(("Tesouro desligado", "Modo Rosto:")):
        return "nenhum"
    return "achado" if linhas[-1].startswith("Tesouro encontrado") else "escondido"


def tesouro_fim():
    if qual() == "rosto":
        _comando("tesouroFim")


def animais(tipo, quantidade=None):
    t = _chave_tema(str(tipo))
    t = {"peixe": "peixes", "carro": "carros", "carrinhos": "carros", "carrinho": "carros", "tudo": "todos"}.get(t, t)
    if t not in ANIMAIS:
        raise Erro("escolha: peixes, carros ou todos")
    n = int(quantidade) if quantidade else (14 if t == "todos" else 10)
    if not 1 <= n <= 24:
        raise Erro("quantidade: de 1 a 24")
    _brincadeira(f"animais {t} {n}")
    return t, n


def animais_fim():
    if qual() == "rosto":
        _comando("animais nada")


def marca_topografia():
    """Só anota no estado que o projetor está na topografia (usado pelo areia-topografia)."""
    e = estado()
    e["modo"] = "original"
    salva(e)


if __name__ == "__main__":
    import sys
    if sys.argv[1:] == ["marca-topografia"]:
        marca_topografia()
    else:
        sys.exit("uso: python3 areia.py marca-topografia")
