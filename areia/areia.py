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
import threading
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
PASTA_FOTOS = "Fotos na areia"  # na pasta Imagens (antes: "2 - Fotos na areia" na Área de Trabalho)
DIFICULDADE = {"facil": 4.0, "normal": 5.5, "dificil": 7.0}  # cm de areia em cima do tesouro
ANIMAIS = ("peixes", "carros", "pessoas", "todos")
TEMAS = os.path.join(D, "temas")
TEMA_ATUAL = os.path.join(TEMAS, "atual.cpt")
NOMES_TEMAS = {"classico": "Clássico", "vulcao": "Vulcão", "inverno": "Inverno", "marte": "Marte",
               "deserto": "Deserto", "oceano": "Oceano", "arco-iris": "Arco-íris"}


class Erro(Exception):
    """Falha com mensagem para mostrar ao usuário."""


# ---------- pasta Areia ----------

PASTA_WILLBERT = "Fotos do Willbert"


def pasta():
    """A pasta das fotos, dentro da pasta Imagens do usuário (criada se não existir).
    Desde 13/11/2026 tudo é feito pelo painel; a Área de Trabalho não tem mais as pastas."""
    p = os.environ.get("AREIA_PASTA")
    if not p:
        try:
            imgs = subprocess.run(["xdg-user-dir", "PICTURES"], capture_output=True, text=True,
                                  timeout=5).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            imgs = ""
        casa = os.path.expanduser("~")
        p = os.path.join(imgs if imgs and imgs != casa else os.path.join(casa, "Pictures"), PASTA_FOTOS)
    os.makedirs(p, exist_ok=True)
    return p


def pasta_willbert():
    """Subpasta para fotos tiradas pelo Willbert ou pela câmera do laboratório."""
    p = os.path.join(pasta(), PASTA_WILLBERT)
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
        if os.path.commonpath([os.path.abspath(caminho), os.path.abspath(pasta())]) == os.path.abspath(pasta()):
            return caminho
        return guarda(caminho)
    achadas = [c for c in lista if pedido.casefold() in os.path.basename(c).casefold()]
    if len(achadas) == 1:
        return achadas[0]
    pw = pasta_willbert()
    if os.path.isdir(pw):
        achadas_w = [os.path.join(pw, f) for f in sorted(os.listdir(pw), key=_ordem)
                     if f.lower().endswith(EXT) and pedido.casefold() in f.casefold()]
        if len(achadas_w) == 1:
            return achadas_w[0]
        if achadas_w:
            return max(achadas_w, key=os.path.getmtime)
    if achadas:
        raise Erro(f'mais de uma imagem com "{pedido}": use o número')
    raise Erro(f'nenhuma imagem com "{pedido}"')


def guarda(caminho, nome=None, pasta_destino=None):
    """Copia um arquivo para a pasta das fotos (ou subpasta indicada)."""
    if not caminho.lower().endswith(EXT) and not (nome or "").lower().endswith(EXT):
        raise Erro("formato não suportado: use .jpg, .png, .webp ou .bmp (HEIC do iPhone: converta antes)")
    p = pasta_destino or pasta()
    os.makedirs(p, exist_ok=True)
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


def arquiva_willbert():
    """Garante que fotos do Willbert fiquem organizadas na subpasta Fotos do Willbert."""
    p = pasta()
    pw = pasta_willbert()
    soltas = [f for f in os.listdir(p) if f.startswith("willbert-") and os.path.isfile(os.path.join(p, f))]
    for f in soltas:
        try:
            os.replace(os.path.join(p, f), os.path.join(pw, f))
        except OSError:
            pass
    return len(soltas)


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

_qual_cache = [0.0, None]  # (quando, resposta): o painel pergunta várias vezes por consulta


