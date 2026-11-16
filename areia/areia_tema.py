"""Cores do relevo na caixa de areia (valem na topografia e no modo relevo).

  areia-tema            lista os temas (o atual marcado)
  areia-tema vulcao     troca na hora; fica escolhido para as próximas vezes
Temas: clássico, vulcão, inverno, marte, deserto, oceano, arco-íris.
"""
import sys

import areia


def main(argv):
    if not argv or argv[0] in ("lista", "-h", "--help", "ajuda"):
        atual = areia.tema_atual()
        print("Temas de cor do relevo (areia-tema <nome>):")
        for t in areia.temas():
            print(f"  {t:<10} {areia.NOMES_TEMAS.get(t, t)}" + ("   <- atual" if t == atual else ""))
        return 0
    t = areia.escolhe_tema(" ".join(argv))
    print(f"Tema {areia.NOMES_TEMAS.get(t, t)} no projetor.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except areia.Erro as err:
        print(err, file=sys.stderr)
        sys.exit(1)
