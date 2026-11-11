# Projetor-LAOB

Imagens projetadas na caixa de areia do laboratório (AR Sandbox / SARndbox 2.8, Kinect v1 e
projetor), rodando no **lab2**. Ao ligar o PC abre a **topografia** de sempre; por comando no
terminal, a caixa passa a mostrar uma **foto** que se deforma conforme mexem na areia.

- **Topografia**: o SARndbox original, sem nenhuma mudança (`~/SARndbox-2.8`).
- **Foto**: uma cópia (`~/SARndbox-rosto`) com um modo novo: a imagem é projetada sobre a areia,
  amplia onde a areia está alta (efeito lente) e ganha a sombra do relevo. Foto com rosto sai
  recortada, sem fundo (preto = projetor apagado).
- **Esculpir**: o rosto vira um relevo-alvo; o projetor pinta onde falta areia (vermelho) e onde
  sobra (azul).
- Tudo roda localmente no PC (Python próprio e modelos na pasta); só a instalação baixa coisas.

## Uso rápido

Coloque as imagens em **Área de Trabalho > Areia** e, no terminal:

    areia-foto            # lista as imagens e pergunta o número
    areia-foto 2          # ou direto pelo número / parte do nome
    areia-foto girar      # se aparecer de lado para quem olha a caixa
    areia-topografia      # volta ao relevo
    areia-tema vulcao     # cores do relevo: classico, vulcao, inverno, marte, deserto, oceano, arco-iris, mapa
    areia-tesouro         # caça ao tesouro (facil|normal|dificil, achou, parar)
    areia-animais todos   # peixes no lago e carrinhos nos vales (peixes|carros|todos [n], parar)
    areia-fossil trex     # escavação de fóssil (trex|triceratops|pterodactilo|pegada [facil|dificil], achou, parar)
    areia-labirinto       # labirinto de água: levar a água da nascente até o alvo (facil|normal|dificil, nova, placar, parar)

Na Área de Trabalho do lab2 há uma pasta para cada coisa, cada uma com o seu `COMO USAR.txt`
(os textos estão em [`area-de-trabalho/`](area-de-trabalho)): 1 - Topografia (ligar e consertar),
2 - Fotos na areia (é também onde se colocam as fotos), 3 - Cores do relevo, 4 - Caça ao tesouro,
5 - Peixes e carrinhos, 7 - Fósseis de dinossauro, 8 - Labirinto de água. A pasta 6 - Willbert vem do repositório do Willbert.
Cada pasta também tem **atalhos de dois cliques** (ex.: "Iniciar caça ao tesouro", "Escolher cores do
relevo"): os que pedem escolha abrem uma janela (`gui-foto.sh`, `gui-tema.sh`, `gui-tesouro.sh`,
`gui-fossil.sh`, `gui-labirinto.sh`) e todos rodam por `gui-rodar.sh`, que avisa por notificação quando dá certo e abre uma
janela com o motivo quando dá errado. Os atalhos ficam em `area-de-trabalho/<pasta>/*.desktop`.

## Instalar

Num PC com o SARndbox 2.8 já compilado em `~/SARndbox-2.8` e calibrado (Ubuntu, sem sudo):

    git clone https://github.com/VituColombo0/Projetor-LAOB.git && cd Projetor-LAOB
    bash instalar.sh

O `instalar.sh` copia o SARndbox para `~/SARndbox-rosto`, aplica os patches e compila, cria o
Python próprio (`requirements.txt`), baixa os modelos, instala os comandos em `~/.local/bin`,
cria a pasta Areia, a ajuda e o atalho na Área de Trabalho, troca o autostart para abrir a
topografia no projetor e liga o painel do celular. Pode rodar de novo para atualizar.
`bash instalar.sh --compilar /tmp/teste` só testa a compilação.

## Organização

| Pasta | O que tem |
|---|---|
| `sarndbox/` | o modo Foto dentro do SARndbox: `FaceImage.h/.cpp` (a imagem como textura e a matriz câmera -> imagem), `patch_surface.py` (shader: só a imagem, lente e sombra), `patch_sandbox.py` (comandos `rosto`, `rostoAjuste`, `esculpir`, `relevo` pelo cano `-cp` e a área iluminada), `projetor.cfg` (tela cheia no HDMI-0) |
| `areia/` | o que roda no dia a dia: `areia.py` (núcleo: pasta, estado, cache, projetor), `areia_foto.py` + `areia-foto` (o comando), `areia-topografia`, `rosto.py` (prepara a imagem), `painel.py` (celular), `sandbox.sh` (troca de programa) |
| `area-de-trabalho/` | `COMO-USAR.txt`, `SE-A-AREIA-NAO-ABRIR.txt` e o atalho `abrir-a-areia.desktop` |
| `sistema/` | `sandbox-painel.service` (serviço de usuário do painel) |
| `testes/` | testes automáticos e o teste no PC de verdade |

No PC tudo fica numa pasta só, `~/SARndbox-rosto` (cópia do SARndbox + os arquivos acima).

