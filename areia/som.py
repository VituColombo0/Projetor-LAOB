"""Sistema de som do Projetor-LAOB: efeitos, ambiente automático e controle de volume.

Efeitos: tocados de forma assíncrona (não travam o painel nem a simulação) pelo paplay do
PulseAudio, na saída combinada (HDMI do projetor + caixas de som analógicas P2). O vigia do
sandbox.log dispara os efeitos nos eventos de jogos, tesouros e seres vivos.

Ambiente (19/11/2026, pedido do Victor: "automático, sem precisar apertar nos botões"): um fundo
baixinho que combina com o que está na areia (o tema do relevo, a brincadeira ligada e os bichos
soltos), trocado com fade. Um processo à parte (python som.py motor) mistura as camadas e escreve
no pacat; o painel só manda a ele uma linha JSON quando o que deve soar muda. Volume e mudo valem
para tudo (efeitos + ambiente); o mudo corta na hora. Pela linha de comando (python som.py mudo 1 /
volume N) o pedido vai ao painel (POST 127.0.0.1:8090/som), que é quem tem o motor e os efeitos; com o
painel fechado só o arquivo de estado muda (sem motor, não há o que calar).
"""
import json
import os
import random
import re
import subprocess
import sys
import threading
import time

def _dir():
    return os.environ.get("AREIA_DIR") or os.path.dirname(os.path.realpath(__file__))

def _arquivo_estado():
    return os.path.join(_dir(), "estado_som.json")

def _dir_sons():
    # Procura na pasta de trabalho ou na pasta original do script
    d1 = os.path.join(_dir(), "sons")
    if os.path.isdir(d1):
        return d1
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), "sons")

def _caminho_log():
    return os.path.join(_dir(), "sandbox.log")

PADRAO = {"volume": 80, "mudo": False, "saida": "combinado"}


def _falso():
    """Testes: não mexe no som de verdade (pactl/paplay). Em 17/11 o teste do painel deixava o volume do laob em 0%."""
    return bool(os.environ.get("AREIA_SOM_FALSO"))

_lock = threading.Lock()
_vigia_thread = None
_vigia_ativo = False
_ultimo_toque = {}
_ultimo_toque_geral = 0.0
_tocando = set()   # paplay em andamento: o mudo mata na hora


def _garantir_sink_combinado():
    """Garante que a saída combinada (HDMI + analógica) existe no PulseAudio sem duplicatas."""
    if _falso():
        return
    try:
        res = subprocess.run(["pactl", "list", "short", "sinks"],
                             capture_output=True, text=True, timeout=1.5)
        if "combinado" not in res.stdout:
            subprocess.run([
                "pactl", "load-module", "module-combine-sink",
                "sink_name=combinado",
                "slaves=alsa_output.pci-0000_01_00.1.hdmi-stereo,alsa_output.pci-0000_00_1b.0.analog-stereo"
            ], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2.0)
            subprocess.run(["pactl", "set-sink-mute", "alsa_output.pci-0000_00_1b.0.analog-stereo", "0"],
                           check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0)
    except Exception:
        pass


def estado():
    """Lê o estado do som (volume 0..100, mudo, saída)."""
    arq = _arquivo_estado()
    with _lock:
        if os.path.isfile(arq):
            try:
                with open(arq, "r", encoding="utf-8") as f:
                    e = json.load(f)
                    return {**PADRAO, **e}
            except Exception:
                pass
        return dict(PADRAO)


def salva_estado(e):
    arq = _arquivo_estado()
    with _lock:
        try:
            with open(arq + ".tmp", "w", encoding="utf-8") as f:
                json.dump(e, f, indent=2)
            os.replace(arq + ".tmp", arq)
        except Exception:
            pass


def definir_volume(vol):
    vol = max(0, min(100, int(vol)))
    e = estado()
    e["volume"] = vol
    salva_estado(e)
    if vol <= 0:
        _cala_tudo()
    _amb_acorda.set()   # o ambiente volta (ou para) já, sem esperar a próxima olhada
    if _falso():
        return e
    # Tenta sincronizar volume do PulseAudio se pactl estiver disponível
    try:
        vol_pct = f"{vol}%"
        subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", vol_pct],
                       check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0)
        subprocess.run(["pactl", "set-sink-volume", "combinado", vol_pct],
                       check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0)
    except Exception:
        pass
    return e


def definir_mudo(mudo):
    e = estado()
    e["mudo"] = bool(mudo)
    salva_estado(e)
    if e["mudo"]:
        _cala_tudo()
    _amb_acorda.set()
    return e


