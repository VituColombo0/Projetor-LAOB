"""Sistema de som do Projetor-LAOB (efeitos sonoros e controle de volume).

Toca de forma assíncrona (não trava o painel nem a simulação) usando o paplay
do PulseAudio (saída combinada HDMI do projetor + caixas de som analógicas P2).
Também vigia o sandbox.log para disparar efeitos sonoros nos eventos de jogos,
tesouros e seres vivos.
"""
import json
import os
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

_lock = threading.Lock()
_vigia_thread = None
_vigia_ativo = False


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
    # Tenta sincronizar volume do PulseAudio se pactl estiver disponível
    try:
        vol_pct = f"{vol}%"
        subprocess.run(["pactl", "set-sink-volume", "@DEFAULT_SINK@", vol_pct],
                       check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["pactl", "set-sink-volume", "combinado", vol_pct],
                       check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass
    return e


def definir_mudo(mudo):
    e = estado()
    e["mudo"] = bool(mudo)
    salva_estado(e)
    return e


def tocar(nome):
    """Toca um efeito sonoro de forma assíncrona."""
    e = estado()
    if e.get("mudo", False) or e.get("volume", 80) <= 0:
        return
    caminho = os.path.join(_dir_sons(), f"{nome}.wav")
    if not os.path.isfile(caminho):
        return

    def _play():
        # Tenta tocar no sink combinado ou default
        cmd = ["paplay"]
        saida = e.get("saida", "combinado")
        if saida:
            cmd.extend(["-d", saida])
        cmd.append(caminho)
        try:
            res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if res.returncode != 0:
                # Fallback sem especificar device
                subprocess.run(["paplay", caminho], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            try:
                subprocess.run(["aplay", "-q", caminho], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
                    if "Tesouro: achou" in linha or "Fossil: achou" in linha:
                        tocar("tesouro")
                    elif ("venceu o time" in linha or "Labirinto: a água chegou" in linha
                          or "Copia: parabens" in linha or "Copia: parabéns" in linha
                          or "casas de pé" in linha):
                        tocar("vitoria")
                    elif "Jogo pedido:" in linha or "Jogo: corrida" in linha or "Labirinto: nascente" in linha:
                        tocar("inicio")
                    elif "aviso de lava" in linha or "boca nova" in linha:
                        tocar("alerta")
                    elif "Vida: o tubarao pegou um peixe" in linha:
                        tocar("splash")
        except Exception:
            time.sleep(1.0)


def iniciar_vigia():
    global _vigia_thread
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