def _sandbox(*args, timeout=60):
    try:
        r = subprocess.run([SANDBOX, *args], capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        _qual_cache[0] = 0.0
        return 1, "a caixa de areia demorou demais para responder"
    if args[:1] != ("estado",):
        _qual_cache[0] = 0.0  # qualquer outro comando pode ter trocado o programa
    return r.returncode, (r.stdout + r.stderr).strip()


def qual():
    """original, rosto ou nenhum. A resposta vale por 1 s (cada pergunta abre um processo)."""
    agora = time.monotonic()
    if _qual_cache[1] is None or agora - _qual_cache[0] > 1.0:
        _qual_cache[:] = [agora, _sandbox("estado", timeout=10)[1] or "nenhum"]
    return _qual_cache[1]


def _comando(*linhas):
    """Manda um ou mais comandos ao SARndbox numa escrita só (cada um custava ~0,5 s separado)."""
    rc, out = _sandbox("cmd", *linhas, timeout=10)
    if rc != 0:
        raise Erro(out or "o SARndbox não aceitou o comando")


def aplica(e):
    """Põe o projetor no modo de e (sobe a versão com Foto se precisar) e salva o estado."""
    if e["modo"] == "original":  # topografia: relevo + efeito do tema (o original só se a cópia falhar)
        rc, out = _sandbox("topografia")
        if rc != 0 or qual() == "nenhum":
            raise Erro("a topografia não abriu: " + out)
        salva(e)
        _volta_tema()  # se o jogo do vulcão estava ligado, o tema de antes volta
        return
    if e["modo"] in ("rosto", "esculpir") and not os.path.exists(os.path.join(FOTOS, "rosto.png")):
        raise Erro("ainda não há imagem: escolha uma")
    if qual() != "rosto":
        rc, out = _sandbox("rosto")
        if rc != 0:
            raise Erro("o modo foto não abriu; a topografia voltou")
    a = e["ajuste"]
    parar = _encerra_outras("foto") if e["modo"] in ("rosto", "esculpir") else ()
    if e["modo"] == "rosto":
        _comando(*parar, f"rosto {os.path.join(FOTOS, 'rosto.png')} {e['giro']} {int(e['espelho'])}",
                 f"rostoAjuste {a['zoom']} {a['desloc']} {a['mistura']} {a['sombra']}")
    elif e["modo"] == "esculpir":
        _comando(*parar, f"esculpir {os.path.join(FOTOS, 'rosto.dem')}")
    else:
        _comando("relevo")
    salva(e)
    if parar:
        _volta_tema()


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
    """Foto pela câmera do laboratório (comando liberado do Willbert), guardada na pasta Fotos do Willbert."""
    r = subprocess.run(["sudo", "-n", "/usr/local/sbin/willbert-controle", "foto"],
                       capture_output=True, text=True, timeout=90)
    origem = os.path.join(FOTOS, "camera.jpg")
    if r.returncode != 0 or not os.path.isfile(origem):
        raise Erro("não consegui a foto da câmera (o Willbert está configurado?)")
    return guarda(origem, time.strftime("camera-%Y-%m-%d-%H%M%S.jpg"), pasta_destino=pasta_willbert())


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
    if t != "vulcao":
        e.pop("tema_antes_do_vulcao", None)  # quem escolheu um tema manda nele
    salva(e)
    return t


# ---------- brincadeiras: caça ao tesouro, peixes e carrinhos ----------

# Uma brincadeira por vez: começar uma encerra as outras (antes elas se acumulavam: peixes,
# carrinhos, labirinto e fóssil juntos; e os animais "voltavam" depois da foto). O tema fica.
PARAR_BRINCADEIRAS = {"tesouro": ("fossilFim", "labirintoFim", "jogoFim", "animais nada", "copiaFim"),
                      "fossil": ("tesouroFim", "labirintoFim", "jogoFim", "animais nada", "copiaFim"),
                      "labirinto": ("tesouroFim", "fossilFim", "jogoFim", "animais nada", "copiaFim"),
                      "jogo": ("tesouroFim", "fossilFim", "labirintoFim", "animais nada", "copiaFim"),
                      "animais": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "copiaFim"),
                      "membros": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "copiaFim"),
                      "mar": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "copiaFim"),
                      "mundo": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "copiaFim"),
                      "copia": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "animais nada"),
                      "foto": ("tesouroFim", "fossilFim", "labirintoFim", "jogoFim", "animais nada", "copiaFim")}


def _encerra_outras(qual_brincadeira):
    return PARAR_BRINCADEIRAS.get(qual_brincadeira, ())


def _brincadeira(comando):
    """Brincadeiras rodam na versão com Foto (sobre o relevo colorido); sobe ela se precisar."""
    if qual() != "rosto":
        rc, _ = _sandbox("rosto")
        if rc != 0:
            raise Erro("a caixa de areia não abriu o modo brincadeira; a topografia voltou")
    _comando(*_encerra_outras(comando.split()[0]), comando)
    e = estado()
    e["modo"] = "relevo"
    salva(e)
    if comando != "jogo vulcao" and not comando.startswith("jogo vulcao "):
        _volta_tema()


def _volta_tema():
    """O jogo do vulcão troca o tema para vulcão (lava); terminado, volta o tema que estava."""
    e = estado()
    antes = e.pop("tema_antes_do_vulcao", None)
    if not antes:
        return
    salva(e)
    if tema_atual() == "vulcao":
        try:
            escolhe_tema(antes)
        except Erro:
            pass


def tesouro(dificuldade="normal"):
    """Esconde um tesouro num lugar sorteado, embaixo da areia. Devolve a profundidade (cm)."""
    prof = _profundidade(dificuldade)
    _brincadeira(f"tesouro {prof:g}")
    return prof


_PREFIXOS_REGISTRO = ("Tesouro", "Fossil", "Labirinto", "Animais", "Jogo", "Mundo", "Copia", "Modo Rosto:", "Modo Esculpir:")
_registro_cache = {"pos": 0, "linhas": [], "resto": b"", "rabo": b""}
_registro_trava = threading.Lock()  # o painel atende várias consultas ao mesmo tempo