def _cala_tudo():
    """Mudo: para o ambiente e os efeitos que estiverem tocando, na hora (sem fade)."""
    manda_ao_motor(None)
    with _lock:
        procs = list(_tocando)
    for p in procs:
        try:
            p.kill()
        except OSError:
            pass


def _roda(cmd):
    """Roda um paplay/aplay guardando o processo (para o mudo poder matar). Devolve o código de saída."""
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with _lock:
        _tocando.add(p)
    try:
        return p.wait(timeout=8.0)
    except subprocess.TimeoutExpired:
        p.kill()
        return 1
    finally:
        with _lock:
            _tocando.discard(p)


def tocar(nome):
    """Toca um efeito sonoro de forma assíncrona com cooldown e debounce."""
    global _ultimo_toque_geral
    e = estado()
    if e.get("mudo", False) or e.get("volume", 80) <= 0:
        return
    caminho = os.path.join(_dir_sons(), f"{nome}.wav")
    if not os.path.isfile(caminho) or _falso():
        return

    agora = time.time()
    with _lock:
        if agora - _ultimo_toque.get(nome, 0.0) < 0.6:
            return
        if agora - _ultimo_toque_geral < 0.12:
            return
        _ultimo_toque[nome] = agora
        _ultimo_toque_geral = agora

    def _play():
        saida = e.get("saida", "combinado")
        cmd = ["paplay"]
        if saida:
            cmd.extend(["-d", saida])
        cmd.append(caminho)
        try:
            if _roda(cmd) not in (0, -9):   # -9: o mudo matou, não tenta de novo
                if saida == "combinado":
                    _garantir_sink_combinado()
                if not estado().get("mudo"):
                    _roda(["paplay", caminho])   # sem especificar a saída
        except Exception:
            try:
                _roda(["aplay", "-q", caminho])
            except Exception:
                pass

    t = threading.Thread(target=_play, daemon=True)
    t.start()


# linhas do registro que mudam o fundo: o ambiente olha a areia de novo na hora
_MUDA_O_AMBIENTE = ("Jogo pedido", "Jogo desligado", "Animais:", "Labirinto: nascente", "Labirinto desligado",
                    "Tesouro: escondendo", "Tesouro desligado", "Fossil: escondendo", "Fossil desligado",
                    "Copia: forma", "Copia desligada", "Modo Rosto:", "Modo Esculpir:", "Chuva:")

# Chuva na areia agora (22/11/2026: o som da chuva só enquanto chove de verdade). O vigia do registro anota:
# a mão chovendo ("Chuva: mao comecou/parou", do SARndbox) e a nuvem do painel ("Chuva: nuvem por N s").
_chuva = {"mao": False, "nuvem_ate": 0.0}


def _anota_chuva(linha):
    """Atualiza _chuva com uma linha do registro; devolve em quantos segundos olhar de novo (fim da nuvem) ou None."""
    if linha.startswith("Chuva: mao comecou"):
        _chuva["mao"] = True
    elif linha.startswith("Chuva: mao parou"):
        _chuva["mao"] = False
    elif linha.startswith("Chuva: nuvem por"):
        m = re.search(r"por ([0-9.]+)", linha)
        seg = float(m.group(1)) if m else 8.0
        _chuva["nuvem_ate"] = max(_chuva["nuvem_ate"], time.monotonic() + seg + 0.9)   # a nuvem some em 0,9 s
        return seg + 1.0
    elif linha.startswith("Chuva: a água foi seca"):
        _chuva["nuvem_ate"] = 0.0
    return None


def chovendo():
    return _chuva["mao"] or time.monotonic() < _chuva["nuvem_ate"]


def _esquece_chuva():
    """A areia reabriu (registro novo): nada está chovendo."""
    _chuva["mao"] = False
    _chuva["nuvem_ate"] = 0.0


