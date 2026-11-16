"""Painel web da caixa de areia (porta 8090): foto, modos, cores e brincadeiras.
Responde à própria máquina, à rede local (10/8, 172.16/12, 192.168/16: aberto pelo Victor em
2026-11-11 para quem está no laboratório, com o endereço no papel de parede) e à Tailscale.
Rodar com o venv da pasta:  venv/bin/python painel.py
"""
import ipaddress
import json
import os
import re
import subprocess
import tempfile
import threading
import time

from flask import Flask, abort, jsonify, request, send_file

import areia

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
    return PAGINA


@app.get("/estado")
def ver_estado():
    e = areia.estado()
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
    e["animais"] = areia.animais_situacao()
    e["mundo"] = areia.mundo_situacao()
    e["mar"] = areia.mar_escolhido()
    e["especies"] = [[k, r, f] for k, (r, f) in areia.ESPECIES.items()]
    e["tema"] = areia.tema_atual()
    e["temas"] = [[t, areia.NOMES_TEMAS.get(t, t)] for t in areia.temas()]
    e["fotos_salvas"] = [os.path.basename(f) for f in areia.imagens()]
    pw = areia.pasta_willbert()
    if os.path.isdir(pw):
        w_fotos = sorted([f for f in os.listdir(pw) if f.lower().endswith(areia.EXT)],
                         key=lambda f: os.path.getmtime(os.path.join(pw, f)), reverse=True)
        e["fotos_willbert"] = w_fotos[:6]
    else:
        e["fotos_willbert"] = []
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
        if acao in areia.JOGOS:
            n, lv = areia.jogo(acao, nivel)
            return f"{areia.JOGOS[n][0]} ({('fácil', 'normal', 'difícil')[lv - 1]})"
        if acao == "jogoNovo":
            areia.jogo_novo()
            return "Nova rodada iniciada"
        if acao in ("mundo", "membros", "laob"):
            nomes, presentes = areia.mundo()
            return "Mundo LAOB na areia" + (": " + ", ".join(n.capitalize() for n in nomes) if nomes else
                                            " (sem fotos de membros: só moradores sem nome)")
        if acao in areia.ANIMAIS:
            t, n = areia.animais(acao)
            return f"{n} {t} na areia"
        if acao == "parar":
            areia.para_tudo()
            return "Brincadeiras paradas"
        raise areia.Erro("brincadeira inválida")
    return resposta(faz)


WILLBERT = ["sudo", "-n", "/usr/local/sbin/willbert-controle"]
_willbert_cache = [0.0, None]


def willbert_estado():
    """'ligado', 'desligado' ou None (sem o Willbert nesta máquina); guarda 10 s (o painel pergunta a cada 5 s)."""
    agora = time.time()
    if agora - _willbert_cache[0] < 10:
        return _willbert_cache[1]
    try:
        out = subprocess.run(WILLBERT + ["estado"], capture_output=True, text=True, timeout=10).stdout
        est = "ligado" if "visao-detector=active" in out else ("desligado" if "visao-detector=" in out else None)
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
        return "Na areia: " + ", ".join(f"{n} {areia.ESPECIES[k][0].lower()}" for k, n in sel.items() if n)
    return resposta(faz)


@app.post("/chuva")
def chuva():
    def faz():
        d = request.get_json(silent=True) or {}
        if d.get("secar"):
            areia.secar()
            return "Água seca"
        areia.chuva(d.get("x"), d.get("y"), d.get("segundos") or 8)
        return "Chovendo nesse ponto por uns segundos"
    return resposta(faz)


