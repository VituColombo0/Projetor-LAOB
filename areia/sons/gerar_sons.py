"""Gera os sons de ambiente da caixa de areia (areia/sons/amb_*, jogo_* e os pontuais) por síntese.

Rodar uma vez quando quiser refazer (precisa só de numpy; os .wav ficam no repositório):
    uv run --with numpy python areia/sons/gerar_sons.py

Tudo em 22,05 kHz mono. Os fundos (amb_* e jogo_*) têm 20 s e são CIRCULARES: o ruído é filtrado no
domínio da frequência (o resultado dá a volta sem emenda), as ondulações lentas têm um número inteiro de
ciclos em 20 s e os eventos (bolhas, gotas, notas) que passam do fim continuam no começo. Por isso o laço
não tem estalo nem "respiro" na emenda. Os pontuais (sino, passarinho, ronco, bichos...) são curtos e
tocados de vez em quando pelo motor do som.py, por cima do fundo.

Os sons de bicho reaproveitam os efeitos que o Victor aprovou em 17/11 (passaro.wav, cachorro.wav,
splash.wav) com pequenas variações de altura; os novos (pato, bolhas, barco, carrinho) seguem o mesmo
jeito: limpos, curtos e macios.
"""
import os
import wave

import numpy as np

SR = 22050
DUR = 20.0
N = int(SR * DUR)
PASTA = os.path.dirname(os.path.abspath(__file__))
rng = np.random.default_rng(20261119)


# ---------- ferramentas ----------

def _freqs(n):
    f = np.fft.rfftfreq(n, 1 / SR)
    f[0] = 1e-3
    return f


def ruido(n, lo, hi, incl=0.0, ordem=3):
    """Ruído na faixa lo..hi Hz (incl 1 = rosa, 2 = marrom), circular, desvio 1."""
    f = _freqs(n)
    g = np.ones_like(f)
    if lo > 0:
        g /= np.sqrt(1 + (lo / f) ** (2 * ordem))
    g /= np.sqrt(1 + (f / hi) ** (2 * ordem))
    g *= (f / 1000.0) ** (-incl / 2)
    x = np.fft.irfft(np.fft.rfft(rng.standard_normal(n)) * g, n)
    return x / (np.std(x) + 1e-12)


def banda(n, fc, q):
    """Ruído numa faixa estreita em volta de fc (assobio do vento), circular."""
    f = _freqs(n)
    g = 1 / (1 + ((f - fc) / (fc / (2 * q))) ** 2)
    x = np.fft.irfft(np.fft.rfft(rng.standard_normal(n)) * g, n)
    return x / (np.std(x) + 1e-12)


def ondula(n, fmax, minimo=0.0, expoente=1.0):
    """Envoltória lenta e aleatória (até fmax Hz), circular, entre minimo e 1."""
    x = ruido(n, 0, fmax, ordem=2)
    x = (x - x.min()) / (x.max() - x.min())
    return minimo + (1 - minimo) * x ** expoente


def tempo(n):
    return np.arange(n) / SR


def poe(buf, ev, pos):
    """Soma o evento em buf a partir de pos; o que passa do fim continua no começo (laço)."""
    idx = (int(pos) + np.arange(len(ev))) % len(buf)
    buf[idx] += ev


def filtra(x, lo=0, hi=None, ordem=3):
    """Filtro passa-faixa para sons curtos (com folga de zeros, para não dar a volta)."""
    n = len(x)
    m = 1 << int(np.ceil(np.log2(n * 2)))
    f = _freqs(m)
    g = np.ones_like(f)
    if lo > 0:
        g /= np.sqrt(1 + (lo / f) ** (2 * ordem))
    if hi:
        g /= np.sqrt(1 + (f / hi) ** (2 * ordem))
    return np.fft.irfft(np.fft.rfft(x, m) * g, m)[:n]


