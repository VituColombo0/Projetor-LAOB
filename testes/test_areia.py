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
        self.assertEqual(self.rodando(), "original")
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
        self.assertEqual(len(self.areia.temas()), 8)
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
