"""Brincadeiras na caixa de areia (comandos areia-tesouro e areia-animais).

  areia-tesouro              esconde um tesouro (normal: 3 cm embaixo da areia); achado, ele
                             some depois de 20 s e reaparece em outro lugar
  areia-tesouro facil        mais raso (2 cm) · areia-tesouro dificil: mais fundo (5 cm)
  areia-tesouro achou        diz se já acharam
  areia-tesouro parar        tira o tesouro

  areia-animais peixes       peixes nadando no lago (cave um buraco e ele vira lago)
  areia-animais carros       carrinhos andando pelos vales
  areia-animais todos        os dois juntos (pode pôr a quantidade: areia-animais peixes 15)
  areia-animais parar        tira os animais

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
        print("Escondendo o tesouro... (se a caixa estava na topografia, leva uns 10 s)")
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
        print("Soltando os animais... (se a caixa estava na topografia, leva uns 10 s)")
        t, n = areia.animais(pedido, args[1] if len(args) > 1 else None)
        dica = " Cavem um buraco para fazer um lago: os peixes nadam nele." if t in ("peixes", "todos") else ""
        print(f"{n} {'peixes' if t == 'peixes' else 'carrinhos' if t == 'carros' else 'peixes e carrinhos'} na areia!{dica}")
    return 0


if __name__ == "__main__":
    qual_cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    try:
        sys.exit({"tesouro": tesouro, "animais": animais}[qual_cmd](sys.argv[2:]))
    except areia.Erro as err:
        print(err, file=sys.stderr)
        sys.exit(1)
    except KeyError:
        sys.exit("uso: areia_brincar.py tesouro|animais ...")
