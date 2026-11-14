"""Brincadeiras na caixa de areia (comandos areia-tesouro, areia-animais, areia-fossil e areia-labirinto).

  areia-tesouro              esconde um tesouro (normal: 3 cm embaixo da areia); achado, ele
                             some depois de 20 s e reaparece em outro lugar
  areia-tesouro facil        mais raso (2 cm) · areia-tesouro dificil: mais fundo (5 cm)
  areia-tesouro achou        diz se já acharam
  areia-tesouro parar        tira o tesouro

  areia-animais peixes       peixes nadando no lago (cave um buraco e ele vira lago)
  areia-animais carros       carrinhos andando pelos vales
  areia-animais pessoas      pessoinhas passeando pela areia
  areia-animais todos        tudo junto (pode pôr a quantidade: areia-animais peixes 15)
  areia-animais parar        tira os animais

  areia-fossil               enterra um fóssil de T-Rex (3 cm); achado, outro aparece em 30 s
  areia-fossil triceratops   ou pterodactilo, pegada (pode pôr a dificuldade: areia-fossil pegada facil)
  areia-fossil facil         T-Rex mais raso (2 cm) · areia-fossil dificil: mais fundo (5 cm)
  areia-fossil achou         diz se está enterrado e quantos já escavaram
  areia-fossil parar         encerra a escavação

  areia-labirinto            labirinto de água: uma nascente joga água e vocês cavam canais
                             até ela chegar ao alvo (bandeira); chegou, comemora e começa outra
  areia-labirinto facil      alvo mais perto · areia-labirinto dificil: cantos opostos e menos água
  areia-labirinto nova       outra nascente e outro alvo agora
  areia-labirinto placar     diz se a água já chegou e quantas rodadas venceram
  areia-labirinto parar      encerra (a água do labirinto vai embora)

Para voltar à topografia normal: areia-topografia
"""
import sys

import areia


def tesouro(args):
    pedido = args[0] if args else "normal"
    if pedido in ("ajuda", "-h", "--help"):
        print(__doc__.strip())
    elif pedido in ("achou", "achou?", "status", "estado"):
        n = areia.tesouro_achados()
        placar = f" Tesouros achados até agora: {n}." if n else ""
        print({"achado": "Acharam o tesouro! Em uns 20 segundos ele some e reaparece em outro lugar.",
               "escondido": "O tesouro está escondido. Cavem para achar!",
               "nenhum": "Não há tesouro escondido agora. Para esconder: areia-tesouro"}[areia.tesouro_situacao()] + placar)
    elif pedido in ("parar", "tirar", "fim"):
        areia.tesouro_fim()
        print("Tesouro removido.")
    else:
        print("Escondendo o tesouro...")
        prof = areia.tesouro(pedido)
        print(f"Tesouro escondido a {prof:g} cm embaixo da areia! Quando chegarem perto, a areia brilha dourado.\n"
              "Depois de achado, ele some em 20 segundos e aparece de novo em outro lugar.")
    return 0


def animais(args):
    pedido = args[0] if args else "ajuda"
    if pedido in ("ajuda", "-h", "--help"):
        print(__doc__.strip())
    elif pedido in ("parar", "tirar", "nada", "fim"):
        areia.animais_fim()
        print("Animais removidos.")
    else:
        print("Soltando os animais...")
        t, n = areia.animais(pedido, args[1] if len(args) > 1 else None)
        dica = " Cavem um buraco para fazer um lago: os peixes nadam nele." if t in ("peixes", "todos") else ""
        nome = {"peixes": "peixes", "carros": "carrinhos", "pessoas": "pessoas"}.get(t, "peixes, carrinhos e pessoas")
        print(f"{n} {nome} na areia!{dica}")
    return 0


