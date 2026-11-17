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

    def test_so_local_rede_do_lab_e_tailscale(self):
        for ip in ("127.0.0.1", "100.64.0.1", "::ffff:100.64.0.9", "192.168.0.50", "10.0.0.2",
                   "172.17.173.5", "::ffff:192.168.0.5"):
            self.assertEqual(self.get("/", ip).status_code, 200, ip)
        for ip in ("8.8.8.8", "200.136.1.1", "::ffff:8.8.4.4", "2001:db8::1"):
            self.assertEqual(self.get("/estado", ip).status_code, 403, ip)

    def test_escolher_so_foto_das_pastas(self):
        self.poe("a.jpg")
        self.assertEqual(self.post("/escolher", json={"imagem": "a.jpg"}).status_code, 200)
        self.assertEqual(self.post("/escolher", json={"imagem": "/etc/x.jpg"}).status_code, 400)
        self.assertEqual(self.post("/escolher", json={}).status_code, 400)
        self.assertEqual(self.post("/ajuste", json={"giro": "abc"}).status_code, 400)

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
        nomes = os.listdir(os.path.join(self.PASTA, self.areia.PASTA_WILLBERT))  # subpasta desde 2026-11-11
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
        self.assertEqual(self.comandos()[-1], "cmd tesouro 5.5")
        self.assertEqual(self.post("/brincar", json={"acao": "peixes"}).status_code, 200)
        self.assertEqual(self.post("/brincar", json={"acao": "parar"}).status_code, 200)
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-5:],
                         ["cmd tesouroFim", "cmd fossilFim", "cmd animais nada", "cmd labirintoFim", "cmd jogoFim"])
        self.assertIn(self.get("/estado").get_json()["tesouro"], ("nenhum", "escondido"))

    def test_niveis_willbert_e_reabrir(self):
        r = self.post("/brincar", json={"acao": "tesouro", "nivel": "facil"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd tesouro 4")
        self.post("/brincar", json={"acao": "labirinto", "nivel": "dificil"})
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd labirinto dificil")
        self.assertEqual(self.post("/brincar", json={"acao": "tesouro", "nivel": "xyz"}).status_code, 400)
        self.assertEqual(self.post("/willbert", json={"acao": "explodir"}).status_code, 400)
        r = self.post("/reabrir", json={})
        self.assertEqual(r.status_code, 200, r.get_json())
        c = [x for x in self.comandos() if x != "estado"]
        self.assertEqual(c[-2:], ["original", "topografia"])   # fecha o que estiver aberto e abre de novo
        self.assertIn("willbert", self.get("/estado").get_json())

    def test_labirinto_pelo_painel(self):
        r = self.post("/brincar", json={"acao": "labirinto"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd labirinto normal")
        e = self.get("/estado").get_json()
        self.assertIn(e["labirinto"], ("nenhum", "correndo"))
        self.assertEqual(e["labirinto_rodadas"], 0)

    def test_jogos_de_agua_pelo_painel(self):
        pagina = self.get("/").get_data(as_text=True)
        self.assertNotIn("__JOGOS__", pagina)
        import areia
        for nome, (titulo, curta, desc) in areia.JOGOS.items():
            self.assertIn(titulo, pagina)
            self.assertIn(desc[:40], pagina)   # a descrição de cada jogo está no painel
        r = self.post("/brincar", json={"acao": "rota", "nivel": "facil"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Rota", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd jogo rota 1")
        self.assertEqual(self.post("/brincar", json={"acao": "barco"}).status_code, 400)   # jogo retirado
        e = self.get("/estado").get_json()
        self.assertIn("casas", e["jogo"])
        self.assertIn(e["animais"], ("nada", "peixes", "carros", "pessoas", "todos"))
        r = self.post("/brincar", json={"acao": "pessoas"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd animais pessoas 10")
        self.assertEqual(self.post("/brincar", json={"acao": "jogoNovo"}).status_code in (200, 400), True)

    def test_mundo_laob_pelo_painel(self):
        pagina = self.get("/").get_data(as_text=True)
        self.assertIn('id="sec-laob"', pagina)
        self.assertIn("Abrir o laboratório", pagina)
        self.assertEqual(self.post("/mundo/rosto", json={"nome": "Victor"}).status_code, 400)   # mundo fechado
        r = self.post("/brincar", json={"acao": "mundo"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Laboratório na areia", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd mundo 0 0 - 0")
        self.assertEqual(self.get("/estado").get_json()["mundo"], [])    # sem fotos nem Willbert nos testes

    def test_mar_e_chuva_pelo_painel(self):
        e = self.get("/estado").get_json()
        self.assertEqual(len(e["especies"]), 12)                         # 8 na água, 3 em terra, 1 no ar
        self.assertEqual(sorted({g for _, _, _, g in e["especies"]}), ["Em terra", "Na água", "No ar"])
        self.assertEqual((e["mar"]["peixe"], e["mar"]["carro"]), (6, 4))
        r = self.post("/mar", json={"peixe": 3, "tartaruga": 1})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("3 peixes", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd mar peixe=3 tartaruga=1")
        self.assertEqual(self.post("/mar", json={}).status_code, 400)
        r = self.post("/chuva", json={"x": 0.4, "y": 0.6})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd chuva 0.400 0.600 8")
        self.assertEqual(self.post("/chuva", json={"x": 3, "y": 0.6}).status_code, 400)
        self.assertEqual(self.post("/chuva", json={"secar": True}).status_code, 200)
        self.assertIn(self.get("/projetor.jpg").status_code, (200, 404))   # sem tela nos testes: 404, nunca erro
        pagina = self.get("/").get_data(as_text=True)
        self.assertIn('id="ao-vivo"', pagina)
        self.assertEqual(pagina.count('data-m="rosto"'), 1)                # a foto só na aba Fotos
        self.assertIn("Vida na areia", pagina)
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd secar")

    def test_fossil_pelo_painel(self):
        r = self.post("/brincar", json={"acao": "triceratops"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Triceratops", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd fossil 3 5.5")
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