def _registro_caca():
    """Linhas do registro do programa da areia sobre as brincadeiras (tesouro, fósseis, labirinto,
    animais e jogos). Lê só o que foi escrito desde a última vez: o painel pergunta a cada poucos
    segundos e o registro cresce o dia todo (antes o arquivo inteiro era relido 6x por consulta).
    O registro recomeça do zero quando a areia reabre: confere os últimos bytes já lidos e,
    se mudaram, lê tudo de novo."""
    with _registro_trava:
        return _le_registro()


def _le_registro():
    c = _registro_cache
    try:
        with open(os.path.join(D, "sandbox.log"), "rb") as f:
            tam = os.fstat(f.fileno()).st_size
            n = len(c["rabo"])
            if tam >= c["pos"] and n:
                f.seek(c["pos"] - n)
                mesmo = f.read(n) == c["rabo"]
            else:
                mesmo = tam >= c["pos"] and c["pos"] == 0
            if not mesmo:
                c.update(pos=0, linhas=[], resto=b"", rabo=b"")
            f.seek(c["pos"])
            novo = f.read()
    except OSError:
        c.update(pos=0, linhas=[], resto=b"", rabo=b"")
        return []
    if novo:
        c["pos"] += len(novo)
        c["rabo"] = (c["rabo"] + novo)[-64:]
        partes = (c["resto"] + novo).split(b"\n")
        c["resto"] = partes.pop()  # linha ainda sendo escrita
        for l in partes:
            t = l.decode("utf-8", "replace") + "\n"
            if t.startswith(_PREFIXOS_REGISTRO):
                c["linhas"].append(t)
        del c["linhas"][:-4000]
    return list(c["linhas"])


def _situacao(prefixo):
    """escondido, achado ou nenhum. Começar outra brincadeira (fóssil x tesouro) encerra esta."""
    if qual() != "rosto":
        return "nenhum"
    linhas = _registro_caca()
    if not linhas or not linhas[-1].startswith(prefixo) or linhas[-1].startswith(prefixo + " desligado"):
        return "nenhum"
    return "achado" if linhas[-1].startswith(prefixo + " encontrado") else "escondido"


def _achados(prefixo):
    linhas = _registro_caca()
    inicio = max((i for i, l in enumerate(linhas) if l.startswith(prefixo + ": escondendo")), default=-1)
    return sum(1 for l in linhas[inicio + 1:] if l.startswith(prefixo + " encontrado"))


def tesouro_situacao():
    return _situacao("Tesouro")


def tesouro_achados():
    """Quantos tesouros acharam desde que a caça começou (cada um reaparece em outro lugar)."""
    return _achados("Tesouro")


def tesouro_fim():
    if qual() == "rosto":
        _comando("tesouroFim")


# ---------- escavação de fósseis (o mesmo mecanismo do tesouro, com esqueletos) ----------

FOSSEIS = {"trex": (2, "Tiranossauro Rex"), "triceratops": (3, "Triceratops"),
           "pterodactilo": (4, "Pterodáctilo"), "pegada": (5, "Pegada Gigante")}
_APELIDOS_FOSSIL = {"t-rex": "trex", "tiranossauro": "trex", "rex": "trex", "trike": "triceratops",
                    "ptero": "pterodactilo", "pterodactil": "pterodactilo", "pterossauro": "pterodactilo",
                    "pegadas": "pegada", "rastro": "pegada", "pe": "pegada"}


def _profundidade(dificuldade):
    d = _chave_tema(str(dificuldade))
    prof = DIFICULDADE.get(d)
    if prof is None:
        try:
            prof = float(d.replace(",", "."))
        except ValueError:
            raise Erro("dificuldade: facil, normal ou dificil (ou a profundidade em cm)")
    if not 1.0 <= prof <= 10.0:
        raise Erro("a profundidade tem que ser entre 1 e 10 cm")
    return prof


def fossil(tipo="trex", dificuldade="normal"):
    """Enterra um fóssil num lugar sorteado. "facil"/"dificil" sozinho vale como dificuldade do T-Rex.
    Devolve (nome do dinossauro, profundidade em cm)."""
    t = _chave_tema(str(tipo))
    if t in DIFICULDADE or t.replace(",", "").replace(".", "").isdigit():
        t, dificuldade = "trex", tipo
    t = _APELIDOS_FOSSIL.get(t, t)
    if t not in FOSSEIS:
        raise Erro("dinossauro: trex, triceratops, pterodactilo ou pegada")
    prof = _profundidade(dificuldade)
    num, nome = FOSSEIS[t]
    _brincadeira(f"fossil {num} {prof:g}")
    return nome, prof


def fossil_situacao():
    return _situacao("Fossil")


def fossil_achados():
    return _achados("Fossil")


def fossil_fim():
    if qual() == "rosto":
        _comando("fossilFim")