def sala(x, rt=1.0, mistura=0.25, circular=False):
    """Reverberação simples (ruído com decaimento). circular=True para os fundos (laço sem emenda)."""
    k = int(rt * SR)
    ir = rng.standard_normal(k) * np.exp(-6.9 * np.arange(k) / k)
    ir = filtra(ir, 150, 5000)
    ir /= np.sqrt((ir ** 2).sum())
    if circular:
        h = np.zeros(len(x))
        h[:k] = ir
        mol = np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(h), len(x))
        return (1 - mistura) * x + mistura * mol * (np.std(x) / (np.std(mol) + 1e-12))
    m = 1 << int(np.ceil(np.log2(len(x) + k)))
    mol = np.fft.irfft(np.fft.rfft(x, m) * np.fft.rfft(ir, m), m)[:len(x) + k]
    y = np.concatenate([x, np.zeros(k)])
    return (1 - mistura) * y + mistura * mol * (np.std(x) / (np.std(mol[:len(x)]) + 1e-12))


def nota_periodica(f):
    """Arredonda a frequência para um número inteiro de ciclos em 20 s (laço sem emenda)."""
    return round(f * DUR) / DUR


def bolha(f0, dur, subida=1.6):
    n = int(dur * SR)
    t = tempo(n)
    f = f0 * (1 + (subida - 1) * t / dur)
    env = (1 - np.exp(-t / 0.003)) * np.exp(-t / (dur * 0.35))
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * env


def sino(f, dur=1.6, parciais=((1, 1), (2.756, .35), (5.404, .14), (8.933, .05)), decai=0.55):
    n = int(dur * SR)
    t = tempo(n)
    x = np.zeros(n)
    for r, a in parciais:
        if f * r < SR / 2 - 500:
            x += a * np.sin(2 * np.pi * f * r * t) * np.exp(-t / (decai / r ** 0.6))
    return x * (1 - np.exp(-t / 0.0015))


def chirp(f_ini, f_fim, dur, forma="reta"):
    n = int(dur * SR)
    t = tempo(n) / dur
    if forma == "arco":
        f = f_ini + (f_fim - f_ini) * np.sin(np.pi * t)
    else:
        f = f_ini + (f_fim - f_ini) * t
    fase = 2 * np.pi * np.cumsum(f) / SR
    return (np.sin(fase) + 0.15 * np.sin(2 * fase)) * np.sin(np.pi * t) ** 2


def junta(*pedacos):
    """[(som, início em s)] -> um som só."""
    fim = max(int(t * SR) + len(s) for s, t in pedacos)
    y = np.zeros(fim)
    for s, t in pedacos:
        i = int(t * SR)
        y[i:i + len(s)] += s
    return y


def le_wav(nome):
    with wave.open(os.path.join(PASTA, nome)) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(float) / 32768
        return x, w.getframerate()


def reamostra(x, sr_de, fator=1.0):
    """De sr_de para 22,05 kHz; fator > 1 deixa mais agudo e curto."""
    x = filtra(x, 0, min(SR, sr_de) / 2 * 0.9 / max(1.0, fator), ordem=6)
    passo = sr_de / SR * fator
    t = np.arange(0, len(x) - 1, passo)
    return np.interp(t, np.arange(len(x)), x)