## Testes

    bash testes/rodar.sh       # 53 testes: imagem, área iluminada, relevo, pasta, cache, comando,
                               # painel (com projetor falso) + sintaxe/shellcheck dos scripts
    bash testes/integracao.sh  # no lab2, depois do instalar.sh: compila, confere tudo instalado,
                               # o painel (aceita Tailscale, recusa a rede local) e o projetor de verdade
                               # (~1 min), terminando na topografia

## Caça ao tesouro, peixes e carrinhos

`sarndbox/Extras.h/.cpp`: um mapa de alturas grosseiro da área iluminada (1,5 cm por célula,
2x por segundo) e a simulação na CPU; o desenho é feito no shader da superfície (uniforms com as
posições). Comandos no cano da versão com Foto: `tesouro [cm]`, `tesouroFim`,
`animais peixes|carros|todos|nada [n]`.
- **Tesouro:** lugar sorteado onde há areia acima da média; aparece onde cavarem abaixo de
  (altura - profundidade); brilha dourado a menos de 2,5 cm; "Tesouro encontrado!" vai para o
  `sandbox.log` (`areia-tesouro achou` lê de lá).
- **Fósseis:** o mesmo mecanismo do tesouro (`tipoTesouro` 2–5), com raio maior; perto aparece a
  quadrícula de escavação e, cavando, o esqueleto em marfim na rocha (T-Rex, Triceratops,
  Pterodáctilo ou pegada). Achado, outro aparece em 30 s. Fóssil e tesouro se substituem.
- **Lago e peixes:** tudo 1,5 cm abaixo da mediana da areia vira água; os peixes só nadam ali e
  desviam das margens (cavar um buraco cria um lago).
  Nadam em cardume (separação de ~5,5 cm, alinhamento e coesão fraca: juntos, sem amontoar), viram suave (no máximo 2,8 rad/s) e dão
  nadadas (a velocidade oscila); preso numa poça por 4 s, o peixe reaparece num lago de verdade
  (só nasce onde há água num raio de 3,5 cm).
- **Carrinhos:** seguem os vales pela curva de nível (baixo, mas sem despencar no fundo do buraco),
  sem rampa íngreme, sem lago e sem sair da área; descem mais rápido e sobem devagar, se espalham
  (custo por carro perto) e freiam para o da frente só se ele tem número menor (nunca travam um
  esperando o outro); travado por 3 s, dá meia-volta.
- **Desenho:** peixes com corpo que ondula, cauda bifurcada, nadadeiras, olhos, listras e sombra;
  carrinhos com carroceria arredondada, cabine, vidros, faróis com facho, lanternas, rodas e sombra.
- **Uma brincadeira por vez:** começar tesouro, fóssil, labirinto ou animais encerra as outras
  (`PARAR_BRINCADEIRAS` em `areia.py`); a foto e o esculpir também encerram; o tema de cores fica.
- **Labirinto de água:** comandos `labirinto [facil|normal|dificil]`, `labirintoNovo`, `labirintoFim`
  (e `labirintoTeste`, que põe o alvo na nascente, para o teste de integração). A nascente é um
  disco de 2 cm que entra na simulação de água do próprio SARndbox (`addWater`, a mesma via da chuva
  com a mão); 2x por segundo o `display` lê a grade de água e a de fundo (`glGetTexImage`) e mede a
  maior profundidade dentro do alvo; com 0,25 cm escreve "Labirinto: a água chegou!" no log, o alvo
  fica dourado e, 20 s depois, rodada nova com a água zerada (`setWaterLevel`). Foto, esculpir,
  `labirintoFim` e a topografia encerram o labirinto e esvaziam a água dele.
- **Fluidez:** a versão com Foto roda com a grade de água em 320x240 (`-wts 320 240`; o padrão
  640x480 não dava conta na GTX 750 Ti e escrevia "Ran out of time" no log a cada quadro). O log
  mostra a cada 30 s uma linha `Desempenho: N quadros/s; água atrasada em X de Y passos`.
- **Willbert:** o gesto das duas mãos ao lado do rosto (antes, "Willbert, grave o meu rosto") manda
  a foto tratada para `POST /foto` do painel (só aceita a própria máquina): PNG já recortado (o Willbert
  usa o fundo vazio da cena), que o `rosto.py` usa como alfa. As fotos `willbert-*` além das 30 mais
  novas vão para a subpasta "antigas do Willbert" (`areia.arquiva_willbert`).

## Temas de cor do relevo

`areia/temas/*.cpt` (altura em cm acima do plano base -> cor RGB; abaixo de 0 é água). O tema
escolhido é o link `temas/atual.cpt`: a topografia (o SARndbox original, sem mudanças) abre com
`-uhm temas/atual.cpt -cp topografia.fifo`, e `areia-tema` troca na hora pelo comando `colorMap`
do próprio SARndbox (~0,1 s, sem fechar o programa). Vale também no modo Relevo da versão com Foto.
Os pontos foram ajustados à areia do lab2 (morros de ~8–10 cm): lava e ilhas aparecem nos picos.

