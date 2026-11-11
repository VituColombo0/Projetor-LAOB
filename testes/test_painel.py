"""painel.py: acesso só local/Tailscale, rotas e mensagens de erro (projetor falso)."""
import importlib
import io
import os
import unittest

import cv2
import numpy as np

from base import Ambiente

TAILSCALE = {"REMOTE_ADDR": "100.101.102.103"}


class Painel(Ambiente):
    def setUp(self):
        super().setUp()
        import painel
        self.painel = importlib.reload(painel)
        self.c = self.painel.app.test_client()

    def get(self, url, ip="100.101.102.103"):
        return self.c.get(url, environ_base={"REMOTE_ADDR": ip})

    def post(self, url, **kw):
        return self.c.post(url, environ_base=TAILSCALE, **kw)

    def test_so_local_e_tailscale(self):
        self.assertEqual(self.get("/", "127.0.0.1").status_code, 200)
        self.assertEqual(self.get("/", "100.64.0.1").status_code, 200)
        self.assertEqual(self.get("/", "::ffff:100.64.0.9").status_code, 200)
        for ip in ("192.168.0.50", "10.0.0.2", "8.8.8.8", "::ffff:192.168.0.5"):
            self.assertEqual(self.get("/estado", ip).status_code, 403, ip)

    def test_estado(self):
        j = self.get("/estado").get_json()
        self.assertEqual((j["sandbox"], j["modo"], j["tem_foto"]), ("nenhum", "original", False))

    def test_envia_foto_do_celular(self):
        ok, png = cv2.imencode(".png", np.full((200, 150, 3), 128, np.uint8))
        r = self.post("/foto", data={"foto": (io.BytesIO(png.tobytes()), "IMG_1.PNG")},
                      content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200, r.get_json())
        guardadas = os.listdir(self.PASTA)
        self.assertEqual(len(guardadas), 1)
        self.assertTrue(guardadas[0].startswith("celular-") and guardadas[0].endswith(".png"))
        self.assertEqual(self.rodando(), "rosto")
        r = self.get("/previa.png")
        self.assertEqual(r.status_code, 200)
        r.close()

    def test_foto_do_willbert(self):
        ok, jpg = cv2.imencode(".jpg", np.full((200, 150, 3), 90, np.uint8))
        r = self.c.post("/foto", environ_base={"REMOTE_ADDR": "127.0.0.1"},
                        data={"foto": (io.BytesIO(jpg.tobytes()), "willbert-Victor.jpg")},
                        content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200, r.get_json())
        nomes = os.listdir(self.PASTA)
        self.assertTrue(nomes[0].startswith("willbert-Victor-") and nomes[0].endswith(".jpg"), nomes)
        self.assertEqual(self.rodando(), "rosto")

    def test_modos(self):
        self.assertEqual(self.post("/modo", json={"modo": "xyz"}).status_code, 400)
        r = self.post("/modo", json={"modo": "rosto"})
        self.assertEqual(r.status_code, 400)
        self.assertIn("ainda não há imagem", r.get_json()["msg"])
        r = self.post("/modo", json={"modo": "original"})
        self.assertEqual(r.status_code, 200)
        self.assertIn("topografia", self.comandos())

    def test_temas(self):
        j = self.get("/estado").get_json()
        self.assertEqual(j["tema"], "classico")
        self.assertIn(["vulcao", "Vulcão"], j["temas"])
        self.assertEqual(self.post("/tema", json={"tema": "nada"}).status_code, 400)
        r = self.post("/tema", json={"tema": "oceano"})
        self.assertEqual(r.status_code, 200)
        self.assertEqual(self.get("/estado").get_json()["tema"], "oceano")

    def test_brincadeiras(self):
        self.assertEqual(self.post("/brincar", json={"acao": "xyz"}).status_code, 400)
        r = self.post("/brincar", json={"acao": "tesouro"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual(self.comandos()[-1], "cmd tesouro 3")
        self.assertEqual(self.post("/brincar", json={"acao": "peixes"}).status_code, 200)
        self.assertEqual(self.post("/brincar", json={"acao": "parar"}).status_code, 200)
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-4:],
                         ["cmd tesouroFim", "cmd fossilFim", "cmd animais nada", "cmd labirintoFim"])
        self.assertIn(self.get("/estado").get_json()["tesouro"], ("nenhum", "escondido"))

    def test_labirinto_pelo_painel(self):
        r = self.post("/brincar", json={"acao": "labirinto"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd labirinto normal")
        e = self.get("/estado").get_json()
        self.assertIn(e["labirinto"], ("nenhum", "correndo"))
        self.assertEqual(e["labirinto_rodadas"], 0)

    def test_fossil_pelo_painel(self):
        r = self.post("/brincar", json={"acao": "triceratops"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Triceratops", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd fossil 3 3")
        self.assertIn("fossil", self.get("/estado").get_json())

    def test_ajustes(self):
        self.areia.mostra(self.poe("a.jpg"))
        self.assertEqual(self.post("/ajuste", json={"zoom": "abc"}).status_code, 400)
        self.assertEqual(self.post("/ajuste", json={"zoom": 0.06}).status_code, 200)
        self.assertEqual(self.comandos()[-1], "cmd rostoAjuste 0.06 0.0 0.0 1.0")
        self.assertEqual(self.post("/ajuste", json={"giro": 270}).status_code, 200)
        self.assertEqual(self.areia.estado()["giro"], 270)
        self.assertTrue(self.comandos()[-2].endswith(" 270 0"))


if __name__ == "__main__":
    unittest.main()
