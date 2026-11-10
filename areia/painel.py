"""Painel do celular da caixa de areia: foto, modos e ajustes.
Escuta em todas as interfaces, mas só responde à própria máquina e à rede Tailscale
(100.64.0.0/10): quem chega aqui já é um aparelho do Victor. Rodar com o venv da pasta:
    venv/bin/python painel.py
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
              ("127.0.0.0/8", "::1/128", "100.64.0.0/10", "fd7a:115c:a1e0::/48")]
app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 25 * 1024 * 1024
trava = threading.Lock()


@app.before_request
def so_tailscale():
    try:
        ip = ipaddress.ip_address((request.remote_addr or "").split("%")[0])
    except ValueError:
        abort(403)
    if ip.version == 6 and ip.ipv4_mapped:
        ip = ip.ipv4_mapped
    if not any(ip in rede for rede in PERMITIDAS):
        abort(403)


def resposta(fn):
    """Executa fn() com a trava; erros conhecidos viram mensagem para o celular."""
    with trava:
        try:
            return jsonify(ok=True, msg=fn() or "feito")
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
    e["tema"] = areia.tema_atual()
    e["temas"] = [[t, areia.NOMES_TEMAS.get(t, t)] for t in areia.temas()]
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
                # do Willbert ("grave o meu rosto") vem como willbert-<nome>.jpg; do celular, sem nome
                base = re.sub(r"[^A-Za-z0-9_-]+", "-", os.path.splitext(arq.filename)[0])[:40]
                prefixo = base if base.startswith("willbert-") else "celular"
                img = areia.guarda(origem, time.strftime(f"{prefixo}-%Y-%m-%d-%H%M%S") + ext)
        else:
            img = areia.foto_da_camera()
        areia.mostra(img, modo="rosto")
        return f"{os.path.basename(img)} no projetor"
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
        return {"rosto": "modo Foto", "original": "topografia"}.get(m, m)
    return resposta(faz)


@app.post("/tema")
def tema():
    def faz():
        t = areia.escolhe_tema((request.get_json(silent=True) or {}).get("tema") or "")
        return f"cores: {areia.NOMES_TEMAS.get(t, t)}"
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
        else:
            areia.ajusta(e)
        return "ajustado"
    return resposta(faz)


PAGINA = """<!doctype html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Caixa de areia</title>
<style>
:root{--bg:#f6f3ee;--fg:#222;--card:#fff;--muted:#777;--acc:#b5651d;--line:#e3ddd3}
@media (prefers-color-scheme:dark){:root{--bg:#171513;--fg:#eee;--card:#221f1c;--muted:#9a948c;--acc:#e0954a;--line:#37322c}}
body{margin:0;background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif}
main{max-width:520px;margin:auto;padding:16px}
h1{font-size:1.3rem;margin:4px 0 12px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.row{display:flex;gap:8px;flex-wrap:wrap}
button,label.btn{flex:1;min-width:120px;padding:14px 10px;border-radius:10px;border:1px solid var(--line);
 background:var(--bg);color:var(--fg);font-size:1rem;text-align:center;cursor:pointer}
button.on{background:var(--acc);color:#fff;border-color:var(--acc)}
.big{background:var(--acc)!important;color:#fff!important;border-color:var(--acc)!important;font-weight:600}
input[type=file]{display:none}
img{width:100%;border-radius:10px;display:block;margin-top:10px}
.small{color:var(--muted);font-size:.9rem}
input[type=range]{width:100%}
#msg{min-height:1.2em}
</style></head><body><main>
<h1>Caixa de areia</h1>
<div class="card"><div class="small">Projetor: <b id="sb">...</b></div><div id="msg" class="small"></div></div>
<div class="card">
 <div class="row">
  <label class="btn big">Tirar selfie<input id="selfie" type="file" accept="image/*" capture="user"></label>
  <label class="btn">Escolher foto<input id="galeria" type="file" accept="image/*"></label>
 </div>
 <div class="row" style="margin-top:8px"><button id="cam">Foto pela câmera do laboratório</button></div>
 <img id="prev" alt="" hidden>
</div>
<div class="card"><div class="small">Modo</div><div class="row" id="modos">
 <button data-m="rosto">Foto</button><button data-m="relevo">Relevo</button>
 <button data-m="esculpir">Esculpir</button></div>
 <p class="small">Foto: a imagem se deforma com a areia. Esculpir: vermelho = falta areia, azul = sobra.</p>
 <div class="row"><button data-m="original">Topografia</button></div>
</div>
<div class="card"><div class="small">Cores do relevo</div><div class="row" id="temas"></div></div>
<div class="card"><div class="small">Posição da foto na areia</div><div class="row">
 <button id="girar">Girar 90°</button><button id="esp">Espelhar</button></div></div>
<div class="card"><div class="small">Deformação pela areia: <span id="vz"></span></div>
 <input id="zoom" type="range" min="0" max="0.08" step="0.005">
 <div class="small">Sombra do relevo: <span id="vs"></span></div>
 <input id="sombra" type="range" min="0" max="3" step="0.1">
 <div class="small">Cores do relevo por cima: <span id="vm"></span></div>
 <input id="mistura" type="range" min="0" max="0.8" step="0.05">
</div>
</main><script>
const $=s=>document.querySelector(s);let E={};
function msg(t){$('#msg').textContent=t||''}
async function post(u,b,isForm){msg('enviando...');
 const r=await fetch(u,{method:'POST',body:isForm?b:JSON.stringify(b),headers:isForm?{}:{'Content-Type':'application/json'}});
 let j={};try{j=await r.json()}catch(e){} msg(j.msg||(r.ok?'feito':'erro')); await carrega(); return j}
async function carrega(){E=await (await fetch('/estado')).json();
 $('#sb').textContent={rosto:'modo Foto',original:'topografia',nenhum:'desligado'}[E.sandbox]||E.sandbox;
 document.querySelectorAll('#modos button,[data-m=original]').forEach(b=>b.classList.toggle('on',
   E.sandbox==='original'?b.dataset.m==='original':(E.sandbox==='rosto'&&b.dataset.m===E.modo)));
 for(const k of ['zoom','sombra','mistura'])$('#'+k).value=E.ajuste[k];
 $('#vz').textContent=Math.round(E.ajuste.zoom*1000)/10+'%/cm';$('#vs').textContent=E.ajuste.sombra;
 $('#vm').textContent=Math.round(E.ajuste.mistura*100)+'%';
 const T=$('#temas');if(!T.children.length)for(const [t,n] of E.temas){const b=document.createElement('button');
  b.textContent=n;b.dataset.t=t;b.onclick=()=>post('/tema',{tema:t});T.appendChild(b)}
 T.querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.dataset.t===E.tema));
 if(E.tem_foto){$('#prev').hidden=false;$('#prev').src='/previa.png?'+Date.now()}}
function envia(inp){inp.onchange=()=>{if(!inp.files[0])return;const f=new FormData();f.append('foto',inp.files[0]);post('/foto',f,true);inp.value=''}}
envia($('#selfie'));envia($('#galeria'));
$('#cam').onclick=()=>post('/foto',new FormData(),true);
document.querySelectorAll('[data-m]').forEach(b=>b.onclick=()=>post('/modo',{modo:b.dataset.m}));
$('#girar').onclick=()=>post('/ajuste',{giro:(E.giro+90)%360});
$('#esp').onclick=()=>post('/ajuste',{espelho:!E.espelho});
for(const k of ['zoom','sombra','mistura'])$('#'+k).onchange=e=>post('/ajuste',{[k]:e.target.value});
carrega();setInterval(carrega,10000);
</script></body></html>"""


if __name__ == "__main__":
    from waitress import serve
    serve(app, listen=f"0.0.0.0:{PORTA}", threads=4)