@app.post("/mundo/rosto")
def mundo_rosto():
    def faz():
        n = areia.mundo_rosto(str((request.get_json(silent=True) or {}).get("nome") or ""))
        return f"Rosto de {n.capitalize()} na areia por alguns segundos"
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
main { max-width: 720px; margin: 0 auto; padding: 20px 16px 72px; }
header {
  position: sticky; top: 0; z-index: 5;
  margin: -20px -16px 18px; padding: 16px 16px 12px;
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
.g3 { grid-template-columns: repeat(3, 1fr); }
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
body.ocupado button, body.ocupado label.btn, body.ocupado .chip { opacity: .6; cursor: progress; }
input[type=file] { display: none; }
[hidden] { display: none !important; }

.seg { display: inline-flex; background: var(--surface2); border: 1px solid var(--border); border-radius: 11px; padding: 3px; gap: 2px; }
.seg button { min-height: 34px; padding: 6px 12px; border: 0; background: transparent; font-size: .84rem; border-radius: 8px; }
.seg button.active { background: var(--accent); color: var(--accent-fg); }

/* temas */
.temas { display: grid; grid-template-columns: repeat(auto-fill, minmax(96px, 1fr)); gap: 8px; }
.temas button { flex-direction: column; gap: 6px; padding: 8px; font-size: .8rem; }
.temas button .sw { width: 100%; height: 18px; border-radius: 6px; }
.temas button.active { background: var(--accent-soft); color: var(--fg); border-color: var(--accent); box-shadow: inset 0 0 0 1px var(--accent); }

/* jogos */
.jogos { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 10px; }
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
.quadro-chuva {
  aspect-ratio: 59 / 42; width: 100%; max-width: 420px; margin: 0 auto; border-radius: 12px; cursor: crosshair;
  display: grid; place-items: center; color: var(--muted); font-size: .9rem; user-select: none; position: relative;
  background: linear-gradient(160deg, color-mix(in srgb, var(--accent) 22%, var(--surface2)), var(--surface2));
  border: 1px dashed var(--border-strong); touch-action: manipulation;
}
.quadro-chuva .pingo {
  position: absolute; width: 26px; height: 26px; margin: -13px 0 0 -13px; border-radius: 50%;
  border: 2px solid var(--accent); animation: pingo 1.2s ease-out forwards; pointer-events: none;
}
@keyframes pingo { from { transform: scale(.3); opacity: 1; } to { transform: scale(2.2); opacity: 0; } }
@media (prefers-reduced-motion: reduce) { .quadro-chuva .pingo { animation-duration: .01s; } }

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
    </div>
    <div class="pills">
      <span class="pill" id="p-sb"><i></i><span id="sb">…</span></span>
      <span class="pill" id="p-sit"><i></i><span id="sit">Nenhuma brincadeira</span></span>
      <span class="pill" id="p-wb"><i></i><span id="wb">Willbert</span></span>
    </div>
    <div id="msg" role="status" aria-live="polite"></div>
  </header>

  <section class="card" id="sec-laob">
    <h2>LAOB <span class="extra" id="laob-estado"></span></h2>
    <p class="hint" id="laob-dica">Os membros do laboratório vivendo numa cidade de três bairros: casas, centro e lazer.</p>
    <div class="moradores" id="moradores"></div>
    <div class="grid" style="margin-top:12px">
      <button class="primary" id="laob-ligar">🏙️ Abrir o mundo LAOB</button>
      <button class="danger" data-b="parar" id="laob-fechar" hidden>Fechar o mundo</button>
    </div>
  </section>

  <section class="card" id="sec-jogos">
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

  <section class="card">
    <h2>Bichos, carros e pessoas <span class="extra" id="mar-total"></span></h2>
    <p class="hint">Escolha quantos de cada um aparecem na areia. Os animais marinhos nadam no lago (onde a areia é baixa) e na água da chuva; carrinhos e pessoas andam em chão firme.</p>
    <div class="especies" id="especies"></div>
    <div class="grid" style="margin-top:12px">
      <button class="primary" id="mar-soltar">▶ Soltar na areia</button>
      <button id="mar-tirar" data-b="parar">Tirar todos</button>
    </div>
  </section>

  <section class="card">
    <h2>Chuva</h2>
    <p class="hint">Além de abrir a mão sobre a areia: toque no quadro, no ponto onde quer que chova. O quadro é a areia vista de cima, como o projetor a mostra.</p>
    <div class="quadro-chuva" id="quadro-chuva" role="button" tabindex="0" aria-label="Tocar onde chover">☁️ toque onde chover</div>
    <div class="grid" style="margin-top:12px">
      <button id="chuva-geral">🌧️ Chover em tudo</button>
      <button id="chuva-secar">☀️ Secar a água</button>
    </div>
  </section>

  <section class="card">
    <h2>Fotos na areia</h2>
    <div class="grid">
      <button id="cam" class="primary full">📷 Tirar foto pela câmera</button>
      <label class="btn">🤳 Foto no aparelho<input id="selfie" type="file" accept="image/*" capture="user"></label>
      <label class="btn">🖼️ Escolher arquivo<input id="galeria" type="file" accept="image/*"></label>
      <button id="girar">↻ Girar 90°</button>
      <button id="espelhar">⇋ Espelhar</button>
    </div>
    <div id="box-fotos" hidden>
      <div class="chips" id="chips"></div>
    </div>
    <img id="prev" alt="Foto que está na areia" hidden>
  </section>

  <section class="card">
    <h2>Modo do projetor</h2>
    <div class="grid g4" id="modos">
      <button data-m="original">⛰️ Topografia</button>
      <button data-m="rosto">🖼️ Foto</button>
      <button data-m="relevo">🎨 Relevo</button>
      <button data-m="esculpir">✋ Esculpir</button>
    </div>
  </section>

  <section class="card">
    <h2>Cores do relevo</h2>
    <div class="temas" id="temas"></div>
  </section>

  <section class="card">
    <h2>Willbert</h2>
    <p class="hint">Foto na areia pelo Willbert: as duas mãos abertas ao lado do rosto, perto da câmera.</p>
    <div class="grid">
      <button data-w="ligar">Ligar</button>
      <button data-w="desligar">Desligar</button>
    </div>
  </section>

  <section class="card">
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
const AGUA = __JOGOS__;
const OUTROS = [
  {id: 'labirinto', ic: '🌊', t: 'Labirinto de água', c: 'Leve a água até a bandeira sem tocar nas armadilhas.', tag: 'Água',
   d: 'No caminho reto entre a nascente e a bandeira há armadilhas vermelhas. Cavem um canal que dê a volta nelas: se a água encostar em uma, some tudo e recomeça. Quando chega à bandeira, o alvo fica dourado e outra rodada começa em 20 s.'},
  {id: 'tesouro', ic: '💰', t: 'Caça ao tesouro', c: 'Um baú escondido embaixo da areia.', tag: 'Escavar',
   d: 'O baú fica enterrado num lugar sorteado. Cavem procurando: quanto mais perto, mais o sinal brilha. Achou? Ele reaparece em outro lugar depois de 20 s. O nível muda a profundidade.'},
  {id: 'trex', ic: '🦖', t: 'Fóssil de dinossauro', c: 'Escavação de um esqueleto de T-Rex.', tag: 'Escavar',
   d: 'Aparece a quadrícula de escavação. Cavando até a profundidade certa, o esqueleto surge na rocha. O nível muda a profundidade.'},
  {id: 'mundo', ic: '🏙️', t: 'LAOB', c: 'Os membros vivendo numa cidadezinha.', tag: 'Mundo',
   d: 'Uma cidade com três bairros: o das casas, o centro (laboratório, café, mercado e praça) e o de lazer (parque, campinho e lago de pesca). Cada membro tem casa e rotina, anda pelas calçadas e vai de carro pela avenida quando é longe. A imagem desliza para o bairro onde eles estão. O rosto aparece num balão quando o membro chega a um lugar, ou quando você toca no nome dele na seção LAOB.'},
];
const ICONES = {corrida: '🏁', vila: '🏘️', rota: '🚩', vulcao: '🌋'};
const TODOS = [...AGUA.map(j => ({id: j[0], ic: ICONES[j[0]] || '💧', t: j[1], c: j[2], d: j[3], tag: 'Jogo de água'})), ...OUTROS];

function msg(t, tipo) {
  const el = $('#msg');
  el.textContent = t || '';
  el.className = tipo || '';
}

let OCUPADO = false;
async function post(u, b, isForm) {
  if (OCUPADO) return;   // um pedido por vez: toques repetidos não empilham trocas no projetor
  OCUPADO = true;
  document.body.classList.add('ocupado');
  msg('Enviando…');
  try {
    const r = await fetch(u, {method: 'POST', body: isForm ? b : JSON.stringify(b),
                              headers: isForm ? {} : {'Content-Type': 'application/json'}});
    let j = {};
    try { j = await r.json(); } catch (e) {}
    if (!r.ok || j.ok === false) msg(j.msg || 'Erro', 'err'); else msg(j.msg || 'Pronto', 'okm');
    await carrega();
    return j;
  } catch (e) { msg('Sem conexão com a areia', 'err'); }
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
    if (j.id === 'labirinto' || AGUA.some(a => a[0] === j.id)) {
      const nova = document.createElement('button');
      nova.className = 'nova';
      nova.textContent = '↻ Nova rodada';
      nova.hidden = true;
      el.querySelector('.acoes').appendChild(nova);
      nova.onclick = ev => { ev.stopPropagation(); post('/brincar', {acao: j.id === 'labirinto' ? 'labirintoNovo' : 'jogoNovo'}); };
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
  if (a === 'labirinto') return E.labirinto === 'chegou' ? 'A água chegou!' : 'Labirinto · água correndo';
  if (a === 'tesouro') return E.tesouro === 'achado' ? 'Tesouro achado!' : 'Tesouro enterrado';
  if (a === 'trex') return E.fossil === 'achado' ? 'Fóssil escavado!' : 'Fóssil enterrado';
  if (a === 'mundo') return 'Mundo LAOB';
  if (a === 'mar' || a === 'peixes' || a === 'carros' || a === 'pessoas' || a === 'todos') return 'Bichos, carros e pessoas';
  return 'Animação na areia';
}

async function carrega() {
  try {
    const r = await fetch('/estado');
    if (!r.ok) return;
    E = await r.json();
    const sb = {rosto: E.modo === 'rosto' ? 'Modo Foto' : (E.modo === 'esculpir' ? 'Esculpir' : 'Relevo'),
                original: 'Topografia', nenhum: 'Areia desligada'}[E.sandbox] || E.sandbox;
    $('#sb').textContent = sb;
    $('#p-sb').classList.toggle('on', E.sandbox !== 'nenhum');
    $$('#modos button').forEach(b => {
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
    }
    pl.style.display = txt ? 'block' : 'none';

    montaMar();
    const M = $('#moradores');
    const mundo = E.mundo || [];
    const ligado = a === 'mundo';
    $('#laob-estado').textContent = ligado ? 'aberto' : '';
    $('#laob-ligar').hidden = ligado;
    $('#laob-fechar').hidden = !ligado;
    $('#laob-dica').textContent = ligado
      ? (mundo.length ? 'Toque num nome: a imagem vai até o bairro onde ele está e o rosto aparece num balão. Bolinha verde: está no laboratório de verdade agora.'
                      : 'Mundo aberto só com moradores sem nome: não há fotos de membros na pasta do projetor.')
      : 'Os membros do laboratório vivendo numa cidade de três bairros: casas, centro e lazer.';
    const chaveM = JSON.stringify(mundo);
    if (M.dataset.chave !== chaveM) {
      M.dataset.chave = chaveM;
      M.innerHTML = '';
      for (const m of mundo) {
        const b = document.createElement('button');
        b.className = 'morador' + (m.presente ? ' aqui' : '');
        b.innerHTML = '<span class="bola"></span><span><span class="n"></span><br><span class="f"></span></span>';
        b.querySelector('.n').textContent = m.nome;
        b.querySelector('.f').textContent = m.fazendo;
        b.title = 'Mostrar o rosto de ' + m.nome + ' na areia';
        b.onclick = () => post('/mundo/rosto', {nome: m.nome});
        M.appendChild(b);
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
    for (const [k, rotulo, fig] of E.especies) {
      const el = document.createElement('div');
      el.className = 'especie';
      el.dataset.k = k;
      el.innerHTML = '<span class="e"></span><span class="r"></span><button aria-label="menos">−</button><span class="n"></span><button aria-label="mais">+</button>';
      el.querySelector('.e').textContent = fig;
      el.querySelector('.r').textContent = rotulo;
      const [menos, mais] = el.querySelectorAll('button');
      menos.onclick = () => { MAR[k] = Math.max(0, (MAR[k] || 0) - 1); pintaMar(); };
      mais.onclick = () => { if (totalMar() < 24) MAR[k] = (MAR[k] || 0) + 1; pintaMar(); };
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
  $('#mar-total').textContent = totalMar() + ' de 24';
}
function chove(ev) {
  const q = $('#quadro-chuva');
  const r = q.getBoundingClientRect();
  const p = ev.touches ? ev.touches[0] : ev;
  const x = Math.min(1, Math.max(0, (p.clientX - r.left) / r.width));
  const y = Math.min(1, Math.max(0, (p.clientY - r.top) / r.height));
  const pingo = document.createElement('span');
  pingo.className = 'pingo';
  pingo.style.left = (x * 100) + '%';
  pingo.style.top = (y * 100) + '%';
  q.appendChild(pingo);
  setTimeout(() => pingo.remove(), 1300);
  post('/chuva', {x: x, y: 1 - y});
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
$('#laob-ligar').onclick = () => post('/brincar', {acao: 'mundo'});
$('#mar-soltar').onclick = () => post('/mar', MAR || {});
$('#quadro-chuva').onclick = chove;
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

def _vigia_membros():
    """No modo membros, confere a cada 5 s quem o Willbert vê e atualiza as pessoinhas."""
    while True:
        time.sleep(5)
        try:
            with trava:
                areia.membros_atualiza()
        except Exception:  # areia fechada, comando recusado...: tenta de novo na próxima
            pass


if __name__ == "__main__":
    threading.Thread(target=_vigia_membros, daemon=True, name="membros").start()
    try:  # papel de parede com o endereço do painel (o timer atualizar-papel-parede repete a cada minuto)
        from atualizar_papel_parede import atualizar as atualiza_papel_parede
        atualiza_papel_parede()
    except Exception as erro:  # sem Pillow ou sem tela: o painel funciona do mesmo jeito
        print("papel de parede não atualizado:", erro)
    from waitress import serve
    serve(app, listen=f"0.0.0.0:{PORTA}", threads=4)
