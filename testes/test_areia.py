"""areia.py e o comando areia-foto: pasta, escolha da imagem, estado, cache e projetor (falso)."""
import io
import json
import os
import unittest
from contextlib import redirect_stdout

from base import Ambiente, escreve, imagem


class Pasta(Ambiente):
    def test_lista_so_imagens_em_ordem(self):
        for n in ("b.jpg", "A.png", "c.JPG", "foto10.jpg", "foto2.jpg", "b-2.jpg",
                  ".escondida.png", "COMO USAR.txt", "d.heic"):
            escreve(os.path.join(self.PASTA, n), b"x")
        os.mkdir(os.path.join(self.PASTA, "sub.png"))
        self.assertEqual([os.path.basename(c) for c in self.areia.imagens()],
                         ["A.png", "b.jpg", "b-2.jpg", "c.JPG", "foto2.jpg", "foto10.jpg"])

    def test_acha_por_numero_nome_e_erros(self):
        a, b = self.poe("ana.jpg"), self.poe("bruno.png")
        self.poe("ana-2.jpg")
        self.assertEqual(self.areia.acha("3"), b)
        self.assertEqual(self.areia.acha("BRU"), b)
        self.assertEqual(self.areia.acha("1"), a)
        for pedido, msg in (("9", "número 9"), ("0", "número 0"), ("ana", "mais de uma"), ("zé", "nenhuma")):
            with self.assertRaisesRegex(self.areia.Erro, msg):
                self.areia.acha(pedido)

    def test_arquivo_de_fora_e_copiado_sem_sobrescrever(self):
        fora = os.path.join(self.tmp, "fora")
        os.mkdir(fora)
        x = imagem(os.path.join(fora, "foto.jpg"), cor=(1, 2, 3))
        y = imagem(os.path.join(fora, "outra.jpg"), cor=(9, 9, 9))
        primeira = self.areia.acha(x)
        self.assertEqual(primeira, os.path.join(self.PASTA, "foto.jpg"))
        self.assertEqual(self.areia.acha(x), primeira)            # mesmo conteúdo: não duplica
        os.replace(y, x)                                          # outro conteúdo, mesmo nome
        self.assertEqual(self.areia.acha(x), os.path.join(self.PASTA, "foto-2.jpg"))

    def test_formato_nao_suportado(self):
        p = os.path.join(self.tmp, "foto.heic")
        escreve(p, b"x")
        with self.assertRaisesRegex(self.areia.Erro, "HEIC"):
            self.areia.acha(p)


