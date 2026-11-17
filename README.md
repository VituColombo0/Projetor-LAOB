# Projetor-LAOB

> Caixa de areia de realidade aumentada do Laboratório LAOB: projeta relevo colorido, fotos que se deformam com a areia e brincadeiras e jogos de água (tesouro, fósseis, peixes, labirinto, corrida de rios, vila, vulcão) a partir de um Kinect e um projetor.

![SARndbox](https://img.shields.io/badge/base-SARndbox%202.8-blue) ![Kinect](https://img.shields.io/badge/sensor-Kinect%20v1-lightgrey) ![Testes](https://img.shields.io/badge/testes-80-brightgreen) ![Licença GPL-2.0](https://img.shields.io/badge/licença-GPL--2.0-orange)

## Sumário

- [Sobre](#sobre) · [Funcionalidades](#funcionalidades) · [Arquitetura](#arquitetura) · [Requisitos](#requisitos)
- [Instalação](#instalação) · [Uso](#uso) · [Configuração](#configuração) · [Testes](#testes)
- [Como funciona por dentro](#como-funciona-por-dentro) · [Estrutura do projeto](#estrutura-do-projeto)
- [Solução de problemas](#solução-de-problemas) · [Licença](#licença)

## Sobre

Roda no PC **laob**, junto ao AR Sandbox (SARndbox 2.8, Kinect v1 e projetor). Ao ligar o PC
abre a **topografia** de sempre; por comando, atalho, painel do celular ou pelo
[Willbert-LAOB](https://github.com/VituColombo0/Willbert-LAOB), a caixa passa a mostrar uma
**foto** que se deforma conforme mexem na areia, ou uma das brincadeiras. Tudo roda localmente
(Python e modelos na própria pasta); só a instalação baixa coisas.

## Funcionalidades

- **Topografia:** o SARndbox original, sem mudanças, com 8 temas de cor e animação por tema (lava, neve, poeira, vento, mar, estrelas); o Clássico é o visual original, sem efeito.
- **Foto:** a imagem é projetada sobre a areia, amplia onde está alta (efeito lente) e ganha a sombra do relevo; rosto sai recortado, sem fundo.
- **Esculpir:** o rosto vira relevo-alvo; o projetor pinta onde falta areia (vermelho) e onde sobra (azul).
- **Caça ao tesouro** e **fósseis de dinossauro** (T-Rex, Triceratops, Pterodáctilo, pegada): cave para achar.
- **Bichos, carros e pessoas à escolha:** no painel escolhe-se quantos de cada aparecem (peixes, tubarões, tartarugas, águas-vivas, polvos, arraias, carrinhos e pessoas; até 24). Os marinhos nadam nos lagos (areia baixa) e na água da chuva.
- **Carrinhos e pessoas:** quatro modelos de carro (sedã, picape, van, esportivo, e táxi) seguindo os vales; pessoas de vários jeitos (criança ou adulto, chapéu, boné, mochila, cabelo) passeando pela areia.
- **Chuva:** pela mão aberta sobre a areia ou pelo painel (toque no ponto onde chover, chover em tudo, secar). A água tem o visual do lago e a cor do tema: lava no vulcão, água gelada no inverno, lama em marte, oásis no deserto.
- **Mundo LAOB:** o laboratório por dentro, desenhado sobre a areia a partir da planta da sala (janelas, porta, quadro, bancadas, caixa de areia). Quem o Willbert está vendo no laboratório de verdade fica lá dentro, na própria bancada ou andando pela sala; os outros entram pela porta de tempos em tempos. O rosto aparece num balão quando alguém entra ou quando se toca no nome dele no painel. À noite as janelas da planta escurecem.
- **Labirinto de água:** leve a água da nascente até o alvo sem encostar nas armadilhas do caminho (encostou, recomeça), com placar.
- **Jogos de água** (fácil, normal, difícil; nova rodada 20 s após o fim):

  | Jogo | Objetivo |
  | :--- | :--- |
  | Corrida de rios | Dois times; ganha quem levar a água primeiro à sua bandeira (pouca água: é preciso cavar) |
  | Salve a vila | A enchente vem de três lados; diques e valas protegem as casinhas (10 s para se preparar) |
  | Rota com paradas | A água passa pelos portões 1, 2, 3… na ordem até a bandeira; portões espalhados pela caixa, em ordem sorteada |
  | Vulcão em erupção | Jogo de reagir: lava em pulsos do pico mais alto e bocas novas que abrem de surpresa (aviso 6 s antes) |

  Na vila e no vulcão, uma fila de casinhas num canto da areia (e o painel) mostra quantas estão de pé e quantas foram perdidas.
- **Uma brincadeira por vez:** começar uma encerra as outras; o tema de cor fica.
- **Painel no navegador** (:8090): controla tudo (modos, temas, jogos em cartões com a descrição ao passar o mouse ou tocar, nível, fotos, Willbert, "Reabrir a areia"); a Área de Trabalho tem só o atalho para ele.
- **Vigia:** reabre a topografia se ela fechar, se ficar parada (sem desenhar por 2 min) ou se o GNOME travar.

## Arquitetura

```mermaid
flowchart LR
    K[Kinect v1] --> SB
    subgraph PC[laob]
        SB["SARndbox-rosto<br/>(cópia com modo Foto e Extras)"]
        ORIG["SARndbox-2.8 original<br/>(topografia)"]
        AR["areia.py<br/>comandos areia-*"]
        PN["painel.py :8090"]
        AR -->|cano .fifo| SB
        AR -->|colorMap| ORIG
        PN --> AR
    end
    WB["Willbert<br/>POST /foto · voz"] --> PN
    CEL[Celular] --> PN
    SB --> PJ[Projetor HDMI-0]
    ORIG --> PJ
```

## Requisitos

- PC Ubuntu com o **SARndbox 2.8 compilado e calibrado** em `~/SARndbox-2.8`.
- Kinect v1 e projetor (tela cheia no HDMI-0).
- Placa NVIDIA com driver (no laob: GTX 750 Ti, driver 535, kernel fixado com `kernel-com-driver.sh` do Willbert).
- Não precisa de sudo; o `instalar.sh` cria o Python próprio e baixa os modelos (YuNet e U²-Net, 176 MB).

## Instalação

1. Clone o repositório:

   ```bash
   git clone https://github.com/VituColombo0/Projetor-LAOB.git
   ```

2. Rode o instalador (pode rodar de novo para atualizar):

   ```bash
   cd Projetor-LAOB && bash instalar.sh
   ```

O `instalar.sh` copia o SARndbox para `~/SARndbox-rosto`, aplica os patches e compila, cria o venv,
baixa os modelos, instala os comandos em `~/.local/bin`, põe só o atalho do painel na Área de
Trabalho (e leva fotos de versões antigas para `Imagens/Fotos na areia`), troca o autostart para abrir a topografia e liga o painel e o vigia.
`bash instalar.sh --compilar /tmp/teste` só testa a compilação.

> [!WARNING]
> Antes de reinstalar no laob, compare o laob com o repositório: mudanças feitas direto na
> máquina se perdem com o `instalar.sh`.

## Uso

O jeito normal é o painel (`http://localhost:8090`, atalho na Área de Trabalho). As fotos ficam em
**`Imagens/Fotos na areia`**. Pelo terminal:

| Comando | O que faz |
| :--- | :--- |
| `areia-foto` | Lista as imagens e pergunta o número (Enter = a última) |
| `areia-foto 3` / `areia-foto gato` | Escolhe pelo número ou parte do nome |
| `areia-foto ~/Downloads/x.jpg` | Usa qualquer arquivo (é copiado para a pasta) |
| `areia-foto girar` / `espelhar` / `esculpir` | Gira 90°, espelha ou entra no modo esculpir |
| `areia-foto camera` | Foto pela câmera do laboratório (via Willbert) |
| `areia-foto lista` / `pasta` | Lista as imagens / abre a pasta |
| `areia-topografia` | Volta ao relevo |
| `areia-tema vulcao` | Tema: `classico`, `vulcao`, `inverno`, `marte`, `deserto`, `oceano`, `arco-iris` |
| `areia-tesouro [facil\|normal\|dificil\|achou\|parar]` | Caça ao tesouro |
| `areia-fossil trex [facil\|dificil]` | Fóssil: `trex`, `triceratops`, `pterodactilo`, `pegada` (também `achou`, `parar`) |
| `areia-animais todos [n]` | `peixes`, `carros`, `pessoas` ou `todos` (também `parar`) |
| `areia-animais mar peixe=8 tubarao=1 carro=3` | Quantos de cada: `peixe`, `tubarao`, `tartaruga`, `aguaviva`, `polvo`, `arraia`, `carro`, `pessoa` |
| `areia-animais mundo` | Mundo LAOB: o laboratório por dentro, com os membros (fotos `Nome.jpg` em `~/SARndbox-rosto/membros/`) |
| `areia-labirinto [facil\|normal\|dificil\|nova\|placar\|parar]` | Labirinto de água |
| `areia-jogo <jogo> [facil\|normal\|dificil]` | Jogo de água: `corrida`, `vila`, `rota`, `vulcao` (também `placar`, `nova`, `parar`, `ajuda`) |
| `areia-vigia pausa` / `liga` | Pausa ou religa o vigia da topografia |

**Painel do celular:** `http://<IP-DO-LAOB>:8090` — selfie, foto salva ou câmera do laboratório,
modo, cores, brincadeiras e girar. O endereço aparece no canto do papel de parede do laob
(`atualizar_papel_parede.py`, a cada minuto) e no atalho "Abrir painel da caixa de areia".

## Configuração

| Onde | O quê |
| :--- | :--- |
| `areia/temas/*.cpt` | Temas: altura em cm → cor RGB (abaixo de 0 é água); o ativo é o link `temas/atual.cpt` |
| `sarndbox/projetor.cfg` | Janela em tela cheia no projetor (HDMI-0) |
| `PARAR_BRINCADEIRAS` em `areia/areia.py` | Quais brincadeiras se encerram entre si |
| `/root/etc/SARndbox-2.8` | Calibração real do Kinect e do projetor |

Opções da foto: `--sem-fundo` recorta a pessoa mesmo sem rosto; `--com-fundo` nunca recorta. No
painel, "mistura" traz o relevo de volta e deformação 0 + sombra 0 projeta a foto sem efeito
(também por `POST /ajuste`).

## Testes

Testes automáticos, com projetor falso (imagem, área iluminada, relevo, pasta, cache, comandos,
painel e sintaxe/shellcheck dos scripts):

```bash
bash testes/rodar.sh
```

No laob, depois do `instalar.sh` (~1 min; compila, confere a instalação, o painel e o projetor de
verdade, terminando na topografia):

```bash
bash testes/integracao.sh
```

## Como funciona por dentro

**Modo Foto** (`sarndbox/FaceImage.*`, `patch_surface.py`, `patch_sandbox.py`): a imagem vira
textura com a matriz câmera → imagem; o shader mostra só a imagem, com lente e sombra. Comandos
pelo cano `-cp`: `rosto`, `rostoAjuste`, `esculpir`, `relevo`. A foto ocupa só a área iluminada
(a calibração marca uma caixa maior que a projeção; hoje ~59 × 42 cm). Rosto detectado com YuNet;
recorte com U²-Net de pessoas (~3 s); a preparação (~5 s) fica em `fotos/cache/` (40 últimas) e
trocar para uma já usada leva ~1 s.

**Temas:** a topografia abre com `-uhm temas/atual.cpt -cp topografia.fifo`; `areia-tema` troca na
hora pelo `colorMap` do SARndbox (~0,1 s). Pontos ajustados aos morros de ~8–10 cm do laob.

**Extras** (`sarndbox/Extras.*`): mapa de alturas de 1,5 cm por célula, 2× por segundo, simulação
na CPU e desenho no shader. Comandos: `tesouro [cm]`, `tesouroFim`, `animais peixes|carros|pessoas|todos|nada [n]`, `mundo …`, `mundoRosto NOME`,
`labirinto [nível]`, `labirintoNovo`, `labirintoFim`, `labirintoTeste`,
`jogo <nome> [1-3]`, `jogoNovo`, `jogoFim`, `jogoTeste`, `jogoInfo`.

- **Tesouro:** sorteado onde há areia acima da média; ao cavar aparece só o baú (madeira, cintas de ferro, fechadura), sem disco em volta; achado, a tampa abre em 1,4 s, aparecem ouro e joias e moedas saltam para fora; grãozinhos dourados piscam a menos de 2,5 cm; "Tesouro encontrado!" vai para o `sandbox.log`. Achado, reaparece em outro lugar após 20 s.
- **Fósseis:** mesmo mecanismo (`tipoTesouro` 2–5), raio maior; aparecem só os ossos em marfim com manchas, poros e rachaduras (Tiranossauro de corpo inteiro, crânio de Triceratops com escudo, Pterodáctilo de asas abertas, pegada calcada na lama) e, perto, o quadro de escavação com estacas e barbante; `tesouroTeste` mostra o desenho sem cavar; outro aparece em 30 s.
- **Água da chuva para os animais:** 2× por segundo o Sandbox lê a água simulada e preenche `aguaSim` (profundidade por célula do mapa); `agua()` vale para o lago e para células com 0,45 cm ou mais. O shader dos extras marca os pixels dos animais (`extrasMarca`) e o da água os deixa à vista. `chuva x y s` cria uma nuvem que joga água por alguns segundos; `secar` tira tudo.
- **Peixes:** água = 1,5 cm abaixo da mediana; só nadam onde o corpo inteiro cabe na água (1,8 cm de folga da margem), com rastro e bolhas; cardume com separação de ~5,5 cm, curvas suaves (até 2,8 rad/s); preso numa poça por 4 s, reaparece num lago de verdade.
- **Carrinhos:** só andam onde cabem inteiros em chão firme (2,2 cm de folga do lago e da borda), levantam poeira; seguem a curva de nível dos vales, evitam rampas e lagos, freiam só para o de número menor; travado 3 s, dá meia-volta.
- **Pessoas:** mesmo movimento dos carrinhos com corpo menor (1,7 cm de folga) e passo lento; desenhadas de cima (sapatos, ombros, braços, cabeça).
- **Mundo LAOB** (`sarndbox/Mundo.cpp`): a planta é desenhada no shader e as posições (porta, oito assentos, quatro pontos da sala) são frações da área iluminada, iguais no C++ e no shader. Cada membro tem três estados: fora, dentro e saindo. Presente (visto pelo Willbert) entra logo e não sai; ausente entra, fica de 25 a 55 s e sai por 35 a 105 s. Dentro, alterna entre a própria bancada (60%) e os pontos da sala. O movimento não depende do relevo da areia. As linhas `Mundo: NOME entrou…/está…/foi…/saiu…` do `sandbox.log` alimentam a seção LAOB do painel; com o mundo aberto as linhas de relevo ficam desligadas (`extrasMundo.w`). `extrasMundo2.z` leva a hora local ao shader; `mundoHora <h|auto>` troca a hora para testes. Até 16/11/2026 havia uma cidade em volta do laboratório; foi retirada.
- **Rostos dos membros:** `membros.py` acha o rosto de cada foto de `membros/` (YuNet), recorta em círculo e monta `fotos/membros.rgba` (8 quadros de 128 px, RGBA); o Sandbox lê esse arquivo (textura na unidade 6) e o shader mostra o quadro num balão quando o painel manda `mundoRosto NOME`. O nome é desenhado em letras de 3×5 pontos. O painel lê `present` do `status.json` do Willbert em `/dev/shm/visao/` a cada 5 s (`+` no nome = presente). As fotos ficam só na máquina, fora do repositório.
- **Labirinto:** de 2 a 4 armadilhas (`P_PERIGO`) perto da reta nascente-alvo, medidas como as peças dos jogos; a nascente (disco de 2 cm) usa a água do próprio SARndbox (`addWater`); 2× por segundo mede a profundidade no alvo (`glGetTexImage`); com 0,25 cm escreve "Labirinto: a água chegou!", e após 20 s começa rodada nova.
- **Jogos de água** (`sarndbox/Jogos.cpp`): até 4 nascentes e 16 peças (casa, portão, bandeira, placar) desenhadas no shader; fazenda, lago, barquinho e resgate foram retirados em 14/11/2026 (o código continua, sem comando). 2× por segundo a GPU devolve a água (`glGetTexImage` do nível e do fundo) e mede a maior e a média da profundidade em cada peça; com 0,3 cm conta como molhada. No vulcão, `extrasEfeito.w` escurece o relevo e pinta a água de lava em qualquer tema. Os avisos vão para o `sandbox.log` como linhas `Jogo: …`, que o painel e `areia-jogo placar` leem.
- **Fluidez:** grade de água em 320×240 (`-wts 320 240`; 640×480 não dava conta na GTX 750 Ti). O log mostra a cada 30 s `Desempenho: N quadros/s` e quanto a água acompanhou o tempo real (`a água andou X% do tempo real`). `AREIA_GRADE_AGUA` e `AREIA_PASSOS_AGUA` trocam a grade e os passos por quadro para testes.

**Willbert:** o gesto das duas mãos manda a foto já recortada para `POST /foto` (só aceita a
própria máquina). Fotos `willbert-*` ficam na subpasta "Fotos do Willbert". O painel responde à
própria máquina, à rede local e à Tailscale; o resto recebe 403.

**Comandos internos** (`areia/sandbox.sh`):

```bash
./sandbox.sh rosto
./sandbox.sh cmd "relevo"
./sandbox.sh original
```

## Estrutura do projeto

```text
Projetor-LAOB/
├── instalar.sh          # instala/atualiza tudo no PC
├── requirements.txt     # Python do venv (OpenCV, NumPy, Flask, waitress)
├── sarndbox/            # modo Foto, Extras, Jogos de água e o laboratório (Mundo): FaceImage, Extras, Jogos, Mundo, patches, projetor.cfg
├── areia/               # areia.py, comandos areia-*, painel.py, rosto.py, temas/
├── area-de-trabalho/    # atalho .desktop do painel
├── sistema/             # serviços e timers de usuário (painel, vigia, papel de parede)
└── testes/              # rodar.sh (automático) e integracao.sh (no laob)
```

No PC, tudo fica em `~/SARndbox-rosto` (cópia do SARndbox + estes arquivos, `venv/`, `modelos/`).

## Solução de problemas

| Sintoma | Causa provável | Correção |
| :--- | :--- | :--- |
| A areia não abre ao ligar | Kinect demorou no USB | O autostart espera até 30 s e tenta 3×; veja `~/SARndbox-rosto/inicio.log` ou o botão "Reabrir a areia" do painel |
| Topografia fechou sozinha | Programa caiu | O `areia-vigia` reabre em até 2 min |
| Imagem parada, botões não respondem | Areia ou GNOME travados | O vigia reabre em até 2 min; na hora: botão "Reabrir a areia" no painel |
| Areia a 1 quadro/s, projetor sumiu | Kernel novo sem o driver NVIDIA | Rode `kernel-com-driver.sh` (Willbert-LAOB) e reinicie |
| Foto aparece de lado | Orientação de quem olha a caixa | `areia-foto girar` |
| "Ran out of time" no log | Grade de água grande demais | Mantenha `-wts 320 240` |


## Equipe

Desenvolvido no **Laboratório LAOB**.

- Victor Colombo
- Prof. Mario Gazziro

## Licença

O SARndbox é de Oliver Kreylos (UC Davis), sob GPL-2.0; os patches e o modo Foto deste
repositório seguem a mesma licença. Os modelos baixados têm licenças próprias: YuNet (OpenCV Zoo,
MIT) e U²-Net de pessoas (rembg / U²-Net, Apache-2.0).