def animais(tipo, quantidade=None):
    t = _chave_tema(str(tipo))
    t = {"peixe": "peixes", "carro": "carros", "carrinhos": "carros", "carrinho": "carros", "tudo": "todos",
         "pessoa": "pessoas", "gente": "pessoas", "pessoinhas": "pessoas", "bonecos": "pessoas"}.get(t, t)
    if t not in ANIMAIS:
        raise Erro("escolha: peixes, carros, pessoas ou todos")
    if not quantidade and t in ("peixes", "todos"):
        # sem quantidade: a variedade toda (antes "peixes" e "todos" só soltavam peixe comum)
        sel = mar({k: (n if t == "todos" or k in MARINHOS else 0) for k, n in MAR_PADRAO.items()})
        return t, sum(sel.values())
    n = int(quantidade) if quantidade else (15 if t == "todos" else 10)
    if not 1 <= n <= 24:
        raise Erro("quantidade: de 1 a 24")
    _brincadeira(f"animais {t} {n}")
    return t, n


def animais_situacao():
    """peixes, carros, pessoas, todos ou nada (último pedido no registro; outra brincadeira tira os animais)."""
    if qual() != "rosto":
        return "nada"
    linhas = [l for l in _registro_caca() if l.startswith(("Animais", "Tesouro: escondendo", "Fossil: escondendo",
                                                          "Labirinto: nascente", "Jogo pedido", "Modo Rosto:",
                                                          "Modo Esculpir:"))]
    if not linhas or not linhas[-1].startswith("Animais: "):
        return "nada"
    t = linhas[-1].split()[1]
    return t if t in ANIMAIS or t in ("membros", "mundo", "mar") else "nada"


# ---------- animais marinhos à escolha (nadam no lago e na água da chuva) ----------

# nome do comando: (rótulo, figurinha)
ESPECIES = {"peixe": ("Peixes", "🐟"), "tubarao": ("Tubarões", "🦈"), "tartaruga": ("Tartarugas", "🐢"),
            "aguaviva": ("Águas-vivas", "🪼"), "polvo": ("Polvos", "🐙"), "arraia": ("Arraias", "🐠"),
            "pato": ("Patos", "🦆"), "barco": ("Barquinhos", "⛵"),
            "carro": ("Carrinhos", "🚗"), "pessoa": ("Pessoas", "🚶"), "cachorro": ("Cachorros", "🐕"),
            "passaro": ("Pássaros", "🐦")}
# onde cada um vive (só para agrupar no painel)
GRUPOS = {"peixe": "Na água", "tubarao": "Na água", "tartaruga": "Na água", "aguaviva": "Na água", "polvo": "Na água",
          "arraia": "Na água", "pato": "Na água", "barco": "Na água", "carro": "Em terra", "pessoa": "Em terra",
          "cachorro": "Em terra", "passaro": "No ar"}
SINGULAR = {"peixe": "peixe", "tubarao": "tubarão", "tartaruga": "tartaruga", "aguaviva": "água-viva", "polvo": "polvo",
            "arraia": "arraia", "pato": "pato", "barco": "barquinho", "carro": "carrinho", "pessoa": "pessoa",
            "cachorro": "cachorro", "passaro": "pássaro"}


def conta_bichos(sel):
    """{"peixe": 8, "tubarao": 1} -> "8 peixes, 1 tubarão" (singular quando é um só)."""
    return ", ".join(f"{n} {SINGULAR[k] if n == 1 else ESPECIES[k][0].lower()}" for k, n in sel.items() if n)


MARINHOS = ("peixe", "tubarao", "tartaruga", "aguaviva", "polvo", "arraia", "pato", "barco")
MAR_PADRAO = {"peixe": 6, "tubarao": 1, "tartaruga": 2, "aguaviva": 2, "polvo": 1, "arraia": 1, "pato": 2, "barco": 1,
              "carro": 4, "pessoa": 4, "cachorro": 2, "passaro": 3}
MAX_BICHOS = 32


def mar_escolhido():
    """A última escolha de animais marinhos (ou a padrão)."""
    e = estado().get("mar")
    sel = dict(MAR_PADRAO)
    if isinstance(e, dict):
        sel = {k: max(0, min(MAX_BICHOS, int(e.get(k) or 0))) for k in ESPECIES}
    return sel


def mar(selecao=None):
    """Solta na areia o que foi escolhido: {"peixe": 8, "tubarao": 1, "carro": 3, "pessoa": 5, ...}
    (animais marinhos, carrinhos e pessoas). Sem escolha, a última. Devolve a escolha usada (o total
    é limitado a 32)."""
    sel = mar_escolhido() if selecao is None else {}
    if selecao is not None:
        for k in ESPECIES:
            try:
                sel[k] = max(0, min(MAX_BICHOS, int(selecao.get(k) or 0)))
            except (TypeError, ValueError, AttributeError):
                raise Erro("quantidade inválida")
    falta = MAX_BICHOS
    for k in ESPECIES:   # corta o que passar de 32, na ordem da lista
        sel[k] = min(sel[k], falta)
        falta -= sel[k]
    if sum(sel.values()) == 0:
        raise Erro("escolha pelo menos um animal")
    _brincadeira("mar " + " ".join(f"{k}={n}" for k, n in sel.items() if n))
    e = estado()
    e["mar"] = sel
    salva(e)
    return sel


# ---------- chuva pelo painel (além da mão aberta sobre a areia) ----------