class UmaBrincadeiraPorVez(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def test_trocar_encerra_a_anterior(self):
        self.areia.animais("todos")
        self.areia.labirinto("facil")
        self.assertEqual(self.cmds()[-3:], ["cmd jogoFim", "cmd animais nada", "cmd labirinto facil"])
        self.areia.tesouro()
        self.assertEqual(self.cmds()[-3:], ["cmd jogoFim", "cmd animais nada", "cmd tesouro 5.5"])
        self.areia.fossil("pegada")
        self.assertEqual(self.cmds()[-3:], ["cmd jogoFim", "cmd animais nada", "cmd fossil 5 5.5"])
        self.areia.animais("peixes")
        self.assertEqual(self.cmds()[-3:], ["cmd labirintoFim", "cmd jogoFim", "cmd mar peixe=6 tubarao=1 tartaruga=2 aguaviva=2 polvo=1 arraia=1 pato=2 barco=1"])

    def test_foto_encerra_as_brincadeiras(self):
        self.areia.animais("todos")
        self.poe("ana.jpg")
        os.makedirs(self.areia.FOTOS, exist_ok=True)
        escreve(os.path.join(self.areia.FOTOS, "rosto.png"), b"x")
        e = self.areia.estado()
        e["modo"] = "rosto"
        self.areia.aplica(e)
        c = self.cmds()
        self.assertIn("cmd animais nada", c[c.index("cmd mar peixe=6 tubarao=1 tartaruga=2 aguaviva=2 polvo=1 arraia=1 pato=2 barco=1 carro=4 pessoa=4 cachorro=2 passaro=3"):])
        self.assertTrue(c[-2].startswith("cmd rosto "))


class FotosDoWillbert(Ambiente):
    def test_fotos_do_willbert_ficam_na_subpasta(self):
        for i in range(3):
            self.poe(f"willbert-Lucas-{i}.png")
        self.poe("minha-foto.jpg")
        self.assertEqual(self.areia.arquiva_willbert(), 3)
        self.assertEqual([os.path.basename(c) for c in self.areia.imagens()], ["minha-foto.jpg"])
        sub = os.path.join(self.PASTA, self.areia.PASTA_WILLBERT)
        self.assertEqual(sorted(os.listdir(sub)), [f"willbert-Lucas-{i}.png" for i in range(3)])
        self.assertEqual(self.areia.arquiva_willbert(), 0)
        # pelo nome, acha a mais nova da subpasta
        self.assertEqual(os.path.basename(self.areia.acha("Lucas-2")), "willbert-Lucas-2.png")


class Estado(Ambiente):
    def test_padrao_e_arquivo_estragado(self):
        self.assertEqual(self.areia.estado()["modo"], "original")
        escreve(self.areia.ESTADO, "{quebrado")
        e = self.areia.estado()
        self.assertEqual((e["giro"], e["espelho"], e["ajuste"]["mistura"]), (0, False, 0.0))

    def test_valores_invalidos_sao_corrigidos(self):
        escreve(self.areia.ESTADO, json.dumps({"modo": "x", "giro": 450, "fundo": "--rm", "ajuste": {"zoom": 0.05}}))
        e = self.areia.estado()
        self.assertEqual((e["modo"], e["giro"], e["fundo"]), ("original", 90, ""))
        self.assertEqual(e["ajuste"], {**self.areia.AJUSTE, "zoom": 0.05})

    def test_salva_e_le_de_volta(self):
        e = self.areia.estado()
        e.update(giro=180, imagem="/x/ç.png")
        self.areia.salva(e)
        self.assertEqual(self.areia.estado()["imagem"], "/x/ç.png")


class Preparo(Ambiente):
    def test_cache(self):
        img = self.poe("a.jpg")
        e = self.areia.estado()
        _, novo = self.areia.prepara(img, e)
        self.assertTrue(novo)
        self.assertTrue(os.path.exists(os.path.join(self.areia.FOTOS, "rosto.png")))
        _, novo = self.areia.prepara(img, e)
        self.assertFalse(novo)                          # segunda vez: do cache
        e["giro"] = 90
        _, novo = self.areia.prepara(img, e)
        self.assertTrue(novo)                           # giro diferente: prepara de novo

    def test_limpa_cache(self):
        for i in range(5):
            os.makedirs(os.path.join(self.areia.CACHE, f"c{i}"))
        self.areia.limpa_cache(manter=2)
        self.assertEqual(len(os.listdir(self.areia.CACHE)), 2)

    def test_erro_ao_preparar(self):
        p = os.path.join(self.PASTA, "ruim.jpg")
        escreve(p, "não é imagem")
        with self.assertRaisesRegex(self.areia.Erro, "não consegui preparar"):
            self.areia.prepara(p, self.areia.estado())
        self.assertEqual(os.listdir(self.areia.CACHE) if os.path.isdir(self.areia.CACHE) else [], [])


class Projetor(Ambiente):
    def test_mostra_sobe_a_versao_com_foto_e_manda_a_imagem(self):
        img = self.poe("a.jpg")
        self.areia.mostra(img)
        cmds = self.comandos()
        self.assertEqual(self.rodando(), "rosto")
        self.assertIn("rosto", cmds)
        self.assertTrue(any(c.startswith("cmd rosto ") and c.endswith(" 0 0") for c in cmds))
        self.assertTrue(any(c.startswith("cmd rostoAjuste 0.03 0.0 0.0 1.0") for c in cmds))
        e = self.areia.estado()
        self.assertEqual((e["modo"], e["imagem"]), ("rosto", img))

    def test_topografia(self):
        self.areia.mostra(self.poe("a.jpg"))
        e = self.areia.estado()
        e["modo"] = "original"
        self.areia.aplica(e)
        self.assertEqual(self.comandos()[-2], "topografia")   # a topografia roda na versão com efeitos
        self.assertEqual(self.areia.estado()["modo"], "original")

    def test_foto_sem_imagem_da_erro(self):
        e = self.areia.estado()
        e["modo"] = "rosto"
        with self.assertRaisesRegex(self.areia.Erro, "ainda não há imagem"):
            self.areia.aplica(e)

    def test_reorienta_refaz_e_reaplica(self):
        self.areia.mostra(self.poe("a.jpg"))
        antes = len(self.comandos())
        e = self.areia.reorienta(giro=450)
        self.assertEqual(e["giro"], 90)
        self.assertTrue(any(c.startswith("cmd rosto ") and c.endswith(" 90 0") for c in self.comandos()[antes:]))

    def test_esculpir(self):
        self.areia.mostra(self.poe("a.jpg"), modo="esculpir")
        self.assertTrue(any(c.startswith("cmd esculpir ") and c.endswith("rosto.dem") for c in self.comandos()))

    def test_ajuste_so_manda_os_controles(self):
        self.areia.mostra(self.poe("a.jpg"))
        e = self.areia.estado()
        e["ajuste"]["zoom"] = 0.07
        self.areia.ajusta(e)
        self.assertEqual(self.comandos()[-1], "cmd rostoAjuste 0.07 0.0 0.0 1.0")


class Temas(Ambiente):
    def test_arquivos_de_cor_validos(self):
        for t in self.areia.temas():
            with open(os.path.join(self.areia.TEMAS, t + ".cpt")) as f:
                linhas = [l.split() for l in f if l.strip()]
            alturas = [float(l[0]) for l in linhas]
            self.assertTrue(all(len(l) == 4 for l in linhas), t)
            self.assertEqual(alturas, sorted(alturas), t)
            self.assertTrue(all(0 <= int(v) <= 255 for l in linhas for v in l[1:]), t)
            self.assertLessEqual(alturas[0], 0, t)       # cobre a água (abaixo do plano base)
            self.assertGreaterEqual(alturas[-1], 15, t)  # e os morros mais altos

    def test_lista_e_padrao(self):
        self.assertEqual(self.areia.temas()[:2], ["classico", "vulcao"])
        self.assertEqual(len(self.areia.temas()), 7)   # o mapa de papel saiu em 16/11/2026
        self.assertNotIn("mapa", self.areia.temas())
        self.assertEqual(self.areia.tema_atual(), "classico")

    def test_escolhe_com_acento_e_abreviado(self):
        self.assertEqual(self.areia.escolhe_tema("Vulcão"), "vulcao")
        self.assertEqual(os.readlink(self.areia.TEMA_ATUAL), "vulcao.cpt")
        self.assertTrue(self.comandos()[-1].startswith("tema ") and self.comandos()[-1].endswith("vulcao.cpt"))
        self.assertEqual(self.areia.escolhe_tema("arco iris"), "arco-iris")
        self.assertEqual(self.areia.escolhe_tema("inv"), "inverno")
        self.assertEqual(self.areia.tema_atual(), "inverno")
        self.assertEqual(self.areia.estado()["tema"], "inverno")

    def test_tema_inexistente(self):
        with self.assertRaisesRegex(self.areia.Erro, "não existe"):
            self.areia.escolhe_tema("xadrez")
        with self.assertRaisesRegex(self.areia.Erro, "não existe"):
            self.areia.escolhe_tema("")              # vazio não escolhe nenhum
        self.assertEqual(self.areia.tema_atual(), "classico")

    def test_comando(self):
        import importlib
        import areia_tema
        areia_tema = importlib.reload(areia_tema)
        saida = io.StringIO()
        with redirect_stdout(saida):
            areia_tema.main([])
            areia_tema.main(["marte"])
            areia_tema.main(["lista"])
        out = saida.getvalue()
        self.assertIn("vulcao", out)
        self.assertIn("Tema Marte no projetor.", out)
        self.assertIn("marte      Marte   <- atual", out)


class Brincadeiras(Ambiente):
    def test_tesouro_sobe_a_versao_com_foto(self):
        self.assertEqual(self.areia.tesouro(), 5.5)  # mais fundo desde 2026-11-11 (Victor)
        self.assertEqual(self.rodando(), "rosto")
        self.assertEqual(self.comandos()[-1], "cmd tesouro 5.5")
        self.assertEqual(self.areia.tesouro("Difícil"), 7.0)
        self.assertEqual(self.areia.tesouro("2,5"), 2.5)
        for ruim in ("xyz", "50"):
            with self.assertRaises(self.areia.Erro):
                self.areia.tesouro(ruim)

    def test_situacao_pelo_registro(self):
        self.assertEqual(self.areia.tesouro_situacao(), "nenhum")
        self.areia.tesouro()
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Tesouro: escondendo a 3 cm\nRan out of time\nTesouro escondido em x 1 y 2\n")
        self.assertEqual(self.areia.tesouro_situacao(), "escondido")
        escreve(log, "Tesouro escondido em x 1 y 2\nTesouro encontrado!\n")
        self.assertEqual(self.areia.tesouro_situacao(), "achado")
        escreve(log, "Tesouro encontrado!\nTesouro desligado\n")
        self.assertEqual(self.areia.tesouro_situacao(), "nenhum")

    def test_reaparece_e_conta(self):
        self.areia.tesouro()
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Tesouro encontrado! (1 achados)\nTesouro: escondendo a 3 cm\nTesouro escondido em x 1 y 2\n"
                     "Tesouro encontrado! (1 achados)\nTesouro escondido em x 9 y 9 (cavar até 2 cm; novo)\n"
                     "Tesouro encontrado! (2 achados)\n")
        self.assertEqual(self.areia.tesouro_achados(), 2)   # só depois do último "escondendo"
        self.assertEqual(self.areia.tesouro_situacao(), "achado")
        escreve(log, "Tesouro: escondendo a 3 cm\nTesouro encontrado! (1 achados)\nTesouro escondido em x 9 y 9 (novo)\n")
        self.assertEqual(self.areia.tesouro_situacao(), "escondido")
        self.assertEqual(self.areia.tesouro_achados(), 1)

    def test_animais(self):
        self.assertEqual(self.areia.animais("Peixes"), ("peixes", 16))      # a variedade toda de marinhos
        self.assertEqual(self.comandos()[-1], "cmd mar peixe=6 tubarao=1 tartaruga=2 aguaviva=2 polvo=1 arraia=1 pato=2 barco=1")
        self.assertEqual(self.areia.animais("peixes", 7), ("peixes", 7))    # com quantidade: só peixes
        self.assertEqual(self.comandos()[-1], "cmd animais peixes 7")
        self.assertEqual(self.areia.animais("carrinhos", "5"), ("carros", 5))
        self.assertEqual(self.areia.animais("todos"), ("todos", 29))         # marinhos, carros e pessoas
        self.assertEqual(self.comandos()[-1], "cmd mar peixe=6 tubarao=1 tartaruga=2 aguaviva=2 polvo=1 arraia=1 pato=2 barco=1 carro=4 pessoa=4 cachorro=2 passaro=3")
        self.assertEqual(self.areia.animais("gente", 6), ("pessoas", 6))
        self.assertEqual(self.comandos()[-1], "cmd animais pessoas 6")
        for t, n in (("gatos", None), ("peixes", "99")):
            with self.assertRaises(self.areia.Erro):
                self.areia.animais(t, n)
        self.areia.animais_fim()
        self.assertEqual(self.comandos()[-1], "cmd animais nada")

    def test_comandos_de_terminal(self):
        import importlib
        import areia_brincar
        ab = importlib.reload(areia_brincar)
        saida = io.StringIO()
        with redirect_stdout(saida):
            ab.tesouro(["facil"])
            ab.tesouro(["achou"])
            ab.animais(["carros", "3"])
            ab.animais(["parar"])
            ab.tesouro(["parar"])
        out = saida.getvalue()
        self.assertIn("Tesouro escondido a 4 cm", out)
        self.assertIn("3 carrinhos na areia!", out)
        self.assertIn("Animais removidos.", out)
        self.assertEqual(self.comandos()[-1], "cmd tesouroFim")