## Painel do celular
`painel.py` (serviço de usuário `sandbox-painel`) em http://100.114.194.28:8090: escuta em todas as
interfaces mas só responde à própria máquina e à rede Tailscale (outros recebem 403): tirar selfie, escolher foto ou usar a câmera do laboratório, trocar de modo,
girar/espelhar e ajustar deformação, sombra e mistura com as cores de relevo.

## Comandos internos (sandbox.sh)
    ./sandbox.sh rosto
    venv/bin/python rosto.py foto.jpg --saida fotos
    ./sandbox.sh cmd "rosto /home/laob/SARndbox-rosto/fotos/rosto.png 0 0"
    ./sandbox.sh cmd "esculpir /home/laob/SARndbox-rosto/fotos/rosto.dem"
    ./sandbox.sh cmd "relevo"
    ./sandbox.sh original

## Projetor

- `sandbox.sh` abre o SARndbox (as duas versões) com `-mergeConfig projetor.cfg`: a janela já sai
  em tela cheia no projetor (HDMI-0). Sem isso ela abre no monitor.
- A calibração do lab2 marca uma caixa maior que a imagem do projetor; a foto é encaixada só na
  parte iluminada (cantos da imagem do projetor levados ao plano base, em `Sandbox.cpp` e `rosto.py`).
  Hoje: x -34,5..24,8 cm, y -28,8..13,1 cm (cerca de 59 × 42 cm).

## Topografia ao ligar, foto por comando

- Ao ligar o lab2, o autostart (`~/.config/autostart/SARndbox.desktop`) roda `areia-topografia`:
  o SARndbox original em tela cheia no projetor. O atalho antigo ficou em
  `~/SARndbox-rosto/SARndbox.desktop.original` (para desfazer, copie-o de volta).
- **Pasta das imagens:** `~/Desktop/Areia`, na Área de Trabalho, com o arquivo `COMO USAR.txt` (cópia de
  `COMO-USAR.txt` deste diretório) e atalho "Areia" na barra lateral do Arquivos. Coloque
  ali as fotos (.jpg, .png, .webp, .bmp). No terminal (comandos em `~/.local/bin`):
  - `areia-foto`: lista as imagens da pasta e pergunta o número (Enter = a última usada).
  - `areia-foto 3` / `areia-foto gato`: escolhe direto pelo número ou por parte do nome.
  - `areia-foto ~/Downloads/x.jpg`: qualquer arquivo; ele é copiado para a pasta Areia.
  - `areia-foto lista`, `areia-foto pasta` (abre a pasta), `areia-foto girar`, `areia-foto espelhar`,
    `areia-foto esculpir`.
  - `areia-foto camera`: foto pela câmera do laboratório (só essa opção depende do Willbert).
  - `areia-topografia`: volta à topografia.
- Com rosto na imagem: recorte retrato em volta do rosto, sem fundo. Sem rosto: a imagem inteira,
  com fundo (`--sem-fundo` recorta a pessoa mesmo assim; `--com-fundo` nunca recorta). PNG já
  transparente usa a própria transparência. A preparação (~5 s) fica guardada em `fotos/cache/`
  (40 últimas): trocar para uma imagem já usada leva ~1 s.
- Tudo local nesta pasta: `venv/` (OpenCV, NumPy, Flask, waitress), `modelos/` (YuNet e U²-Net) e a
  calibração de `etc/` (o `instalar.sh` recria o que faltar). As fotos enviadas pelo painel do celular também vão para a pasta Areia.
- No modo foto não aparecem cores de relevo, curvas de nível nem água: só a foto, que amplia onde a
  areia está mais alta e ganha sombra do relevo. No painel, "mistura" traz o relevo de volta, e
  deformação 0 + sombra 0 projeta a foto sem efeito nenhum.
- Recorte do fundo: U²-Net de pessoas (`modelos/u2net_human_seg.onnx`, 176 MB, release v0.0.0 do
  projeto rembg) rodando no OpenCV do venv local, ~3 s (o `instalar.sh` baixa).

## Ao ligar e se não abrir

- O lab2 tem login automático (GDM) e o autostart roda `areia-topografia`, que espera o Kinect
  aparecer no USB (até 30 s), tenta abrir 3 vezes e registra em `~/SARndbox-rosto/inicio.log`.
- Na Área de Trabalho: `SE A AREIA NÃO ABRIR.txt` e o atalho `Abrir a areia` (o `instalar.sh` o marca
  como confiável para abrir com dois cliques).

## Licença

O SARndbox é de Oliver Kreylos (UC Davis), sob GPL-2.0; os patches e o modo Foto deste repositório
seguem a mesma licença. Os modelos baixados pelo instalador têm licenças próprias: YuNet
(OpenCV Zoo, MIT) e U²-Net de pessoas (projeto rembg / U²-Net, Apache-2.0).