def chuva(fx, fy, segundos=8):
    """Uma nuvem joga água no ponto (fx, fy), de 0 a 1 dentro da área iluminada (0, 0 = canto de
    baixo, à esquerda, de quem olha a imagem do projetor)."""
    try:
        fx, fy, segundos = float(fx), float(fy), float(segundos)
    except (TypeError, ValueError):
        raise Erro("ponto inválido")
    if not (0 <= fx <= 1 and 0 <= fy <= 1) or not 1 <= segundos <= 60:
        raise Erro("ponto fora da areia")
    if qual() != "rosto":
        raise Erro("a areia não está aberta")
    _comando(f"chuva {fx:.3f} {fy:.3f} {segundos:g}")


def comida(fx, fy, segundos=20):
    """Joga comida no ponto (fx, fy) da área iluminada: os pássaros pousam em volta e os cachorros
    sentam do lado (a mão de verdade só faz chover)."""
    try:
        fx, fy, segundos = float(fx), float(fy), float(segundos)
    except (TypeError, ValueError):
        raise Erro("ponto inválido")
    if not (0 <= fx <= 1 and 0 <= fy <= 1) or not 1 <= segundos <= 60:
        raise Erro("ponto fora da areia")
    if qual() != "rosto":
        raise Erro("a areia não está aberta")
    _comando(f"comida {fx:.3f} {fy:.3f} {segundos:g}")


def secar():
    """Tira toda a água da areia (a da chuva e a dos jogos)."""
    if qual() != "rosto":
        raise Erro("a areia não está aberta")
    _comando("secar")


# ---------- membros do laboratório andando na areia (quem o Willbert está vendo) ----------

STATUS_WILLBERT = os.environ.get("AREIA_WILLBERT_STATUS") or "/dev/shm/visao/status.json"
_membros_mandados = [None]


def _nome_curto(nome):
    """Primeiro nome, sem acento, em maiúsculas, só letras, até 8 (o que cabe na plaquinha)."""
    t = unicodedata.normalize("NFKD", str(nome)).encode("ascii", "ignore").decode().upper()
    t = re.sub(r"[^A-Z ]", "", t).split()
    return t[0][:8] if t else ""


def membros_presentes():
    """Nomes de quem o Willbert considera presente agora (na ordem de chegada). Lista vazia se o
    Willbert está desligado, parado há mais de 90 s ou não viu ninguém."""
    try:
        with open(STATUS_WILLBERT) as f:
            st = json.load(f)
        if time.time() - float(st.get("ts") or 0) > 90:
            return []
        pres = sorted(st.get("present") or [], key=lambda p: p.get("since") or 0)
    except (OSError, ValueError, TypeError, AttributeError):
        return []
    nomes = []
    for p in pres:
        n = _nome_curto(p.get("name") or "") if isinstance(p, dict) else ""
        if n and n not in nomes:
            nomes.append(n)
    return nomes[:8]


PASTA_MEMBROS = os.environ.get("AREIA_MEMBROS") or os.path.join(D, "membros")
PAINEL_ROSTOS = os.path.join(FOTOS, "membros.rgba")
MEMBROS_PY = os.path.join(D, "membros.py")


def membros_com_foto(refaz=None):
    """Nomes dos membros que têm foto em membros/ (Nome.jpg ou .png), na ordem do painel de rostos.
    O painel (fotos/membros.rgba) é refeito quando a pasta muda. Devolve (nomes, refez)."""
    lista = os.path.join(FOTOS, "membros.json")
    try:
        fotos = [os.path.join(PASTA_MEMBROS, f) for f in os.listdir(PASTA_MEMBROS) if f.lower().endswith(EXT)]
    except OSError:
        fotos = []
    if not fotos:
        return [], False
    novo = max([os.path.getmtime(f) for f in fotos] + [os.path.getmtime(PASTA_MEMBROS)])
    velho = os.path.getmtime(lista) if os.path.exists(lista) and os.path.exists(PAINEL_ROSTOS) else 0
    refez = False
    if refaz or novo > velho:
        r = subprocess.run([PY, MEMBROS_PY], capture_output=True, text=True, timeout=120,
                           env={**os.environ, "AREIA_MEMBROS": PASTA_MEMBROS, "AREIA_DIR": D})
        if r.returncode != 0:
            raise Erro("não consegui montar os rostos dos membros: " + (r.stderr.strip().splitlines() or ["erro"])[-1])
        refez = True
    try:
        with open(lista) as f:
            return [str(n) for n in json.load(f).get("nomes", [])][:8], refez
    except (OSError, ValueError, AttributeError):
        return [], refez


def membros_lista():
    """(nomes na ordem do comando, quantos têm rosto, presentes). Primeiro quem tem foto (aparece
    sempre, esteja ou não no laboratório); depois quem o Willbert vê e não tem foto (só o nome)."""
    com_foto, _ = membros_com_foto()
    presentes = membros_presentes()
    nomes = com_foto + [n for n in presentes if n not in com_foto]
    return nomes[:8], len(com_foto), [n for n in nomes[:8] if n in presentes]