class Fosseis(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def test_dinossauros_e_niveis(self):
        self.assertEqual(self.areia.fossil(), ("Tiranossauro Rex", 5.5))
        self.assertEqual(self.cmds()[-1], "cmd fossil 2 5.5")
        self.assertEqual(self.areia.fossil("Triceratops")[0], "Triceratops")
        self.assertEqual(self.cmds()[-1], "cmd fossil 3 5.5")
        self.areia.fossil("pegada", "facil")
        self.assertEqual(self.cmds()[-1], "cmd fossil 5 4")
        self.areia.fossil("ptero", "dificil")
        self.assertEqual(self.cmds()[-1], "cmd fossil 4 7")

    def test_dificuldade_sozinha_vale_para_o_trex(self):
        self.assertEqual(self.areia.fossil("facil"), ("Tiranossauro Rex", 4.0))
        self.assertEqual(self.cmds()[-1], "cmd fossil 2 4")

    def test_erros(self):
        with self.assertRaisesRegex(self.areia.Erro, "dinossauro"):
            self.areia.fossil("gato")
        with self.assertRaisesRegex(self.areia.Erro, "dificuldade"):
            self.areia.fossil("trex", "xyz")

    def test_tesouro_e_fossil_nao_se_misturam(self):
        self.areia.fossil()
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Tesouro: escondendo a 3 cm\nTesouro escondido em x 1 y 1\nTesouro encontrado! (1 achados)\n"
                     "Fossil: escondendo tipo 2 a 3 cm\nFossil escondido em x 2 y 2\n")
        self.assertEqual(self.areia.fossil_situacao(), "escondido")
        self.assertEqual(self.areia.tesouro_situacao(), "nenhum")   # o fóssil substituiu o tesouro
        self.assertEqual(self.areia.tesouro_achados(), 1)
        escreve(log, "Fossil: escondendo tipo 2 a 3 cm\nFossil encontrado! (1 escavados)\n"
                     "Fossil escondido em x 5 y 5 (escavar ate 2 cm; novo)\nFossil encontrado! (2 escavados)\n")
        self.assertEqual(self.areia.fossil_situacao(), "achado")
        self.assertEqual(self.areia.fossil_achados(), 2)
        escreve(log, "Fossil encontrado! (2 escavados)\nFossil desligado\n")
        self.assertEqual(self.areia.fossil_situacao(), "nenhum")

    def test_comando_de_terminal(self):
        import importlib
        import areia_brincar
        ab = importlib.reload(areia_brincar)
        saida = io.StringIO()
        with redirect_stdout(saida):
            ab.fossil([])
            ab.fossil(["triceratops", "dificil"])
            ab.fossil(["achou"])
            ab.fossil(["parar"])
        out = saida.getvalue()
        self.assertIn("Fóssil de Tiranossauro Rex enterrado a 5.5 cm", out)
        self.assertIn("Fóssil de Triceratops enterrado a 7 cm", out)
        self.assertIn("Escavação encerrada.", out)
        self.assertEqual(self.cmds()[-1], "cmd fossilFim")