def _loop_vigia():
    global _vigia_ativo
    _vigia_ativo = True
    pos = 0
    resto = b""   # pedaço de linha ainda sendo escrita: fica para a próxima volta (como em areia._le_registro)
    log_path = _caminho_log()
    if os.path.isfile(log_path):
        try:
            pos = os.path.getsize(log_path)
        except Exception:
            pos = 0

    while _vigia_ativo:
        time.sleep(0.3)
        log_path = _caminho_log()
        if not os.path.isfile(log_path):
            continue
        try:
            tam = os.path.getsize(log_path)
            if tam < pos:
                pos, resto = 0, b""  # arquivo foi truncado/reiniciado (a areia reabriu)
                _esquece_chuva()
                _amb_acorda.set()
            if tam > pos:
                with open(log_path, "rb") as f:
                    f.seek(pos)
                    novo = f.read()
                pos += len(novo)
                partes = (resto + novo).split(b"\n")
                resto = partes.pop()
                for linha in partes:
                    linha = linha.decode("utf-8", "replace").strip()
                    if not linha:
                        continue
                    if linha.startswith("Chuva:"):
                        depois = _anota_chuva(linha)
                        if depois:
                            acorda_ambiente(depois)   # a nuvem acabou: a chuva some do som também
                    if linha.startswith(_MUDA_O_AMBIENTE):
                        _amb_acorda.set()   # brincadeira ou bichos mudaram (também por voz ou terminal)
                    # Detecção de eventos chave (22/11/2026: cada som com o que aparece na areia)
                    if "Tesouro encontrado" in linha:
                        tocar("tesouro")       # o baú abre: moedas e joias
                    elif "Fossil encontrado" in linha:
                        tocar("fossil")        # osso achado: pincel na terra e a descoberta (moedas não)
                    elif ((linha.startswith("Gesto: dedo em") and "nada caiu" not in linha)
                          or linha.startswith("Comida em x")):
                        # a comida que cai do dedo ou do painel: ração na água (plic-plic), pão na terra (puf)
                        tocar("racao_agua" if "(agua)" in linha else "racao_terra")
                    elif linha.startswith("Mundo: ") and ("entrou no laboratório" in linha or "saiu do laboratório" in linha):
                        tocar("porta")         # no laboratório LAOB, alguém passou pela porta
                    elif linha.startswith("Jogo: fim da rodada"):
                        # "N de M casas a salvo da lava"/"casas secas": ganhou se salvou alguma; empate e
                        # "o tempo acabou" (sem chegar) contam como derrota
                        m = re.search(r"(\d+) de (\d+)", linha)
                        perdeu = ("empate" in linha or "o tempo acabou" in linha
                                  or (m is not None and int(m.group(1)) == 0))
                        tocar("alerta" if perdeu else "vitoria")
                    elif ("Labirinto: a água chegou" in linha or "Copia: conseguiu" in linha):
                        tocar("vitoria")
                    elif ("Jogo pedido:" in linha or "Jogo: corrida" in linha
                          or "Labirinto: nascente" in linha or "Labirinto: rodada nova" in linha
                          or "Copia: forma" in linha):
                        tocar("inicio")
                    elif ("boca de lava" in linha or "terra tremeu" in linha
                          or "tocou numa armadilha" in linha or "casa alagou" in linha or "lava pegou" in linha):
                        tocar("alerta")
                    elif "passaro pescou um peixe" in linha:
                        tocar("passaro")
                    elif "o tubarao pegou um peixe" in linha:
                        tocar("splash")
        except Exception:
            time.sleep(1.0)


def iniciar_vigia():
    global _vigia_thread
    _garantir_sink_combinado()
    if _vigia_thread is None or not _vigia_thread.is_alive():
        _vigia_thread = threading.Thread(target=_loop_vigia, daemon=True)
        _vigia_thread.start()


# ---------- ambiente: o fundo que combina com o que está na areia ----------

TAXA = 22050          # os sons de ambiente (sons/amb_*, jogo_*, bichos) são 22,05 kHz mono
BLOCO = 1024          # amostras por volta do motor (~46 ms)
FADE = 1.5            # s para uma camada entrar ou sair (troca de tema, jogo começando...)
INTERVALO_BICHOS = (15.0, 60.0)   # s entre um som de bicho e outro (sorteado; um bicho por vez)
CHECA_A_CADA = 5.0    # s entre uma olhada e outra (cada uma abre o sandbox.sh estado: ~18 ms de CPU no laob);
                      # o que vem pelo painel não espera: acorda_ambiente() logo depois de cada comando


def _v(prefixo, n):
    return [f"{prefixo}_{i}" for i in range(1, n + 1)]