def _comando_membros(nomes, n_fotos, presentes, com_painel):
    return " ".join(["membros", str(estado()["giro"] // 90), str(n_fotos), PAINEL_ROSTOS if com_painel and n_fotos else "-",
                     *[n + ("+" if n in presentes else "") for n in nomes]])


def membros():
    """Liga as pessoinhas dos membros: todos os que têm foto (com o rosto), mais quem o Willbert
    está vendo. Devolve (nomes, presentes)."""
    membros_com_foto()   # refaz o painel de rostos se a pasta mudou
    nomes, n_fotos, presentes = membros_lista()
    _brincadeira(_comando_membros(nomes, n_fotos, presentes, True))
    _membros_mandados[0] = (nomes, presentes)
    return nomes, presentes


def membros_atualiza():
    """Chamado de tempos em tempos (painel): no modo membros ou no mundo LAOB, se mudou quem está
    presente (ou as fotos), manda a lista nova. Devolve True se mandou."""
    sit = animais_situacao()
    if sit not in ("membros", "mundo"):
        _membros_mandados[0] = None
        return False
    _, refez = membros_com_foto()
    nomes, n_fotos, presentes = membros_lista()
    if (nomes, presentes) == _membros_mandados[0] and not refez:
        return False
    com_painel = refez or _membros_mandados[0] is None
    _comando(_comando_mundo(nomes, n_fotos, presentes, com_painel) if sit == "mundo"
             else _comando_membros(nomes, n_fotos, presentes, com_painel))
    _membros_mandados[0] = (nomes, presentes)
    return True


# ---------- mundo LAOB: o laboratório por dentro, com os membros (Mundo.cpp) ----------

MORADORES_DO_MUNDO = 0   # (a cidade com moradores sem nome saiu em 16/11/2026: ficou só o laboratório)


def _comando_mundo(nomes, n_fotos, presentes, com_painel):
    return " ".join(["mundo", str(estado()["giro"] // 90), str(n_fotos), PAINEL_ROSTOS if com_painel and n_fotos else "-",
                     str(MORADORES_DO_MUNDO), *[n + ("+" if n in presentes else "") for n in nomes]])


def mundo():
    """Liga o mundo LAOB: a planta do laboratório, com os membros lá dentro. Quem o Willbert está
    vendo fica na sala; os outros entram pela porta de tempos em tempos. Devolve (nomes, presentes)."""
    membros_com_foto()   # refaz o painel de rostos se a pasta mudou
    nomes, n_fotos, presentes = membros_lista()
    _brincadeira(_comando_mundo(nomes, n_fotos, presentes, True))
    _membros_mandados[0] = (nomes, presentes)
    return nomes, presentes


def mundo_situacao():
    """O que cada membro está fazendo no mundo LAOB: lista de {nome, fazendo, presente}, na ordem
    dos bonecos. Vazia se o mundo não está ligado."""
    if animais_situacao() != "mundo":
        return []
    linhas = _registro_caca()
    inicio = max((i for i, l in enumerate(linhas) if l.startswith("Animais: mundo")), default=0)
    fazendo = {}
    for l in linhas[inicio:]:
        m = re.match(r"Mundo: ([A-Z]+) (foi .+|está .+|entrou .+|saiu .+)$", l.strip())
        if m:
            fazendo[m.group(1)] = m.group(2)
    nomes, _, presentes = membros_lista()
    return [{"nome": n.capitalize(), "fazendo": fazendo.get(n, "chegando"), "presente": n in presentes}
            for n in nomes]


def mundo_rosto(nome):
    """Mostra por uns segundos o rosto do membro num balão acima do boneco dele."""
    if animais_situacao() != "mundo":
        raise Erro("o mundo LAOB não está ligado")
    n = _nome_curto(nome)
    if not n or n not in membros_lista()[0]:
        raise Erro("membro não encontrado")
    _comando(f"mundoRosto {n}")
    return n


def animais_fim():
    if qual() == "rosto":
        _comando("animais nada")


# ---------- labirinto de água: uma nascente joga água; cavem canais até ela chegar ao alvo ----------

LABIRINTO = {"facil": "Fácil (alvo perto, 2 armadilhas)", "normal": "Normal (lados opostos, 3 armadilhas)",
             "dificil": "Difícil (cantos opostos, menos água, 4 armadilhas)"}
_APELIDOS_LABIRINTO = {"1": "facil", "2": "normal", "3": "dificil", "medio": "normal"}


def labirinto(dificuldade="normal"):
    """Começa o labirinto (nascente e alvo sorteados). Devolve a dificuldade usada."""
    d = _chave_tema(str(dificuldade or "normal"))
    d = _APELIDOS_LABIRINTO.get(d, d)
    if d not in LABIRINTO:
        raise Erro("dificuldade: facil, normal ou dificil")
    _brincadeira(f"labirinto {d}")
    return d


def _registro_labirinto():
    return _registro_caca()


def labirinto_situacao():
    """(situação, rodadas vencidas): situação = correndo, chegou ou nenhum."""
    if qual() != "rosto":
        return "nenhum", 0
    linhas = _registro_labirinto()
    inicio = max((i for i, l in enumerate(linhas) if l.startswith("Labirinto: nascente")), default=-1)
    if inicio < 0:
        return "nenhum", 0
    depois = linhas[inicio:]
    vencidas = sum(1 for l in depois if l.startswith("Labirinto: a água chegou"))
    ultima = depois[-1]
    if not ultima.startswith("Labirinto") or ultima.startswith("Labirinto desligado"):
        return "nenhum", vencidas
    return ("chegou" if ultima.startswith("Labirinto: a água chegou") else "correndo"), vencidas


def labirinto_novo():
    """Rodada nova (outra nascente e outro alvo) sem esperar."""
    if labirinto_situacao()[0] == "nenhum":
        raise Erro("o labirinto não está ligado (para começar: areia-labirinto)")
    _comando("labirintoNovo")


def labirinto_fim():
    if qual() == "rosto":
        _comando("labirintoFim")


# ---------- jogos de água (Jogos.cpp): a água da simulação decide quem ganha ----------

# nome: (título, descrição curta, o que fazer)
JOGOS = {
    "corrida": ("Corrida de rios", "Dois times, duas nascentes e duas bandeiras.",
                "Cada time cava o canal da sua nascente até a sua bandeira (azul ou vermelha). "
                "A água é pouca e não chega sozinha: ganha quem abrir o melhor caminho. "
                "Dá para atrapalhar o outro time com diques!"),
    "vila": ("Salve a vila", "A enchente vem de três lados; proteja as casinhas.",
             "A água começa a descer na hora, de três bordas diferentes: "
             "levantem diques e cavem valas em volta das casas. No canto da areia, uma fila de "
             "casinhas mostra quantas continuam de pé (branca) e quantas alagaram (vermelha)."),
    "rota": ("Rota com paradas", "A água tem que passar pelos portões 1, 2, 3… na ordem.",
             "Os portões ficam espalhados pela areia toda, em ordem sorteada: o caminho vira um "
             "zigue-zague. O que pisca amarelo é o próximo. Levem a água por todos, na ordem, até a "
             "bandeira. Pular um portão não vale."),
    "vulcao": ("Vulcão em erupção", "Erupções em pulsos e bocas de lava que abrem de surpresa.",
               "É um jogo de reagir: a cratera, no ponto mais alto, cospe lava em pulsos, e durante a "
               "erupção abrem bocas novas em lugares sorteados. Um aviso piscando mostra onde a próxima "
               "vai abrir, 6 segundos antes: corram para proteger as casas daquele lado. A fila de "
               "casinhas no canto mostra as que estão de pé (brancas) e as perdidas (vermelhas)."),
}
# Tirados em 14/11/2026 a pedido do Victor (repetitivos): fazenda, lago, barco e resgate.
NIVEIS_JOGO = {"facil": 1, "normal": 2, "dificil": 3, "1": 1, "2": 2, "3": 3, "medio": 2}


def jogo(nome, nivel="normal"):
    """Começa um jogo de água. Devolve (nome, nível 1-3)."""
    n = _chave_tema(str(nome or ""))
    n = {"corrida-de-rios": "corrida", "rios": "corrida", "salve-a-vila": "vila", "enchente": "vila",
         "portoes": "rota", "paradas": "rota", "lava": "vulcao", "erupcao": "vulcao"}.get(n, n)
    if n not in JOGOS:
        raise Erro("jogo: " + ", ".join(JOGOS))
    lv = NIVEIS_JOGO.get(_chave_tema(str(nivel or "normal")))
    if lv is None:
        raise Erro("nível: facil, normal ou dificil")
    # (o vulcão não troca mais o tema: o próprio jogo escurece a areia e pinta a água de lava)
    _brincadeira(f"jogo {n} {lv}")
    return n, lv


def jogo_situacao():
    """Dicionário com o jogo atual: nome, fase (correndo, fim ou nenhum), rodadas, último aviso."""
    vazio = {"jogo": None, "fase": "nenhum", "rodadas": 0, "aviso": "", "casas": 0, "perdidas": 0}
    if qual() != "rosto":
        return vazio
    linhas = _registro_caca()
    inicio = max((i for i, l in enumerate(linhas) if l.startswith("Jogo pedido:")), default=-1)
    if inicio < 0:
        return vazio
    depois = [l.strip() for l in linhas[inicio:]]
    if any(l.startswith(("Jogo desligado", "Tesouro: escondendo", "Fossil: escondendo", "Labirinto: nascente",
                         "Modo Rosto:", "Modo Esculpir:")) for l in depois[1:]):
        return vazio
    nome = depois[0].split(":", 1)[1].split("(")[0].strip()
    rodadas = sum(1 for l in depois if l.startswith("Jogo: fim da rodada"))
    avisos = [l[len("Jogo: "):] for l in depois if l.startswith("Jogo: ") and not l.startswith("Jogo: teste")]
    ult = avisos[-1] if avisos else ""
    fase = "fim" if ult.startswith("fim da rodada") else "correndo"
    # vila e vulcão: casas da rodada atual e quantas já foram perdidas
    casas = perdidas = 0
    for l in depois:
        m = re.search(r"\((\d+) casas\)", l)
        if m and l.startswith("Jogo: "):
            casas, perdidas = int(m.group(1)), 0   # começo de rodada
        elif l.startswith(("Jogo: uma casa alagou", "Jogo: a lava pegou uma casa")):
            perdidas += 1
    return {"jogo": nome, "fase": fase, "rodadas": rodadas, "aviso": ult.replace("fim da rodada; ", ""),
            "casas": casas, "perdidas": min(perdidas, casas)}


def jogo_novo():
    if jogo_situacao()["jogo"] is None:
        raise Erro("nenhum jogo de água ligado")
    _comando("jogoNovo")


def jogo_fim():
    if qual() == "rosto":
        _comando("jogoFim")
    _volta_tema()


# ---------- copie o relevo: o projetor mostra um relevo-alvo; a nota diz quanto já foi feito ----------

FORMAS = {"montanha": "Montanha", "duas": "Duas montanhas", "vulcao": "Vulcão", "cratera": "Cratera",
          "ilha": "Ilha e lago", "serra": "Serra"}


def copia(forma="sorteio", nivel="normal"):
    """Começa o Copie o relevo. forma: uma de FORMAS ou "sorteio". Devolve (forma, nível 1-3)."""
    f = _chave_tema(str(forma or "sorteio"))
    f = {"duas-montanhas": "duas", "ilha-e-lago": "ilha", "lago": "ilha", "aleatorio": "sorteio"}.get(f, f)
    if f != "sorteio" and f not in FORMAS:
        raise Erro("forma: sorteio, " + ", ".join(FORMAS))
    lv = NIVEIS_JOGO.get(_chave_tema(str(nivel or "normal")))
    if lv is None:
        raise Erro("nível: facil, normal ou dificil")
    _brincadeira(f"copia {f} {lv}")
    return f, lv


def copia_situacao():
    """{"forma", "nota" (0-100), "conseguiu" (bool), "rodadas", "alvo" ([x, y, altura cm, largura] por morro,
    x/y/largura em fração da área iluminada)} ou forma None se o jogo não está ligado."""
    vazio = {"forma": None, "nota": 0, "conseguiu": False, "rodadas": 0, "alvo": []}
    if qual() != "rosto":
        return vazio
    linhas = _registro_caca()
    inicio = max((i for i, l in enumerate(linhas) if l.startswith("Copia: forma")), default=-1)
    if inicio < 0:
        return vazio
    depois = [l.strip() for l in linhas[inicio:]]
    if any(l.startswith(("Copia desligada", "Tesouro: escondendo", "Fossil: escondendo", "Labirinto: nascente",
                         "Jogo pedido", "Animais:", "Modo Rosto:", "Modo Esculpir:")) for l in depois[1:]):
        return vazio
    m = re.match(r"Copia: forma (.+) \(nível", depois[0])
    nota, conseguiu, alvo = 0, False, []
    for l in depois[1:]:
        if l.startswith("Copia: alvo"):
            try:
                alvo = [[float(v) for v in g.split(",")] for g in l.split()[2:]]
            except ValueError:
                alvo = []
            continue
        n = re.match(r"Copia: (?:conseguiu! )?(\d+)%", l)
        if n:
            nota, conseguiu = int(n.group(1)), l.startswith("Copia: conseguiu")
    rodadas = sum(1 for l in linhas if l.startswith("Copia: conseguiu"))
    return {"forma": m.group(1) if m else "?", "nota": nota, "conseguiu": conseguiu, "rodadas": rodadas, "alvo": alvo}


def copia_nova():
    if copia_situacao()["forma"] is None:
        raise Erro("o Copie o relevo não está ligado")
    _comando("copiaNova")


def para_tudo():
    """Encerra todas as brincadeiras de uma vez (botão "Parar brincadeiras")."""
    if qual() == "rosto":
        _comando("tesouroFim", "fossilFim", "animais nada", "labirintoFim", "jogoFim", "copiaFim")
    _volta_tema()


def reabre():
    """Fecha a areia (mesmo travada) e abre a topografia de novo. Botão "Reabrir a areia" do painel."""
    _sandbox("original")      # o sandbox.sh fecha o que estiver aberto antes de abrir o outro
    rc, out = _sandbox("topografia")
    if rc != 0 or qual() == "nenhum":
        raise Erro("a areia não abriu: " + out)
    e = estado()
    e["modo"] = "original"
    salva(e)


def marca_topografia():
    """Só anota no estado que o projetor está na topografia (usado pelo areia-topografia)."""
    e = estado()
    e["modo"] = "original"
    salva(e)


if __name__ == "__main__":
    import sys
    if sys.argv[1:] == ["marca-topografia"]:
        marca_topografia()
    elif sys.argv[1:] == ["volta-tema"]:
        _volta_tema()
    else:
        sys.exit("uso: python3 areia.py marca-topografia|volta-tema")