class Comando(Ambiente):
    """areia-foto (areia_foto.main) sem terminal interativo."""

    def roda(self, *args):
        import importlib
        import areia_foto
        areia_foto = importlib.reload(areia_foto)
        saida = io.StringIO()
        with redirect_stdout(saida):
            rc = areia_foto.main(list(args))
        return rc, saida.getvalue()

    def test_ajuda_e_lista(self):
        self.poe("a.jpg")
        self.assertIn("areia-foto girar", self.roda("ajuda")[1])
        self.assertIn(" 1  a.jpg", self.roda("lista")[1])

    def test_por_numero_e_depois_a_ultima(self):
        self.poe("a.jpg")
        b = self.poe("b.jpg")
        rc, out = self.roda("2")
        self.assertEqual(rc, 0)
        self.assertIn("Modo FOTO", out)
        self.assertEqual(self.areia.estado()["imagem"], b)
        rc, out = self.roda()                     # sem terminal: repete a última
        self.assertIn("b.jpg", out)

    def test_girar_espelhar_e_opcoes(self):
        self.poe("a.jpg")
        self.roda("1")
        self.assertIn("Giro 90°", self.roda("girar")[1])
        self.assertIn("espelho sim", self.roda("espelhar")[1])
        self.roda("1", "--com-fundo")
        self.assertEqual(self.areia.estado()["fundo"], "--com-fundo")
        with self.assertRaisesRegex(self.areia.Erro, "opção desconhecida"):
            self.roda("1", "--xyz")

    def test_esculpir_precisa_de_imagem(self):
        with self.assertRaisesRegex(self.areia.Erro, "escolha uma imagem"):
            self.roda("esculpir")


