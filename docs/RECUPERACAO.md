# Recuperação: "o disco do laob morreu"

Como montar a caixa de areia do zero numa máquina nova (ou no laob com disco novo), e o que precisa estar
guardado **fora** do laob para isso dar certo. Escrito em 23/11/2026, a partir do levantamento da auditoria
(tudo conferido no laob, só leitura).

O repositório traz o código, os sons, os temas, os serviços e uma cópia da calibração. O que **não** está
no repositório (e só volta com backup) está na tabela do fim.

## Antes: o backup que precisa existir

Sem estes arquivos a recuperação vira "recalibrar e recompilar tudo do zero". Ao todo, menos de 300 MB.
Guardar fora do laob (no PC do Victor ou em outra máquina do laboratório) e renovar quando algo mudar.

| O quê | Onde fica no laob | Por quê |
|---|---|---|
| Tarballs e fontes do Vrui-8.0-002, Kinect-3.10 e SARndbox-2.8 | `~/src`, `~/Downloads` | versões antigas, podem sumir da internet |
| Configuração do Vrui/Kinect (inclui `IntrinsicParameters-<serial>.dat`) | `/usr/local/etc/Vrui-8.0` | calibração interna do Kinect |
| Calibração da caixa e do projetor | `/root/etc/SARndbox-2.8` (cópia em `~/SARndbox-rosto/etc/SARndbox-2.8`) | relevo e cores no lugar certo |
| Fotos dos membros | `~/SARndbox-rosto/membros` | rostos do Mundo LAOB (fora do git) |
| Fotos dos usuários | `~/Pictures/Fotos na areia` | só o backup recupera |
| Modelos (YuNet e U²-Net, 176 MB) | `~/SARndbox-rosto/modelos` | o instalar.sh baixa de novo, mas o endereço pode mudar |
| Lista do Python | `~/SARndbox-rosto/venv/bin/pip freeze` | já fixada no `requirements.txt` |
| Layout das telas | `~/.config/monitors.xml` | monitor à esquerda, projetor à direita |

Exemplo de backup feito do outro PC (por ssh, sem sudo; ajuste o destino):

```bash
B=~/backup-laob/$(date +%Y-%m-%d); mkdir -p "$B"
rsync -a laob:src laob:Downloads/{Kinect-3.10,SARndbox-2.8}* "$B/fontes/"
rsync -a laob:/usr/local/etc/Vrui-8.0 "$B/"
rsync -a laob:/root/etc/SARndbox-2.8 "$B/calibracao-root/"
rsync -a laob:SARndbox-rosto/etc/SARndbox-2.8 "$B/calibracao-copia/"
rsync -a laob:SARndbox-rosto/membros laob:SARndbox-rosto/modelos "$B/"
rsync -a "laob:Pictures/Fotos na areia" "$B/"
rsync -a laob:.config/monitors.xml "$B/"
ssh laob '~/SARndbox-rosto/venv/bin/pip freeze' > "$B/pip-freeze.txt"
```

> [!CAUTION]
> As fotos têm rostos de pessoas: o backup fica numa máquina do laboratório ou do Victor, nunca em
> serviço público nem no repositório.

## Passo a passo

1. **Ubuntu 22.04** com o usuário `laob`. Ligar o login automático (Configurações > Usuários). Instalar o
   driver NVIDIA 535 (`sudo ubuntu-drivers install` ou pelo apt), reiniciar e conferir com `nvidia-smi`.
2. **Pacotes** (sudo):

   ```bash
   sudo apt install build-essential python3-venv ffmpeg wget curl git x11-utils x11-xserver-utils \
     pulseaudio-utils usbutils zenity
   ```

   mais as dependências do Vrui (o script de compilação do Vrui lista o que falta).