# Ganhos: os fundos têm -20 dB de média no arquivo; com 0,15 ficam uns 20 dB abaixo dos efeitos
# (vitória, tesouro: -16 dB de média). Os pontuais e os bichos têm pico -3 dB no arquivo.
# tema: {"base": {fundo: ganho}, "pontuais": [[variações, ganho, intervalo mín, máx (s)]]}
# 22/11/2026 (pedido do Victor: "os sons fazendo sentido com cada coisa que está sendo projetada"): cada
# tema só toca o que a animação dele mostra. Saíram passarinho (clássico: não há pássaro desenhado; o
# pássaro só canta quando está na areia), sino de vento e neve do galho (inverno: não há sino nem árvore),
# cigarra (deserto), gaivota (oceano) e o eco (marte).
AMBIENTE_TEMAS = {
    "classico": {"base": {"amb_classico": 0.12}},                       # brisa leve no relevo
    "inverno": {"base": {"amb_inverno": 0.15}},                         # vento frio com a neve caindo
    "deserto": {"base": {"amb_deserto": 0.16}},                         # vento seco levando a areia
    "vulcao": {"base": {"amb_vulcao": 0.15}, "pontuais": [[_v("ronco_vulcao", 2), 0.30, 35, 90]]},  # lava
    "oceano": {"base": {"amb_oceano": 0.16}},                           # as ondas do mar
    "marte": {"base": {"amb_marte": 0.13}},                             # vento ralo com a poeira
    "arco-iris": {"base": {"amb_arcoiris": 0.11}, "pontuais": [[_v("brilho", 3), 0.10, 25, 60]]},  # estrelas piscando
}
# o laboratório LAOB (Mundo): sala fechada, sem o fundo do tema; o ar e os computadores e, de vez em
# quando, alguém digitando (os membros trabalham nas bancadas)
AMBIENTE_LABORATORIO = {"base": {"amb_laboratorio": 0.13}, "pontuais": [[_v("teclado", 3), 0.16, 14, 40]]}
CHUVA = {"jogo_chuva": 0.14}   # enquanto chove (mão ou nuvem do painel)
# brincadeira: camada própria por cima; "tema" = quanto do fundo do tema continua por baixo (os pontuais
# do tema saem: fica só o da brincadeira). No vulcão o jogo é o mundo todo (escurece a areia).
AMBIENTE_BRINCADEIRAS = {
    "corrida": {"base": {"jogo_correnteza": 0.13}, "tema": 0.5},
    "rota": {"base": {"jogo_riacho": 0.12}, "tema": 0.5},
    "labirinto": {"base": {"jogo_riacho": 0.12}, "pontuais": [[_v("gota", 2), 0.14, 12, 30]], "tema": 0.5},
    "vila": {"base": {"jogo_riacho": 0.12}, "tema": 0.4},   # a água vem das nascentes (não é chuva: sem trovão)
    "vulcao": {"base": {"amb_vulcao": 0.13, "jogo_tensao": 0.11},
               "pontuais": [[_v("ronco_vulcao", 2), 0.30, 18, 40]], "tema": 0.0},
    "copia": {"base": {"jogo_atelie": 0.09}, "tema": 0.5},
    "tesouro": {"base": {"jogo_misterio": 0.09}, "tema": 0.5},
    "fossil": {"base": {"jogo_misterio": 0.09}, "tema": 0.5},
}
# bicho (nome do comando "mar"): [variações, ganho]. Pessoas não fazem som.
SONS_BICHOS = {
    "passaro": [_v("bicho_passaro", 3), 0.16], "pato": [_v("bicho_pato", 2), 0.20],
    "cachorro": [_v("bicho_cachorro", 3), 0.14], "peixe": [_v("bicho_peixe", 3), 0.20],
    "tartaruga": [_v("bicho_peixe", 2), 0.18], "aguaviva": [_v("bicho_peixe", 2), 0.16],
    "polvo": [_v("bicho_peixe", 2), 0.18], "arraia": [_v("bicho_peixe", 2), 0.16],
    "tubarao": [["bicho_tubarao"], 0.16], "barco": [["bicho_barco"], 0.14], "carro": [["bicho_carro"], 0.08],
}
_BICHOS_DOS_ANIMAIS = {"peixes": ["peixe"], "carros": ["carro"], "pessoas": [], "todos": ["peixe", "carro"]}


