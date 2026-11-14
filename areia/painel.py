"""Painel web da caixa de areia (porta 8090): foto, modos, cores e brincadeiras.
Responde à própria máquina, à rede local (10/8, 172.16/12, 192.168/16: aberto pelo Victor em
2026-11-11 para quem está no laboratório, com o endereço no papel de parede) e à Tailscale.
Rodar com o venv da pasta:  venv/bin/python painel.py
"""
import ipaddress
import os
import re
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


@app.get("/")
def pagina():
    return PAGINA


@app.get("/estado")
def ver_estado():
    e = areia.estado()
    e["sandbox"] = areia.qual()
    e["tem_foto"] = os.path.exists(os.path.join(areia.FOTOS, "rosto.png"))
    e["imagem"] = os.path.basename(e["imagem"]) if e.get("imagem") else None
    e["tesouro"] = areia.tesouro_situacao()
    e["tesouros_achados"] = areia.tesouro_achados()
    e["fossil"] = areia.fossil_situacao()
    e["labirinto"], e["labirinto_rodadas"] = areia.labirinto_situacao()
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
        acao = (request.get_json(silent=True) or {}).get("acao")
        if acao == "fossil" or acao in areia.FOSSEIS:
            nome, prof = areia.fossil("trex" if acao == "fossil" else acao, "normal")
            return f"Fóssil de {nome} enterrado a {prof:g} cm"
        if acao == "tesouro":
            prof = areia.tesouro("normal")
            return f"Tesouro escondido a {prof:g} cm"
        if acao == "labirinto":
            d = areia.labirinto("normal")
            return f"Labirinto de água ({areia.LABIRINTO[d].split()[0]})"
        if acao == "labirintoNovo":
            areia.labirinto_novo()
            return "Nova rodada iniciada"
        if acao in areia.ANIMAIS:
            t, n = areia.animais(acao)
            return f"{n} {t} na areia"
        if acao == "parar":
            areia.tesouro_fim()
            areia.fossil_fim()
            areia.animais_fim()
            areia.labirinto_fim()
            return "Brincadeiras paradas"
        raise areia.Erro("brincadeira inválida")
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
<title>Caixa de Areia</title>
<style>
:root {
  --bg: #f5f5f7;
  --surface: #ffffff;
  --border: #e2e4e8;
  --border-strong: #c5c8cf;
  --fg: #1d1d1f;
  --muted: #6e6e73;
  --accent: #2c2c2e;
  --accent-fg: #ffffff;
  --danger-bg: #fff1f0;
  --danger-border: #ffccc7;
  --danger-fg: #cf1322;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #000000;
    --surface: #1c1c1e;
    --border: #2c2c2e;
    --border-strong: #3a3a3c;
    --fg: #f5f5f7;
    --muted: #8e8e93;
    --accent: #f5f5f7;
    --accent-fg: #000000;
    --danger-bg: #2a1215;
    --danger-border: #431418;
    --danger-fg: #ff7875;
  }
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--fg);
  font: 14px/1.4 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif;
  letter-spacing: -0.01em;
  -webkit-font-smoothing: antialiased;
}
main {
  max-width: 440px;
  margin: 0 auto;
  padding: 24px 14px 60px;
}
header {
  margin-bottom: 20px;
}
.header-row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
}
h1 {
  font-size: 1.25rem;
  font-weight: 600;
  color: var(--fg);
  letter-spacing: -0.02em;
}
.status-tag {
  font-size: 0.82rem;
  font-weight: 500;
  color: var(--muted);
}
.status-tag b {
  color: var(--fg);
  font-weight: 600;
}
#msg {
  font-size: 0.82rem;
  font-weight: 500;
  margin-top: 6px;
  color: var(--muted);
  min-height: 1.2em;
}
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 14px;
  margin-bottom: 14px;
}
.card-header {
  font-size: 0.72rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--muted);
  margin-bottom: 10px;
}
.btn-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
}
.btn-grid-4 {
  grid-template-columns: repeat(4, 1fr);
}
.full-width {
  grid-column: 1 / -1;
}
button, label.btn {
  appearance: none;
  border: 1px solid var(--border);
  background: var(--surface);
  color: var(--fg);
  padding: 11px 10px;
  border-radius: 8px;
  font-size: 0.88rem;
  font-weight: 500;
  text-align: center;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  user-select: none;
  transition: all 0.12s ease;
  min-height: 42px;
}
button:hover, label.btn:hover {
  border-color: var(--border-strong);
}
button:active, label.btn:active {
  transform: scale(0.98);
}
button.active {
  background: var(--accent);
  color: var(--accent-fg);
  border-color: var(--accent);
  font-weight: 600;
}
button.primary-btn {
  background: var(--accent);
  color: var(--accent-fg);
  border-color: var(--accent);
  font-weight: 600;
}
button.danger-btn {
  background: var(--danger-bg);
  border-color: var(--danger-border);
  color: var(--danger-fg);
  font-weight: 600;
}
button.danger-btn:active {
  background: var(--danger-fg);
  color: #ffffff;
}
input[type=file] {
  display: none;
}
.saved-photos {
  margin-top: 10px;
  padding-top: 10px;
  border-top: 1px solid var(--border);
}
.saved-label {
  font-size: 0.72rem;
  color: var(--muted);
  text-transform: uppercase;
  letter-spacing: 0.05em;
  margin-bottom: 6px;
}
.tag-container {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.photo-tag {
  font-size: 0.78rem;
  color: var(--muted);
  border: 1px solid var(--border);
  background: var(--bg);
  padding: 5px 9px;
  border-radius: 6px;
  cursor: pointer;
  transition: color 0.1s, border-color 0.1s;
}
.photo-tag:hover {
  color: var(--fg);
  border-color: var(--border-strong);
}
img#prev {
  display: block;
  width: 100%;
  max-height: 180px;
  object-fit: contain;
  margin-top: 10px;
  border: 1px solid var(--border);
  border-radius: 8px;
  background: var(--bg);
}
</style>
</head>
<body>
<main>
  <header>
    <div class="header-row">
      <h1>Caixa de Areia</h1>
      <div class="status-tag"><span id="sb">Topografia</span> · <b id="tes">Normal</b></div>
    </div>
    <div id="msg"></div>
  </header>

  <!-- TÓPICO 1: FOTOS -->
  <div class="card">
    <div class="card-header">Fotos na Areia</div>
    <div class="btn-grid">
      <button id="cam" class="primary-btn full-width">Tirar foto pela câmera</button>
      <label class="btn">Tirar foto no aparelho<input id="selfie" type="file" accept="image/*" capture="user"></label>
      <label class="btn">Escolher arquivo<input id="galeria" type="file" accept="image/*"></label>
      <button id="girar" class="full-width">Girar imagem 90°</button>
    </div>
    <div id="box-fotos" class="saved-photos" style="display:none">
      <div class="saved-label">Fotos salvas</div>
      <div id="chips" class="tag-container"></div>
    </div>
    <img id="prev" alt="" hidden>
  </div>

  <!-- TÓPICO 2: MODOS DO PROJETOR -->
  <div class="card">
    <div class="card-header">Modos do Projetor</div>
    <div class="btn-grid btn-grid-4" id="modos">
      <button data-m="original">Topografia</button>
      <button data-m="rosto">Foto</button>
      <button data-m="relevo">Relevo</button>
      <button data-m="esculpir">Esculpir</button>
    </div>
  </div>

  <!-- TÓPICO 3: CORES DO RELEVO -->
  <div class="card">
    <div class="card-header">Cores do Relevo</div>
    <div class="btn-grid btn-grid-4" id="temas"></div>
  </div>

  <!-- TÓPICO 4: BRINCADEIRAS -->
  <div class="card">
    <div class="card-header">Brincadeiras</div>
    <div class="btn-grid">
      <button data-b="tesouro">Caça ao tesouro</button>
      <button data-b="trex">Fóssil de T-Rex</button>
      <button data-b="peixes">Peixes</button>
      <button data-b="carros">Carrinhos</button>
      <button data-b="todos" class="full-width">Peixes e carrinhos</button>
      <button data-b="labirinto">Labirinto de água</button>
      <button data-b="labirintoNovo">Nova rodada</button>
      <button data-b="parar" class="danger-btn full-width">Parar brincadeiras</button>
    </div>
  </div>
