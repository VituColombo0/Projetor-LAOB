"""Painel web da caixa de areia (porta 8090): foto, modos, cores e brincadeiras.
Responde à própria máquina, à rede local (10/8, 172.16/12, 192.168/16: aberto pelo Victor em
2026-11-11 para quem está no laboratório, com o endereço no papel de parede) e à Tailscale.
Rodar com o venv da pasta:  venv/bin/python painel.py
"""
import hashlib
import ipaddress
import json
import os
import re
import subprocess
import tempfile
import threading
import time

from flask import Flask, Response, abort, jsonify, request, send_file

import areia
import som

som.iniciar_vigia()

PORTA = int(os.environ.get("AREIA_PORTA", "8090"))
PERMITIDAS = [ipaddress.ip_network(n) for n in
              ("127.0.0.0/8", "::1/128", "10.0.0.0/8", "172.16.0.0/12",
               "192.168.0.0/16", "100.64.0.0/10", "fd7a:115c:a1e0::/48")]
app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 25 * 1024 * 1024
trava = threading.Lock()


@app.before_request
def valida_rede():
    try:
        ip = ipaddress.ip_address((request.remote_addr or "").split("%")[0])
    except ValueError:
        abort(403)
    if ip.version == 6 and ip.ipv4_mapped:
        ip = ip.ipv4_mapped
    if not any(ip in rede for rede in PERMITIDAS):
        abort(403)


@app.after_request
def acorda_o_som(resp):
    """Depois de cada comando (tema, jogo, bichos, foto...) o ambiente se ajusta logo, sem esperar a
    próxima olhada; duas vezes porque o SARndbox leva um instante para anotar no registro."""
    if request.method == "POST" and request.path != "/som":
        som.acorda_ambiente(1.0)
        som.acorda_ambiente(3.0)
    return resp


def resposta(fn):
    with trava:
        try:
            return jsonify(ok=True, msg=fn() or "Feito")
        except areia.Erro as err:
            return jsonify(ok=False, msg=str(err)), 400
        except subprocess.TimeoutExpired:
            return jsonify(ok=False, msg="demorou demais; tente de novo"), 504
        except Exception as err:  # erro inesperado: mensagem legível no painel em vez de uma página 500
            app.logger.exception("erro no painel")
            return jsonify(ok=False, msg=f"erro inesperado ({type(err).__name__}); tente de novo"), 500


@app.get("/")
def pagina():
    return PAGINA, 200, {"Cache-Control": "no-cache"}   # o recarregar automático pega sempre a nova


@app.get("/estado")
def ver_estado():
    e = areia.estado()
    e["versao"] = VERSAO   # a página aberta compara com a dela e recarrega quando o painel é atualizado
    e["sandbox"] = areia.qual()
    try:  # a página só baixa a prévia de novo quando a foto muda
        e["foto_versao"] = int(os.path.getmtime(os.path.join(areia.FOTOS, "rosto.png")) * 1000)
    except OSError:
        e["foto_versao"] = 0
    e["tem_foto"] = bool(e["foto_versao"])
    e["imagem"] = os.path.basename(e["imagem"]) if e.get("imagem") else None
    e["tesouro"] = areia.tesouro_situacao()
    e["tesouros_achados"] = areia.tesouro_achados()
    e["fossil"] = areia.fossil_situacao()
    e["willbert"] = willbert_estado()
    e["labirinto"], e["labirinto_rodadas"] = areia.labirinto_situacao()
    e["jogo"] = areia.jogo_situacao()
    e["copia"] = areia.copia_situacao()
    e["animais"] = areia.animais_situacao()
    e["mundo"] = areia.mundo_situacao()
    e["mundo_vista"] = areia.mundo_vista_atual()   # o que o laboratório mostra (área ou quem segue)
    e["mar"] = areia.mar_escolhido()
    e["especies"] = [[k, r, f, areia.GRUPOS.get(k, "")] for k, (r, f) in areia.ESPECIES.items()]
    e["tema"] = areia.tema_atual()
    e["luz"] = areia.luz_atual()
    e["temas"] = [[t, areia.NOMES_TEMAS.get(t, t)] for t in areia.temas()]
    e["fotos_salvas"] = [os.path.basename(f) for f in areia.imagens()]
    pw = areia.pasta_willbert()
    if os.path.isdir(pw):
        w_fotos = sorted([f for f in os.listdir(pw) if f.lower().endswith(areia.EXT)],
                         key=lambda f: os.path.getmtime(os.path.join(pw, f)), reverse=True)
        e["fotos_willbert"] = w_fotos[:6]
    else:
        e["fotos_willbert"] = []
    e["som"] = som.estado()
    return jsonify(e)


@app.get("/previa.png")
def previa():
    p = os.path.join(areia.FOTOS, "rosto.png")
    if not os.path.exists(p):
        return "", 404
    return send_file(p, mimetype="image/png", max_age=0)


@app.post("/foto")
def foto():
    def faz():
        arq = request.files.get("foto")
        if arq and arq.filename:
            ext = os.path.splitext(arq.filename)[1].lower()
            ext = ext if ext in areia.EXT else ".jpg"
            with tempfile.TemporaryDirectory() as tmp:
                origem = os.path.join(tmp, "foto" + ext)
                arq.save(origem)
                base = re.sub(r"[^A-Za-z0-9_-]+", "-", os.path.splitext(arq.filename)[0])[:40]
                prefixo = base if base.startswith("willbert-") else "celular"
                sub = areia.pasta_willbert() if prefixo.startswith("willbert-") else None
                img = areia.guarda(origem, time.strftime(f"{prefixo}-%Y-%m-%d-%H%M%S") + ext, pasta_destino=sub)
                if prefixo.startswith("willbert-"):
                    areia.arquiva_willbert()
        else:
            img = areia.foto_da_camera()
        areia.mostra(img, modo="rosto")
        return f"{os.path.basename(img)} projetado"
    return resposta(faz)


@app.post("/escolher")
def escolher():
    def faz():
        dados = request.get_json(silent=True) or {}
        nome = os.path.basename(str(dados.get("imagem") or ""))  # só fotos das pastas do painel
        if not nome:
            raise areia.Erro("escolha uma foto")
        img = areia.acha(nome)
        areia.mostra(img, modo="rosto")
        return f"{os.path.basename(img)} projetado"
    return resposta(faz)


@app.post("/modo")
def modo():
    def faz():
        m = (request.get_json(silent=True) or {}).get("modo")
        if m not in areia.MODOS:
            raise areia.Erro("modo inválido")
        e = areia.estado()
        e["modo"] = m
        areia.aplica(e)
        return {"rosto": "Modo Foto", "original": "Topografia", "esculpir": "Modo Esculpir", "relevo": "Relevo"}.get(m, m)
    return resposta(faz)


@app.post("/brincar")
def brincar():
    def faz():
        dados = request.get_json(silent=True) or {}
        acao = dados.get("acao")
        nivel = str(dados.get("nivel") or "normal")
        if nivel not in ("facil", "normal", "dificil"):
            raise areia.Erro("nível: facil, normal ou dificil")
        if acao == "fossil" or acao in areia.FOSSEIS:
            nome, prof = areia.fossil("trex" if acao == "fossil" else acao, nivel)
            return f"Fóssil de {nome} enterrado a {prof:g} cm"
        if acao == "tesouro":
            prof = areia.tesouro(nivel)
            return f"Tesouro escondido a {prof:g} cm"
        if acao == "labirinto":
            d = areia.labirinto(nivel)
            return f"Labirinto de água ({areia.LABIRINTO[d].split()[0]})"
        if acao == "labirintoNovo":
            areia.labirinto_novo()
            return "Nova rodada iniciada"
        if acao == "copia":
            f, lv = areia.copia("sorteio", nivel)
            return f"Copie o relevo ({('fácil', 'normal', 'difícil')[lv - 1]}): {areia.FORMAS.get(f, 'sorteado')}"
        if acao == "copiaNova":
            areia.copia_nova()
            return "Outro relevo sorteado"
        if acao in areia.JOGOS:
            n, lv = areia.jogo(acao, nivel)
            return f"{areia.JOGOS[n][0]} ({('fácil', 'normal', 'difícil')[lv - 1]})"
        if acao == "jogoNovo":
            areia.jogo_novo()
            return "Nova rodada iniciada"
        if acao in ("mundo", "membros", "laob"):
            nomes, presentes = areia.mundo()
            return "Laboratório na areia" + (": " + ", ".join(n.capitalize() for n in nomes) if nomes else
                                             " (sem fotos de membros: a sala fica vazia)")
        if acao in areia.ANIMAIS:
            t, n = areia.animais(acao)
            nome = {"peixes": "bichos da água", "carros": "carrinhos", "pessoas": "pessoas", "todos": "bichos"}.get(t, t)
            return f"{n} {nome} na areia"
        if acao == "parar":
            areia.para_tudo()
            return "Brincadeiras paradas"
        raise areia.Erro("brincadeira inválida")
    return resposta(faz)


WILLBERT = ["sudo", "-n", "/usr/local/sbin/willbert-controle"]
_willbert_cache = [0.0, None]


def willbert_estado():
    """'ligado', 'desligado' ou None (sem o Willbert nesta máquina); guarda 10 s (o painel pergunta a cada 4 s).
    Pergunta direto ao systemd, sem sudo: antes o "sudo willbert-controle estado" deixava ~950 linhas por hora
    no registro do sistema enquanto alguém estava com o painel aberto."""
    agora = time.time()
    if agora - _willbert_cache[0] < 10:
        return _willbert_cache[1]
    try:
        out = subprocess.run(["systemctl", "show", "visao-detector", "-p", "LoadState,ActiveState"],
                             capture_output=True, text=True, timeout=10).stdout
        est = None if "LoadState=loaded" not in out else ("ligado" if "ActiveState=active" in out else "desligado")
    except (OSError, subprocess.SubprocessError):
        est = None
    _willbert_cache[:] = [agora, est]
    return est