def fossil(args):
    pedido = args[0] if args else "trex"
    if pedido in ("ajuda", "-h", "--help"):
        print(__doc__.strip())
    elif pedido in ("achou", "achou?", "status", "estado"):
        n = areia.fossil_achados()
        placar = f" Fósseis escavados até agora: {n}." if n else ""
        print({"achado": "O fóssil foi escavado! Em uns 30 segundos outro aparece em outro lugar.",
               "escondido": "O fóssil está enterrado. Escavem com as mãos ou pincéis!",
               "nenhum": "Não há fóssil enterrado agora. Para começar: areia-fossil"}[areia.fossil_situacao()] + placar)
    elif pedido in ("parar", "tirar", "fim"):
        areia.fossil_fim()
        print("Escavação encerrada.")
    else:
        print("Enterrando o fóssil...")
        nome, prof = areia.fossil(pedido, args[1] if len(args) > 1 else "normal")
        print(f"Fóssil de {nome} enterrado a {prof:g} cm! Perto dele aparece a quadrícula de escavação;\n"
              "cavando até a profundidade, o esqueleto surge na rocha.")
    return 0


def labirinto(args):
    pedido = args[0] if args else "normal"
    if pedido in ("ajuda", "-h", "--help"):
        print(__doc__.strip())
    elif pedido in ("placar", "achou", "chegou", "status", "estado"):
        sit, n = areia.labirinto_situacao()
        placar = f" Rodadas vencidas: {n}." if n else ""
        print({"chegou": "A água chegou ao alvo! Em uns 20 segundos começa outra rodada.",
               "correndo": "A água está correndo. Cavem canais até a bandeira!",
               "nenhum": "O labirinto não está ligado. Para começar: areia-labirinto"}[sit] + placar)
    elif pedido in ("nova", "novo", "outra", "proxima"):
        areia.labirinto_novo()
        print("Rodada nova: olhem onde está a nascente e a bandeira!")
    elif pedido in ("parar", "tirar", "fim"):
        areia.labirinto_fim()
        print("Labirinto encerrado.")
    else:
        d = areia.labirinto(pedido)
        print(f"Labirinto de água ligado ({areia.LABIRINTO[d]})!\n"
              "A água nasce nas ondas azuis. Cavem canais e levantem paredes de areia\n"
              "para levar a água até a bandeira vermelha SEM encostar nas armadilhas vermelhas\n"
              "(se encostar, a água some e recomeça). Quando chegar, o alvo fica dourado.")
    return 0


def jogo(args):
    pedido = args[0] if args else "ajuda"
    if pedido in ("ajuda", "-h", "--help", "lista"):
        print("uso: areia-jogo <jogo> [facil|normal|dificil] | placar | nova | parar\njogos:")
        for nome, (titulo, curta, _) in areia.JOGOS.items():
            print(f"  {nome:8s} {titulo}: {curta}")
    elif pedido in ("placar", "status", "estado"):
        s = areia.jogo_situacao()
        if s["jogo"] is None:
            print("Nenhum jogo de água ligado. Para começar: areia-jogo <jogo>")
        else:
            print(f"{areia.JOGOS.get(s['jogo'], (s['jogo'],))[0]}: {'rodada terminou' if s['fase'] == 'fim' else 'rodada correndo'}"
                  f" ({s['rodadas']} rodadas). {s['aviso']}")
    elif pedido in ("nova", "novo", "outra"):
        areia.jogo_novo()
        print("Rodada nova!")
    elif pedido in ("parar", "tirar", "fim"):
        areia.jogo_fim()
        print("Jogo encerrado.")
    else:
        n, lv = areia.jogo(pedido, args[1] if len(args) > 1 else "normal")
        titulo, _, como = areia.JOGOS[n]
        print(f"{titulo} ligado (nível {lv})!\n{como}")
    return 0


if __name__ == "__main__":
    qual_cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    try:
        sys.exit({"tesouro": tesouro, "animais": animais, "fossil": fossil, "labirinto": labirinto, "jogo": jogo}[qual_cmd](sys.argv[2:]))
    except areia.Erro as err:
        print(err, file=sys.stderr)
        sys.exit(1)
    except KeyError:
        sys.exit("uso: areia_brincar.py tesouro|animais|fossil|labirinto|jogo ...")