def monta_cena(tema, brincadeira=None, bichos=(), chuva=False, laboratorio=False):
    """O que deve soar: {"base": {fundo: ganho}, "pontuais": [...], "bichos": [[bicho, variações, ganho]]}."""
    t = AMBIENTE_LABORATORIO if laboratorio else AMBIENTE_TEMAS.get(tema, {})
    b = AMBIENTE_BRINCADEIRAS.get(brincadeira)
    if b:
        f = b.get("tema", 0.5)
        base = {k: round(g * f, 4) for k, g in t.get("base", {}).items() if f > 0}
        for k, g in b["base"].items():
            base[k] = max(base.get(k, 0.0), g)
        pontuais = b.get("pontuais", [])
    else:
        base = dict(t.get("base", {}))
        pontuais = t.get("pontuais", [])
    if chuva:
        for k, g in CHUVA.items():
            base[k] = max(base.get(k, 0.0), g)
    return {"base": base, "pontuais": [[list(v), g, lo, hi] for v, g, lo, hi in pontuais],
            "bichos": [[k, list(SONS_BICHOS[k][0]), SONS_BICHOS[k][1]] for k in sorted(set(bichos)) if k in SONS_BICHOS]}


def brincadeira_atual(areia):
    """corrida, vila, rota, vulcao, labirinto, copia, tesouro, fossil ou None (uma por vez)."""
    j = areia.jogo_situacao()["jogo"]
    if j:
        return j
    if areia.labirinto_situacao()[0] != "nenhum":
        return "labirinto"
    if areia.copia_situacao()["forma"]:
        return "copia"
    if areia.tesouro_situacao() != "nenhum":
        return "tesouro"
    if areia.fossil_situacao() != "nenhum":
        return "fossil"
    return None


def bichos_atuais(areia):
    """Os bichos soltos na areia agora (nomes do comando "mar")."""
    sit = areia.animais_situacao()
    if sit == "mar":
        return [k for k, n in areia.mar_escolhido().items() if n]
    return list(_BICHOS_DOS_ANIMAIS.get(sit, []))


def cena_atual():
    """A cena do que está na areia agora, ou None: areia fechada, só a topografia de reserva (o SARndbox
    original) ou uma foto/escultura no projetor."""
    import areia
    if areia.qual() != "rosto":
        return None
    if areia.estado().get("modo") in ("rosto", "esculpir"):
        return None
    return monta_cena(areia.tema_atual(), brincadeira_atual(areia), bichos_atuais(areia), chovendo(),
                      areia.animais_situacao() == "mundo")


def _tem_som(cena):
    return bool(cena and (cena.get("base") or cena.get("pontuais") or cena.get("bichos")))


# --- lado do painel: uma linha olha a areia e avisa o motor quando muda ---

_amb = {"proc": None, "enviada": None, "morreu": -1e9, "historico": []}
_amb_trava = threading.Lock()
_amb_acorda = threading.Event()
_amb_thread = None


def _morre_com_o_pai():
    """Nos processos filhos (antes de rodar): morre junto se o pai morrer (como o ffmpeg do painel)."""
    try:
        import ctypes
        import signal
        ctypes.CDLL("libc.so.6", use_errno=True).prctl(1, signal.SIGTERM)   # PR_SET_PDEATHSIG
    except Exception:
        pass


def _motor_vivo():
    p = _amb["proc"]
    return p is not None and p.poll() is None


def _para_motor():
    p, _amb["proc"] = _amb["proc"], None
    if p is not None and p.poll() is None:
        p.kill()   # o pacat dele morre junto (PDEATHSIG) e o PulseAudio joga fora o que estava no buffer
        try:
            p.wait(timeout=2)
        except subprocess.TimeoutExpired:
            pass


def manda_ao_motor(cena):
    """Diz ao motor o que tocar (só quando muda). None = parar já, sem fade (mudo); {} = sumir com fade."""
    with _amb_trava:
        if cena is None:
            _para_motor()
            if _amb["enviada"] is not None:
                _amb["historico"].append(None)
            _amb["enviada"] = None
            return
        if cena == _amb["enviada"] and (_falso() or _motor_vivo()):
            return
        _amb["enviada"] = cena
        _amb["historico"].append(cena)
        del _amb["historico"][:-50]
        if _falso():   # testes: só anota (nada de pacat)
            return
        if not _motor_vivo():
            if _amb["proc"] is not None:   # morreu sozinho (o mudo zera o proc antes de matar)
                _amb["proc"], _amb["morreu"] = None, time.monotonic()
            if not _tem_som(cena):
                return   # nada a tocar: nem abre o motor
            if time.monotonic() - _amb["morreu"] < 20:   # acabou de morrer: espera, para não ficar reabrindo
                _amb["enviada"] = None
                return
            try:
                _amb["proc"] = subprocess.Popen([sys.executable, os.path.realpath(__file__), "motor"],
                                                stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                                preexec_fn=_morre_com_o_pai)
            except OSError:
                _amb["enviada"], _amb["morreu"] = None, time.monotonic()
                return
        try:
            _amb["proc"].stdin.write((json.dumps(cena) + "\n").encode())
            _amb["proc"].stdin.flush()
        except (OSError, ValueError):
            _para_motor()
            _amb["enviada"], _amb["morreu"] = None, time.monotonic()


