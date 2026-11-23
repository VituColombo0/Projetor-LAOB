# Changelog

Mudanças do Projetor-LAOB por versão. O histórico detalhado está no `git log`.

## [1.0] — 2026-11-23

Versão final da revisão completa: tudo medido no laob, a 30 quadros/s, água a 100% do tempo real e
p99 do quadro em ~34 ms em todos os modos, sem nenhum quadro acima de 50 ms.

### Fluidez e robustez (Lista 1)

- Trocas sem recompilar o shader: as variantes comuns são compiladas ao abrir a areia. Voltar à
  topografia caiu de 2,2 s para 0,18 s, e abrir a foto não trava mais (era um quadro de 3,7 s).
- GPU do relevo pela metade nos temas com efeito (4,7 → 2,2 ms) e mais leve na Vida (12,2 → 9,4 ms): o
  shader só leva o código da brincadeira em uso.
- Uma troca de programa por vez (trava entre painel, vigia e terminal) e `sandbox.sh parar|reabrir`.
- Vigia: pausa que vence sozinha em 2 h (`areia-vigia pausa [30m|3h|sempre]`), `estado --json` e
  topografia de **reserva** visível no painel, com nova tentativa a cada 10 min (no máximo 3 por hora).
- Registro: `sandbox.log.1` da vez anterior e rotação acima de 5 MB sem fechar a areia; journal calado
  quando está tudo bem.
- A areia roda num escopo próprio do systemd do usuário: fechar o ssh ou reiniciar o painel não a derruba.
- Painel: linha de **saúde** na aba Projetor (`GET /saude`), câmera e Willbert em segundo plano, envio de
  fotos validado, conferência de Host/Origin, páginas abertas que recarregam sozinhas após instalar.
- Instalador: confere os pré-requisitos antes de mudar algo, para com a etapa e a linha do erro, confere
  os modelos por sha256, preserva uma recalibração mais nova e ganhou `instalar.sh --verificar`.
- Medidas: linha `Perfil:` no `sandbox.log`, `testes/medir.sh`, `testes/prancha.sh` e
  `testes/teste_shader.cpp` (variantes do shader comparadas pixel a pixel na GPU).

### Visual

- Leitura de longe: fóssil sépia no chão claro, baú e fóssil sem curvas por cima, cartão claro e grande no
  Copie o relevo, água dos jogos azul em todo tema e leito seco pela altura.
- Água com fundo azul firme, margem clara e brilho que anda com a correnteza.
- Placares com algarismos grandes e contorno, uma paleta única de interface e a mesma comemoração em todos
  os jogos (`copiaTeste` para conferir).

### Documentação

- README no padrão do laboratório, com imagens da areia e o detalhe técnico em
  [docs/COMO-FUNCIONA.md](docs/COMO-FUNCIONA.md).
- [docs/MANUAL.md](docs/MANUAL.md): manual do laboratório para quem nunca usou.
- [docs/APRESENTACAO.md](docs/APRESENTACAO.md): roteiro de 5 a 10 minutos.
- [docs/RECUPERACAO.md](docs/RECUPERACAO.md): montar tudo de novo se o disco do laob morrer.
- `LEIA-ME - caixa de areia.txt` na Área de Trabalho do laob, ao lado do atalho do painel.
- Comentários nos patches do SARndbox (por que cada arquivo muda) e nas armadilhas conhecidas.
- Textos do painel e do terminal sobre o Copie o relevo corrigidos (falavam das cores antigas).

### Máquina

- `deploy/laob/otimizar-projetor.sh`: journal limitado a 500 MB e atualização automática fora do boot
  (com sudo), tela do GNOME que não apaga (sem sudo). Tem `--simular` e `--desfazer`. Ainda não rodado com
  sudo no laob.
- Pendente, só com o Victor: o kernel 6.8 instalado pelas atualizações não tem o driver NVIDIA; o GRUB
  continua fixado no 6.5 com o driver.