@app.post("/willbert")
def willbert():
    def faz():
        acao = (request.get_json(silent=True) or {}).get("acao")
        if acao not in ("ligar", "desligar"):
            raise areia.Erro("use ligar ou desligar")
        try:
            r = subprocess.run(WILLBERT + [acao], capture_output=True, text=True, timeout=90)
        except (OSError, subprocess.SubprocessError) as e:
            raise areia.Erro(f"não consegui {acao} o Willbert ({e})")
        if r.returncode != 0:
            raise areia.Erro(f"não consegui {acao} o Willbert: {(r.stdout + r.stderr).strip()[:200]}")
        _willbert_cache[0] = 0.0
        return "Willbert ligado (pronto em uns 30 s)" if acao == "ligar" else "Willbert desligado"
    return resposta(faz)


@app.post("/mar")
def mar():
    def faz():
        sel = areia.mar(request.get_json(silent=True) or {})
        return "Na areia: " + areia.conta_bichos(sel)
    return resposta(faz)


@app.post("/som")
def ajustar_som():
    """Volume e mudo do cabeçalho: valem para tudo (ambiente automático e efeitos). Os botões de testar
    cada efeito saíram em 19/11/2026: o som é automático (pedido do Victor)."""
    dados = request.get_json(silent=True) or {}
    if "volume" in dados:
        som.definir_volume(dados["volume"])
    if "mudo" in dados:
        som.definir_mudo(dados["mudo"])
    return jsonify(ok=True, som=som.estado())


@app.post("/chuva")
def chuva():
    def faz():
        d = request.get_json(silent=True) or {}
        if d.get("secar"):
            areia.secar()
            return "Água seca"
        areia.chuva(d.get("x"), d.get("y"), d.get("segundos") or 8)
        return "Chovendo nesse ponto por uns segundos"   # o som vem do vigia do som.py ("Chuva: nuvem" no registro)
    return resposta(faz)


@app.post("/comida")
def comida():
    def faz():
        d = request.get_json(silent=True) or {}
        c = areia.comida(d.get("x"), d.get("y"))
        o_que = {"agua": "Ração na água", "terra": "Pão na areia"}.get(c["onde"], "Comida jogada")
        return o_que + ": " + quem_vem_comer(c["comem"])
    return resposta(faz)


def quem_vem_comer(comem):
    """{"passaro": 3, "cachorro": 1} -> "os pássaros e o cachorro vêm comer" (só o que está na areia);
    com três ou mais: "os peixes, a tartaruga e o pato vêm comer"."""
    artigo = {"tartaruga": "a", "aguaviva": "a", "arraia": "a"}
    plural = {"tartaruga": "as", "aguaviva": "as", "arraia": "as"}
    partes = [(artigo.get(k, "o") + " " + areia.SINGULAR[k]) if n == 1
              else (plural.get(k, "os") + " " + areia.ESPECIES[k][0].lower())
              for k, n in comem.items() if n]
    if not partes:
        return "nenhum bicho vem comer"
    lista = partes[0] if len(partes) == 1 else ", ".join(partes[:-1]) + " e " + partes[-1]
    return lista + (" vem comer" if sum(comem.values()) == 1 else " vêm comer")


# Imagem do projetor ao vivo (quadro da chuva): um ffmpeg captura a saída do projetor 6x por segundo
# para um arquivo na memória, só enquanto alguém está olhando (para depois de 15 s sem pedido).
# Escrita atômica (-atomic_writing: arquivo .tmp + rename): antes o ffmpeg truncava e regravava o JPEG
# no mesmo lugar e o painel às vezes mandava um quadro cortado ou vazio (avisos do waitress no journal).
AO_VIVO = f"/dev/shm/areia-projetor-{os.getuid()}.jpg"
_ao_vivo = {"proc": None, "pedido": 0.0}
_ao_vivo_trava = threading.Lock()


def _tela_do_projetor():
    """(largura, altura, x, y) da saída do projetor (HDMI-0), ou None se ele não está ligado."""
    env = dict(os.environ, DISPLAY=os.environ.get("DISPLAY") or ":0")
    xa = f"/run/user/{os.getuid()}/gdm/Xauthority"
    if not env.get("XAUTHORITY") and os.path.exists(xa):
        env["XAUTHORITY"] = xa
    try:
        # --current: lê o que o X já sabe; o xrandr puro faz o driver NVIDIA sondar todas as saídas
        # (umas 20 linhas no journal) no X que está desenhando a areia
        out = subprocess.run(["xrandr", "--current"], capture_output=True, text=True, timeout=5, env=env).stdout
    except (OSError, subprocess.SubprocessError):
        return None, env
    m = re.search(r"^HDMI-0 connected (?:primary )?(\d+)x(\d+)\+(\d+)\+(\d+)", out, re.M)
    return (tuple(int(v) for v in m.groups()) if m else None), env


def _morre_com_o_painel():
    """No ffmpeg (antes de rodar): morre junto se o painel morrer. O serviço tem KillMode=process e,
    sem isto, uma captura em andamento ficava órfã para sempre quando o painel reiniciava."""
    try:
        import ctypes
        import signal
        ctypes.CDLL("libc.so.6", use_errno=True).prctl(1, signal.SIGTERM)   # PR_SET_PDEATHSIG
    except Exception:
        pass


def _limpa_capturas_velhas():
    subprocess.run(["pkill", "-f", f"x11grab.*{AO_VIVO}"], capture_output=True)


def _liga_ao_vivo():
    with _ao_vivo_trava:
        _ao_vivo["pedido"] = time.time()
        proc = _ao_vivo["proc"]
        if proc is not None and proc.poll() is None:
            return True
        tela, env = _tela_do_projetor()
        if tela is None:
            return False
        w, h, x, y = tela
        try:
            _ao_vivo["proc"] = subprocess.Popen(
                ["ffmpeg", "-nostdin", "-loglevel", "error", "-f", "x11grab", "-framerate", "6",
                 "-video_size", f"{w}x{h}", "-i", f"{env['DISPLAY']}.0+{x},{y}", "-vf", "scale=512:-2",
                 "-q:v", "6", "-update", "1", "-atomic_writing", "1", "-y", AO_VIVO],   # AO_VIVO por último (pkill)
                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env,
                preexec_fn=_morre_com_o_painel)
        except OSError:
            return False
        return True


def _desliga_ao_vivo_parado():
    while True:
        time.sleep(3)
        with _ao_vivo_trava:
            proc = _ao_vivo["proc"]
            if proc is not None and proc.poll() is None and time.time() - _ao_vivo["pedido"] > 15:
                proc.terminate()
                _ao_vivo["proc"] = None


@app.get("/projetor.jpg")
def projetor_jpg():
    if not _liga_ao_vivo():
        return "", 404
    for _ in range(20):   # o primeiro quadro leva uns instantes
        try:
            if time.time() - os.path.getmtime(AO_VIVO) < 5:
                # lê de uma vez e manda os bytes lidos: o send_file media o tamanho e só depois abria o
                # arquivo, e um quadro novo no meio fazia o tamanho não bater com o que foi enviado
                with open(AO_VIVO, "rb") as f:
                    dados = f.read()
                if dados:
                    return Response(dados, mimetype="image/jpeg", headers={"Cache-Control": "no-store"})
        except OSError:
            pass
        time.sleep(0.1)
    return "", 404


@app.post("/mundo/rosto")
def mundo_rosto():
    def faz():
        n = areia.mundo_rosto(str((request.get_json(silent=True) or {}).get("nome") or ""))
        return f"Rosto de {n.capitalize()} na areia por alguns segundos"
    return resposta(faz)


@app.post("/mundo/vista")
def mundo_vista():
    """Área do laboratório que aparece na areia (tudo, principal, forense, mini) ou seguir um membro (20/11/2026)."""
    def faz():
        tipo, q = areia.mundo_vista(str((request.get_json(silent=True) or {}).get("vista") or ""))
        return f"Seguindo {q.capitalize()} na areia" if tipo == "segue" else f"Na areia: {areia.VISTAS[q]}"
    return resposta(faz)


@app.post("/reabrir")
def reabrir():
    def faz():
        areia.reabre()
        return "Areia reaberta na topografia"
    return resposta(faz)


@app.post("/tema")
def tema():
    def faz():
        t = areia.escolhe_tema((request.get_json(silent=True) or {}).get("tema") or "")
        return f"Cores: {areia.NOMES_TEMAS.get(t, t)}"
    return resposta(faz)


@app.post("/luz")
def luz():
    def faz():
        n = areia.escolhe_luz(str((request.get_json(silent=True) or {}).get("luz") or ""))
        return "Brilho: " + {"normal": "normal", "forte": "forte", "maximo": "máximo"}[n]
    return resposta(faz)


@app.post("/ajuste")
def ajuste():
    def faz():
        d = request.get_json(silent=True) or {}
        e = areia.estado()
        try:
            for k in areia.AJUSTE:
                if k in d:
                    e["ajuste"][k] = float(d[k])
            giro = int(d["giro"]) if "giro" in d else None
        except (TypeError, ValueError):
            raise areia.Erro("valor inválido")
        if giro is not None or "espelho" in d:
            areia.salva(e)
            areia.reorienta(giro=giro, espelho=bool(d["espelho"]) if "espelho" in d else None)
            return "Giro ajustado"
        areia.ajusta(e)
        return "Ajustado"
    return resposta(faz)