def atualiza_ambiente():
    """Uma olhada: o que está na areia -> o que o motor toca."""
    e = estado()
    if e.get("mudo") or int(e.get("volume", 80)) <= 0:
        manda_ao_motor(None)
        return
    try:
        cena = cena_atual()
    except Exception:   # areia reabrindo, registro sumiu...: silêncio com fade até a próxima olhada
        cena = None
    manda_ao_motor(cena or {})


def _loop_ambiente():
    while True:
        _amb_acorda.clear()   # antes de olhar: um aviso que chegue durante a olhada não se perde
        try:
            atualiza_ambiente()
        except Exception:
            pass
        _amb_acorda.wait(CHECA_A_CADA)


def acorda_ambiente(atraso=0.0):
    """Olha a areia de novo já (ou daqui a atraso s: o SARndbox leva um instante para anotar no registro
    o jogo ou os bichos que acabaram de pedir)."""
    if atraso <= 0:
        _amb_acorda.set()
        return
    t = threading.Timer(atraso, _amb_acorda.set)
    t.daemon = True
    t.start()


def iniciar_ambiente():
    """Liga o ambiente automático (o painel chama ao subir). A linha vive o tempo todo: o motor morre
    junto com ela (PDEATHSIG é por linha que criou o processo)."""
    global _amb_thread
    if _amb_thread is None or not _amb_thread.is_alive():
        _amb_thread = threading.Thread(target=_loop_ambiente, daemon=True, name="ambiente")
        _amb_thread.start()


# --- o motor: mistura as camadas e escreve no pacat ---

_sons_cache = {}


def _carrega_som(nome):
    """sons/<nome>.wav (16 bits, mono, 22,05 kHz) como int16, ou None se não houver."""
    if nome in _sons_cache:
        return _sons_cache[nome]
    import wave
    import numpy as np
    dados = None
    try:
        with wave.open(os.path.join(_dir_sons(), nome + ".wav")) as w:
            if w.getsampwidth() == 2 and w.getnchannels() == 1 and w.getframerate() == TAXA:
                dados = np.frombuffer(w.readframes(w.getnframes()), "<i2")
    except Exception:   # arquivo faltando ou estragado: essa camada fica muda
        dados = None
    if dados is not None and len(dados) < BLOCO:
        dados = None
    _sons_cache[nome] = dados
    return dados


