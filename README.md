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

Guia para o laboratório: [`COMO-USAR.txt`](COMO-USAR.txt) e [`SE-A-AREIA-NAO-ABRIR.txt`](SE-A-AREIA-NAO-ABRIR.txt)
(os dois ficam também na Área de Trabalho do lab2).

## Instalar

Num PC com o SARndbox 2.8 já compilado em `~/SARndbox-2.8` e calibrado (Ubuntu, sem sudo):

    git clone https://github.com/VituColombo0/Projetor-LAOB.git && cd Projetor-LAOB
    bash instalar.sh

O `instalar.sh` copia o SARndbox para `~/SARndbox-rosto`, aplica os patches e compila, cria o
Python próprio (`requirements.txt`), baixa os modelos, instala os comandos em `~/.local/bin`,
cria a pasta Areia, a ajuda e o atalho na Área de Trabalho, troca o autostart para abrir a
topografia no projetor e liga o painel do celular. Pode rodar de novo para atualizar.
`bash instalar.sh --compilar /tmp/teste` só testa a compilação.

## Arquivos
- `FaceImage.h/.cpp`: a foto como textura e a matriz câmera -> foto (giro, espelho, proporção).
- `patch_surface.py`, `patch_sandbox.py`: alterações no SurfaceRenderer e no Sandbox (shader e
  comandos `rosto`, `rostoAjuste`, `esculpir`, `relevo` no cano de controle `-cp`).
- `rosto.py`: recorta o rosto (YuNet em `modelos/`), tira o fundo, reforça o contraste e gera `rosto.dem`.
- `sandbox.sh`: troca entre o original e esta versão (volta ao original se ela falhar).
- `areia-foto`, `areia-topografia`: os comandos do dia a dia (pasta Areia, menu, cache).
- `projetor.cfg`: abre a janela em tela cheia no projetor (HDMI-0) via `-mergeConfig`.
- `painel.py`, `sandbox-painel.service`: painel web para o celular.
- `instalar.sh`, `requirements.txt`: instalação.
- `COMO-USAR.txt`, `SE-A-AREIA-NAO-ABRIR.txt`, `abrir-a-areia.desktop`: o que vai para a Área de Trabalho.

## Painel do celular
`painel.py` (serviço de usuário `sandbox-painel`) em http://100.114.194.28:8090 (só Tailscale e
127.0.0.1): tirar selfie, escolher foto ou usar a câmera do laboratório, trocar de modo,
girar/espelhar e ajustar deformação, sombra e mistura com as cores de relevo.

## Comandos internos (sandbox.sh)
    ./sandbox.sh rosto
    venv/bin/python rosto.py foto.jpg
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
  - `areia-foto lista`, `areia-foto pasta` (abre a pasta), `areia-foto girar`, `areia-foto espelhar`.
  - `areia-foto camera`: foto pela câmera do laboratório (só essa opção depende do Willbert).
  - `areia-topografia`: volta à topografia.
- Com rosto na imagem: recorte retrato em volta do rosto, sem fundo. Sem rosto: a imagem inteira,
  com fundo (`--sem-fundo` recorta a pessoa mesmo assim; `--com-fundo` nunca recorta). PNG já
  transparente usa a própria transparência. A preparação (~5 s) fica guardada em `fotos/cache/`
  (40 últimas): trocar para uma imagem já usada leva ~1 s.
- Tudo local nesta pasta: `venv/` (OpenCV, NumPy, Flask, waitress), `modelos/` (YuNet e U²-Net) e a
  calibração de `etc/`. Recriar o venv:
  `python3 -m venv venv && venv/bin/pip install numpy==2.2.6 opencv-python-headless==5.0.0.93 Flask==3.1.3 waitress==3.0.2`
  As fotos enviadas pelo painel do celular também vão para a pasta Areia.
- No modo foto não aparecem cores de relevo, curvas de nível nem água: só a foto, que amplia onde a
  areia está mais alta e ganha sombra do relevo. No painel, "mistura" traz o relevo de volta, e
  deformação 0 + sombra 0 projeta a foto sem efeito nenhum.
- Recorte do fundo: U²-Net de pessoas (`modelos/u2net_human_seg.onnx`, 176 MB, release v0.0.0 do
  projeto rembg) rodando no OpenCV do venv local, ~3 s. Baixar com:
  `wget -P modelos https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net_human_seg.onnx`

## Ao ligar e se não abrir

- O lab2 tem login automático (GDM) e o autostart roda `areia-topografia`, que espera o Kinect
  aparecer no USB (até 30 s), tenta abrir 3 vezes e registra em `~/SARndbox-rosto/inicio.log`.
- Na Área de Trabalho: `SE A AREIA NÃO ABRIR.txt` (cópia de `SE-A-AREIA-NAO-ABRIR.txt`) e o atalho
  `Abrir a areia.desktop` (de `abrir-a-areia.desktop`; precisa de `chmod +x` e
  `gio set <arquivo> metadata::trusted true` para abrir com dois cliques).

## Licença

O SARndbox é de Oliver Kreylos (UC Davis), sob GPL-2.0; os patches e o modo Foto deste repositório
seguem a mesma licença. Os modelos baixados pelo instalador têm licenças próprias: YuNet
(OpenCV Zoo, MIT) e U²-Net de pessoas (projeto rembg / U²-Net, Apache-2.0).
