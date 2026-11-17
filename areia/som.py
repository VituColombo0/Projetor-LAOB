"""Sistema de som do Projetor-LAOB (efeitos sonoros e controle de volume).

Toca de forma assíncrona (não trava o painel nem a simulação) usando o paplay
do PulseAudio (saída combinada HDMI do projetor + caixas de som analógicas P2).
Também vigia o sandbox.log para disparar efeitos sonoros nos eventos de jogos,
tesouros e seres vivos.
"""
import json
import os
import re
import subprocess
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
    return e


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
            res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0)
            if res.returncode != 0:
                if saida == "combinado":
                    _garantir_sink_combinado()
                # Fallback sem especificar device
                subprocess.run(["paplay", caminho], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0)
        except Exception:
            try:
                subprocess.run(["aplay", "-q", caminho], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0)
            except Exception:
                pass

    t = threading.Thread(target=_play, daemon=True)
    t.start()


def _loop_vigia():
    global _vigia_ativo
    _vigia_ativo = True
    pos = 0
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
                pos = 0  # arquivo foi truncado/reiniciado
            if tam > pos:
                with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                    f.seek(pos)
                    linhas = f.readlines()
                    pos = f.tell()
                for linha in linhas:
                    linha = linha.strip()
                    if not linha:
                        continue
                    # Detecção de eventos chave
                    if ("Tesouro encontrado" in linha or "Fossil encontrado" in linha
                            or "Tesouro: achou" in linha or "Fossil: achou" in linha):
                        tocar("tesouro")
                    elif linha.startswith("Jogo: fim da rodada"):
                        # "N de M casas a salvo", "N de M peixes salvos"...: ganhou se salvou algo; empate e
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
                    elif ("aviso de lava" in linha or "boca de lava" in linha or "terra tremeu" in linha
                          or "tocou numa armadilha" in linha or "casa alagou" in linha or "lava pegou" in linha):
                        tocar("alerta")
                    elif "passaro pescou um peixe" in linha:
                        tocar("passaro")
                    elif "o tubarao pegou um peixe" in linha or "Chuva: nuvem" in linha:
                        tocar("splash")
        except Exception:
            time.sleep(1.0)


def iniciar_vigia():
    global _vigia_thread
    _garantir_sink_combinado()
    if _vigia_thread is None or not _vigia_thread.is_alive():
        _vigia_thread = threading.Thread(target=_loop_vigia, daemon=True)
        _vigia_thread.start()


if __name__ == "__main__":
    import sys
    acao = sys.argv[1] if len(sys.argv) > 1 else "estado"
    if acao == "tocar" and len(sys.argv) > 2:
        tocar(sys.argv[2])
        time.sleep(2)
    elif acao == "volume" and len(sys.argv) > 2:
        print(definir_volume(sys.argv[2]))
    elif acao == "mudo" and len(sys.argv) > 2:
        print(definir_mudo(sys.argv[2] in ("1", "true", "True", "sim")))
    else:
        print(estado())