3. **Vrui-8.0-002 e Kinect-3.10** a partir do backup das fontes, instalados em `/usr/local` (com as regras
   udev `69-Kinect.rules` e `69-Vrui-devices.rules`). Ligar o Kinect e conferir: `lsusb -d 045e:02ae`.
   Calibração interna do Kinect: copiar `/usr/local/etc/Vrui-8.0` do backup, ou
   `sudo /usr/local/bin/KinectUtil getCalib 0` (lê a de fábrica do Kinect v1).
4. **SARndbox-2.8** em `~/SARndbox-2.8`, compilado como o usuário (no laob antigo foi compilado como root,
   por isso a calibração ficava em `/root/etc`; compilando como usuário ela vai para `~/SARndbox-2.8/etc`).
5. **Calibração**: copiar a do backup para `~/SARndbox-2.8/etc/SARndbox-2.8`. Sem backup, se a caixa, o
   Kinect e o projetor são os mesmos e não saíram do lugar, `testes/dados/` do repositório tem o
   `BoxLayout.txt` e o `ProjectorMatrix.dat` do laob. Se algo mudou de lugar, recalibrar
   (CalibrateProjector + BoxLayout, uns 20 min, manual do SARndbox).
6. **Telas**: projetor em 1024x768 à direita do monitor (posição +1280+0), em Configurações > Telas.
7. **Login gráfico uma vez** (cria os favoritos do Arquivos e o barramento da sessão). Trazer o repositório
   (clone ou rsync a partir do PC do Victor) e conferir o que falta, sem mudar nada:

   ```bash
   bash instalar.sh --verificar
   ```

   Depois instalar, apontando para a calibração do passo 5:

   ```bash
   SARNDBOX_CALIB=~/SARndbox-2.8/etc/SARndbox-2.8 bash instalar.sh
   ```

   (ou `SARNDBOX_CALIB=$PWD/testes/dados` no caso do passo 5 sem backup). Se faltar rede, o instalar.sh
   termina dizendo o que faltou; copiar os modelos do backup para `~/SARndbox-rosto/modelos` e rodar de novo.
8. **Fotos**: membros do backup para `~/SARndbox-rosto/membros` (o nome do arquivo é o nome do membro) e as
   fotos para `~/Pictures/Fotos na areia`.
9. **Máquina**: `bash deploy/laob/otimizar-projetor.sh` (tela do GNOME, sem sudo) e
   `sudo bash deploy/laob/otimizar-projetor.sh` (limite do journal e apt no boot). Se o Porta-LAOB ou o
   Willbert precisarem dos serviços no boot sem login: `loginctl enable-linger laob`.
10. **Willbert-LAOB** (se for voltar): `otimizar.sh` dele (processador, desempenho da placa, liga/desliga
    sem senha, driver fora das atualizações automáticas) e `kernel-com-driver.sh` se aparecer um kernel sem
    o driver NVIDIA. Entrar no Tailscale (`sudo tailscale up`); sem ele o painel só responde na rede local.
11. **Conferir**: `bash instalar.sh --verificar` sem FALTA, `bash testes/integracao.sh` com TUDO OK, e
    reiniciar uma vez para ver a topografia abrir sozinha e a saída de som `combinado` aparecer
    (`pactl list short sinks`).

## O que só volta com backup (ou refazendo à mão)

| Item | Como refazer sem backup |
|---|---|
| Calibração da caixa/projetor | `testes/dados/` (mesma montagem) ou recalibrar |
| Calibração interna do Kinect | `sudo KinectUtil getCalib 0` |
| Vrui/Kinect/SARndbox | baixar as mesmas versões, se ainda existirem, e compilar |
| Fotos dos membros | pedir as fotos de novo |
| Fotos dos usuários | não tem como |
| Ajustes do GNOME | `otimizar-projetor.sh` (sem sudo) |
| Login automático, linger, Tailscale | passos 1, 9 e 10 |
| Partes do Willbert (sudoers, kernel fixado, desempenho da placa) | scripts do Willbert-LAOB |

Estado da areia (`estado.json`, `estado_som.json`, `luz.txt`, tema escolhido, cache de fotos) volta ao
padrão sozinho na primeira vez que a areia abre.