def escreve(nome, x, fundo=False):
    x = np.asarray(x, float)
    if fundo:   # fundos: mesmo volume médio (-20 dB), sem passar de -1 dB no pico
        x = x / (np.sqrt((x ** 2).mean()) + 1e-12) * 10 ** (-20 / 20)
        pico = np.abs(x).max()
        if pico > 10 ** (-1 / 20):
            x *= 10 ** (-1 / 20) / pico
    else:       # pontuais: pico em -3 dB, com entrada e saída macias (sem estalo)
        x = x / (np.abs(x).max() + 1e-12) * 10 ** (-3 / 20)
        k = min(len(x) // 4, int(0.004 * SR))
        x[:k] *= np.linspace(0, 1, k)
        k = min(len(x) // 4, int(0.03 * SR))
        x[-k:] *= np.linspace(1, 0, k)
    dados = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    with wave.open(os.path.join(PASTA, nome + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(dados.tobytes())
    print(f"{nome:22s} {len(x) / SR:5.1f} s  {os.path.getsize(os.path.join(PASTA, nome + '.wav')) // 1024:4d} KB")


# ---------- fundos dos temas ----------

def amb_classico():
    """Brisa bem leve e folhas mexendo de leve (os passarinhos vêm à parte, de vez em quando)."""
    vento = ruido(N, 120, 900, incl=1.2) * ondula(N, 0.2, 0.35)
    folhas = ruido(N, 1800, 6000, incl=0.5) * ondula(N, 0.15, 0.0, 2.5) * ondula(N, 7, 0.3)
    return sala(vento + 0.18 * folhas, 0.8, 0.2, circular=True)


def amb_inverno():
    """Vento frio: chiado mais agudo, rajadas e um assobio que aparece e some."""
    vento = ruido(N, 200, 2400, incl=0.8) * ondula(N, 0.25, 0.25, 1.5)
    assobio = sum(p * banda(N, fc, 45) * ondula(N, 0.12, 0.0, 3)
                  for fc, p in ((560, 1.0), (745, 0.7), (980, 0.45)))
    grave = ruido(N, 40, 160, incl=1) * ondula(N, 0.1, 0.3)
    return sala(vento + 0.3 * assobio + 0.35 * grave, 1.0, 0.2, circular=True)


def amb_deserto():
    """Calor: vento seco e grave, areia chiando nas rajadas e cigarras bem ao longe."""
    t = tempo(N)
    vento = ruido(N, 90, 1100, incl=1.3) * ondula(N, 0.12, 0.45)
    areia = ruido(N, 2500, 6000) * ondula(N, 0.2, 0.0, 3) * ondula(N, 6, 0.4)
    pulso = (0.5 + 0.5 * np.sin(2 * np.pi * 150 * t)) ** 3
    cigarra = (np.sin(2 * np.pi * 3200 * t) * pulso) * ondula(N, 0.05, 0.0, 4)
    return sala(vento + 0.16 * areia + 0.05 * cigarra, 0.6, 0.15, circular=True)


def amb_vulcao():
    """Lava borbulhando, estalinhos e um ronco grave constante por baixo."""
    ronco = ruido(N, 30, 180, incl=2) * ondula(N, 0.08, 0.5)
    bolhas = np.zeros(N)
    t = 0.0
    while t < DUR:
        n = rng.integers(1, 4)   # bolhas em grupinhos, como lava grossa
        for _ in range(n):
            f0 = np.exp(rng.uniform(np.log(70), np.log(240)))
            b = bolha(f0, rng.uniform(0.06, 0.18), rng.uniform(1.3, 1.9))
            pop = filtra(rng.standard_normal(len(b)), 80, 600) * np.exp(-tempo(len(b)) / 0.02) * 0.4
            poe(bolhas, (b + pop) * rng.uniform(0.3, 1.0), (t + rng.uniform(0, 0.25)) * SR)
        t += rng.exponential(0.45)
    estalos = np.zeros(N)
    for _ in range(40):
        k = int(rng.uniform(0.003, 0.008) * SR)
        poe(estalos, filtra(rng.standard_normal(k), 1500, 5000) * np.exp(-np.arange(k) / (k / 4)) * rng.uniform(.2, 1),
            rng.uniform(0, N))
    # camadas esparsas medidas pelo pico: pelo desvio, os estalinhos (40 em 20 s) viravam estalos fortes
    return sala(0.45 * ronco + 3.5 * bolhas / np.abs(bolhas).max() + 0.35 * estalos / np.abs(estalos).max(),
                0.8, 0.25, circular=True)


def _ondas(atraso=0.0, desce=3.8):
    env = np.zeros(N)
    inicios = np.array([0.4, 7.0, 13.5]) + rng.uniform(-0.6, 0.6, 3)
    for t0 in inicios:
        sobe = rng.uniform(1.8, 2.6)
        n = int((sobe + desce * 1.6) * SR)
        t = tempo(n)
        e = np.where(t < sobe, np.sin(np.pi / 2 * t / sobe) ** 2, np.exp(-(t - sobe) / desce * 2.2))
        poe(env, e * rng.uniform(0.7, 1.0), (t0 + atraso) * SR)
    return env / env.max()


def amb_oceano():
    """Ondas chegando e voltando, espuma depois de cada uma."""
    e = _ondas()
    espuma_env = np.roll(e, int(0.6 * SR)) ** 1.5
    corpo = ruido(N, 90, 900, incl=1) * (0.25 + e)
    espuma = ruido(N, 700, 6000, incl=0.3) * (0.08 + espuma_env)
    return sala(corpo + 0.45 * espuma, 1.2, 0.25, circular=True)


def amb_marte():
    """Vento ralo e estranho: faixas estreitas que cantam baixinho e um zumbido grave lento."""
    t = tempo(N)
    vento = ruido(N, 500, 3500, incl=0.5) * ondula(N, 0.15, 0.1, 2)
    canto = sum(p * banda(N, fc, 60) * ondula(N, 0.1, 0.0, 2) for fc, p in ((330, 1.0), (495, 0.7), (742, 0.5)))
    zumbido = (np.sin(2 * np.pi * 55 * t) + 0.5 * np.sin(2 * np.pi * 82.5 * t)) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.05 * t))
    return sala(0.5 * vento + 0.5 * canto + 0.18 * zumbido, 2.0, 0.45, circular=True)


def amb_arcoiris():
    """Leve e alegre: um acorde macio respirando e brilhinhos de sino (escala pentatônica de dó)."""
    t = tempo(N)
    pad = np.zeros(N)
    for f in (261.63, 329.63, 392.0, 587.33):
        f = nota_periodica(f)
        pad += (np.sin(2 * np.pi * f * t) + 0.25 * np.sin(4 * np.pi * f * t)) * ondula(N, 0.1, 0.3)
    brilhos = np.zeros(N)
    escala = (1046.5, 1174.7, 1318.5, 1568.0, 1760.0, 2093.0)
    tt = 0.3
    while tt < DUR:
        poe(brilhos, sino(rng.choice(escala), 1.2, decai=0.45) * rng.uniform(0.4, 1.0), tt * SR)
        tt += rng.uniform(1.2, 2.8)
    pad /= np.std(pad)
    brilhos /= np.std(brilhos)
    return sala(0.45 * pad + 0.6 * brilhos, 1.6, 0.35, circular=True)


# ---------- fundos dos jogos ----------

def _riacho(densidade, f_lo, f_hi, chiado):
    bolhas = np.zeros(N)
    for _ in range(int(densidade * DUR)):
        f0 = np.exp(rng.uniform(np.log(f_lo), np.log(f_hi)))
        poe(bolhas, bolha(f0, rng.uniform(0.015, 0.05), rng.uniform(1.3, 2.0)) * rng.exponential(0.5),
            rng.uniform(0, N))
    ch = ruido(N, 250, 4000, incl=0.8) * ondula(N, 4, 0.5)
    return sala(bolhas / np.std(bolhas) + chiado * ch, 0.6, 0.2, circular=True)


def jogo_riacho():
    """Água correndo na nascente (rota com paradas, labirinto)."""
    return _riacho(20, 400, 1400, 0.5)


def jogo_correnteza():
    """Corrida de rios: água mais forte e apressada."""
    return _riacho(34, 260, 1100, 0.9) + 0.3 * ruido(N, 150, 2500, incl=1) * ondula(N, 0.3, 0.6)


def jogo_chuva():
    """Salve a vila: chuva miúda caindo e água escorrendo."""
    gotas = np.zeros(N)
    banco = []
    for _ in range(24):
        k = int(rng.uniform(0.006, 0.02) * SR)
        f0 = rng.uniform(1200, 3800)
        s = np.sin(2 * np.pi * f0 * np.arange(k) / SR) * np.exp(-np.arange(k) / (k / 5))
        s += filtra(rng.standard_normal(k), 1500, 5000) * np.exp(-np.arange(k) / (k / 8)) * 0.6
        banco.append(s)
    for _ in range(int(220 * DUR)):
        poe(gotas, banco[rng.integers(len(banco))] * rng.lognormal(-1.2, 0.6), rng.uniform(0, N))
    base = ruido(N, 300, 4500, incl=0.8) * ondula(N, 0.2, 0.75)
    escorre = ruido(N, 60, 300, incl=1) * ondula(N, 0.3, 0.5)
    return sala(0.6 * base + 0.8 * gotas / np.std(gotas) + 0.3 * escorre, 0.7, 0.2, circular=True)


def jogo_tensao():
    """Vulcão em erupção: um coração grave que vai e vem e um zumbido baixo batendo devagar."""
    t = tempo(N)
    batida = np.zeros(N)
    k = int(0.25 * SR)
    tk = tempo(k)
    tum = (np.sin(2 * np.pi * 80 * tk) + 0.6 * np.sin(2 * np.pi * 160 * tk) + 0.3 * np.sin(2 * np.pi * 240 * tk)
           + 0.15 * np.sin(2 * np.pi * 320 * tk))
    tum *= (1 - np.exp(-tk / 0.004)) * np.exp(-tk / 0.08)
    for i in range(24):   # 72 batidas por minuto: 24 em 20 s, laço exato
        poe(batida, tum, i * DUR / 24 * SR)
        poe(batida, 0.6 * tum, (i * DUR / 24 + 0.28) * SR)
    batida *= ondula(N, 0.05, 0.25)
    zumbido = (np.sin(2 * np.pi * 110 * t) + np.sin(2 * np.pi * 111.5 * t)
               + 0.3 * np.sin(2 * np.pi * 220 * t) + 0.3 * np.sin(2 * np.pi * 223 * t))
    return sala(batida / np.std(batida) + 0.25 * zumbido / np.std(zumbido), 1.0, 0.2, circular=True)


def jogo_misterio():
    """Tesouro e fósseis: acorde menor escuro e notinhas soltas de celesta."""
    t = tempo(N)
    pad = np.zeros(N)
    for f, a in ((110.0, 1.0), (164.8, 0.7), (220.0, 0.4), (261.63, 0.35)):
        f = nota_periodica(f)
        pad += a * np.sin(2 * np.pi * f * t) * ondula(N, 0.08, 0.3)
    caverna = ruido(N, 100, 500, incl=1) * ondula(N, 0.1, 0.3)
    notas = np.zeros(N)
    tt = 0.8
    while tt < DUR:
        f = rng.choice((440.0, 523.25, 587.33, 659.25, 783.99))
        poe(notas, sino(f, 2.0, parciais=((1, 1), (3.0, .12), (4.0, .08)), decai=0.8) * rng.uniform(.5, 1), tt * SR)
        tt += rng.uniform(2.4, 4.0)
    return sala(0.5 * pad / np.std(pad) + 0.15 * caverna + 0.55 * notas / np.std(notas), 2.0, 0.4, circular=True)


def jogo_atelie():
    """Copie o relevo: acorde calmo e notas de marimba bem espaçadas, para concentrar."""
    t = tempo(N)
    pad = np.zeros(N)
    for f in (174.61, 220.0, 261.63, 329.63):
        f = nota_periodica(f)
        pad += (np.sin(2 * np.pi * f * t) + 0.2 * np.sin(4 * np.pi * f * t)) * ondula(N, 0.08, 0.35)
    notas = np.zeros(N)
    tt = 0.5
    while tt < DUR:
        f = rng.choice((349.23, 392.0, 440.0, 523.25, 587.33))
        poe(notas, sino(f, 1.0, parciais=((1, 1), (4.0, .1), (9.9, .03)), decai=0.35) * rng.uniform(.5, 1), tt * SR)
        tt += rng.uniform(1.8, 3.4)
    return sala(0.5 * pad / np.std(pad) + 0.5 * notas / np.std(notas), 1.2, 0.3, circular=True)


# ---------- pontuais dos temas e jogos ----------

def passarinho(variante):
    """Passarinho ao longe (tema clássico): frases curtas, abafadas pela distância."""
    pedacos = []
    t = 0.0
    if variante == 3:   # trinado
        for i in range(9):
            pedacos.append((chirp(3000 if i % 2 else 3300, 3150, 0.035, "arco"), t))
            t += 0.05
    else:
        for _ in range(rng.integers(2, 5) if variante == 1 else 3):
            a = rng.uniform(2400, 3300)
            pedacos.append((chirp(a, a + rng.choice((-1, 1)) * rng.uniform(400, 800),
                                  rng.uniform(0.05, 0.11), "arco" if variante == 2 else "reta"), t))
            t += rng.uniform(0.12, 0.22)
    return sala(filtra(junta(*pedacos), 1500, 4300), 0.6, 0.3)


def sino_neve(variante):
    """Sininho de vento no frio: 2 a 4 notas agudas de vidro."""
    escala = (1568.0, 1760.0, 2093.0, 2349.3, 2637.0)
    n = (2, 3, 4)[variante - 1]
    pedacos, t = [], 0.0
    for _ in range(n):
        pedacos.append((sino(rng.choice(escala), 1.8, decai=0.6) * rng.uniform(0.6, 1.0), t))
        t += rng.uniform(0.15, 0.45)
    return sala(junta(*pedacos), 1.2, 0.3)


def neve_galho():
    """Neve caindo de um galho: um "fuff" macio e uns grãozinhos depois."""
    k = int(0.6 * SR)
    t = tempo(k)
    fuff = filtra(rng.standard_normal(k), 200, 1800) * (1 - np.exp(-t / 0.03)) * np.exp(-t / 0.12)
    graos = np.zeros(int(1.2 * SR))
    for _ in range(40):
        j = int(rng.uniform(0.05, 1.0) ** 1.5 * len(graos) * 0.8)
        m = int(0.004 * SR)
        graos[j:j + m] += filtra(rng.standard_normal(m), 2000, 6000) * rng.uniform(0.05, 0.25) * (1 - j / len(graos))
    return sala(junta((fuff, 0.0), (graos, 0.1)), 0.6, 0.2)


def cigarra(variante):
    """Cigarra no calor: o chiado cresce, fica e some (macio, sem ser estridente)."""
    dur = (4.5, 6.0)[variante - 1]
    t = tempo(int(dur * SR))
    fc = (3100, 3350)[variante - 1]
    som = np.sin(2 * np.pi * fc * t + 0.3 * np.sin(2 * np.pi * 7 * t)) + 0.3 * np.sin(2 * np.pi * 2 * fc * t)
    pulso = (0.5 + 0.5 * np.sin(2 * np.pi * 160 * t)) ** 3
    trilo = 0.6 + 0.4 * (0.5 + 0.5 * np.sin(2 * np.pi * 5.5 * t))
    env = np.minimum(1, t / 1.6) ** 2 * np.minimum(1, (dur - t) / 2.0) ** 2
    ruido_c = filtra(rng.standard_normal(len(t)), 2600, 4200) * 0.3
    return filtra((som + ruido_c) * pulso * trilo * env, 0, 4500)


def ronco_vulcao(variante):
    """Ronco grave e distante do vulcão, rolando por uns segundos."""
    dur = (5.0, 6.5)[variante - 1]
    n = int(dur * SR)
    t = tempo(n)
    env = np.zeros(n)
    for c in rng.uniform(0.3, dur * 0.55, 3 + variante):
        env += np.exp(-((t - c) / 0.9) ** 2) * rng.uniform(0.5, 1.0)
    env *= np.minimum(1, (dur - t) / 1.5)
    base = filtra(rng.standard_normal(n), 30, 340, ordem=4) * 2.5
    sub = np.sin(2 * np.pi * 48 * t) + 0.6 * np.sin(2 * np.pi * 96 * t) + 0.4 * np.sin(2 * np.pi * 144 * t) + 0.25 * np.sin(2 * np.pi * 192 * t)
    return filtra((base + 0.4 * sub) * env, 0, 520)


def gaivota(variante):
    """Gaivota ao longe (oceano): 2 ou 3 gritinhos descendo."""
    pedacos, t = [], 0.0
    for i in range(variante + 1):
        d = 0.32 if i < variante else 0.42
        n = int(d * SR)
        tt = tempo(n) / d
        f = 1150 - 330 * tt + 80 * np.sin(np.pi * tt)
        fase = 2 * np.pi * np.cumsum(f) / SR
        voz = sum(np.sin(k * fase) / k for k in range(1, 6)) * (1 + 0.15 * np.sin(2 * np.pi * 40 * tempo(n)))
        pedacos.append((voz * np.sin(np.pi * tt) ** 1.5, t))
        t += d + rng.uniform(0.08, 0.16)
    return sala(filtra(junta(*pedacos), 600, 3200), 1.2, 0.35)


def eco_marte(variante):
    """Um som estranho que desce e volta em eco (marte)."""
    d = 1.4
    n = int(d * SR)
    t = tempo(n)
    f0, f1 = ((880, 520), (660, 410))[variante - 1]
    f = (f0 + (f1 - f0) * t / d) * (1 + 0.01 * np.sin(2 * np.pi * 5 * t))
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.sin(np.pi * t / d) ** 2
    return sala(junta((s, 0), (0.5 * s, 0.38), (0.25 * s, 0.76), (0.12 * s, 1.14)), 1.8, 0.4)


def brilho(variante):
    """Arco-íris: um arpejo de sino subindo."""
    escala = (1046.5, 1174.7, 1318.5, 1568.0, 1760.0, 2093.0, 2349.3)
    i0 = rng.integers(0, 3)
    n = (3, 4, 5)[variante - 1]
    return sala(junta(*[(sino(escala[i0 + i], 1.2, decai=0.45) * (1 - 0.1 * i), i * 0.11) for i in range(n)]), 1.4, 0.35)


def trovao(variante):
    """Trovão distante (salve a vila)."""
    dur = (6.0, 7.5)[variante - 1]
    n = int(dur * SR)
    t = tempo(n)
    env = np.zeros(n)
    for c in np.sort(rng.uniform(0.2, dur * 0.5, 5)):
        env += np.exp(-np.maximum(0, t - c) / 0.8) * (t >= c) * rng.uniform(0.4, 1.0)
    env = filtra(env, 0, 30) * np.minimum(1, (dur - t) / 2.0)
    return filtra(filtra(rng.standard_normal(n), 30, 400, ordem=2) * env, 0, 600)


def gota(variante):
    """Gota pingando numa poça de caverna (labirinto)."""
    d = 0.05
    f0 = (900, 1200)[variante - 1]
    return sala(bolha(f0, d, 1.8), 1.5, 0.45)


# ---------- bichos ----------

def pato(variante):
    """Quá-quá: voz anasalada com formantes de pato."""
    pedacos, t = [], 0.0
    n_quacks = (2, 3)[variante - 1]
    for i in range(n_quacks):
        d = 0.14 if i < n_quacks - 1 else 0.18
        n = int(d * SR)
        tt = tempo(n)
        f = np.linspace(300, 240, n) * (0.97 if i == n_quacks - 1 else 1.0)
        fase = 2 * np.pi * np.cumsum(f) / SR
        voz = sum(np.sin(k * fase) / k ** 0.9 for k in range(1, 30) if k * 300 < SR / 2 - 500)
        voz *= (1 - np.exp(-tt / 0.008)) * np.minimum(1, (d - tt) / 0.04) * (1 + 0.2 * np.sin(2 * np.pi * 70 * tt))
        pedacos.append((voz, t))
        t += d + 0.07
    x = junta(*pedacos)
    m = 1 << int(np.ceil(np.log2(len(x) * 2)))
    f = _freqs(m)
    g = sum(a * np.exp(-((f - fc) / bw) ** 2) for fc, bw, a in ((1000, 250, 1.0), (1600, 300, 0.7), (2800, 400, 0.3)))
    g *= 1 / np.sqrt(1 + (400 / f) ** 4)
    x = np.fft.irfft(np.fft.rfft(x, m) * g, m)[:len(x)]
    return sala(x, 0.4, 0.15)


def bolhas_peixe(variante):
    """Blub-blub: 2 a 4 bolhas subindo."""
    pedacos, t = [], 0.0
    for _ in range((2, 3)[variante - 1] + rng.integers(0, 2)):
        pedacos.append((bolha(rng.uniform(260, 420), rng.uniform(0.06, 0.1), rng.uniform(1.5, 2.2)), t))
        t += rng.uniform(0.08, 0.16)
    return sala(junta(*pedacos), 0.5, 0.25)


def barco():
    """Apito de barco bem ao longe (quinta grave, macia)."""
    d = 1.6
    t = tempo(int(d * SR))
    s = sum((np.sin(2 * np.pi * 110 * k * t) + 0.8 * np.sin(2 * np.pi * 165 * k * t)) / k for k in range(1, 8))
    s *= (1 + 0.004 * np.sin(2 * np.pi * 4 * t))
    env = np.minimum(1, t / 0.25) * np.minimum(1, (d - t) / 0.5)
    return sala(filtra(s * env, 0, 1100), 2.0, 0.4)


def carrinho():
    """Bibi de carrinho de brinquedo, curtinho e macio."""
    def bip(d=0.09):
        t = tempo(int(d * SR))
        s = sum(w * (np.sin(2 * np.pi * 660 * k * t) + np.sin(2 * np.pi * 830 * k * t)) for k, w in ((1, 1), (3, .3), (5, .12)))
        return s * np.minimum(1, t / 0.006) * np.minimum(1, (d - t) / 0.02)
    return sala(filtra(junta((bip(), 0), (bip(0.11), 0.16)), 0, 3000), 0.4, 0.15)


def main():
    fundos = {"amb_classico": amb_classico, "amb_inverno": amb_inverno, "amb_deserto": amb_deserto,
              "amb_vulcao": amb_vulcao, "amb_oceano": amb_oceano, "amb_marte": amb_marte,
              "amb_arcoiris": amb_arcoiris, "jogo_riacho": jogo_riacho, "jogo_correnteza": jogo_correnteza,
              "jogo_chuva": jogo_chuva, "jogo_tensao": jogo_tensao, "jogo_misterio": jogo_misterio,
              "jogo_atelie": jogo_atelie}
    for nome, fn in fundos.items():
        escreve(nome, fn(), fundo=True)
    for v in (1, 2, 3):
        escreve(f"passarinho_{v}", passarinho(v))
        escreve(f"sino_neve_{v}", sino_neve(v))
        escreve(f"brilho_{v}", brilho(v))
    escreve("neve_galho", neve_galho())
    for v in (1, 2):
        escreve(f"cigarra_{v}", cigarra(v))
        escreve(f"ronco_vulcao_{v}", ronco_vulcao(v))
        escreve(f"gaivota_{v}", gaivota(v))
        escreve(f"eco_marte_{v}", eco_marte(v))
        escreve(f"trovao_{v}", trovao(v))
        escreve(f"gota_{v}", gota(v))
        escreve(f"bicho_pato_{v}", pato(v))
        escreve(f"bicho_peixe_{v}", bolhas_peixe(v))
    # bichos com os sons aprovados em 17/11, em alturas um pouco diferentes (não soa sempre igual)
    p, sr = le_wav("passaro.wav")
    escreve("bicho_passaro_1", reamostra(p, sr))
    escreve("bicho_passaro_2", reamostra(p, sr, 1.12))
    escreve("bicho_passaro_3", junta((reamostra(p, sr, 0.95), 0), (0.7 * reamostra(p, sr, 1.05), 0.42)))
    c, sr = le_wav("cachorro.wav")
    escreve("bicho_cachorro_1", reamostra(c, sr))
    escreve("bicho_cachorro_2", junta((reamostra(c, sr, 1.02), 0), (0.8 * reamostra(c, sr, 0.95), 0.5)))
    escreve("bicho_cachorro_3", reamostra(c, sr, 0.9))
    s, sr = le_wav("splash.wav")
    escreve("bicho_peixe_3", reamostra(s, sr, 1.18))
    escreve("bicho_tubarao", reamostra(s, sr, 0.82))
    escreve("bicho_barco", barco())
    escreve("bicho_carro", carrinho())


if __name__ == "__main__":
    main()