PAGINA = """<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0f1720">
<title>Caixa de Areia</title>
<style>
:root {
  --bg: #f4efe6;
  --bg2: #ebe3d4;
  --surface: #fffdf8;
  --surface2: #f8f3ea;
  --border: #e3d9c7;
  --border-strong: #cdbfa6;
  --fg: #2a2218;
  --muted: #7a6d5a;
  --accent: #1f7a8c;
  --accent-soft: #d8eef1;
  --accent-fg: #ffffff;
  --gold: #c9971c;
  --ok: #2f8f4e;
  --danger-bg: #fdecea;
  --danger-border: #f3c2bc;
  --danger-fg: #b3261e;
  --shadow: 0 1px 2px rgba(60,45,20,.06), 0 6px 18px rgba(60,45,20,.06);
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0f1720;
    --bg2: #131e29;
    --surface: #17222e;
    --surface2: #1c2936;
    --border: #253545;
    --border-strong: #35495c;
    --fg: #eef2f5;
    --muted: #8da0b2;
    --accent: #4cc3d9;
    --accent-soft: #163a45;
    --accent-fg: #06222a;
    --gold: #f0c04a;
    --ok: #5fd08a;
    --danger-bg: #2c1618;
    --danger-border: #55252a;
    --danger-fg: #ff8a80;
    --shadow: 0 1px 2px rgba(0,0,0,.3), 0 8px 24px rgba(0,0,0,.25);
  }
}
* { box-sizing: border-box; margin: 0; padding: 0; }
html { -webkit-text-size-adjust: 100%; }
body {
  background: radial-gradient(1200px 500px at 50% -200px, var(--bg2), var(--bg));
  background-color: var(--bg);
  min-height: 100vh;
  color: var(--fg);
  font: 15px/1.45 system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", sans-serif;
  -webkit-font-smoothing: antialiased;
}
main { max-width: 760px; margin: 0 auto; padding: 14px 16px 40px; }
header {
  position: sticky; top: 0; z-index: 5;
  margin: -14px -16px 14px; padding: 12px 16px 10px;
  background: color-mix(in srgb, var(--bg) 88%, transparent);
  backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px);
  border-bottom: 1px solid var(--border);
}
.top { display: flex; align-items: center; gap: 12px; }
.logo {
  width: 40px; height: 40px; border-radius: 12px; flex: none;
  background: linear-gradient(160deg, #e9c98f 0%, #c99a55 55%, #2a8aa0 56%, #1f6f86 100%);
  box-shadow: var(--shadow);
}
h1 { font-size: 1.2rem; font-weight: 700; letter-spacing: -.02em; }
.sub { font-size: .82rem; color: var(--muted); }
.pills { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 10px; }
.pill {
  font-size: .76rem; font-weight: 600; padding: 4px 10px; border-radius: 999px;
  background: var(--surface); border: 1px solid var(--border); color: var(--muted);
  display: inline-flex; align-items: center; gap: 6px;
}
.pill i { width: 7px; height: 7px; border-radius: 50%; background: var(--muted); display: inline-block; }
.pill.on i { background: var(--ok); box-shadow: 0 0 0 3px color-mix(in srgb, var(--ok) 25%, transparent); }
.pill.on { color: var(--fg); }
#msg { font-size: .84rem; margin-top: 8px; min-height: 1.25em; color: var(--muted); transition: color .2s; }
#msg.err { color: var(--danger-fg); }
#msg.okm { color: var(--ok); }

section.card {
  background: var(--surface); border: 1px solid var(--border); border-radius: 16px;
  padding: 16px; margin-bottom: 16px; box-shadow: var(--shadow);
}
h2 {
  font-size: .78rem; font-weight: 700; text-transform: uppercase; letter-spacing: .08em;
  color: var(--muted); margin-bottom: 12px; display: flex; align-items: center; gap: 8px;
}
h2 .extra { margin-left: auto; text-transform: none; letter-spacing: 0; font-weight: 500; }
.hint { font-size: .84rem; color: var(--muted); margin-bottom: 12px; }
.grid { display: grid; gap: 8px; grid-template-columns: repeat(2, 1fr); }
.g4 { grid-template-columns: repeat(4, 1fr); }
@media (max-width: 420px) { .g4 { grid-template-columns: repeat(2, 1fr); } }
.full { grid-column: 1 / -1; }
button, label.btn {
  appearance: none; font: inherit; font-size: .9rem; font-weight: 600;
  border: 1px solid var(--border); background: var(--surface2); color: var(--fg);
  padding: 10px 12px; min-height: 44px; border-radius: 11px; cursor: pointer;
  display: flex; align-items: center; justify-content: center; gap: 8px; text-align: center;
  transition: background .15s, border-color .15s, transform .08s, box-shadow .15s;
  user-select: none; -webkit-tap-highlight-color: transparent;
}
button:hover, label.btn:hover { border-color: var(--border-strong); }
button:active, label.btn:active { transform: scale(.97); }
button:focus-visible, label.btn:focus-within, .jogo:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
button.active { background: var(--accent); border-color: var(--accent); color: var(--accent-fg); }
.primary { background: var(--accent); border-color: var(--accent); color: var(--accent-fg); }
.danger { background: var(--danger-bg); border-color: var(--danger-border); color: var(--danger-fg); }
button:disabled { opacity: .55; cursor: progress; }
body.ocupado button, body.ocupado label.btn, body.ocupado .chip { cursor: progress; }
/* retorno no próprio botão: girando enquanto envia, ✓ quando a areia aceitou, tremida se falhou */
.carregando, .feito, .falhou { position: relative; }
.carregando::after {
  content: ""; position: absolute; right: 8px; top: 50%; width: 14px; height: 14px; margin-top: -7px;
  border-radius: 50%; border: 2px solid currentColor; border-right-color: transparent; animation: gira .7s linear infinite;
}
.feito::after {
  content: "✓"; position: absolute; right: 8px; top: 50%; transform: translateY(-50%); font-weight: 800;
  color: var(--ok); animation: aparece .2s ease-out;
}
.falhou { animation: treme .35s; }
@keyframes gira { to { transform: rotate(360deg); } }
@keyframes aparece { from { opacity: 0; transform: translateY(-50%) scale(.5); } to { opacity: 1; } }
@keyframes treme { 25% { transform: translateX(-4px); } 75% { transform: translateX(4px); } }
@media (prefers-reduced-motion: reduce) { .carregando::after { animation-duration: 2s; } .falhou { animation: none; } }
input[type=file] { display: none; }
[hidden] { display: none !important; }

.seg { display: inline-flex; background: var(--surface2); border: 1px solid var(--border); border-radius: 11px; padding: 3px; gap: 2px; }
.seg button { min-height: 34px; padding: 6px 12px; border: 0; background: transparent; font-size: .84rem; border-radius: 8px; }
.seg button.active { background: var(--accent); color: var(--accent-fg); }

/* som no cabeçalho: mudo (um toque) e volume; valem para o ambiente e os efeitos */
.som { margin-left: auto; display: flex; align-items: center; gap: 8px; flex: none; }
.som .btn-som { width: 44px; min-height: 44px; padding: 0; font-size: 1.15rem; border-radius: 11px; }
.som .btn-som.mudo { background: var(--danger-bg); border-color: var(--danger-border); }
.som input[type=range] { width: 110px; accent-color: var(--accent); cursor: pointer; }
.som.mudo input[type=range] { opacity: .45; }
.som span { font-size: .8rem; font-weight: 700; color: var(--muted); min-width: 34px; font-variant-numeric: tabular-nums; }
@media (max-width: 520px) { .som input[type=range] { width: 76px; } .som span { display: none; } }
@media (min-width: 900px) { header:has(.mini-vivo:not([hidden])) .som { margin-right: 168px; } }

/* miniatura ao vivo do projetor no cabeçalho (só em tela larga) */
.mini-vivo { position: absolute; right: 16px; top: 10px; padding: 0; min-height: 0; border-radius: 10px; overflow: hidden;
  width: 156px; aspect-ratio: 4 / 3; background: #000; border: 1px solid var(--border-strong); z-index: 1; }
.mini-vivo img { width: 100%; height: 100%; object-fit: fill; display: block; }
.mini-vivo span { position: absolute; left: 6px; top: 5px; font-size: .62rem; font-weight: 700; color: #ff6b6b;
  background: rgba(0,0,0,.55); padding: 1px 6px; border-radius: 999px; }
@media (max-width: 899px) { .mini-vivo { display: none; } }

/* abas: uma parte do painel por vez (antes era uma página só, comprida) */
.abas { display: flex; gap: 6px; margin-top: 10px; overflow-x: auto; scrollbar-width: none; -webkit-overflow-scrolling: touch; }
.abas::-webkit-scrollbar { display: none; }
.abas button { flex: 1 0 auto; min-height: 40px; padding: 6px 12px; font-size: .86rem; white-space: nowrap; border-radius: 999px; }
.abas button.active { background: var(--accent); border-color: var(--accent); color: var(--accent-fg); }
section.card[hidden] { display: none; }
section.card { animation: entra .18s ease-out; }
@keyframes entra { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: none; } }
@media (prefers-reduced-motion: reduce) { section.card { animation: none; } }

/* compacto: o cartão do jogo mostra figura e nome; a frase curta e a explicação só ao tocar ou passar o mouse */
.jogo { padding: 10px; gap: 4px; }
.jogo .ic { width: 32px; height: 32px; font-size: 1.1rem; border-radius: 9px; }
.jogo .t { font-size: .88rem; }
.jogo .c { display: none; }
.jogo:hover .c, .jogo.aberto .c { display: block; }
.jogo.aberto { grid-column: 1 / -1; }
.barra { flex-wrap: nowrap; margin-bottom: 10px; }
.barra .danger { min-height: 34px; padding: 6px 12px; font-size: .84rem; white-space: nowrap; }
section.card { padding: 14px; margin-bottom: 12px; }
h2 { margin-bottom: 10px; }
.hint { margin-bottom: 10px; }
@media (max-width: 520px) {
  header { margin: -14px -16px 12px; padding: 10px 16px 8px; }
  .logo { width: 30px; height: 30px; border-radius: 9px; }
  h1 { font-size: 1.05rem; }
  .sub { display: none; }
  .pills { flex-wrap: nowrap; overflow-x: auto; margin-top: 8px; scrollbar-width: none; }
  .pills::-webkit-scrollbar { display: none; }
  .pill { white-space: nowrap; flex: none; }
  .abas { margin-top: 8px; }
  .abas button { min-height: 36px; padding: 5px 10px; font-size: .82rem; }
  .barra .lbl { display: none; }
  .seg button { padding: 6px 9px; }
  #sec-jogos .hint { display: none; }
  .temas { grid-template-columns: repeat(4, 1fr); }
  .temas button { font-size: .7rem; padding: 6px 4px; min-height: 40px; }
}

/* temas */
.temas { display: grid; grid-template-columns: repeat(auto-fill, minmax(96px, 1fr)); gap: 8px; }
.temas button { flex-direction: column; gap: 6px; padding: 8px; font-size: .8rem; }
.temas button .sw { width: 100%; height: 18px; border-radius: 6px; }
.temas button.active { background: var(--accent-soft); color: var(--fg); border-color: var(--accent); box-shadow: inset 0 0 0 1px var(--accent); }

/* jogos */
.jogos { display: grid; grid-template-columns: repeat(auto-fill, minmax(145px, 1fr)); gap: 8px; }
.jogo {
  position: relative; border: 1px solid var(--border); background: var(--surface2);
  border-radius: 14px; padding: 12px; cursor: pointer; outline: none;
  display: flex; flex-direction: column; gap: 6px;
  transition: border-color .15s, box-shadow .15s, transform .15s;
}
.jogo:hover, .jogo.aberto { border-color: var(--accent); box-shadow: 0 0 0 1px var(--accent), var(--shadow); }
.jogo .cab { display: flex; align-items: center; gap: 10px; }
.jogo .ic {
  width: 38px; height: 38px; border-radius: 11px; flex: none; font-size: 1.3rem;
  display: grid; place-items: center; background: var(--accent-soft);
}
.jogo .t { font-weight: 700; font-size: .95rem; line-height: 1.2; }
.jogo .c { font-size: .8rem; color: var(--muted); }
.jogo .tag { font-size: .68rem; font-weight: 700; text-transform: uppercase; letter-spacing: .06em; color: var(--accent); }
.jogo .desc {
  font-size: .84rem; color: var(--fg); line-height: 1.45;
  max-height: 0; overflow: hidden; opacity: 0;
  transition: max-height .25s ease, opacity .2s ease, margin .2s;
}
.jogo .acoes { display: none; gap: 6px; }
.jogo .acoes button { flex: 1; min-height: 38px; font-size: .84rem; }
.jogo:hover .desc, .jogo.aberto .desc { max-height: 220px; opacity: 1; margin-top: 2px; }
.jogo.aberto .acoes { display: flex; }
@media (hover: hover) { .jogo:hover .acoes { display: flex; } }
.jogo.ativo { border-color: var(--gold); box-shadow: 0 0 0 1px var(--gold), var(--shadow); }
.jogo.ativo .ic { background: color-mix(in srgb, var(--gold) 25%, transparent); }
.badge {
  position: absolute; top: 10px; right: 10px; font-size: .68rem; font-weight: 700;
  padding: 2px 8px; border-radius: 999px; background: var(--gold); color: #2a1f00; display: none;
}
.jogo.ativo .badge { display: inline-block; }
.placar {
  display: none; margin-bottom: 12px; padding: 10px 12px; border-radius: 12px;
  background: color-mix(in srgb, var(--gold) 14%, var(--surface)); border: 1px solid color-mix(in srgb, var(--gold) 45%, var(--border));
  font-size: .88rem;
}
.placar b { font-weight: 700; }
input[type=range] { accent-color: var(--accent); }
.alvo-copia { display: flex; gap: 12px; align-items: center; margin-top: 8px; font-size: .86rem; line-height: 1.35; }
.alvo-copia canvas { width: 118px; height: 84px; border-radius: 8px; flex: none; border: 1px solid var(--border-strong); }
.placar .nota { height: 10px; border-radius: 999px; background: color-mix(in srgb, var(--fg) 12%, transparent); margin: 8px 0 4px; overflow: hidden; }
.placar .nota-barra { height: 100%; width: 0; border-radius: 999px; transition: width .6s ease;
  background: linear-gradient(90deg, #e2463a, #e8b72c 45%, #3cb35b 85%); background-size: 600px 100%; }
.placar .nota-barra.feito { background: linear-gradient(90deg, #f4c94a, #fff2b0, #f4c94a); }
.placar .nota-txt { font-size: .82rem; color: var(--muted); }
.barra { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; margin-bottom: 12px; }
.barra .lbl { font-size: .82rem; color: var(--muted); }
.barra .sp { flex: 1; }

/* animais marinhos e chuva */
.especies { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 8px; }
.especie {
  display: flex; align-items: center; gap: 8px; padding: 8px 10px; border-radius: 11px;
  background: var(--surface2); border: 1px solid var(--border);
}
.especie .e { font-size: 1.3rem; }
.especie .r { flex: 1; font-weight: 600; font-size: .9rem; }
.especie button { min-height: 36px; width: 36px; padding: 0; font-size: 1.1rem; }
.especie .n { width: 1.6em; text-align: center; font-weight: 700; font-variant-numeric: tabular-nums; }
.especie.zero .r, .especie.zero .e { opacity: .45; }
.grupo-especie { grid-column: 1 / -1; font-size: .72rem; font-weight: 700; text-transform: uppercase; letter-spacing: .07em;
  color: var(--muted); margin-top: 4px; }
.quadro-chuva {
  aspect-ratio: 4 / 3; width: 100%; max-width: 420px; overflow: hidden; margin: 0 auto; border-radius: 12px; cursor: crosshair;
  display: grid; place-items: center; color: var(--muted); font-size: .9rem; user-select: none; position: relative;
  background: linear-gradient(160deg, color-mix(in srgb, var(--accent) 22%, var(--surface2)), var(--surface2));
  border: 1px dashed var(--border-strong); touch-action: manipulation;
}
.quadro-chuva img { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: fill; pointer-events: none; }
.quadro-chuva .selo-vivo {
  position: absolute; top: 8px; left: 8px; font-size: .72rem; font-weight: 700; padding: 3px 8px; border-radius: 999px;
  background: rgba(0,0,0,.55); color: #ff6b6b; pointer-events: none;
}
.quadro-chuva .nuvem-chuva { z-index: 2; position: absolute; width: 64px; height: 64px; margin: -40px 0 0 -32px;
  pointer-events: none; animation: nuvem-chega .45s ease-out both; transition: opacity .8s ease; }
.quadro-chuva .nuvem-chuva.indo { opacity: 0; }
.quadro-chuva .nuvem-corpo { position: absolute; left: 4px; right: 4px; top: 0; height: 26px; border-radius: 14px;
  background: radial-gradient(circle at 30% 60%, #fff 0 40%, transparent 41%), radial-gradient(circle at 62% 40%, #fff 0 46%, transparent 47%),
              linear-gradient(#f4f7fb, #dfe6ef); box-shadow: 0 3px 8px rgba(0,0,0,.25); }
.quadro-chuva .nuvem-chuva i { position: absolute; top: 24px; width: 2px; height: 10px; border-radius: 2px;
  background: linear-gradient(transparent, #8fd0ff); animation: gota .6s linear infinite; }
@keyframes nuvem-chega { from { transform: translateY(-8px) scale(.7); opacity: 0; } to { transform: none; opacity: 1; } }
@keyframes gota { from { transform: translateY(0); opacity: 0; } 20% { opacity: 1; } to { transform: translateY(30px); opacity: 0; } }
@media (prefers-reduced-motion: reduce) { .quadro-chuva .nuvem-chuva, .quadro-chuva .nuvem-chuva i { animation: none; } }

/* mundo LAOB */
.moradores { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 8px; }
.morador {
  display: flex; align-items: center; gap: 10px; text-align: left; justify-content: flex-start;
  padding: 10px 12px; min-height: 52px;
}
.morador .bola { width: 10px; height: 10px; border-radius: 50%; background: var(--border-strong); flex: none; }
.morador.aqui .bola { background: var(--ok); box-shadow: 0 0 0 3px color-mix(in srgb, var(--ok) 25%, transparent); }
.morador .n { font-weight: 700; }
.morador .f { font-size: .8rem; font-weight: 500; color: var(--muted); }
/* vista do laboratório (20/11/2026): área que aparece na areia e "seguir" ao lado de cada membro */
.vistas { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; margin-bottom: 10px; }
.vistas button { min-height: 38px; padding: 6px 8px; font-size: .84rem; }
.morador-linha { display: flex; gap: 6px; min-width: 0; }
.morador-linha .morador { flex: 1; min-width: 0; }
.morador-linha .seguir { flex: none; padding: 6px 10px; font-size: .8rem; }
.morador .n, .morador .f { overflow-wrap: anywhere; }
@media (max-width: 420px) { .vistas { grid-template-columns: 1fr 1fr; } }

/* fotos */
.chips { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 12px; }
.chip {
  font-size: .78rem; color: var(--muted); border: 1px solid var(--border); background: var(--surface2);
  padding: 5px 10px; border-radius: 999px; cursor: pointer; max-width: 100%;
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.chip:hover { color: var(--fg); border-color: var(--border-strong); }
img#prev {
  display: block; width: 100%; max-height: 220px; object-fit: contain; margin-top: 12px;
  border-radius: 12px; border: 1px solid var(--border);
  background: repeating-conic-gradient(var(--surface2) 0 25%, var(--surface) 0 50%) 50% / 16px 16px;
}
details summary { cursor: pointer; font-size: .84rem; color: var(--muted); margin-top: 12px; list-style: none; }
details summary::-webkit-details-marker { display: none; }
footer { text-align: center; font-size: .76rem; color: var(--muted); margin-top: 8px; }
/* computador: a página usa a largura da tela. Jogos em mais colunas e com a frase curta à vista;
   na aba Projetor os quatro quadros ficam lado a lado (2x2); listas em mais colunas. */
@media (min-width: 900px) {
  main { max-width: 1180px; display: grid; grid-template-columns: 1fr 1fr; column-gap: 14px; align-items: start; }
  main > header, main > footer, main > section.card { grid-column: 1 / -1; }
  main > section.card[data-aba="projetor"] { grid-column: auto; }
  .jogos { grid-template-columns: repeat(auto-fill, minmax(210px, 1fr)); gap: 10px; }
  .jogo { padding: 12px; }
  .jogo .c { display: block; }
  .especies { grid-template-columns: repeat(4, 1fr); }
  .moradores { grid-template-columns: repeat(4, 1fr); }
  .quadro-chuva { max-width: 640px; }
  .abas { flex-wrap: nowrap; }
  .abas button { flex: 0 1 auto; padding: 6px 18px; }
  img#prev { max-height: 300px; }
}

/* celular: listas mais baixas e sem os textos longos (ficam no computador) */
@media (max-width: 520px) {
  .especies { grid-template-columns: 1fr; gap: 5px; }
  .especie { padding: 3px 8px; }
  .especie .e { font-size: 1.1rem; }
  .especie button { min-height: 34px; width: 38px; }
  section.card[data-aba="bichos"] .hint, section.card[data-aba="chuva"] .hint, section.card[data-aba="projetor"] .hint { display: none; }
  .moradores { grid-template-columns: 1fr; gap: 6px; }   /* uma coluna: cada membro tem o botão "seguir" ao lado */
  .morador { padding: 8px 10px; min-height: 46px; }
  .quadro-chuva { max-width: none; }
}
</style>
</head>
<body>
<main>
  <header>
    <div class="top">
      <div class="logo" aria-hidden="true"></div>
      <div>
        <h1>Caixa de Areia</h1>
        <div class="sub">Laboratório LAOB · painel do projetor</div>
      </div>
      <div class="som" title="Som da areia (ambiente e efeitos)">
        <button class="btn-som" id="som-top" aria-label="Silenciar o som">🔊</button>
        <input type="range" id="som-vol" min="0" max="100" value="80" aria-label="Volume do som">
        <span id="som-vol-val">80%</span>
      </div>
      <button class="mini-vivo" id="mini-vivo" hidden title="O que o projetor mostra agora (toque para a chuva)">
        <img id="mini-img" alt="Imagem do projetor agora"><span>● ao vivo</span>
      </button>
    </div>
    <div class="pills">
      <span class="pill" id="p-sb"><i></i><span id="sb">…</span></span>
      <span class="pill" id="p-sit"><i></i><span id="sit">Nenhuma brincadeira</span></span>
      <span class="pill" id="p-wb"><i></i><span id="wb">Willbert</span></span>
    </div>
    <nav class="abas" id="abas" aria-label="Partes do painel">
      <button data-ir="jogos">🎮 Jogos</button>
      <button data-ir="bichos">🐾 Vida</button>
      <button data-ir="chuva">🌧️ Chuva</button>
      <button data-ir="laob">🔬 LAOB</button>
      <button data-ir="fotos">📷 Fotos</button>
      <button data-ir="projetor">⚙️ Projetor</button>
    </nav>
    <div id="msg" role="status" aria-live="polite"></div>
  </header>

  <section class="card" data-aba="laob" id="sec-laob">
    <h2>LAOB <span class="extra" id="laob-estado"></span></h2>
    <p class="hint" id="laob-dica">O laboratório por dentro, desenhado na areia, com os membros lá.</p>
    <div class="vistas" id="vistas" hidden role="group" aria-label="Área do laboratório que aparece na areia">
      <button data-vista="tudo">Tudo</button>
      <button data-vista="principal">Sala principal</button>
      <button data-vista="forense">Forense</button>
      <button data-vista="mini">Miniauditório</button>
    </div>
    <div class="moradores" id="moradores"></div>
    <div class="grid" style="margin-top:12px">
      <button class="primary" id="laob-ligar">🔬 Abrir o laboratório</button>
      <button class="danger" data-b="parar" id="laob-fechar" hidden>Fechar o laboratório</button>
    </div>
  </section>

  <section class="card" data-aba="jogos" id="sec-jogos">
    <h2>Jogos e brincadeiras <span class="extra" id="njogos"></span></h2>
    <div class="placar" id="placar"></div>
    <div class="barra">
      <span class="lbl">Nível</span>
      <div class="seg" id="niveis">
        <button data-n="facil">Fácil</button>
        <button data-n="normal" class="active">Normal</button>
        <button data-n="dificil">Difícil</button>
      </div>
      <span class="sp"></span>
      <button class="danger" data-b="parar">Parar tudo</button>
    </div>
    <p class="hint">Passe o mouse ou toque num jogo para ver como se joga.</p>
    <div class="jogos" id="jogos"></div>
  </section>

  <section class="card" data-aba="bichos">
    <h2>Vida na areia <span class="extra" id="mar-total"></span></h2>
    <p class="hint">Escolha quantos de cada um aparecem. Os da água nadam no lago (onde a areia é baixa) e na água da chuva; os de terra andam em chão firme; os pássaros voam por cima de tudo.</p>
    <div class="especies" id="especies"></div>
    <div class="grid" style="margin-top:12px">
      <button class="primary" id="mar-soltar">▶ Soltar na areia</button>
      <button id="mar-tirar" data-b="parar">Tirar todos</button>
    </div>
    <div class="quadro-chuva" id="quadro-comida" role="button" tabindex="0" aria-label="Tocar onde jogar comida" style="margin-top:12px">
      <img id="ao-vivo-comida" alt="" hidden>
      <span class="aviso-vivo" id="aviso-comida">🍞 toque para jogar comida (na água: ração)</span>
      <span class="selo-vivo" id="selo-comida" hidden>● ao vivo</span>
    </div>
    <p class="hint">Na água cai ração para os peixes, tartarugas e patos; na terra, pão para os pássaros e cachorros. Ou aponte com o indicador para cima e os outros dedos fechados: cai comida embaixo do dedo.</p>
  </section>

  <section class="card" data-aba="chuva">
    <h2>Chuva</h2>
    <p class="hint">Abra a mão sobre a areia ou toque na imagem, no ponto onde quer que chova. É o que o projetor está mostrando agora, atualizada ao vivo.</p>
    <div class="quadro-chuva" id="quadro-chuva" role="button" tabindex="0" aria-label="Tocar onde chover">
      <img id="ao-vivo" alt="" hidden>
      <span class="aviso-vivo" id="aviso-vivo">☁️ toque onde chover</span>
      <span class="selo-vivo" id="selo-vivo" hidden>● ao vivo</span>
    </div>
    <div class="grid" style="margin-top:12px">
      <button id="chuva-geral">🌧️ Chover em tudo</button>
      <button id="chuva-secar">☀️ Secar a água</button>
    </div>
  </section>

  <section class="card" data-aba="fotos">
    <h2>Fotos na areia</h2>
    <div class="grid">
      <button id="cam" class="primary full">📷 Tirar foto pela câmera</button>
      <label class="btn">🤳 Foto no aparelho<input id="selfie" type="file" accept="image/*" capture="user"></label>
      <label class="btn">🖼️ Escolher arquivo<input id="galeria" type="file" accept="image/*"></label>
      <button id="girar">↻ Girar 90°</button>
      <button id="espelhar">⇋ Espelhar</button>
      <button data-m="rosto">🖼️ Mostrar a foto na areia</button>
      <button data-m="esculpir">✋ Esculpir a foto</button>
    </div>
    <div id="box-fotos" hidden>
      <div class="chips" id="chips"></div>
    </div>
    <img id="prev" alt="Foto que está na areia" hidden>
  </section>

  <section class="card" data-aba="projetor">
    <h2>Areia</h2>
    <p class="hint">Volta ao relevo colorido de sempre e encerra o que estiver na areia (foto, jogo, bichos).</p>
    <div class="grid" id="modos">
      <button data-m="original" class="full">⛰️ Voltar à topografia</button>
    </div>
  </section>

  <section class="card" data-aba="projetor">
    <h2>Cores do relevo</h2>
    <div class="temas" id="temas"></div>
    <h2 style="margin-top:14px">Brilho da imagem</h2>
    <div class="grid" id="luz" style="grid-template-columns:repeat(3,1fr)">
      <button data-luz="normal">Normal</button>
      <button data-luz="forte">Forte</button>
      <button data-luz="maximo">Máximo</button>
    </div>
  </section>

  <section class="card" data-aba="projetor">
    <h2>Willbert</h2>
    <p class="hint">Foto na areia pelo Willbert: as duas mãos abertas ao lado do rosto, perto da câmera.</p>
    <div class="grid">
      <button data-w="ligar">Ligar</button>
      <button data-w="desligar">Desligar</button>
    </div>
  </section>

  <section class="card" data-aba="projetor">
    <h2>Se a areia travar</h2>
    <p class="hint">A imagem parou ou não responde? Este botão fecha e abre a areia de novo (uns 10 s).</p>
    <button id="reabrir" class="danger" style="width:100%">Reabrir a areia</button>
  </section>
  <footer>Projetor-LAOB</footer>
</main>

<script>
const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);
let E = {}, NIVEL = 'normal';
const VERSAO = '__VERSAO__';
const AGUA = __JOGOS__;
const OUTROS = [
  {id: 'copia', ic: '⛰️', t: 'Copie o relevo', c: 'Esculpa a areia igual ao relevo-alvo.', tag: 'Areia',
   d: 'O projetor mostra um relevo sorteado (montanha, vulcão, cratera, serra...), com as curvas de nível do alvo em branco. Azul: falta areia, ponha mais. Vermelho: sobra, tire. Verde: já está certo. A barra no canto da areia dá a nota; passando de 85% comemora e sorteia outro. O nível muda a altura e a folga.'},
  {id: 'labirinto', ic: '🌊', t: 'Labirinto de água', c: 'Leve a água até a bandeira sem tocar nas armadilhas.', tag: 'Água',
   d: 'No caminho reto entre a nascente e a bandeira há armadilhas vermelhas. Cavem um canal que dê a volta nelas: se a água encostar em uma, some tudo e recomeça. Quando chega à bandeira, o alvo fica dourado e outra rodada começa em 20 s.'},
  {id: 'tesouro', ic: '💰', t: 'Caça ao tesouro', c: 'Um baú escondido embaixo da areia.', tag: 'Escavar',
   d: 'O baú fica enterrado num lugar sorteado. Cavem procurando: quanto mais perto, mais o sinal brilha. Achou? Ele reaparece em outro lugar depois de 20 s. O nível muda a profundidade.'},
  {id: 'trex', ic: '🦖', t: 'Fóssil de dinossauro', c: 'Escavação de um esqueleto de T-Rex.', tag: 'Escavar',
   d: 'Aparece a quadrícula de escavação. Cavando até a profundidade certa, o esqueleto surge na rocha. O nível muda a profundidade.'},
  {id: 'mundo', ic: '🔬', t: 'LAOB', c: 'O laboratório por dentro, com os membros.', tag: 'Mundo',
   d: 'A planta do laboratório desenhada na areia: bancadas, quadro, porta azul, caixa de areia. Quem o Willbert está vendo fica lá dentro, na própria bancada ou andando pela sala; os outros entram pela porta de vez em quando. O rosto aparece num balão quando alguém entra, ou quando você toca no nome dele na seção LAOB.'},
];
const ICONES = {corrida: '🏁', vila: '🏘️', rota: '🚩', vulcao: '🌋'};
const TODOS = [...AGUA.map(j => ({id: j[0], ic: ICONES[j[0]] || '💧', t: j[1], c: j[2], d: j[3], tag: 'Jogo de água'})), ...OUTROS];

function msg(t, tipo) {
  const el = $('#msg');
  el.textContent = t || '';
  el.className = tipo || '';
}

let OCUPADO = false;
let TOCADO = null;   // o botão que o usuário acabou de tocar (recebe o "carregando" e o ✓)
document.addEventListener('pointerdown', ev => { TOCADO = ev.target.closest('button, label.btn, .chip, .jogo'); }, true);
async function post(u, b, isForm) {
  if (OCUPADO) return;   // um pedido por vez: toques repetidos não empilham trocas no projetor
  OCUPADO = true;
  document.body.classList.add('ocupado');
  const bt = TOCADO;
  if (bt) { bt.classList.remove('feito', 'falhou'); bt.classList.add('carregando'); }
  msg('Enviando…');
  try {
    const r = await fetch(u, {method: 'POST', body: isForm ? b : JSON.stringify(b),
                              headers: isForm ? {} : {'Content-Type': 'application/json'}});
    let j = {};
    try { j = await r.json(); } catch (e) {}
    const deu = r.ok && j.ok !== false;
    if (!deu) msg(j.msg || 'Erro', 'err'); else msg(j.msg || 'Pronto', 'okm');
    if (bt) {
      bt.classList.remove('carregando');
      bt.classList.add(deu ? 'feito' : 'falhou');
      setTimeout(() => bt.classList.remove('feito', 'falhou'), deu ? 1200 : 900);
    }
    await carrega();
    return j;
  } catch (e) {
    msg('Sem conexão com a areia', 'err');
    if (bt) { bt.classList.remove('carregando'); bt.classList.add('falhou'); setTimeout(() => bt.classList.remove('falhou'), 900); }
  }
  finally { OCUPADO = false; document.body.classList.remove('ocupado'); }
}

function monta() {
  const box = $('#jogos');
  for (const j of TODOS) {
    const el = document.createElement('div');
    el.className = 'jogo';
    el.tabIndex = 0;
    el.dataset.id = j.id;
    el.setAttribute('role', 'button');
    el.setAttribute('aria-expanded', 'false');
    el.innerHTML = '<span class="badge">Jogando</span><div class="cab"><div class="ic"></div><div><div class="tag"></div><div class="t"></div></div></div>'
      + '<div class="c"></div><div class="desc"></div><div class="acoes"><button class="primary jogar">▶ Jogar</button></div>';
    el.querySelector('.ic').textContent = j.ic;
    el.querySelector('.tag').textContent = j.tag;
    el.querySelector('.t').textContent = j.t;
    el.querySelector('.c').textContent = j.c;
    el.querySelector('.desc').textContent = j.d;
    if (j.id === 'labirinto' || j.id === 'copia' || AGUA.some(a => a[0] === j.id)) {
      const nova = document.createElement('button');
      nova.className = 'nova';
      nova.textContent = '↻ Nova rodada';
      nova.hidden = true;
      el.querySelector('.acoes').appendChild(nova);
      nova.onclick = ev => { ev.stopPropagation(); post('/brincar', {acao: j.id === 'labirinto' ? 'labirintoNovo' : j.id === 'copia' ? 'copiaNova' : 'jogoNovo'}); };
    }
    el.querySelector('.jogar').onclick = ev => { ev.stopPropagation(); post('/brincar', {acao: j.id, nivel: NIVEL}); };
    const abre = () => {
      const ja = el.classList.contains('aberto');
      $$('.jogo.aberto').forEach(x => { x.classList.remove('aberto'); x.setAttribute('aria-expanded', 'false'); });
      if (!ja) { el.classList.add('aberto'); el.setAttribute('aria-expanded', 'true'); }
    };
    el.onclick = abre;
    el.onkeydown = ev => { if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); abre(); } };
    box.appendChild(el);
  }
  $('#njogos').textContent = TODOS.length + ' opções';
}

function ativo() {
  if (E.jogo && E.jogo.jogo) return E.jogo.jogo;
  if (E.copia && E.copia.forma) return 'copia';
  if (E.labirinto && E.labirinto !== 'nenhum') return 'labirinto';
  if (E.tesouro && E.tesouro !== 'nenhum') return 'tesouro';
  if (E.fossil && E.fossil !== 'nenhum') return 'trex';
  if (E.animais && E.animais !== 'nada') return E.animais;
  return null;
}

function situacao(a) {
  if (!a) return 'Nenhuma brincadeira';
  if (E.jogo && E.jogo.jogo) {
    const n = (AGUA.find(x => x[0] === E.jogo.jogo) || [0, E.jogo.jogo])[1];
    return n + (E.jogo.fase === 'fim' ? ' · fim da rodada' : ' · valendo');
  }
  if (a === 'copia') return 'Copie o relevo · ' + (E.copia.conseguiu ? 'conseguiu!' : E.copia.nota + '%');
  if (a === 'labirinto') return E.labirinto === 'chegou' ? 'A água chegou!' : 'Labirinto · água correndo';
  if (a === 'tesouro') return E.tesouro === 'achado' ? 'Tesouro achado!' : 'Tesouro enterrado';
  if (a === 'trex') return E.fossil === 'achado' ? 'Fóssil escavado!' : 'Fóssil enterrado';
  if (a === 'mundo') return 'Laboratório LAOB';
  if (a === 'mar' || a === 'peixes' || a === 'carros' || a === 'pessoas' || a === 'todos') return 'Vida na areia';
  return 'Animação na areia';
}

async function carrega() {
  try {
    const r = await fetch('/estado');
    if (!r.ok) return;
    E = await r.json();
    if (E.versao && E.versao !== VERSAO) { location.reload(); return; }   // painel atualizado: pega a página nova
    const sb = {rosto: E.modo === 'rosto' ? 'Modo Foto' : (E.modo === 'esculpir' ? 'Esculpir' : 'Relevo'),
                original: 'Topografia', nenhum: 'Areia desligada'}[E.sandbox] || E.sandbox;
    $('#sb').textContent = sb;
    $('#p-sb').classList.toggle('on', E.sandbox !== 'nenhum');
    $$('[data-m]').forEach(b => {
      const m = b.dataset.m;
      b.classList.toggle('active', E.sandbox === 'original' ? m === 'original' : (E.sandbox === 'rosto' && m === E.modo));
    });

    const a = ativo();
    $('#sit').textContent = situacao(a);
    $('#p-sit').classList.toggle('on', !!a);
    $$('.jogo').forEach(el => {
      const on = el.dataset.id === a;
      el.classList.toggle('ativo', on);
      const nv = el.querySelector('.nova');
      if (nv) nv.hidden = !on;
    });
    const pl = $('#placar');
    let txt = '';
    if (E.jogo && E.jogo.jogo) {
      const n = (AGUA.find(x => x[0] === E.jogo.jogo) || [0, E.jogo.jogo])[1];
      txt = '<b></b> · rodada ' + (E.jogo.rodadas + (E.jogo.fase === 'fim' ? 0 : 1)) + '<br><span></span>';
      pl.innerHTML = txt;
      pl.querySelector('b').textContent = n;
      const casas = E.jogo.casas ? '🏠 ' + (E.jogo.casas - E.jogo.perdidas) + ' de ' + E.jogo.casas + ' casas de pé'
        + (E.jogo.perdidas ? ' · ' + E.jogo.perdidas + ' perdida' + (E.jogo.perdidas > 1 ? 's' : '') : '') : '';
      pl.querySelector('span').textContent = (casas && E.jogo.fase !== 'fim') ? casas : E.jogo.aviso ? (E.jogo.fase === 'fim' ? '🏆 ' : '') + E.jogo.aviso + (E.jogo.fase === 'fim' ? ' — nova rodada em 20 s' : '') : '';
    } else if (a === 'labirinto') {
      pl.innerHTML = '<b>Labirinto de água</b> · rodadas vencidas: ' + (E.labirinto_rodadas || 0);
      txt = 'x';
    } else if (a === 'copia') {
      const c = E.copia;
      pl.innerHTML = '<b>Copie o relevo</b> · <span class="forma"></span>'
        + '<div class="alvo-copia"><canvas width="236" height="168" aria-label="Como o relevo deve ficar"></canvas>'
        + '<span>Faça este relevo <b>no meio da areia</b>. Verde e marrom = monte (põe areia), azul = buraco (tira areia). '
        + 'A barra no canto da areia mostra quanto já está certo: passe de 85%.</span></div>'
        + '<div class="nota"><div class="nota-barra"></div></div><span class="nota-txt"></span>';
      desenhaAlvo(pl.querySelector('.alvo-copia canvas'), c.alvo || []);
      pl.querySelector('.forma').textContent = ({vulcao: 'Vulcão', montanha: 'Montanha', 'duas montanhas': 'Duas montanhas',
        cratera: 'Cratera', 'ilha e lago': 'Ilha e lago', serra: 'Serra'})[c.forma] || c.forma;
      pl.querySelector('.nota-barra').style.width = Math.min(100, c.nota) + '%';
      pl.querySelector('.nota-barra').classList.toggle('feito', c.conseguiu);
      pl.querySelector('.nota-txt').textContent = c.conseguiu ? '🏆 conseguiu! ' + c.nota + '% — outro relevo em 20 s'
        : 'nota ' + c.nota + '% (passe de 85%)' + (c.rodadas ? ' · ' + c.rodadas + ' feito' + (c.rodadas > 1 ? 's' : '') : '');
      txt = 'x';
    }
    pl.style.display = txt ? 'block' : 'none';

    if (E.som) mostraSom(E.som);
    montaMar();
    const M = $('#moradores');
    const mundo = E.mundo || [];
    const ligado = a === 'mundo';
    $('#laob-estado').textContent = ligado ? 'aberto' : '';
    $('#laob-ligar').hidden = ligado;
    $('#laob-fechar').hidden = !ligado;
    $('#laob-dica').textContent = ligado
      ? (mundo.length ? 'Escolha a área que aparece na areia, ou 👁 para seguir alguém com zoom. Toque num nome para ver o rosto num balão (quem está fora entra para aparecer). Bolinha verde: está no laboratório de verdade agora.'
                      : 'A sala está vazia: não há fotos de membros na pasta do projetor.')
      : 'O laboratório por dentro, desenhado na areia, com os membros lá.';
    /* vista (20/11/2026): a área que aparece na areia ou quem está sendo seguido */
    const V = E.mundo_vista || {};
    $('#vistas').hidden = !ligado;
    for (const b of document.querySelectorAll('#vistas button')) {
      const ativa = ligado && V.area === b.dataset.vista;
      b.classList.toggle('active', ativa);
      b.setAttribute('aria-pressed', ativa ? 'true' : 'false');
    }
    const chaveM = JSON.stringify([mundo, V.segue || '', !!V.fora]);
    if (M.dataset.chave !== chaveM) {
      M.dataset.chave = chaveM;
      M.innerHTML = '';
      for (const m of mundo) {
        const linha = document.createElement('div');
        linha.className = 'morador-linha';
        const b = document.createElement('button');
        b.className = 'morador' + (m.presente ? ' aqui' : '');
        b.innerHTML = '<span class="bola"></span><span><span class="n"></span><br><span class="f"></span></span>';
        b.querySelector('.n').textContent = m.nome;
        b.querySelector('.f').textContent = m.fazendo;
        b.title = 'Mostrar o rosto de ' + m.nome + ' na areia';
        b.onclick = () => post('/mundo/rosto', {nome: m.nome});
        const s = document.createElement('button');
        const segue = V.segue === m.nome;
        s.className = 'seguir' + (segue ? ' active' : '');
        s.textContent = segue ? (V.fora ? '👁 fora' : '👁 seguindo') : '👁 seguir';
        s.setAttribute('aria-pressed', segue ? 'true' : 'false');
        s.title = segue ? 'Voltar ao laboratório inteiro' : 'Seguir ' + m.nome + ' na areia (com zoom)';
        s.onclick = () => post('/mundo/vista', {vista: segue ? 'tudo' : m.nome});
        linha.append(b, s);
        M.appendChild(linha);
      }
    }

    $('#wb').textContent = E.willbert ? 'Willbert ' + E.willbert : 'Willbert';
    $('#p-wb').classList.toggle('on', E.willbert === 'ligado');

    const T = $('#temas');
    if (!T.children.length && E.temas) {
      const CORES = {classico: 'linear-gradient(90deg,#2b5fd9,#3fae4f,#e8d36a,#b0703a,#fff)',
        vulcao: 'linear-gradient(90deg,#1a0d08,#7a1c0c,#e2471a,#ffb02e)',
        inverno: 'linear-gradient(90deg,#24406e,#8fb5df,#e6f0fb,#fff)',
        marte: 'linear-gradient(90deg,#3a160c,#8c3a1c,#c8673a,#e8b48a)',
        deserto: 'linear-gradient(90deg,#8a5a2b,#c9934d,#e9c98f,#f6e7c4)',
        oceano: 'linear-gradient(90deg,#04163a,#0a4f8a,#1ea0c8,#bfeaf3)',
        'arco-iris': 'linear-gradient(90deg,#e33,#f93,#fd3,#3c6,#39f,#93f)'};
      for (const [t, n] of E.temas) {
        const b = document.createElement('button');
        const sw = document.createElement('span');
        sw.className = 'sw';
        sw.style.background = CORES[t] || 'var(--border)';
        b.appendChild(sw);
        b.appendChild(document.createTextNode(n));
        b.dataset.t = t;
        b.onclick = () => post('/tema', {tema: t});
        T.appendChild(b);
      }
    }
    T.querySelectorAll('button').forEach(b => b.classList.toggle('active', b.dataset.t === E.tema));
    $$('#luz button').forEach(b => b.classList.toggle('active', b.dataset.luz === E.luz));

    const fotos = [...(E.fotos_salvas || []), ...(E.fotos_willbert || [])];
    $('#box-fotos').hidden = !fotos.length;
    const C = $('#chips');
    const lista = fotos.join('|');
    if (C.dataset.lista !== lista) {
    C.dataset.lista = lista;
    C.innerHTML = '';
    fotos.forEach(nome => {
      const tag = document.createElement('span');
      tag.className = 'chip';
      tag.textContent = nome;
      tag.title = 'Projetar ' + nome;
      tag.onclick = () => post('/escolher', {imagem: nome});
      C.appendChild(tag);
    });
    }
    if (E.tem_foto) {
      const P = $('#prev');
      P.hidden = false;
      if (P.dataset.v !== String(E.foto_versao)) { P.dataset.v = String(E.foto_versao); P.src = '/previa.png?' + E.foto_versao; }
    }
  } catch (e) {}
}

let MAR = null;
function montaMar() {
  const box = $('#especies');
  if (!E.especies) return;
  if (MAR === null) MAR = Object.assign({}, E.mar || {});
  if (!box.children.length) {
    let grupoAntes = '';
    for (const [k, rotulo, fig, grupo] of E.especies) {
      if (grupo && grupo !== grupoAntes) {
        const t = document.createElement('div');
        t.className = 'grupo-especie';
        t.textContent = grupo;
        box.appendChild(t);
        grupoAntes = grupo;
      }
      const el = document.createElement('div');
      el.className = 'especie';
      el.dataset.k = k;
      el.innerHTML = '<span class="e"></span><span class="r"></span><button aria-label="menos">−</button><span class="n"></span><button aria-label="mais">+</button>';
      el.querySelector('.e').textContent = fig;
      el.querySelector('.r').textContent = rotulo;
      const [menos, mais] = el.querySelectorAll('button');
      menos.onclick = () => { MAR[k] = Math.max(0, (MAR[k] || 0) - 1); pintaMar(); };
      mais.onclick = () => { if (totalMar() < 32) MAR[k] = (MAR[k] || 0) + 1; pintaMar(); };
      box.appendChild(el);
    }
    pintaMar();
  }
}
function totalMar() { return Object.values(MAR || {}).reduce((a, b) => a + b, 0); }
function pintaMar() {
  $$('.especie').forEach(el => {
    const n = MAR[el.dataset.k] || 0;
    el.querySelector('.n').textContent = n;
    el.classList.toggle('zero', n === 0);
  });
  $('#mar-total').textContent = totalMar() + ' de 32';
}
let VIVO = false;
let MEXEU = Date.now();
let ERROS_VIVO = 0;   // quadros ao vivo que falharam em seguida
['pointerdown', 'keydown', 'touchstart', 'wheel'].forEach(t => document.addEventListener(t, () => {
  MEXEU = Date.now(); aoVivo();
}, {passive: true}));
function querVivo() {
  /* aba Chuva aberta: rápido (6 por segundo); tela larga: a miniatura do cabeçalho, 1 por segundo */
  if (document.hidden) return 0;
  /* ninguém mexe na página há 3 min (ex.: o Firefox aberto no próprio laob): para a captura, que gasta CPU */
  if (Date.now() - MEXEU > 180000) return 0;
  if (!$('section.card[data-aba="chuva"]').hidden) return 160;
  if (!$('section.card[data-aba="bichos"]').hidden) return 330;
  if (window.matchMedia('(min-width: 900px)').matches) return 1000;
  return 0;
}
function pausaVivo() {
  /* a captura parou (ninguém mexe há 3 min, aba escondida...): o último quadro fica, mas sem dizer
     "ao vivo" (antes o selo continuava aceso numa imagem parada). Um toque religa. */
  $('#selo-vivo').hidden = true; $('#selo-comida').hidden = true;
  $('#mini-vivo span').textContent = '⏸ pausado';
}
function aoVivo() {
  if (!querVivo()) { if (VIVO) pausaVivo(); VIVO = false; return; }
  if (VIVO) return;
  VIVO = true;
  const img = $('#ao-vivo');
  const proxima = () => {
    const passo = querVivo();
    if (!passo) { VIVO = false; pausaVivo(); return; }
    const n = new Image();
    n.onload = () => {
      ERROS_VIVO = 0;
      img.src = n.src; img.hidden = false;
      $('#ao-vivo-comida').src = n.src; $('#ao-vivo-comida').hidden = false; $('#aviso-comida').hidden = true;
      $('#mini-img').src = n.src; $('#mini-vivo').hidden = false; $('#mini-vivo span').textContent = '● ao vivo';
      $('#selo-vivo').hidden = false; $('#selo-comida').hidden = false; $('#aviso-vivo').hidden = true;
      setTimeout(proxima, querVivo() || 1000);
    };
    n.onerror = () => {
      /* um quadro ruim tenta de novo logo, sem esconder a imagem; três seguidos = projetor desligado ou
         areia fechada: esconde as imagens velhas (as duas: chuva e comida) e tenta de novo devagar */
      if (++ERROS_VIVO < 3) { setTimeout(proxima, 500); return; }
      img.hidden = true; $('#mini-vivo').hidden = true; $('#selo-vivo').hidden = true; $('#aviso-vivo').hidden = false;
      $('#ao-vivo-comida').hidden = true; $('#selo-comida').hidden = true; $('#aviso-comida').hidden = false;
      setTimeout(proxima, 4000);
    };
    n.src = '/projetor.jpg?' + Date.now();
  };
  proxima();
}
window.addEventListener('resize', aoVivo);
document.addEventListener('visibilitychange', aoVivo);

function desenhaAlvo(cv, alvo) {
  /* miniatura do relevo-alvo do Copie o relevo: mesmo enquadramento da imagem do projetor */
  if (!cv || !cv.getContext) return;
  const g = cv.getContext('2d'), W = cv.width, H = cv.height, img = g.createImageData(W, H);
  let maior = 1;
  alvo.forEach(m => { maior = Math.max(maior, Math.abs(m[2])); });
  for (let py = 0; py < H; py++) for (let px = 0; px < W; px++) {
    const x = (px + .5) / W, y = 1 - (py + .5) / H;
    let t = 0;
    alvo.forEach(m => { const dx = x - m[0], dy = (y - m[1]) * H / W, l = m[3]; t += m[2] * Math.exp(-(dx*dx + dy*dy) / (2*l*l)); });
    const u = t / maior;                                   // -1 buraco .. 1 topo
    let c;
    if (u < -0.06) { const k = Math.min(1, -u); c = [70 - 50*k, 150 - 60*k, 220 - 40*k]; }
    else if (u < 0.08) c = [214, 196, 150];                // areia como está
    else { const k = Math.min(1, u);
      c = k < 0.5 ? [120 + 200*k, 180 - 20*k, 80] : [220 - 60*(k-.5), 160 - 140*(k-.5), 70 - 40*(k-.5)]; }
    const i = (py * W + px) * 4;
    img.data[i] = c[0]; img.data[i+1] = c[1]; img.data[i+2] = c[2]; img.data[i+3] = 255;
  }
  g.putImageData(img, 0, 0);
}

async function chove(ev) {
  if (OCUPADO) { msg('Espere um instante…'); return; }   // o post() ignoraria o toque: não desenha a nuvem à toa
  const q = $('#quadro-chuva');
  const r = q.getBoundingClientRect();
  const p = ev.touches ? ev.touches[0] : ev;
  const x = Math.min(1, Math.max(0, (p.clientX - r.left) / r.width));
  const y = Math.min(1, Math.max(0, (p.clientY - r.top) / r.height));
  /* nuvem com chuva desenhada aqui mesmo (suave): a imagem ao vivo do projetor tem poucos quadros por segundo */
  const nuvem = document.createElement('div');
  nuvem.className = 'nuvem-chuva';
  nuvem.style.left = (x * 100) + '%';
  nuvem.style.top = (y * 100) + '%';
  nuvem.innerHTML = '<span class="nuvem-corpo"></span>' + Array.from({length: 9}, (_, i) =>
    `<i style="left:${8 + i * 10 + (i % 2) * 3}%;animation-delay:${(i * 0.137) % 0.6}s"></i>`).join('');
  q.appendChild(nuvem);
  setTimeout(() => nuvem.classList.add('indo'), 7400);
  setTimeout(() => nuvem.remove(), 8200);
  const j = await post('/chuva', {x: x, y: 1 - y});
  if (!j || !j.ok) nuvem.remove();   // não choveu (areia fechada, erro, sem conexão)
}

async function jogaComida(ev) {
  if (OCUPADO) { msg('Espere um instante…'); return; }
  const q = $('#quadro-comida');
  const r = q.getBoundingClientRect();
  const p = ev.touches ? ev.touches[0] : ev;
  const x = Math.min(1, Math.max(0, (p.clientX - r.left) / r.width));
  const y = Math.min(1, Math.max(0, (p.clientY - r.top) / r.height));
  const m = document.createElement('span');
  m.textContent = '🍞'; m.className = 'aviso-vivo pao';
  m.style.cssText = `left:${x*100}%;top:${y*100}%;transform:translate(-50%,-50%);position:absolute`;
  q.appendChild(m);
  const tira = setTimeout(() => m.remove(), 20000);
  const j = await post('/comida', {x: x, y: 1 - y});
  if (!j || !j.ok) { clearTimeout(tira); m.remove(); return; }   // não caiu comida (sem bichos que comam ali, erro...)
  /* o SARndbox guarda um pão só (um pão só no quadro); a ração na água é outra comida, ao mesmo tempo */
  const racao = (j.msg || '').startsWith('Ração');
  if (racao) { m.textContent = '🟤'; m.className = 'aviso-vivo racao'; }
  q.querySelectorAll(racao ? '.racao' : '.pao').forEach(e => { if (e !== m) e.remove(); });
}

function abre(aba) {
  $$('section.card').forEach(sec => { sec.hidden = sec.dataset.aba !== aba; });
  $$('#abas button').forEach(b => b.classList.toggle('active', b.dataset.ir === aba));
  try { localStorage.setItem('areia-aba', aba); } catch (e) {}
  window.scrollTo(0, 0);
  aoVivo();
}

function envia(inp) {
  inp.onchange = () => {
    if (!inp.files[0]) return;
    const f = new FormData();
    f.append('foto', inp.files[0]);
    post('/foto', f, true);
    inp.value = '';
  };
}

monta();
$$('#abas button').forEach(b => b.onclick = () => abre(b.dataset.ir));
let ABA = 'jogos';
try { ABA = localStorage.getItem('areia-aba') || 'jogos'; } catch (e) {}
if (location.hash.length > 1) ABA = location.hash.slice(1);   // ex.: http://...:8090/#chuva abre direto na aba
if (ABA === 'vida') ABA = 'bichos';
abre($$('#abas button[data-ir="' + ABA + '"]').length ? ABA : 'jogos');
envia($('#selfie'));
envia($('#galeria'));
$('#cam').onclick = () => post('/foto', new FormData(), true);
$$('[data-n]').forEach(b => b.onclick = () => {
  NIVEL = b.dataset.n;
  $$('[data-n]').forEach(x => x.classList.toggle('active', x === b));
});
$$('[data-b]').forEach(b => b.onclick = () => post('/brincar', {acao: b.dataset.b, nivel: NIVEL}));
$$('[data-w]').forEach(b => b.onclick = () => post('/willbert', {acao: b.dataset.w}));
$$('[data-m]').forEach(b => b.onclick = () => post('/modo', {modo: b.dataset.m}));
$('#reabrir').onclick = () => post('/reabrir', {});
$$('#luz button').forEach(b => b.onclick = () => post('/luz', {luz: b.dataset.luz}));
// som (cabeçalho): mudo e volume valem para o ambiente automático e para os efeitos
let SOM_MEXENDO = false;   // enquanto o dedo está no volume, o /estado não puxa a barra de volta
function mostraSom(s) {
  const b = $('#som-top');
  b.textContent = s.mudo ? '🔇' : '🔊';
  b.setAttribute('aria-label', s.mudo ? 'Ligar o som' : 'Silenciar o som');
  b.title = s.mudo ? 'Som desligado (toque para ligar)' : 'Toque para silenciar';
  b.classList.toggle('mudo', !!s.mudo);
  $('.som').classList.toggle('mudo', !!s.mudo);
  if (!SOM_MEXENDO) { $('#som-vol').value = s.volume; $('#som-vol-val').textContent = s.volume + '%'; }
}
async function mandaSom(dados) {
  try {
    const r = await fetch('/som', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(dados)});
    const j = await r.json();
    if (j.som) { E.som = j.som; mostraSom(j.som); }
  } catch (e) { /* sem rede: o próximo /estado acerta o botão */ }
}
$('#som-top').onclick = () => mandaSom({mudo: !(E.som && E.som.mudo)});
$('#som-vol').oninput = () => { SOM_MEXENDO = true; $('#som-vol-val').textContent = $('#som-vol').value + '%'; };
$('#som-vol').onchange = () => { SOM_MEXENDO = false; mandaSom({volume: +$('#som-vol').value}); };
$('#laob-ligar').onclick = () => post('/brincar', {acao: 'mundo'});
for (const b of document.querySelectorAll('#vistas button')) b.onclick = () => post('/mundo/vista', {vista: b.dataset.vista});
$('#mar-soltar').onclick = () => post('/mar', MAR || {});
$('#quadro-chuva').onclick = chove;
$('#quadro-comida').onclick = jogaComida;
$('#mini-vivo').onclick = () => abre('chuva');
$('#chuva-secar').onclick = () => post('/chuva', {secar: true});
$('#chuva-geral').onclick = async () => {
  for (const [x, y] of [[0.25, 0.3], [0.75, 0.3], [0.25, 0.7], [0.75, 0.7]]) await post('/chuva', {x: x, y: y, segundos: 10});
};
$('#espelhar').onclick = () => post('/ajuste', {espelho: !E.espelho});
$('#girar').onclick = () => post('/ajuste', {giro: ((E.giro || 0) + 90) % 360});
carrega();
setInterval(() => { if (!document.hidden) carrega(); }, 4000);   // aba escondida não fica perguntando
document.addEventListener('visibilitychange', () => { if (!document.hidden) carrega(); });
</script>
</body>
</html>""".replace(
    "__JOGOS__", json.dumps([[k, *v] for k, v in areia.JOGOS.items()], ensure_ascii=False))
