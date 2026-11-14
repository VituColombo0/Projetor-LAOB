"""Ambiente de teste: pasta do programa, pasta Areia e um projetor falso, tudo em diretório temporário.

O sandbox.sh falso anota os comandos em comandos.log e guarda qual programa "está rodando".
Nada do SARndbox real é tocado.
"""
import importlib
import os
import shutil
import sys
import tempfile
import unittest

import cv2
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AREIA_SRC = os.path.join(REPO, "areia")
DADOS = os.path.join(REPO, "testes", "dados")
if AREIA_SRC not in sys.path:
    sys.path.insert(0, AREIA_SRC)

SANDBOX_FALSO = """#!/bin/bash
F=$(dirname "$0")
if [ "$1" = cmd ]; then shift; for c in "$@"; do echo "cmd $c" >> "$F/comandos.log"; done; set -- cmd "$@"
else echo "$*" >> "$F/comandos.log"; fi
case "$1" in
  estado) cat "$F/qual" 2>/dev/null || echo nenhum ;;
  original|rosto) echo "$1" > "$F/qual"; echo "$1" ;;
  topografia) echo rosto > "$F/qual"; echo topografia ;;
  cmd) [ "$(cat "$F/qual" 2>/dev/null)" = rosto ] || { echo "a versão com Foto não está rodando"; exit 1; } ;;
  tema) [ -f "$2" ] || exit 1; echo tema ;;
esac
"""


def escreve(caminho, dados):
    with open(caminho, "wb" if isinstance(dados, bytes) else "w") as f:
        f.write(dados)


def imagem(caminho, w=320, h=240, cor=(40, 120, 200), alfa=False):
    img = np.zeros((h, w, 4 if alfa else 3), np.uint8)
    img[..., :3] = cor
    cv2.rectangle(img, (w // 4, h // 4), (3 * w // 4, 3 * h // 4), (250, 250, 250, 255), -1)
    if alfa:
        img[..., 3] = 0
        img[h // 4:3 * h // 4, w // 4:3 * w // 4, 3] = 255
    cv2.imwrite(caminho, img)
    return caminho


class Ambiente(unittest.TestCase):
    """Cada teste ganha D (programa), PASTA (Areia) e o projetor falso; o módulo areia é recarregado."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="areia-teste-")
        self.D = os.path.join(self.tmp, "programa")
        self.PASTA = os.path.join(self.tmp, "Areia")
        self.FALSO = os.path.join(self.tmp, "projetor")
        for p in (self.D, self.PASTA, self.FALSO, os.path.join(self.D, "venv", "bin")):
            os.makedirs(p, exist_ok=True)
        os.symlink(os.path.join(AREIA_SRC, "rosto.py"), os.path.join(self.D, "rosto.py"))
        py = os.path.join(self.D, "venv", "bin", "python")  # o mesmo Python (e pacotes) dos testes
        with open(py, "w") as f:
            f.write(f'#!/bin/sh\nexec "{sys.executable}" "$@"\n')
        os.chmod(py, 0o755)
        shutil.copytree(os.path.join(AREIA_SRC, "temas"), os.path.join(self.D, "temas"))
        sb = os.path.join(self.FALSO, "sandbox.sh")
        with open(sb, "w") as f:
            f.write(SANDBOX_FALSO)
        os.chmod(sb, 0o755)
        self.amb_antigo = {k: os.environ.get(k) for k in ("AREIA_DIR", "AREIA_PASTA", "AREIA_SANDBOX", "AREIA_CALIB",
                                                               "AREIA_WILLBERT_STATUS")}
        # os testes não leem o Willbert de verdade da máquina (no laob ele está ligado)
        os.environ.update(AREIA_DIR=self.D, AREIA_PASTA=self.PASTA, AREIA_SANDBOX=sb, AREIA_CALIB=DADOS,
                          AREIA_WILLBERT_STATUS=os.path.join(self.D, "status-willbert.json"))
        import areia
        self.areia = importlib.reload(areia)

    def tearDown(self):
        for k, v in self.amb_antigo.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        shutil.rmtree(self.tmp, ignore_errors=True)

    def comandos(self):
        try:
            with open(os.path.join(self.FALSO, "comandos.log")) as f:
                return [l.rstrip("\n") for l in f]
        except OSError:
            return []

    def rodando(self):
        try:
            with open(os.path.join(self.FALSO, "qual")) as f:
                return f.read().strip()
        except OSError:
            return "nenhum"

    def poe(self, nome, **kw):
        return imagem(os.path.join(self.PASTA, nome), **kw)