class Mistura:
    """As camadas do ambiente: fundos em laço (entram e saem com fade de FADE s) e sons pontuais
    sorteados por cima, um de cada vez. gera(n) devolve o próximo pedaço (float32, -1..1)."""

    def __init__(self, carrega=None, sorteio=None):
        self.carrega = carrega or _carrega_som
        self.r = sorteio or random.Random()
        self.vozes = []      # {"chave", "dados", "pos", "ganho", "alvo", "passo", "laco"}
        self.pontuais = {}   # chave -> [variações, ganho, mín, máx, quando]
        self.bichos = []     # [[bicho, variações, ganho]]
        self.prox_bicho = None
        self.t = 0.0         # segundos já gerados

    def _alvo(self, v, alvo):
        v["alvo"] = float(alvo)
        v["passo"] = max(abs(v["alvo"] - v["ganho"]), 1e-6) / (FADE * TAXA)

    def cena(self, c):
        c = c or {}
        base = {k: float(g) for k, g in (c.get("base") or {}).items()}
        novos = {}
        for variantes, g, lo, hi in c.get("pontuais") or []:
            k = "p:" + variantes[0]
            antigo = self.pontuais.get(k)
            quando = antigo[4] if antigo else self.t + self.r.uniform(lo * 0.3, hi)
            novos[k] = [list(variantes), float(g), float(lo), float(hi), quando]
        bichos = [[b, list(v), float(g)] for b, v, g in c.get("bichos") or []]
        validos = set(novos) | {"b:" + b[0] for b in bichos}
        for v in self.vozes:
            if v["laco"]:
                self._alvo(v, base.get(v["chave"], 0.0))
            elif v["chave"] not in validos:
                self._alvo(v, 0.0)   # o bicho saiu da areia: o som dele some com fade
        presentes = {v["chave"] for v in self.vozes if v["laco"]}
        for nome, g in base.items():
            if nome in presentes or g <= 0:
                continue
            dados = self.carrega(nome)
            if dados is None:
                continue
            v = {"chave": nome, "dados": dados, "pos": self.r.randrange(len(dados)), "ganho": 0.0, "laco": True}
            self._alvo(v, g)
            self.vozes.append(v)
        self.pontuais = novos
        if bichos and not self.bichos:
            self.prox_bicho = self.t + self.r.uniform(6.0, 25.0)   # o primeiro vem logo: dá para notar
        elif not bichos:
            self.prox_bicho = None
        self.bichos = bichos

    def _um(self, chave, nome, ganho):
        dados = self.carrega(nome)
        if dados is not None:
            g = ganho * self.r.uniform(0.8, 1.0)
            self.vozes.append({"chave": chave, "dados": dados, "pos": 0, "ganho": g, "alvo": g, "passo": 0.0,
                               "laco": False})

    def tocando_pontual(self):
        return any(not v["laco"] for v in self.vozes)

    def _dispara(self):
        ocupado = self.tocando_pontual()
        for k, p in self.pontuais.items():
            if self.t < p[4]:
                continue
            if ocupado:   # um de cada vez: espera o outro acabar
                p[4] = self.t + self.r.uniform(2.0, 5.0)
                continue
            self._um(k, self.r.choice(p[0]), p[1])
            p[4] = self.t + self.r.uniform(p[2], p[3])
            ocupado = True
        if self.prox_bicho is not None and self.t >= self.prox_bicho:
            if ocupado:
                self.prox_bicho = self.t + self.r.uniform(2.0, 5.0)
            else:
                b, variantes, g = self.r.choice(self.bichos)
                self._um("b:" + b, self.r.choice(variantes), g)
                self.prox_bicho = self.t + self.r.uniform(*INTERVALO_BICHOS)

    def ativa(self):
        return bool(self.vozes or self.pontuais or self.bichos)

    def gera(self, n=BLOCO):
        import numpy as np
        self._dispara()
        out = np.zeros(n, np.float32)
        for v in self.vozes:
            g0, alvo = v["ganho"], v["alvo"]
            if g0 != alvo:
                d = v["passo"] * n
                g1 = min(alvo, g0 + d) if alvo > g0 else max(alvo, g0 - d)
                ganho = np.linspace(g0, g1, n, endpoint=False, dtype=np.float32)
                v["ganho"] = g1
            else:
                ganho = np.float32(g0)
            dados = v["dados"]
            if v["laco"]:
                i = v["pos"]
                seg = dados[i:i + n]
                if len(seg) < n:
                    seg = np.concatenate([seg, dados[:n - len(seg)]])
                v["pos"] = (i + n) % len(dados)
            else:
                seg = dados[v["pos"]:v["pos"] + n]
                v["pos"] += n
                if len(seg) < n:
                    seg = np.pad(seg, (0, n - len(seg)))
            out += seg * ganho
        self.vozes = [v for v in self.vozes if (v["ganho"] > 0 or v["alvo"] > 0)
                      and (v["laco"] or v["pos"] < len(v["dados"]))]
        self.t += n / TAXA
        return out * np.float32(1 / 32768)