# Versão da página (calculada com o marcador ainda no lugar, para não depender dela mesma): vai no /estado e
# a página aberta recarrega sozinha quando ela muda. Antes o Firefox do laob, aberto havia dias, seguia com o
# JS antigo depois de cada instalação (e com a captura ao vivo ligada para ninguém).
VERSAO = hashlib.sha1(PAGINA.encode()).hexdigest()[:10]
PAGINA = PAGINA.replace("__VERSAO__", VERSAO)

def _vigia_membros():
    """Com o mundo LAOB ligado, confere a cada 5 s quem o Willbert vê e atualiza os bonecos."""
    while True:
        time.sleep(5)
        try:
            with trava:
                areia.membros_atualiza()
        except Exception:  # areia fechada, comando recusado...: tenta de novo na próxima
            pass


if __name__ == "__main__":
    threading.Thread(target=_vigia_membros, daemon=True, name="membros").start()
    som.iniciar_ambiente()   # fundo automático pelo que está na areia (tema, brincadeira, bichos)
    _limpa_capturas_velhas()   # de um painel anterior (antes da correção, ou se ele foi morto à força)
    threading.Thread(target=_desliga_ao_vivo_parado, daemon=True, name="ao-vivo").start()
    try:  # papel de parede com o endereço do painel (o timer atualizar-papel-parede repete a cada minuto)
        from atualizar_papel_parede import atualizar as atualiza_papel_parede
        atualiza_papel_parede()
    except Exception as erro:  # sem Pillow ou sem tela: o painel funciona do mesmo jeito
        print("papel de parede não atualizado:", erro)
    from waitress import serve
    serve(app, listen=f"0.0.0.0:{PORTA}", threads=4)
