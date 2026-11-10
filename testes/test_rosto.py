"""rosto.py: área iluminada, relevo (DEM) e preparo de imagens sem rosto / com transparência."""
import os
import struct
import subprocess
import sys
import tempfile
import unittest

import cv2
import numpy as np

from base import AREIA_SRC, DADOS, escreve, imagem

os.environ["AREIA_CALIB"] = DADOS
import rosto  # noqa: E402

# o que o Sandbox.cpp do lab2 imprime ao abrir ("área iluminada x -34.5358..24.7993 y -28.8498..13.0744")
AREA_CPP = (-34.5358, 24.7993, -28.8498, 13.0744)


def le_dem(caminho):
    with open(caminho, "rb") as f:
        nx, ny = struct.unpack("<2i", f.read(8))
        caixa = struct.unpack("<4f", f.read(16))
        dem = np.frombuffer(f.read(), "<f4").reshape(ny, nx)
    return nx, ny, caixa, dem


class AreaIluminada(unittest.TestCase):
    def test_igual_ao_cpp(self):
        bmin, bmax = rosto.caixa_cm()
        self.assertAlmostEqual(bmin[0], AREA_CPP[0], places=2)
        self.assertAlmostEqual(bmax[0], AREA_CPP[1], places=2)
        self.assertAlmostEqual(bmin[1], AREA_CPP[2], places=2)
        self.assertAlmostEqual(bmax[1], AREA_CPP[3], places=2)

    def test_menor_que_a_caixa_calibrada(self):
        bmin, bmax = rosto.caixa_cm()
        self.assertTrue(40 < bmax[0] - bmin[0] < 99)
        self.assertTrue(30 < bmax[1] - bmin[1] < 73)


class Relevo(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def test_dem_cobre_a_area_e_respeita_o_formato(self):
        h = np.full((800, 600), 5.0, np.float32)
        p = os.path.join(self.tmp, "a.dem")
        rosto.grava_dem(h, 0, False, p)
        nx, ny, caixa, dem = le_dem(p)
        bmin, bmax = rosto.caixa_cm()
        self.assertEqual(nx, 200)
        self.assertAlmostEqual(caixa[0], bmin[0], places=3)
        self.assertAlmostEqual(caixa[3], bmax[1], places=3)
        dentro = dem > 0
        # retrato 3:4 numa área paisagem: ocupa toda a altura e só o meio da largura
        self.assertTrue(dentro[ny // 2, nx // 2])
        self.assertFalse(dentro[ny // 2, 0])
        self.assertTrue(dentro[1, nx // 2] and dentro[ny - 2, nx // 2])

    def test_giro_de_90_troca_o_lado_maior(self):
        h = np.full((800, 600), 5.0, np.float32)
        a, b = os.path.join(self.tmp, "a.dem"), os.path.join(self.tmp, "b.dem")
        rosto.grava_dem(h, 0, False, a)
        rosto.grava_dem(h, 90, False, b)
        _, _, _, d0 = le_dem(a)
        _, _, _, d90 = le_dem(b)
        larg0 = (d0[d0.shape[0] // 2] > 0).sum()
        larg90 = (d90[d90.shape[0] // 2] > 0).sum()
        self.assertGreater(larg90, larg0)

    def test_relevo_tem_altura_limitada(self):
        img = cv2.imread(imagem(os.path.join(self.tmp, "x.png"), 600, 800))
        h = rosto.relevo(img)
        self.assertEqual(h.shape, (800, 600))
        self.assertGreaterEqual(h.min(), 0)
        self.assertLessEqual(h.max(), 14)


class Preparo(unittest.TestCase):
    """Roda o rosto.py como o areia.py roda (processo separado)."""

    def roda(self, entrada, *opcoes):
        saida = tempfile.mkdtemp()
        r = subprocess.run([sys.executable, os.path.join(AREIA_SRC, "rosto.py"), entrada, "--saida", saida, *opcoes],
                           capture_output=True, text=True, env={**os.environ, "AREIA_CALIB": DADOS})
        return r, saida

    def test_imagem_sem_rosto_fica_inteira_com_fundo(self):
        tmp = tempfile.mkdtemp()
        r, saida = self.roda(imagem(os.path.join(tmp, "paisagem.jpg"), 1600, 900))
        self.assertEqual(r.returncode, 0, r.stderr)
        out = cv2.imread(os.path.join(saida, "rosto.png"))
        self.assertEqual(out.shape[:2], (576, 1024))  # maior lado 1024, mesma proporção
        self.assertGreater(out[5, 5].sum(), 60)       # o fundo continua lá
        self.assertTrue(os.path.getsize(os.path.join(saida, "rosto.dem")) > 1000)

    def test_png_transparente_usa_o_proprio_alfa(self):
        tmp = tempfile.mkdtemp()
        r, saida = self.roda(imagem(os.path.join(tmp, "logo.png"), 400, 300, alfa=True))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("sem fundo", r.stdout)
        out = cv2.imread(os.path.join(saida, "rosto.png"))
        self.assertEqual(out[3, 3].sum(), 0)              # fora: preto (projetor apagado)
        self.assertGreater(out[150, 200].sum(), 600)      # dentro: a imagem

    def test_com_fundo_ignora_o_alfa(self):
        tmp = tempfile.mkdtemp()
        r, _ = self.roda(imagem(os.path.join(tmp, "logo.png"), 400, 300, alfa=True), "--com-fundo")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("com fundo", r.stdout)

    def test_arquivo_que_nao_e_imagem(self):
        tmp = tempfile.mkdtemp()
        p = os.path.join(tmp, "x.jpg")
        escreve(p, "não sou imagem")
        r, _ = self.roda(p)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("não consegui abrir", r.stderr)


if __name__ == "__main__":
    unittest.main()
