# Manual da caixa de areia

Para quem nunca usou. Tudo se faz pelo **painel**, uma página no navegador. Não precisa de terminal.
Resumo deste manual na Área de Trabalho do laob: `LEIA-ME - caixa de areia.txt`.

## Ligar e desligar

**Ligar:** ligue o projetor e depois o PC **laob**. O PC entra sozinho e, em 1 ou 2 minutos, a areia
aparece com o relevo colorido (a topografia).

**Desligar:** no laob, menu do canto de cima, à direita > **Desligar**. Depois desligue o projetor no
botão dele. Não tire o PC da tomada ligado.

> [!TIP]
> Dá para deixar tudo ligado de um dia para o outro: a areia fica no relevo colorido e o vigia cuida dela.

## Abrir o painel

- **No laob:** dois cliques em **Abrir painel da caixa de areia**, na Área de Trabalho.
- **Pelo celular ou outro computador do laboratório** (mesma rede): digite no navegador o endereço que
  aparece no canto de baixo, à esquerda, do papel de parede do laob. Ele termina em `:8090`.

O painel tem seis abas no topo:

| Aba | Para quê |
| :--- | :--- |
| Jogos | Jogos de água, labirinto, Copie o relevo, tesouro e fóssil. Escolha o nível antes |
| Vida | Peixes, patos, carros, cachorros, pássaros... Escolha quantos e toque em **Soltar na areia**. Toque na imagem ao vivo para jogar comida |
| Chuva | Toque na imagem ao vivo no ponto onde quer que chova; **Chover em tudo** ou **Secar a água** |
| LAOB | O laboratório desenhado na areia, com os membros. Toque num nome para ver o rosto |
| Fotos | Uma foto projetada na areia (do celular, de um arquivo ou da câmera do Willbert) |
| Projetor | Voltar à topografia, cores do relevo, brilho, Willbert, **Reabrir a areia** e a linha de saúde |

## Trocar de brincadeira

Toque na brincadeira que quiser: **uma por vez**. Começar outra encerra a anterior; a cor do relevo
continua a mesma. A troca leva menos de meio segundo.

- **Parar tudo** (aba Jogos) ou **Voltar à topografia** (aba Projetor): volta ao relevo colorido.
- **Cores do relevo** (aba Projetor): Clássico, Vulcão, Inverno, Marte, Deserto, Oceano e Arco-íris.
- **Brilho da imagem** (aba Projetor): Normal, Forte (o padrão) ou Máximo. Com a sala clara, use Máximo.
- **Chuva com a mão:** abra a mão sobre a areia, sem encostar, e chove embaixo dela.
- **Comida com o dedo** (com bichos na areia): aponte com o indicador, os outros dedos fechados.

## Se a areia não abrir

Siga na ordem e pare quando voltar:

1. **Espere 2 minutos.** O vigia confere a areia a cada 2 minutos e abre de novo se ela fechou ou travou.
2. **Painel > aba Projetor > Reabrir a areia.** Fecha e abre de novo, mesmo travada (uns 10 segundos).
3. **Olhe a linha de saúde** no alto da aba Projetor:
   - **vigia pausado:** alguém pausou o vigia. O botão **Reabrir a areia** religa.
   - **Areia: reserva (SARndbox original):** a versão completa não abriu e entrou a de reserva (só o relevo,
     sem jogos, bichos nem chuva). Toque em **Reabrir a areia**; o vigia também tenta de novo sozinho a
     cada 10 minutos.
   - **Areia: fechada:** toque em **Reabrir a areia**.
4. **Projetor sem imagem nenhuma:** confira se ele está ligado e na entrada HDMI. A imagem do projetor
   fica à direita do monitor do laob.
5. **O painel não abre:** reinicie o laob (menu > Desligar > **Reiniciar**). A areia e o painel voltam
   sozinhos em 1 ou 2 minutos.
6. **Nada resolveu:** chame o Victor (veja o fim deste manual).

## Pôr um membro novo no LAOB

1. Pegue uma foto de rosto, de frente e bem iluminada, com o nome da pessoa no nome do arquivo:
   `Nome.jpg` (por exemplo `Ana.jpg`). Só o primeiro nome, até 8 letras.
2. Copie a foto para a pasta `~/SARndbox-rosto/membros/` do laob (Arquivos > Pasta pessoal >
   SARndbox-rosto > membros).
3. Abra o **LAOB** no painel. Os rostos são refeitos sozinhos quando a pasta muda; o membro novo ganha
   um lugar na sala e o rosto aparece no balão. Cabem até 8 membros.

Para conferir o rosto na hora, sem abrir o LAOB, no terminal do laob:

```bash
cd ~/SARndbox-rosto && venv/bin/python membros.py
```

> [!IMPORTANT]
> As fotos dos membros ficam só no laob, nunca no repositório nem em serviço público.

## Ver a saúde

Na aba **Projetor**, a primeira linha mostra como a areia está. Em vermelho, o que está fora do normal.

| Item | Normal | Se estiver fora |
| :--- | :--- | :--- |
| qps (quadros por segundo) | 30 | Abaixo de 25: a imagem fica lenta. Reabra a areia |
| água | 100% | Abaixo de 90%: a água anda devagar. Seque a água ou reabra a areia |
| quadro p99 | perto de 34 ms | Acima de 50 ms: a imagem dá travadinhas |
| GPU | até uns 50 °C | 80 °C ou mais: veja se a ventilação do PC está livre |
| disco | dezenas de GB livres | Menos de 5 GB: chame o Victor |
| vigia | vigiando | **pausado:** Reabrir a areia religa |
| Último aviso | (nada) | O último problema anotado ao abrir a areia nas últimas 48 h (Kinect, areia fechada ou parada) |

## O que é normal

- Ao abrir ou reabrir a areia, o projetor pisca uma vez.
- O som está **mudo** de propósito. O mudo e o volume ficam no cabeçalho do painel.
- Com o Willbert desligado, o botão da câmera fica cinza e o LAOB não sabe quem está no laboratório:
  todos os membros entram e saem de tempos em tempos.

## Quem ajuda

**Victor** cuida da caixa de areia: chame-o se a areia não voltar com os passos acima, se o disco estiver
cheio ou se algo parecer quebrado. Ele usa o [README](../README.md) e o
[plano de recuperação](RECUPERACAO.md).