class _Saida:
    """O pacat do motor: abre quando há o que tocar, fecha (deixando o fim do fade sair) quando não há."""

    def __init__(self, nulo=False):
        self.nulo = nulo        # medir CPU sem tocar nada: só segue o relógio
        self.proc = None
        self.falhas = 0
        self.espera = 0.0
        self.aberta_em = 0.0
        self.t0 = None
        self.amostras = 0

    def abre(self):
        if self.nulo:
            if self.t0 is None:
                self.t0, self.amostras = time.monotonic(), 0
            return True
        if self.proc is not None:
            return True
        if time.monotonic() < self.espera:
            return False
        saida = estado().get("saida") or ""
        cmd = ["pacat", "--playback", f"--rate={TAXA}", "--channels=1", "--format=s16le", "--latency-msec=250",
               "--client-name=Caixa de Areia", "--stream-name=ambiente"]
        if saida and self.falhas < 2:   # duas falhas seguidas: tenta a saída padrão
            cmd.append(f"--device={saida}")
        try:
            self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                         stderr=subprocess.DEVNULL, bufsize=0, preexec_fn=_morre_com_o_pai)
        except OSError:   # sem pacat: tenta de novo daqui a 5 min
            print("som: pacat não encontrado; ambiente desligado por 5 min", file=sys.stderr)
            self.espera = time.monotonic() + 300
            return False
        try:   # cano pequeno (8 KB = 0,19 s): troca de cena e fade respondem logo
            import fcntl
            fcntl.fcntl(self.proc.stdin.fileno(), 1031, 8192)   # F_SETPIPE_SZ
        except (OSError, ImportError):
            pass
        self.aberta_em = time.monotonic()
        return True

    def escreve(self, bloco):
        import numpy as np
        pcm = (np.clip(bloco, -1.0, 1.0) * 32767).astype("<i2").tobytes()
        if self.nulo:
            self.amostras += len(bloco)
            adiante = self.amostras / TAXA - (time.monotonic() - self.t0)
            if adiante > 0.2:
                time.sleep(adiante - 0.2)
            return True
        try:
            self.proc.stdin.write(pcm)   # bloqueia quando o buffer enche: é isso que dá o ritmo
            if self.falhas and time.monotonic() - self.aberta_em > 10:
                self.falhas = 0
            return True
        except (OSError, ValueError):
            self._morreu()
            return False

    def _morreu(self):
        self.falhas += 1
        print(f"som: o pacat do ambiente parou ({self.falhas}ª vez)", file=sys.stderr)
        if self.falhas == 1:
            _garantir_sink_combinado()
        self.fecha()
        self.espera = time.monotonic() + min(60, 2 ** self.falhas)

    def fecha(self):
        self.t0 = None
        p, self.proc = self.proc, None
        if p is None:
            return
        try:
            p.stdin.close()   # o pacat toca o que falta (o fim do fade) e sai
            p.wait(timeout=2)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            p.kill()


def motor(nulo=False):
    """python som.py motor: lê cenas (uma linha JSON cada) da entrada e toca. Sai quando a entrada
    fecha (o painel morreu) ou com o PDEATHSIG."""
    import queue
    fila = queue.Queue()

    def le():
        for linha in sys.stdin.buffer:
            try:
                fila.put(json.loads(linha))
            except ValueError:
                pass
        fila.put(None)

    threading.Thread(target=le, daemon=True, name="entrada").start()
    mix = Mistura()
    saida = _Saida(nulo)
    while True:
        try:
            c = fila.get_nowait() if mix.ativa() else fila.get(timeout=2.0)
        except queue.Empty:
            c = False
        if c is None:
            break
        if c is not False:
            mix.cena(c)
            continue
        if not mix.ativa():
            saida.fecha()
            continue
        if not saida.abre():
            time.sleep(0.5)
            continue
        saida.escreve(mix.gera(BLOCO))
    saida.fecha()


def _pelo_painel(dados):
    """Linha de comando: manda o volume/mudo ao painel, dono do motor do ambiente e dos efeitos tocando
    (só ele corta na hora; daqui, outro processo, o ambiente seguiria até a próxima olhada, até 5 s).
    Devolve o estado novo ou None se o painel não respondeu (ou nos testes, AREIA_SOM_FALSO: não mexe no
    painel de verdade)."""
    if _falso():
        return None
    import urllib.request
    porta = os.environ.get("AREIA_PORTA", "8090")
    try:
        req = urllib.request.Request(f"http://127.0.0.1:{porta}/som", data=json.dumps(dados).encode(),
                                     headers={"Content-Type": "application/json"}, method="POST")
        with urllib.request.urlopen(req, timeout=3.0) as r:
            return json.load(r).get("som")
    except Exception:
        return None


if __name__ == "__main__":
    acao = sys.argv[1] if len(sys.argv) > 1 else "estado"
    if acao == "motor":
        motor(nulo="--nulo" in sys.argv)
    elif acao == "cena":   # o que o ambiente tocaria agora (para conferir pelo terminal)
        print(json.dumps(cena_atual(), ensure_ascii=False, indent=1))
    elif acao == "tocar" and len(sys.argv) > 2:
        tocar(sys.argv[2])
        time.sleep(2)
    elif acao == "volume" and len(sys.argv) > 2:
        print(_pelo_painel({"volume": int(sys.argv[2])}) or definir_volume(sys.argv[2]))
    elif acao == "mudo" and len(sys.argv) > 2:
        mudo = sys.argv[2] in ("1", "true", "True", "sim")
        print(_pelo_painel({"mudo": mudo}) or definir_mudo(mudo))
    else:
        print(estado())