if __name__ == "__main__":
    unittest.main()


class JogosDeAgua(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def test_todos_os_jogos_e_niveis(self):
        self.assertEqual(sorted(self.areia.JOGOS), ['corrida', 'rota', 'vila', 'vulcao'])
        for fora in ('fazenda', 'lago', 'barco', 'resgate'):
            with self.assertRaisesRegex(self.areia.Erro, 'jogo:'):
                self.areia.jogo(fora)
        for nome in self.areia.JOGOS:
            self.assertEqual(self.areia.jogo(nome, "dificil"), (nome, 3))
            self.assertEqual(self.cmds()[-1], f"cmd jogo {nome} 3")
        self.assertEqual(self.areia.jogo("Corrida de rios"), ("corrida", 2))
        self.assertEqual(self.areia.jogo("erupção", "Fácil"), ("vulcao", 1))
        with self.assertRaisesRegex(self.areia.Erro, "jogo:"):
            self.areia.jogo("xadrez")
        with self.assertRaisesRegex(self.areia.Erro, "nível"):
            self.areia.jogo("vila", "xyz")

    def test_um_por_vez(self):
        self.areia.labirinto()
        self.areia.jogo("rota")
        self.assertEqual(self.cmds()[-5:], ["cmd tesouroFim", "cmd fossilFim", "cmd labirintoFim", "cmd animais nada", "cmd jogo rota 2"])
        self.areia.para_tudo()
        self.assertEqual(self.cmds()[-1], "cmd jogoFim")

    def test_situacao_pelo_registro(self):
        self.assertIsNone(self.areia.jogo_situacao()["jogo"])
        with self.assertRaisesRegex(self.areia.Erro, "nenhum jogo"):
            self.areia.jogo_novo()
        self.areia.jogo("vila")
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Jogo pedido: vila (nível 2)\nJogo: salve a vila (4 casas); a chuva começa em 10 s\nJogo: uma casa alagou!\n")
        s = self.areia.jogo_situacao()
        self.assertEqual((s["jogo"], s["fase"], s["rodadas"], s["casas"], s["perdidas"]), ("vila", "correndo", 0, 4, 1))
        escreve(log, "Jogo pedido: vila (nível 2)\nJogo: salve a vila (4 casas)\nJogo: uma casa alagou!\n"
                     "Jogo: fim da rodada; 3 de 4 casas secas\n")
        s = self.areia.jogo_situacao()
        self.assertEqual((s["fase"], s["rodadas"], s["aviso"]), ("fim", 1, "3 de 4 casas secas"))
        self.areia.jogo_novo()
        self.assertEqual(self.cmds()[-1], "cmd jogoNovo")
        for fim in ("Jogo desligado\n", "Modo Rosto: foto.png\n", "Labirinto: nascente em x 1 y 2\n"):
            escreve(log, "Jogo pedido: vila (nível 1)\nJogo: salve a vila\n" + fim)
            self.assertIsNone(self.areia.jogo_situacao()["jogo"])

    def test_vulcao_nao_troca_o_tema(self):
        self.areia.escolhe_tema("oceano")
        self.areia.jogo("vulcao")   # o próprio jogo escurece a areia e pinta a lava
        self.assertEqual(self.areia.tema_atual(), "oceano")
        self.assertEqual(self.cmds()[-1], "cmd jogo vulcao 2")

    def test_comando_de_terminal(self):
        import importlib
        import areia_brincar
        ab = importlib.reload(areia_brincar)
        saida = io.StringIO()
        with redirect_stdout(saida):
            ab.jogo(["ajuda"])
            ab.jogo(["vulcao", "facil"])
            ab.jogo(["placar"])
            ab.jogo(["parar"])
        out = saida.getvalue()
        self.assertIn("vulcao", out)
        self.assertIn("Vulcão em erupção ligado", out)
        self.assertIn("cmd jogo vulcao 1", self.cmds())
        self.assertEqual(self.cmds()[-1], "cmd jogoFim")


class MembrosNaAreia(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def status(self, presentes, idade=0):
        import time
        escreve(self.areia.STATUS_WILLBERT, json.dumps(
            {"ts": time.time() - idade, "present": [{"name": n, "since": i} for i, n in enumerate(presentes)]}))

    def setUp(self):
        super().setUp()
        self.areia.STATUS_WILLBERT = os.path.join(self.D, "status-willbert.json")
        self.areia._membros_mandados[0] = None

    def test_nomes_de_quem_esta_presente(self):
        self.assertEqual(self.areia.membros_presentes(), [])            # Willbert desligado
        self.status(["Victor Vieira", "Gazziro", "João-Pedro", "Maria Antonieta", "Victor"])
        self.assertEqual(self.areia.membros_presentes(), ["VICTOR", "GAZZIRO", "JOAOPEDR", "MARIA"])
        self.status(["Victor"], idade=600)                              # Willbert parou de publicar
        self.assertEqual(self.areia.membros_presentes(), [])
        escreve(self.areia.STATUS_WILLBERT, "{ quebrado")
        self.assertEqual(self.areia.membros_presentes(), [])

    def test_liga_e_acompanha_quem_chega_e_sai(self):
        self.status(["Victor", "Lucas"])
        self.assertEqual(self.areia.membros(), (["VICTOR", "LUCAS"], ["VICTOR", "LUCAS"]))
        self.assertEqual(self.cmds()[-1], "cmd membros 0 0 - VICTOR+ LUCAS+")   # sem fotos: só o nome
        self.assertIn("cmd jogoFim", self.cmds()[-5:])                  # uma brincadeira por vez
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Animais: membros (2, 0 com rosto)\n")
        self.assertEqual(self.areia.animais_situacao(), "membros")
        n = len(self.cmds())
        self.assertFalse(self.areia.membros_atualiza())                 # ninguém chegou nem saiu
        self.assertEqual(len(self.cmds()), n)
        self.status(["Victor", "Lucas", "Mario"])
        self.assertTrue(self.areia.membros_atualiza())
        self.assertEqual(self.cmds()[-1], "cmd membros 0 0 - VICTOR+ LUCAS+ MARIO+")
        self.status([])
        self.assertTrue(self.areia.membros_atualiza())
        self.assertEqual(self.cmds()[-1], "cmd membros 0 0 -")
        escreve(log, "Animais: membros (0, 0 com rosto)\nTesouro: escondendo a 4 cm\n")   # outra brincadeira entrou
        self.status(["Victor"])
        self.assertFalse(self.areia.membros_atualiza())

    def test_membros_com_foto_aparecem_sempre(self):
        # painel de rostos já montado (o membros.py de verdade precisa do OpenCV e é testado no laob)
        pasta = os.path.join(self.D, "membros")
        os.makedirs(pasta)
        for n in ("Gazziro.png", "Victor.png"):
            escreve(os.path.join(pasta, n), b"x")
        os.makedirs(self.areia.FOTOS, exist_ok=True)
        escreve(self.areia.PAINEL_ROSTOS, b"x")
        escreve(os.path.join(self.areia.FOTOS, "membros.json"), json.dumps({"nomes": ["GAZZIRO", "VICTOR"]}))
        self.areia.PASTA_MEMBROS = pasta
        self.status(["Lucas", "Victor"])
        nomes, presentes = self.areia.membros()
        self.assertEqual(nomes, ["GAZZIRO", "VICTOR", "LUCAS"])         # com foto primeiro; Lucas só com o nome
        self.assertEqual(presentes, ["VICTOR", "LUCAS"])
        self.assertEqual(self.cmds()[-1], f"cmd membros 0 2 {self.areia.PAINEL_ROSTOS} GAZZIRO VICTOR+ LUCAS+")
        escreve(os.path.join(self.D, "sandbox.log"), "Animais: membros (3, 2 com rosto)\n")
        self.status([])                                                 # todos foram embora: os com foto continuam
        self.assertTrue(self.areia.membros_atualiza())
        self.assertEqual(self.cmds()[-1], "cmd membros 0 2 - GAZZIRO VICTOR")


class MundoLaob(MembrosNaAreia):
    def test_liga_acompanha_e_mostra_rosto(self):
        pasta = os.path.join(self.D, "membros")
        os.makedirs(pasta)
        for n in ("Gazziro.png", "Victor.png"):
            escreve(os.path.join(pasta, n), b"x")
        os.makedirs(self.areia.FOTOS, exist_ok=True)
        escreve(self.areia.PAINEL_ROSTOS, b"x")
        escreve(os.path.join(self.areia.FOTOS, "membros.json"), json.dumps({"nomes": ["GAZZIRO", "VICTOR"]}))
        self.areia.PASTA_MEMBROS = pasta
        with self.assertRaisesRegex(self.areia.Erro, "não está ligado"):
            self.areia.mundo_rosto("Victor")
        self.status(["Victor"])
        self.assertEqual(self.areia.mundo(), (["GAZZIRO", "VICTOR"], ["VICTOR"]))
        self.assertEqual(self.cmds()[-1], f"cmd mundo 0 2 {self.areia.PAINEL_ROSTOS} 0 GAZZIRO VICTOR+")
        self.assertIn("cmd jogoFim", self.cmds()[-5:])
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Mundo: VICTOR está na bancada\nAnimais: mundo (2)\nMundo: laboratório montado\n"
                     "Mundo: VICTOR entrou no laboratório\nMundo: GAZZIRO saiu do laboratório\n"
                     "Mundo: VICTOR foi até a caixa de areia\n")
        self.assertEqual(self.areia.animais_situacao(), "mundo")
        self.assertEqual(self.areia.mundo_situacao(), [
            {"nome": "Gazziro", "fazendo": "saiu do laboratório", "presente": False},
            {"nome": "Victor", "fazendo": "foi até a caixa de areia", "presente": True}])
        self.assertEqual(self.areia.mundo_rosto("victor"), "VICTOR")
        self.assertEqual(self.cmds()[-1], "cmd mundoRosto VICTOR")
        with self.assertRaisesRegex(self.areia.Erro, "não encontrado"):
            self.areia.mundo_rosto("Fulano")
        self.status(["Victor", "Gazziro"])                              # Gazziro chegou ao laboratório
        self.assertTrue(self.areia.membros_atualiza())
        self.assertEqual(self.cmds()[-1], "cmd mundo 0 2 - 0 GAZZIRO+ VICTOR+")
        self.assertFalse(self.areia.membros_atualiza())


class MarEChuva(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def test_animais_marinhos_a_escolha(self):
        self.assertEqual(sorted(self.areia.ESPECIES), ["aguaviva", "arraia", "barco", "cachorro", "carro", "passaro", "pato", "peixe", "pessoa", "polvo", "tartaruga", "tubarao"])
        self.assertEqual(self.areia.mar_escolhido(), self.areia.MAR_PADRAO)
        sel = self.areia.mar({"peixe": 5, "tubarao": 2, "polvo": 0})
        self.assertEqual(sel, {"peixe": 5, "tubarao": 2, "tartaruga": 0, "aguaviva": 0, "polvo": 0, "arraia": 0, "pato": 0, "barco": 0, "carro": 0, "pessoa": 0, "cachorro": 0, "passaro": 0})
        self.assertEqual(self.cmds()[-1], "cmd mar peixe=5 tubarao=2")
        self.assertIn("cmd jogoFim", self.cmds()[-5:])                       # uma brincadeira por vez
        self.assertEqual(self.areia.mar_escolhido()["tubarao"], 2)          # a escolha fica guardada
        self.areia.mar()                                                     # sem escolha: repete a última
        self.assertEqual(self.cmds()[-1], "cmd mar peixe=5 tubarao=2")
        sel = self.areia.mar({"polvo": 1, "carro": 3, "pessoa": 5})          # carros e pessoas entram na mesma escolha
        self.assertEqual(self.cmds()[-1], "cmd mar polvo=1 carro=3 pessoa=5")
        sel = self.areia.mar({"peixe": 30, "arraia": 9})                     # no máximo 32 no total
        self.assertEqual((sel["peixe"], sel["arraia"]), (30, 2))
        sel = self.areia.mar({"pato": 2, "barco": 1, "cachorro": 3, "passaro": 4})
        self.assertEqual(self.cmds()[-1], "cmd mar pato=2 barco=1 cachorro=3 passaro=4")
        with self.assertRaisesRegex(self.areia.Erro, "pelo menos um"):
            self.areia.mar({"peixe": 0})
        with self.assertRaisesRegex(self.areia.Erro, "inválida"):
            self.areia.mar({"peixe": "muitos"})
        escreve(os.path.join(self.D, "sandbox.log"), "Animais: mar (24)\n")
        self.assertEqual(self.areia.animais_situacao(), "mar")

    def test_chuva_e_secar(self):
        self.areia.animais("peixes")                                         # abre a versão que aceita comandos
        self.areia.chuva(0.25, 0.8)
        self.assertEqual(self.cmds()[-1], "cmd chuva 0.250 0.800 8")
        self.areia.chuva("0.5", "0.5", 20)
        self.assertEqual(self.cmds()[-1], "cmd chuva 0.500 0.500 20")
        for ruim in ((1.2, 0.5, 8), (0.5, -0.1, 8), ("x", 0.5, 8), (0.5, 0.5, 500)):
            with self.assertRaises(self.areia.Erro):
                self.areia.chuva(*ruim)
        self.areia.secar()
        self.assertEqual(self.cmds()[-1], "cmd secar")


class Labirinto(Ambiente):
    def cmds(self):
        return [c for c in self.comandos() if c.startswith("cmd")]

    def test_niveis(self):
        self.assertEqual(self.areia.labirinto(), "normal")
        self.assertEqual(self.cmds()[-1], "cmd labirinto normal")
        self.assertEqual(self.areia.labirinto("Fácil"), "facil")
        self.assertEqual(self.areia.labirinto("3"), "dificil")
        self.assertEqual(self.cmds()[-1], "cmd labirinto dificil")
        with self.assertRaisesRegex(self.areia.Erro, "dificuldade"):
            self.areia.labirinto("xyz")

    def test_situacao_pelo_registro(self):
        self.assertEqual(self.areia.labirinto_situacao(), ("nenhum", 0))
        with self.assertRaisesRegex(self.areia.Erro, "não está ligado"):
            self.areia.labirinto_novo()
        self.areia.labirinto()
        log = os.path.join(self.D, "sandbox.log")
        escreve(log, "Labirinto: a água chegou! (1 rodadas vencidas, 30 s)\n"   # de uma partida antiga
                     "Labirinto: nascente em x 1 y 2, alvo em x 30 y 20 (dificuldade 2)\nRan out of time\n")
        self.assertEqual(self.areia.labirinto_situacao(), ("correndo", 0))
        escreve(log, "Labirinto: nascente em x 1 y 2, alvo em x 30 y 20 (dificuldade 2)\n"
                     "Tesouro: escondendo a 3 cm\nLabirinto: a água chegou! (1 rodadas vencidas, 41 s)\n")
        self.assertEqual(self.areia.labirinto_situacao(), ("chegou", 1))   # o tesouro não atrapalha
        escreve(log, "Labirinto: nascente em x 1 y 2, alvo em x 30 y 20 (dificuldade 2)\n"
                     "Labirinto: a água chegou! (1 rodadas vencidas, 41 s)\nLabirinto: rodada nova, nascente em x 5 y 5\n")
        self.assertEqual(self.areia.labirinto_situacao(), ("correndo", 1))
        self.areia.labirinto_novo()
        self.assertEqual(self.cmds()[-1], "cmd labirintoNovo")
        for fim in ("Labirinto desligado\n", "Modo Rosto: foto.png\n"):
            escreve(log, "Labirinto: nascente em x 1 y 2, alvo em x 30 y 20 (dificuldade 2)\n" + fim)
            self.assertEqual(self.areia.labirinto_situacao()[0], "nenhum")
        self.areia.labirinto_fim()
        self.assertEqual(self.cmds()[-1], "cmd labirintoFim")

    def test_comando_de_terminal(self):
        import importlib
        import areia_brincar
        ab = importlib.reload(areia_brincar)
        saida = io.StringIO()
        with redirect_stdout(saida):
            ab.labirinto(["facil"])
            ab.labirinto(["placar"])
            ab.labirinto(["parar"])
        out = saida.getvalue()
        self.assertIn("Labirinto de água ligado (Fácil", out)
        self.assertIn("Labirinto encerrado.", out)
        self.assertEqual(self.cmds()[-2:], ["cmd labirinto facil", "cmd labirintoFim"])
