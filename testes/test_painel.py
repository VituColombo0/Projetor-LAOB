"""painel.py: acesso só local/Tailscale, rotas e mensagens de erro (projetor falso)."""
import importlib
import io
import os
import unittest
from unittest import mock

import cv2
import numpy as np

from base import Ambiente

TAILSCALE = {"REMOTE_ADDR": "100.101.102.103"}


class Painel(Ambiente):
    def setUp(self):
        super().setUp()
        import som
        importlib.reload(som)
        import painel
        self.painel = importlib.reload(painel)
        # nada de capturar a tela de verdade: no laob o teste abria um ffmpeg no projetor e gravava no
        # mesmo /dev/shm/areia-projetor-*.jpg do painel em uso
        self.tela_de_verdade = self.painel._tela_do_projetor
        self.painel._tela_do_projetor = lambda: (None, {})
        self.painel.AO_VIVO = os.path.join(self.tmp, "ao-vivo.jpg")
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

    def test_som_pelo_painel(self):
        r = self.get("/estado")
        self.assertEqual(r.status_code, 200)
        e = r.get_json()
        self.assertIn("som", e)
        self.assertIn("volume", e["som"])
        self.assertIn("mudo", e["som"])

        r = self.post("/som", json={"volume": 50, "mudo": True})
        self.assertEqual(r.status_code, 200)
        self.assertEqual(r.get_json()["som"]["volume"], 50)
        self.assertTrue(r.get_json()["som"]["mudo"])

        r = self.post("/som", json={"volume": 150})
        self.assertEqual(r.status_code, 200)
        self.assertEqual(r.get_json()["som"]["volume"], 100)

        r = self.post("/som", json={"volume": -20})
        self.assertEqual(r.status_code, 200)
        self.assertEqual(r.get_json()["som"]["volume"], 0)

        # os botões de testar cada efeito saíram (19/11/2026): o som é automático
        self.assertEqual(self.post("/som/testar", json={"nome": "inicio"}).status_code, 404)
        pagina = self.get("/").get_data(as_text=True)
        for velho in ("Efeitos sonoros", "som-teste-sel", "som-testar-btn", "som-mudo-btn", "/som/testar"):
            self.assertNotIn(velho, pagina)
        cabecalho = pagina[pagina.index("<header>"):pagina.index("</header>")]
        self.assertIn('id="som-top"', cabecalho)                      # mudo e volume ficam no cabeçalho
        self.assertIn('id="som-vol"', cabecalho)

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
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-6:],
                         ["cmd tesouroFim", "cmd fossilFim", "cmd animais nada", "cmd labirintoFim", "cmd jogoFim", "cmd copiaFim"])
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

    def test_vista_do_laboratorio_pelo_painel(self):
        """20/11/2026: botões de área (Tudo, Sala principal, Forense, Miniauditório) e "seguir" por membro."""
        pagina = self.get("/").get_data(as_text=True)
        for v in ("tudo", "principal", "forense", "mini"):
            self.assertIn(f'data-vista="{v}"', pagina)
        self.assertIn("/mundo/vista", pagina)
        self.assertEqual(self.post("/mundo/vista", json={"vista": "forense"}).status_code, 400)   # mundo fechado
        self.assertIsNone(self.get("/estado").get_json()["mundo_vista"])
        self.assertEqual(self.post("/brincar", json={"acao": "mundo"}).status_code, 200)
        log = os.path.join(self.D, "sandbox.log")
        with open(log, "w") as f:
            f.write("Animais: mundo (0)\n")
        self.assertEqual(self.get("/estado").get_json()["mundo_vista"], {"area": "tudo"})
        r = self.post("/mundo/vista", json={"vista": "forense"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Forense", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd mundoVista forense")
        self.assertEqual(self.post("/mundo/vista", json={"vista": "Fulano"}).status_code, 400)   # não é membro
        with open(log, "a") as f:
            f.write("Mundo: vista forense\n")
        self.assertEqual(self.get("/estado").get_json()["mundo_vista"], {"area": "forense"})

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
        self.assertEqual(self.get("/projetor.jpg").status_code, 404)    # sem tela nos testes: 404, nunca erro
        pagina = self.get("/").get_data(as_text=True)
        self.assertIn('id="ao-vivo"', pagina)
        self.assertEqual(pagina.count('data-m="rosto"'), 1)                # a foto só na aba Fotos
        self.assertIn("Vida na areia", pagina)
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd secar")

    def test_comida_pelo_painel(self):
        log = os.path.join(self.D, "sandbox.log")
        self.painel.areia.ESPERA_COMIDA = 0.05                           # o projetor falso não responde
        self.assertEqual(self.post("/comida", json={"x": 0.4, "y": 0.6}).status_code, 400)   # areia fechada
        self.assertEqual(self.post("/mar", json={"carro": 3}).status_code, 200)
        with open(log, "w") as f:
            f.write("Animais: mar (3)\n")
        r = self.post("/comida", json={"x": 0.4, "y": 0.6})
        self.assertEqual(r.status_code, 400)                             # só carrinhos: ninguém vem comer
        self.assertIn("não há bichos que comam", r.get_json()["msg"])
        self.assertNotIn("cmd comida", " ".join(self.comandos()))
        self.assertEqual(self.post("/mar", json={"passaro": 3, "cachorro": 1}).status_code, 200)
        with open(log, "w") as f:
            f.write("Animais: mar (4)\n")
        r = self.post("/comida", json={"x": 0.4, "y": 0.6})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertEqual(r.get_json()["msg"], "Comida jogada: os pássaros e o cachorro vêm comer")
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd comida 0.400 0.600 20 terra")
        # a resposta do SARndbox diz o que caiu
        comando = self.painel.areia._comando

        def com_resposta(*linhas):
            comando(*linhas)
            with open(log, "a") as f:
                f.write("Comida em x -3 y 2 por 20 s (agua)\n")
        self.painel.areia._comando = com_resposta
        try:
            self.assertEqual(self.post("/mar", json={"peixe": 6, "tartaruga": 1, "pato": 2, "passaro": 1}).status_code, 200)
            with open(log, "w") as f:
                f.write("Animais: mar (10)\n")
            r = self.post("/comida", json={"x": 0.2, "y": 0.3})
            self.assertEqual(r.get_json()["msg"], "Ração na água: os peixes, a tartaruga e os patos vêm comer")
        finally:
            self.painel.areia._comando = comando
        for ruim in ({"x": 3, "y": 0.6}, {"y": 0.6}, {}):
            self.assertEqual(self.post("/comida", json=ruim).status_code, 400, ruim)
        self.assertEqual(self.painel.quem_vem_comer({"passaro": 1}), "o pássaro vem comer")
        self.assertEqual(self.painel.quem_vem_comer({"cachorro": 2}), "os cachorros vêm comer")
        self.assertEqual(self.painel.quem_vem_comer({"arraia": 2}), "as arraias vêm comer")
        pagina = self.get("/").get_data(as_text=True)
        self.assertIn('id="selo-comida"', pagina)                        # selo "ao vivo" também na comida
        self.assertIn("m.remove()", pagina)                              # o pão sai se o pedido não deu certo
        self.assertIn("'.racao'", pagina)                                # ração e pão são marcas separadas no quadro

    def test_versao_da_pagina(self):
        """A página aberta recarrega sozinha quando o painel é atualizado (versão no /estado)."""
        pagina = self.get("/").get_data(as_text=True)
        self.assertNotIn("__VERSAO__", pagina)
        v = self.get("/estado").get_json()["versao"]
        self.assertEqual(v, self.painel.VERSAO)
        self.assertRegex(v, r"^[0-9a-f]{10}$")
        self.assertIn(f"const VERSAO = '{v}';", pagina)
        self.assertIn("location.reload()", pagina)
        self.assertNotIn(".g3 ", pagina)                                 # classe CSS sem uso, tirada

    def test_imagem_ao_vivo_inteira(self):
        """/projetor.jpg manda os bytes lidos de uma vez (o tamanho sempre bate) e o ffmpeg grava atômico."""
        arq = os.path.join(self.tmp, "projetor.jpg")
        with open(arq, "wb") as f:
            f.write(b"\xff\xd8quadro\xff\xd9")
        with mock.patch.object(self.painel, "AO_VIVO", arq), mock.patch.object(self.painel, "_liga_ao_vivo", lambda: True):
            r = self.get("/projetor.jpg")
            self.assertEqual(r.status_code, 200)
            self.assertEqual(r.get_data(), b"\xff\xd8quadro\xff\xd9")
            self.assertEqual(r.headers["Content-Length"], str(len(r.get_data())))
            self.assertEqual(r.headers["Content-Type"], "image/jpeg")
        chamadas = []

        class Proc:
            def poll(self):
                return None

        saida = mock.Mock(stdout="HDMI-0 connected 1024x768+1280+0 (normal left inverted right x axis y axis)\n")
        with mock.patch.object(self.painel, "_tela_do_projetor", self.tela_de_verdade), \
                mock.patch.object(self.painel.subprocess, "run", lambda a, **kw: chamadas.append(a) or saida), \
                mock.patch.object(self.painel.subprocess, "Popen", lambda a, **kw: chamadas.append(a) or Proc()):
            self.painel._ao_vivo["proc"] = None
            self.assertTrue(self.painel._liga_ao_vivo())
        self.painel._ao_vivo["proc"] = None
        self.assertEqual(chamadas[0], ["xrandr", "--current"])           # sem sondar as saídas de vídeo
        ff = chamadas[1]
        self.assertEqual(ff[ff.index("-atomic_writing") + 1], "1")
        self.assertEqual(ff[-1], self.painel.AO_VIVO)                    # por último: o pkill casa
        self.assertTrue(any(x.endswith(".0+1280,0") for x in ff), ff)   # a saída do projetor

    def test_copie_o_relevo_pelo_painel(self):
        self.assertIn("Copie o relevo", self.get("/").get_data(as_text=True))
        r = self.post("/brincar", json={"acao": "copia", "nivel": "facil"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("Copie o relevo (fácil)", r.get_json()["msg"])
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd")][-1], "cmd copia sorteio 1")
        self.assertIn("copia", self.get("/estado").get_json())

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


class PainelRevisao(Painel):
    """Revisão final (23/11/2026): câmera e Willbert fora da trava, foto pelo nome exato, upload que não
    abre, números com limite, POST só da própria página, linha de saúde e textos da página."""

    def setUp(self):
        super().setUp()
        self.painel._willbert_cache[:] = [0.0, None]
        # nunca o Willbert nem a câmera de verdade nos testes (no laob eles existem)
        falso = os.path.join(self.tmp, "willbert-falso")
        with open(falso, "w") as f:
            f.write('#!/bin/sh\necho "$1" >> "$(dirname "$0")/willbert.log"\n')
        os.chmod(falso, 0o755)
        self.painel.WILLBERT = [falso]
        self.painel.areia.foto_da_camera = lambda: self.fail("câmera de verdade chamada no teste")
        self.painel.VIGIA = os.path.join(self.tmp, "sem-vigia-de-verdade")
        self.painel.NVIDIA_SMI = os.path.join(self.tmp, "sem-nvidia-smi")

    def tearDown(self):
        self.painel.TAREFA.espera()
        super().tearDown()

    def json_cru(self, url, texto, **kw):
        return self.c.post(url, data=texto, content_type="application/json", environ_base=TAILSCALE, **kw)

    def test_camera_fora_da_trava_e_uma_tarefa_por_vez(self):
        import threading
        import time
        foto = self.poe("camera-teste.jpg")
        solta = threading.Event()

        def camera_lenta():
            solta.wait(20)
            return foto
        self.painel.areia.foto_da_camera = camera_lenta
        r = self.post("/foto", data={}, content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200, r.get_json())
        n = r.get_json()["tarefa"]
        self.assertIn("Tirando a foto", r.get_json()["msg"])
        t0 = time.monotonic()
        r = self.post("/tema", json={"tema": "oceano"})                  # a trava geral está livre
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertLess(time.monotonic() - t0, 5)
        r = self.post("/foto", data={}, content_type="multipart/form-data")
        self.assertEqual(r.status_code, 409)                             # uma tarefa por vez
        self.assertIn("tirando a foto", r.get_json()["msg"])
        self.assertEqual(self.post("/willbert", json={"acao": "ligar"}).status_code, 409)
        t = self.get("/estado").get_json()["tarefa"]
        self.assertEqual((t["id"], t["acao"], t["rodando"]), (n, "foto", True))
        solta.set()
        self.painel.TAREFA.espera()
        t = self.get("/estado").get_json()["tarefa"]
        self.assertEqual((t["rodando"], t["ok"]), (False, True), t)
        self.assertIn("camera-teste.jpg", t["msg"])
        self.assertEqual(self.rodando(), "rosto")                        # projetou

    def test_camera_que_falha_diz_por_que(self):
        def sem_camera():
            raise self.areia.Erro("a câmera não respondeu")
        self.painel.areia.foto_da_camera = sem_camera
        self.assertEqual(self.post("/foto", data={}, content_type="multipart/form-data").status_code, 200)
        self.painel.TAREFA.espera()
        t = self.get("/estado").get_json()["tarefa"]
        self.assertEqual((t["ok"], t["msg"]), (False, "a câmera não respondeu"))

    def test_willbert_em_segundo_plano(self):
        r = self.post("/willbert", json={"acao": "ligar"})
        self.assertEqual(r.status_code, 200, r.get_json())
        self.assertIn("tarefa", r.get_json())
        self.painel.TAREFA.espera()
        t = self.get("/estado").get_json()["tarefa"]
        self.assertEqual((t["acao"], t["ok"]), ("ligar", True), t)
        self.assertIn("Willbert ligado", t["msg"])
        with open(os.path.join(self.tmp, "willbert.log")) as f:
            self.assertEqual(f.read().split(), ["ligar"])
        self.assertEqual(self.post("/willbert", json={"acao": "rm -rf"}).status_code, 400)
        quebrado = os.path.join(self.tmp, "willbert-quebrado")
        with open(quebrado, "w") as f:
            f.write('#!/bin/sh\necho "sem permissão" >&2\nexit 1\n')
        os.chmod(quebrado, 0o755)
        self.painel.WILLBERT = [quebrado]
        self.assertEqual(self.post("/willbert", json={"acao": "desligar"}).status_code, 200)
        self.painel.TAREFA.espera()
        t = self.get("/estado").get_json()["tarefa"]
        self.assertEqual(t["ok"], False)
        self.assertIn("sem permissão", t["msg"])

    def test_escolher_pelo_nome_exato(self):
        self.poe("ana.png")
        self.poe("mariana.png")
        os.makedirs(os.path.join(self.PASTA, self.areia.PASTA_WILLBERT), exist_ok=True)
        imagem_wb = self.poe(os.path.join(self.areia.PASTA_WILLBERT, "willbert-ana.png"))
        for nome, caminho in (("ana.png", os.path.join(self.PASTA, "ana.png")),
                              ("mariana.png", os.path.join(self.PASTA, "mariana.png")),
                              ("willbert-ana.png", imagem_wb)):
            r = self.post("/escolher", json={"imagem": nome})
            self.assertEqual(r.status_code, 200, (nome, r.get_json()))
            self.assertEqual(self.areia.estado()["imagem"], caminho)
        for ruim in ("ana", "1", "an", "nada.png", "", None):
            r = self.post("/escolher", json={"imagem": ruim})
            self.assertEqual(r.status_code, 400, ruim)

    def test_upload_que_nao_abre_nao_fica_na_galeria(self):
        for nome, dados in (("IMG_1.HEIC", b"ftypheic nada de imagem"), ("foto.jpg", b"\xff\xd8 quebrada"),
                            ("x.gif", b"GIF89a quebrado"), ("blob", b"nada")):
            r = self.post("/foto", data={"foto": (io.BytesIO(dados), nome)}, content_type="multipart/form-data")
            self.assertEqual(r.status_code, 400, nome)
            self.assertIn("não consegui abrir essa imagem", r.get_json()["msg"])
        self.assertEqual([n for n in os.listdir(self.PASTA) if not os.path.isdir(os.path.join(self.PASTA, n))], [])
        self.assertNotIn("rosto", self.comandos())
        # o que o OpenCV lê, mas sem extensão conhecida (alguns apps mandam "blob"), vira PNG
        ok, png = cv2.imencode(".png", np.full((120, 90, 3), 200, np.uint8))
        r = self.post("/foto", data={"foto": (io.BytesIO(png.tobytes()), "blob")}, content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200, r.get_json())
        guardadas = [n for n in os.listdir(self.PASTA) if not os.path.isdir(os.path.join(self.PASTA, n))]
        self.assertEqual(len(guardadas), 1)
        self.assertTrue(guardadas[0].endswith(".png"), guardadas)
        self.assertIsNotNone(cv2.imread(os.path.join(self.PASTA, guardadas[0])))

    def test_foto_boa_fica_mesmo_se_o_projetor_falhar(self):
        def projetor_quebrado(*a, **kw):
            raise self.areia.Erro("o SARndbox não aceitou o comando")
        ok, png = cv2.imencode(".png", np.full((120, 90, 3), 150, np.uint8))
        with mock.patch.object(self.painel.areia, "aplica", projetor_quebrado):
            r = self.post("/foto", data={"foto": (io.BytesIO(png.tobytes()), "boa.png")}, content_type="multipart/form-data")
        self.assertEqual(r.status_code, 400)
        self.assertEqual(len([n for n in os.listdir(self.PASTA) if n.endswith(".png")]), 1)   # a foto é boa: fica

    def test_numeros_com_limite(self):
        self.areia.mostra(self.poe("a.jpg"))
        antes = self.areia.estado()["ajuste"]
        for texto in ('{"zoom": NaN}', '{"zoom": Infinity}', '{"zoom": -Infinity}', '{"zoom": "nan"}',
                      '{"zoom": 9}', '{"sombra": -1}', '{"giro": 45}', '{"giro": NaN}', '{"giro": 1e400}',
                      '{"espelho": "sim"}', '{"zoom": true}'):
            r = self.json_cru("/ajuste", texto)
            self.assertEqual(r.status_code, 400, texto)
            self.assertFalse(r.get_json()["ok"], texto)
        self.assertEqual(self.areia.estado()["ajuste"], antes)          # nada gravado
        for texto in ('{"volume": "x"}', '{"volume": NaN}', '{"volume": Infinity}', '{"volume": [1]}',
                      '{"mudo": "talvez"}'):
            r = self.json_cru("/som", texto)
            self.assertEqual(r.status_code, 400, texto)                  # antes: página 500 em HTML
            self.assertIn("som", r.get_json())
        self.assertEqual(self.post("/som", json={"volume": 33.6}).get_json()["som"]["volume"], 34)
        for url, texto in (("/chuva", '{"x": NaN, "y": 0.5}'), ("/chuva", '{"x": 0.5, "y": Infinity}'),
                           ("/chuva", '{"x": 0.5, "y": 0.5, "segundos": 1e9}'), ("/chuva", '{"x": true, "y": 0.5}'),
                           ("/comida", '{"x": NaN, "y": 0.5}'), ("/comida", '{"x": 0.5, "y": -Infinity}'),
                           ("/mar", '{"peixe": 1e400}'), ("/mar", '{"peixe": NaN}'), ("/luz", '{"luz": NaN}'),
                           ("/luz", '{"luz": "fortissimo"}')):
            r = self.json_cru(url, texto)
            self.assertEqual(r.status_code, 400, (url, texto))
        self.assertFalse([c for c in self.comandos() if "nan" in c.lower() or "inf" in c.lower()])

    def test_post_so_da_propria_pagina(self):
        for cab in ({"Origin": "http://exemplo.com"}, {"Origin": "null"},
                    {"Referer": "http://exemplo.com/ataque.html"}, {"Origin": "http://localhost:9999"},
                    {"Origin": "https://localhost"}, {"Sec-Fetch-Site": "cross-site"}):
            r = self.c.post("/reabrir", environ_base=TAILSCALE, headers=cab)
            self.assertEqual(r.status_code, 403, cab)
            r = self.c.post("/tema", json={"tema": "oceano"}, environ_base=TAILSCALE, headers=cab)
            self.assertEqual(r.status_code, 403, cab)
        self.assertEqual([c for c in self.comandos() if c != "estado"], [])   # nada chegou ao projetor
        # a própria página (Origin ou Referer do painel) e quem não é navegador (Willbert, curl) passam
        for cab in ({"Origin": "http://localhost"}, {"Referer": "http://localhost/#chuva"},
                    {"Sec-Fetch-Site": "same-origin", "Origin": "http://localhost"}, {}):
            r = self.c.post("/tema", json={"tema": "oceano"}, environ_base=TAILSCALE, headers=cab)
            self.assertEqual(r.status_code, 200, cab)
        r = self.c.post("/tema", json={"tema": "vulcao"}, environ_base=TAILSCALE,
                        headers={"Host": "100.101.0.1:8090", "Origin": "http://100.101.0.1:8090"})   # segredo-ok (IP inventado)
        self.assertEqual(r.status_code, 200)
        # a foto do Willbert: POST de 127.0.0.1, sem Origin (test_foto_do_willbert cobre o resto)
        ok, jpg = cv2.imencode(".jpg", np.full((80, 60, 3), 90, np.uint8))
        r = self.c.post("/foto", environ_base={"REMOTE_ADDR": "127.0.0.1"}, headers={"Host": "127.0.0.1:8090"},
                        data={"foto": (io.BytesIO(jpg.tobytes()), "willbert-Ana.jpg")}, content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200, r.get_json())

    def test_host_esperado_contra_dns_rebinding(self):
        for host in ("localhost", "localhost:8090", "127.0.0.1:8090", "100.101.0.1:8090", "192.168.0.10:8090",   # segredo-ok (IPs inventados)
                     "laob:8090", "laob", "laob.local:8090", "laob.tail1234.ts.net", "[::1]:8090",
                     "[fd7a:115c:a1e0::1]:8090", "LAOB:8090"):
            self.assertTrue(self.painel.host_esperado(host), host)
            self.assertEqual(self.c.get("/estado", environ_base=TAILSCALE, headers={"Host": host}).status_code, 200, host)
        for host in ("ataque.exemplo.com", "ataque.exemplo.com:8090", "8.8.8.8:8090", "laob:porta", "",
                     "[2001:db8::1]:8090", "a b", "laob.exemplo.com.br"):
            self.assertFalse(self.painel.host_esperado(host), host)
        r = self.c.get("/previa.png", environ_base=TAILSCALE, headers={"Host": "ataque.exemplo.com:8090"})
        self.assertEqual(r.status_code, 403)

    def test_linha_de_saude(self):
        with open(os.path.join(self.D, "sandbox.log"), "w") as f:
            f.write("Desempenho: 30 quadros/s; água atrasada em 0 de 900 passos; a água andou 100% do tempo real\n"
                    "Perfil: quadro p50 33.9 p95 34.1 p99 34.2 max 35 ms, 0 acima de 50 ms; CPU frame 0.01\n"
                    "Animais: mar (3)\n"
                    "Desempenho: 28 quadros/s; água atrasada em 9 de 900 passos; a água andou 97% do tempo real\n")
        with open(os.path.join(self.D, "inicio.log"), "w") as f:
            f.write("2026-11-22 20:13:16 topografia aberta (tentativa 1)\n"
                    "2026-11-23 00:30:10 vigia: a areia estava fechada; abrindo a topografia\n"
                    "2026-11-23 00:40:50 topografia aberta (tentativa 1)\n")
        smi = os.path.join(self.tmp, "nvidia-smi")
        with open(smi, "w") as f:
            f.write('#!/bin/sh\necho "47, 12"\n')
        os.chmod(smi, 0o755)
        vigia = os.path.join(self.tmp, "areia-vigia")
        with open(vigia, "w") as f:
            f.write('#!/bin/sh\n[ "$1" = estado ] && echo "pausado até 14:30"\n')
        os.chmod(vigia, 0o755)
        self.painel.NVIDIA_SMI, self.painel.VIGIA = smi, vigia
        s = self.get("/saude").get_json()
        self.assertEqual((s["qps"], s["agua"], s["p99"]), (28, 97, 34.2))
        self.assertEqual((s["gpu_temp"], s["gpu_uso"]), (47, 12))
        self.assertEqual(s["vigia"], {"estado": "pausado", "ate": "14:30", "reserva": False})
        self.assertIn("a areia estava fechada", s["ultimo_aviso"])
        self.assertGreater(s["disco_livre_gb"], 0)
        self.assertGreater(s["ligado_s"], 0)
        self.assertFalse(s["reserva"])
        self.assertEqual(self.get("/estado").get_json()["vigia"]["estado"], "pausado")   # resumo no /estado
        # guardado: o nvidia-smi não roda a cada pergunta
        os.remove(smi)
        self.assertEqual(self.get("/saude").get_json()["gpu_temp"], 47)
        # sem nada (máquina sem placa, registro vazio): tudo vazio, nunca erro
        self.painel._caches.clear()
        os.remove(os.path.join(self.D, "sandbox.log"))
        self.painel.VIGIA = os.path.join(self.tmp, "nao-existe")
        s = self.get("/saude").get_json()
        self.assertEqual((s["qps"], s["agua"], s["p99"], s["gpu_temp"]), (None, None, None, None))
        self.assertEqual(s["vigia"]["estado"], None)
        with open(os.path.join(self.D, "sem-vigia"), "w"):
            pass
        self.painel._caches.clear()
        self.assertEqual(self.get("/saude").get_json()["vigia"]["estado"], "pausado")
        # areia de reserva (o SARndbox original)
        with open(os.path.join(self.FALSO, "qual"), "w") as f:
            f.write("original\n")
        self.areia._qual_cache[0] = 0.0
        self.painel._caches.clear()
        self.assertTrue(self.get("/saude").get_json()["reserva"])
        self.assertTrue(self.get("/estado").get_json()["reserva"])

    def test_estado_do_vigia_novo_e_antigo(self):
        vigia = os.path.join(self.tmp, "areia-vigia")
        self.painel.VIGIA = vigia
        for saida, esperado in (("vigiando", ("vigiando", None, False)),
                                ("pausado", ("pausado", None, False)),
                                ("pausado até 9:05", ("pausado", "09:05", False)),
                                ("pausado até às 23h40 (volta sozinho)", ("pausado", "23:40", False)),
                                ("vigiando\nreserva", ("vigiando", None, True)),
                                ("vigiando\nreserva: tentando voltar à versão completa", ("vigiando", None, True)),
                                ("vigiando\nreserva: não", ("vigiando", None, False)),
                                ("vigiando\nreserva=0", ("vigiando", None, False)),
                                ("sem reserva\nvigiando", ("vigiando", None, False)),
                                ("", (None, None, False))):
            with open(vigia, "w") as f:
                f.write("#!/bin/sh\ncat <<'FIM'\n" + saida + "\nFIM\n")
            os.chmod(vigia, 0o755)
            v = self.painel._le_vigia()
            self.assertEqual((v["estado"], v["ate"], v["reserva"]), esperado, saida)

    def test_mensagens_de_retorno(self):
        r = self.post("/brincar", json={"acao": "tesouro"})
        self.assertIn("5,5 cm", r.get_json()["msg"])                    # vírgula, não ponto
        r = self.post("/chuva", json={"tudo": True})
        self.assertEqual(r.get_json()["msg"], "Chovendo na areia toda")
        self.assertEqual([c for c in self.comandos() if c.startswith("cmd chuva")][-4:],
                         ["cmd chuva 0.250 0.300 10", "cmd chuva 0.750 0.300 10",
                          "cmd chuva 0.250 0.700 10", "cmd chuva 0.750 0.700 10"])
        self.areia.mostra(self.poe("a.jpg"))
        self.assertEqual(self.post("/ajuste", json={"espelho": True}).get_json()["msg"], "Foto espelhada")
        self.assertEqual(self.post("/ajuste", json={"giro": 90}).get_json()["msg"], "Foto girada: 90°")
        # comida na terra: a tigela de ração (o pão saiu em 22/11/2026)
        log = os.path.join(self.D, "sandbox.log")
        self.painel.areia.ESPERA_COMIDA = 0.05
        self.assertEqual(self.post("/mar", json={"passaro": 2}).status_code, 200)
        comando = self.painel.areia._comando

        def com_resposta(*linhas):
            comando(*linhas)
            with open(log, "a") as f:
                f.write("Comida em x 3 y 2 por 20 s (terra)\n")
        with open(log, "w") as f:
            f.write("Animais: mar (2)\n")
        with mock.patch.object(self.painel.areia, "_comando", com_resposta):
            r = self.post("/comida", json={"x": 0.5, "y": 0.5})
        self.assertEqual(r.get_json()["msg"], "Tigela de ração na areia: os pássaros vêm comer")

    def test_pagina_acabamento(self):
        p = self.get("/").get_data(as_text=True)
        self.assertEqual(p.count('class="toque"'), 2)                   # "toque na imagem" em Chuva e Vida
        self.assertIn('id="cam-motivo"', p)                             # câmera desligada diz por quê
        self.assertIn("Câmera indisponível: o Willbert está desligado", p)
        self.assertIn('id="saude"', p)
        self.assertIn("tag.type = 'button'", p)                          # chips das fotos pelo teclado
        self.assertIn("document.addEventListener('click'", p)            # ✓ no botão ativado pelo teclado
        self.assertIn("Espere: ainda fazendo o pedido anterior", p)
        self.assertIn("Máximo de 32 bichos", p)
        self.assertIn("--ok: #24733d", p)                                # contraste da mensagem de sucesso
        self.assertNotRegex(p, r"min-height: 3[0-9]px")                  # alvos de toque com 44 px
        for aba in ("jogos", "bichos", "chuva", "laob", "fotos", "projetor"):   # as seis abas de sempre
            self.assertIn(f'data-ir="{aba}"', p)


class AmbienteDoSom(Ambiente):
    """som.py: o fundo automático (tema, brincadeira e bichos), o fade do motor e o mudo (AREIA_SOM_FALSO)."""

    def setUp(self):
        super().setUp()
        import som
        self.som = importlib.reload(som)
        self.log = os.path.join(self.D, "sandbox.log")

    def escreve_log(self, texto):
        with open(self.log, "w") as f:
            f.write(texto)

    def abre(self, qual="rosto", modo="original"):
        with open(os.path.join(self.FALSO, "qual"), "w") as f:
            f.write(qual + "\n")
        self.areia._qual_cache[0] = 0.0   # o "qual" vale 1 s: pergunta de novo
        e = self.areia.estado()
        e["modo"] = modo
        self.areia.salva(e)

    def test_arquivos_de_som(self):
        """Todo som citado nas tabelas existe, é 22,05 kHz mono 16 bits e tem menos de 1 MB."""
        import wave
        nomes = set()
        for tabela in (self.som.AMBIENTE_TEMAS, self.som.AMBIENTE_BRINCADEIRAS,
                       {"lab": self.som.AMBIENTE_LABORATORIO, "chuva": {"base": self.som.CHUVA}}):
            for c in tabela.values():
                nomes |= set(c["base"])
                for variantes, *_ in c.get("pontuais", []):
                    nomes |= set(variantes)
        for variantes, _ in self.som.SONS_BICHOS.values():
            nomes |= set(variantes)
        for nome in ("tesouro", "fossil", "racao_agua", "racao_terra", "porta", "vitoria", "alerta", "inicio", "splash", "passaro"):
            self.assertTrue(os.path.isfile(os.path.join(self.som._dir_sons(), nome + ".wav")), nome)   # efeitos do vigia (paplay: qualquer taxa)
        for nome in sorted(nomes):
            arq = os.path.join(self.som._dir_sons(), nome + ".wav")
            self.assertLess(os.path.getsize(arq), 1024 * 1024, nome)
            with wave.open(arq) as w:
                self.assertEqual((w.getframerate(), w.getnchannels(), w.getsampwidth()), (22050, 1, 2), nome)
            self.assertIsNotNone(self.som._carrega_som(nome), nome)
        for tema in self.areia.NOMES_TEMAS:   # todo tema do painel tem o seu fundo
            self.assertIn(tema, self.som.AMBIENTE_TEMAS)
        for jogo in self.areia.JOGOS:          # e todo jogo de água a sua camada
            self.assertIn(jogo, self.som.AMBIENTE_BRINCADEIRAS)

    def test_laco_sem_emenda(self):
        """O fim de cada fundo emenda no começo sem degrau (o laço não estala)."""
        for c in list(self.som.AMBIENTE_TEMAS.values()) + list(self.som.AMBIENTE_BRINCADEIRAS.values()):
            for nome in c["base"]:
                d = self.som._carrega_som(nome).astype(float)
                tipico = np.abs(np.diff(d)).mean()
                self.assertLess(abs(d[0] - d[-1]), 8 * tipico + 50, nome)

    def test_cena_por_tema_jogo_e_bichos(self):
        m = self.som.monta_cena
        self.assertEqual(list(m("inverno")["base"]), ["amb_inverno"])
        # 22/11/2026: só o que aparece na areia (sem sino de vento, passarinho, gaivota, cigarra, eco)
        for tema in ("inverno", "classico", "oceano", "deserto", "marte"):
            self.assertEqual(m(tema)["pontuais"], [], tema)
        self.assertIn("ronco_vulcao_1", m("vulcao")["pontuais"][0][0])   # a lava ronca
        self.assertIn("brilho_1", m("arco-iris")["pontuais"][0][0])      # as estrelas piscam
        self.assertEqual(list(m("deserto")["base"]), ["amb_deserto"])
        self.assertEqual(list(m("vulcao")["base"]), ["amb_vulcao"])
        self.assertEqual(list(m("oceano")["base"]), ["amb_oceano"])
        self.assertEqual(list(m("arco-iris")["base"]), ["amb_arcoiris"])
        # jogo: camada própria por cima do tema mais baixo; os pontuais passam a ser os do jogo
        v = m("inverno", "vila")
        self.assertEqual(set(v["base"]), {"amb_inverno", "jogo_riacho"})   # a água da vila vem das nascentes
        self.assertLess(v["base"]["amb_inverno"], m("inverno")["base"]["amb_inverno"])
        self.assertEqual(v["pontuais"], [])                                 # sem trovão: não chove na vila
        self.assertEqual(set(m("classico", "labirinto")["base"]), {"amb_classico", "jogo_riacho"})
        self.assertIn("jogo_correnteza", m("classico", "corrida")["base"])
        self.assertIn("jogo_misterio", m("deserto", "fossil")["base"])
        self.assertIn("jogo_atelie", m("marte", "copia")["base"])
        self.assertEqual(set(m("inverno", "vulcao")["base"]), {"amb_vulcao", "jogo_tensao"})   # o vulcão é o mundo todo
        # bichos: só quem está na areia e faz som
        b = m("oceano", None, ["pato", "pessoa", "peixe"])["bichos"]
        self.assertEqual([x[0] for x in b], ["pato", "peixe"])
        self.assertEqual(m("classico", None, ["pessoa"])["bichos"], [])
        self.assertEqual(m("tema-novo")["base"], {})   # tema sem som próprio: silêncio (sem erro)
        # chovendo: a chuva entra por cima; no laboratório, o som da sala no lugar do tema
        self.assertIn("jogo_chuva", m("deserto", None, (), True)["base"])
        self.assertNotIn("jogo_chuva", m("deserto")["base"])
        lab = m("classico", None, (), False, True)
        self.assertEqual(set(lab["base"]), {"amb_laboratorio"})
        self.assertIn("teclado_1", lab["pontuais"][0][0])

    def test_chuva_pelo_registro(self):
        """A chuva do som segue o registro: a mão começa/para, a nuvem do painel dura o que pediram."""
        s = self.som
        s._esquece_chuva()
        self.assertFalse(s.chovendo())
        s._anota_chuva("Chuva: mao comecou")
        self.assertTrue(s.chovendo())
        s._anota_chuva("Chuva: mao parou")
        self.assertFalse(s.chovendo())
        self.assertAlmostEqual(s._anota_chuva("Chuva: nuvem por 8 s"), 9.0)
        self.assertTrue(s.chovendo())
        s._anota_chuva("Chuva: a água foi seca")
        self.assertFalse(s.chovendo())

    def test_cena_pelo_que_esta_na_areia(self):
        self.assertIsNone(self.som.cena_atual())                              # areia fechada
        self.abre("original")
        self.assertIsNone(self.som.cena_atual())                              # topografia de reserva
        self.abre("rosto", "rosto")
        self.assertIsNone(self.som.cena_atual())                              # foto no projetor
        self.abre("rosto", "original")
        self.assertEqual(list(self.som.cena_atual()["base"]), ["amb_classico"])
        self.areia.escolhe_tema("inverno")
        self.assertEqual(list(self.som.cena_atual()["base"]), ["amb_inverno"])
        self.abre("rosto", "relevo")
        self.escreve_log("Jogo pedido: vila (nível 2)\nJogo: salve a vila (4 casas)\n")
        self.assertIn("jogo_riacho", self.som.cena_atual()["base"])   # a água da vila vem das nascentes
        self.assertNotIn("jogo_chuva", self.som.cena_atual()["base"])
        self.escreve_log("Jogo pedido: vila (nível 2)\nJogo desligado\nLabirinto: nascente em x 1 y 2, alvo em x 3 y 4 (dificuldade 1)\n")
        self.assertIn("jogo_riacho", self.som.cena_atual()["base"])
        self.escreve_log("Tesouro: escondendo a 4 cm\n")
        self.assertIn("jogo_misterio", self.som.cena_atual()["base"])
        e = self.areia.estado()
        e["mar"] = {"pato": 2, "peixe": 3, "pessoa": 1}
        self.areia.salva(e)
        self.escreve_log("Tesouro: escondendo a 4 cm\nTesouro desligado\nAnimais: mar (6)\n")
        c = self.som.cena_atual()
        self.assertEqual(list(c["base"]), ["amb_inverno"])
        self.assertEqual([b[0] for b in c["bichos"]], ["pato", "peixe"])
        self.escreve_log("Animais: carros (5)\n")
        self.assertEqual([b[0] for b in self.som.cena_atual()["bichos"]], ["carro"])

    def test_ambiente_segue_a_areia_e_o_mudo(self):
        s = self.som
        s.atualiza_ambiente()
        self.assertEqual(s._amb["enviada"], {})                               # areia fechada: nada toca
        self.assertFalse(s._tem_som(s._amb["enviada"]))
        self.abre("rosto", "original")
        s.atualiza_ambiente()
        self.assertEqual(list(s._amb["enviada"]["base"]), ["amb_classico"])
        n = len(s._amb["historico"])
        s.atualiza_ambiente()
        self.assertEqual(len(s._amb["historico"]), n)                        # sem mudança, nada é reenviado

        class Proc:   # um motor e um efeito "tocando"
            mortos = []

            def poll(self):
                return None

            def kill(self):
                Proc.mortos.append(self)

            def wait(self, timeout=None):
                return 0
        motor, efeito = Proc(), Proc()
        s._amb["proc"] = motor
        s._tocando.add(efeito)
        s.definir_mudo(True)
        self.assertEqual(Proc.mortos, [motor, efeito])                       # para tudo na hora, sem fade
        self.assertIsNone(s._amb["proc"])
        self.assertIsNone(s._amb["enviada"])
        s.atualiza_ambiente()
        self.assertIsNone(s._amb["enviada"])                                  # mudo: continua parado
        s.definir_mudo(False)
        s.atualiza_ambiente()
        self.assertEqual(list(s._amb["enviada"]["base"]), ["amb_classico"])   # volta sozinho
        s.definir_volume(0)
        s.atualiza_ambiente()
        self.assertIsNone(s._amb["enviada"])                                  # volume 0 = mudo
        s.definir_volume(60)
        self.abre("original")
        s.atualiza_ambiente()
        self.assertEqual(s._amb["enviada"], {})                               # reserva: some com fade

    def test_motor_troca_com_fade(self):
        nivel = {"a": 8000, "b": 16000}
        carrega = lambda nome: np.full(44100, nivel[nome], np.int16) if nome in nivel else None
        mix = self.som.Mistura(carrega)
        taxa, bloco = self.som.TAXA, 1024

        def anda(segundos):
            ultimo = None
            for _ in range(int(segundos * taxa / bloco)):
                ultimo = mix.gera(bloco)
            return ultimo
        mix.cena({"base": {"a": 0.5}})
        self.assertLess(float(mix.gera(bloco).max()), 0.02)                   # entra devagar, sem estalo
        meio = anda(0.7)
        self.assertTrue(0.03 < float(meio.mean()) < 0.10)                     # no meio do fade
        cheio = anda(1.0)
        self.assertAlmostEqual(float(cheio.mean()), 0.5 * 8000 / 32768, places=3)
        mix.cena({"base": {"b": 0.5}})                                        # troca de tema
        logo = mix.gera(bloco)
        self.assertGreater(float(logo.mean()), 0.11)                          # o antigo ainda está lá...
        self.assertEqual(len(mix.vozes), 2)
        depois = anda(1.6)
        self.assertEqual([v["chave"] for v in mix.vozes], ["b"])             # ...e sai sozinho
        self.assertAlmostEqual(float(depois.mean()), 0.5 * 16000 / 32768, places=3)
        mix.cena({})
        anda(1.6)
        self.assertFalse(mix.ativa())                                         # silêncio: o motor fecha o pacat

    def test_bichos_um_de_cada_vez(self):
        import random
        curto = np.full(22050, 1000, np.int16)                               # 1 s cada
        mix = self.som.Mistura(lambda nome: curto, random.Random(7))
        mix.cena({"base": {}, "pontuais": [[["sino"], 0.1, 20, 40]],
                  "bichos": [["pato", ["quack"], 0.2], ["passaro", ["piu"], 0.2]]})
        inicios, juntos, um = [], 0, mix._um

        def anota(chave, nome, ganho):
            if chave.startswith("b:"):
                inicios.append((mix.t, chave))
            um(chave, nome, ganho)
        mix._um = anota
        for _ in range(int(600 * self.som.TAXA / 1024)):
            mix.gera(1024)
            juntos = max(juntos, sum(1 for v in mix.vozes if not v["laco"]))
        self.assertEqual(juntos, 1)                                           # nunca dois ao mesmo tempo
        self.assertLess(inicios[0][0], 31)                                    # o primeiro vem logo
        intervalos = [b[0] - a[0] for a, b in zip(inicios, inicios[1:])]
        self.assertGreaterEqual(min(intervalos), 14.9)                        # raros: 15 a 60 s
        self.assertEqual({c for _, c in inicios}, {"b:pato", "b:passaro"})
        mix.cena({"base": {}, "bichos": [["pato", ["quack"], 0.2]]})          # o pássaro saiu da areia
        self.assertTrue(all(v["alvo"] == 0 for v in mix.vozes if v["chave"] == "b:passaro"))

    def test_motor_sai_com_o_painel(self):
        """python som.py motor --nulo (sem pacat): toca a cena e sai quando a entrada fecha."""
        import json
        import subprocess
        import sys
        import time
        p = subprocess.Popen([sys.executable, os.path.join(self.som.__file__), "motor", "--nulo"],
                             stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            p.stdin.write((json.dumps(self.som.monta_cena("oceano", None, ["pato"])) + "\n").encode())
            p.stdin.flush()
            time.sleep(1.0)
            self.assertIsNone(p.poll())
            p.stdin.close()                                                   # o painel morreu
            self.assertEqual(p.wait(timeout=5), 0)
        finally:
            if p.poll() is None:
                p.kill()
            erros = p.stderr.read()
            p.stderr.close()
        self.assertEqual(erros, b"")

    def test_motor_com_pacat_falso(self):
        """O caminho de verdade com um pacat de mentira (sem som): recebe 22,05 kHz mono no ritmo do
        relógio, fecha depois do fade quando a areia silencia e reabre sozinho se o pacat cair."""
        import json
        import subprocess
        import sys
        import time
        pasta = os.path.join(self.tmp, "bin")
        os.makedirs(pasta)
        registro = os.path.join(self.tmp, "pacat.log")
        with open(os.path.join(pasta, "pacat"), "w") as f:
            f.write(f"""#!{sys.executable}
import os, sys, time
log = {registro!r}
with open(log, "a") as f:
    f.write(" ".join(sys.argv[1:]) + "\\n")
if not os.path.exists(log + ".caiu"):   # o primeiro cai logo, como sem a saída combinada
    open(log + ".caiu", "w").close()
    sys.exit(1)
total, t0 = 0, time.time()
while True:
    b = sys.stdin.buffer.read(4410)
    if not b:
        break
    total += len(b)
    adiante = total / 44100 - (time.time() - t0)
    if adiante > 0:
        time.sleep(adiante)
with open(log + ".bytes", "w") as f:
    f.write(str(total))
""")
        os.chmod(os.path.join(pasta, "pacat"), 0o755)
        amb = dict(os.environ, PATH=pasta + os.pathsep + os.environ["PATH"])
        p = subprocess.Popen([sys.executable, self.som.__file__, "motor"], stdin=subprocess.PIPE,
                             stderr=subprocess.PIPE, env=amb)
        try:
            p.stdin.write((json.dumps(self.som.monta_cena("inverno")) + "\n").encode())
            p.stdin.flush()
            time.sleep(4.5)                                                   # cai, espera 2 s e reabre
            p.stdin.write(b"{}\n")                                            # areia fechou: fade e fecha
            p.stdin.flush()
            for _ in range(40):
                if os.path.exists(registro + ".bytes"):
                    break
                time.sleep(0.1)
            self.assertIsNone(p.poll())                                       # o motor segue de pé, calado
            p.stdin.close()
            self.assertEqual(p.wait(timeout=5), 0)
        finally:
            if p.poll() is None:
                p.kill()
            erros = p.stderr.read().decode()
            p.stderr.close()
        with open(registro) as f:
            chamadas = f.read().split("\n")[:-1]
        self.assertEqual(len(chamadas), 2, chamadas)
        for c in chamadas:
            self.assertIn("--rate=22050", c)
            self.assertIn("--channels=1", c)
            self.assertIn("--device=combinado", c)
        self.assertIn("parou", erros)
        with open(registro + ".bytes") as f:
            recebido = int(f.read()) / 44100                                  # segundos de som
        self.assertGreater(recebido, 1.5)                                     # ~2 s tocando + 1,5 s de fade
        self.assertLess(recebido, 6.0)


if __name__ == "__main__":
    unittest.main()