</main>

<script>
const $ = s => document.querySelector(s);
let E = {};

function msg(t, err) {
  const el = $('#msg');
  el.textContent = t || '';
  el.style.color = err ? 'var(--danger-fg)' : 'var(--muted)';
}

async function post(u, b, isForm) {
  msg('Enviando...');
  try {
    const r = await fetch(u, {
      method: 'POST',
      body: isForm ? b : JSON.stringify(b),
      headers: isForm ? {} : {'Content-Type': 'application/json'}
    });
    let j = {};
    try { j = await r.json(); } catch(e) {}
    if (!r.ok || j.ok === false) {
      msg(j.msg || 'Erro', true);
    } else {
      msg(j.msg || 'Pronto');
    }
    await carrega();
    return j;
  } catch(e) {
    msg('Erro de conexão', true);
  }
}

async function carrega() {
  try {
    const r = await fetch('/estado');
    if (!r.ok) return;
    E = await r.json();

    $('#sb').textContent = {
      rosto: 'Modo Foto',
      original: 'Topografia',
      relevo: 'Relevo',
      esculpir: 'Esculpir',
      nenhum: 'Desligado'
    }[E.sandbox] || E.sandbox;

    document.querySelectorAll('#modos button').forEach(b => {
      const m = b.dataset.m;
      b.classList.toggle('active', E.sandbox === 'original' ? m === 'original' : (E.sandbox === 'rosto' && m === E.modo));
    });

    let sit = 'Normal';
    if (E.tesouro === 'achado') sit = 'Tesouro achado!';
    else if (E.tesouro === 'escondido') sit = 'Tesouro enterrado (5.5 cm)';
    else if (E.fossil === 'achado') sit = 'Fóssil escavado!';
    else if (E.fossil === 'escondido') sit = 'T-Rex enterrado (5.5 cm)';
    else if (E.labirinto === 'chegou') sit = 'Água chegou' + (E.labirinto_rodadas ? ' (' + E.labirinto_rodadas + ')' : '');
    else if (E.labirinto === 'correndo') sit = 'Água correndo';

    $('#tes').textContent = sit;

    const T = $('#temas');
    if (T && !T.children.length && E.temas) {
      for (const [t, n] of E.temas) {
        const b = document.createElement('button');
        b.textContent = n;
        b.dataset.t = t;
        b.onclick = () => post('/tema', {tema: t});
        T.appendChild(b);
      }
    }
    if (T) T.querySelectorAll('button').forEach(b => b.classList.toggle('active', b.dataset.t === E.tema));

    const chipsEl = $('#chips');
    const boxEl = $('#box-fotos');
    const fotos = [...(E.fotos_salvas || []), ...(E.fotos_willbert || [])];
    if (fotos.length) {
      boxEl.style.display = 'block';
      chipsEl.innerHTML = '';
      fotos.forEach(nome => {
        const tag = document.createElement('span');
        tag.className = 'photo-tag';
        tag.textContent = nome.length > 22 ? nome.slice(0, 19) + '...' : nome;
        tag.title = nome;
        tag.onclick = () => post('/escolher', {imagem: nome});
        chipsEl.appendChild(tag);
      });
    }

    if (E.tem_foto) {
      $('#prev').hidden = false;
      $('#prev').src = '/previa.png?' + Date.now();
    }
  } catch(e) {}
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

envia($('#selfie'));
envia($('#galeria'));
$('#cam').onclick = () => post('/foto', new FormData(), true);
document.querySelectorAll('[data-b]').forEach(b => b.onclick = () => post('/brincar', {acao: b.dataset.b}));
document.querySelectorAll('[data-m]').forEach(b => b.onclick = () => post('/modo', {modo: b.dataset.m}));
$('#girar').onclick = () => post('/ajuste', {giro: (E.giro + 90) % 360});

carrega();
setInterval(carrega, 5000);
</script>
</body>
</html>"""

if __name__ == "__main__":
    try:  # papel de parede com o endereço do painel (o timer atualizar-papel-parede repete a cada minuto)
        from atualizar_papel_parede import atualizar as atualiza_papel_parede
        atualiza_papel_parede()
    except Exception as erro:  # sem Pillow ou sem tela: o painel funciona do mesmo jeito
        print("papel de parede não atualizado:", erro)
    from waitress import serve
    serve(app, listen=f"0.0.0.0:{PORTA}", threads=4)
