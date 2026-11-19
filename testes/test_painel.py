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
        for tabela in (self.som.AMBIENTE_TEMAS, self.som.AMBIENTE_BRINCADEIRAS):
            for c in tabela.values():
                nomes |= set(c["base"])
                for variantes, *_ in c.get("pontuais", []):
                    nomes |= set(variantes)
        for variantes, _ in self.som.SONS_BICHOS.values():
            nomes |= set(variantes)
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
        self.assertIn("sino_neve_1", m("inverno")["pontuais"][0][0])
        self.assertEqual(list(m("deserto")["base"]), ["amb_deserto"])
        self.assertEqual(list(m("vulcao")["base"]), ["amb_vulcao"])
        self.assertEqual(list(m("oceano")["base"]), ["amb_oceano"])
        self.assertEqual(list(m("arco-iris")["base"]), ["amb_arcoiris"])
        # jogo: camada própria por cima do tema mais baixo; os pontuais passam a ser os do jogo
        v = m("inverno", "vila")
        self.assertEqual(set(v["base"]), {"amb_inverno", "jogo_chuva"})
        self.assertLess(v["base"]["amb_inverno"], m("inverno")["base"]["amb_inverno"])
        self.assertEqual([p[0][0] for p in v["pontuais"]], ["trovao_1"])
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
        self.assertIn("jogo_chuva", self.som.cena_atual()["base"])
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
