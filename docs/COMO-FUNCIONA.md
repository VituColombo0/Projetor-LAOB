# Como funciona por dentro

Referência técnica do Projetor-LAOB: o que cada brincadeira faz em detalhe e como as peças funcionam
(shader, simulação, comandos do cano, medidas). Para instalar e usar, veja o [README](../README.md); para o
dia a dia do laboratório, o [manual](MANUAL.md).

## Sumário

- [Brincadeiras em detalhe](#brincadeiras-em-detalhe)
- [Foto e temas](#foto-e-temas)
- [Extras e comandos do cano](#extras-e-comandos-do-cano)
- [Cada peça dos Extras](#cada-peça-dos-extras)
- [Chuva, brilho e dedo apontado](#chuva-brilho-e-dedo-apontado)
- [Willbert, fotos e registros](#willbert-fotos-e-registros)
- [Comandos internos](#comandos-internos)
- [Som e calibração](#som-e-calibração)
- [Testes e medidas](#testes-e-medidas)

## Brincadeiras em detalhe

- **Topografia:** o relevo colorido de sempre, com 7 temas de cor (Clássico, Vulcão, Inverno, Marte, Deserto, Oceano e Arco-íris) e animação por tema (lava, neve, poeira, vento, mar, estrelas); o Clássico é o visual original, sem efeito. O relevo tem sombreamento suave (a areia parece 3D) e curvas de nível a cada 1 cm, sem serrilhado, com a linha principal mais forte a cada 5 cm.
- **Foto:** a imagem é projetada sobre a areia, amplia onde está alta (efeito lente) e ganha a sombra do relevo; rosto sai recortado, sem fundo.
- **Esculpir:** o rosto vira relevo-alvo; o projetor pinta onde falta areia (vermelho) e onde sobra (azul).
- **Caça ao tesouro** e **fósseis de dinossauro** (T-Rex, Triceratops, Pterodáctilo, pegada): cave para achar.
- **Vida na areia:** no painel (aba Vida, em grupos água / terra / ar) escolhe-se quantos de cada aparecem, até 32: na água (peixes, tubarões, tartarugas, águas-vivas, polvos, arraias, patos e barquinhos a vela), em terra (carrinhos, pessoas e cachorros) e no ar (pássaros, que voam por cima de tudo com a sombra no chão). Os da água nadam na água de verdade (o lago e a chuva são a mesma água) e a correnteza leva quem boia; os de terra andam só na areia seca e, quando a água chega, correm para o seco.
- **Bichos que reagem:** tocar na imagem ao vivo da aba Vida joga comida naquele ponto. Na terra aparece uma tigela vermelha cheia de ração, com uns grãos caídos de um lado (a ração vai acabando em 20 s): os pássaros pousam em volta, os cachorros correm até lá e sentam abanando o rabo e quem passeia para um pouco para olhar. Na água caem flocos coloridos de ração de peixe (laranja, vermelho, amarelo, verde) que boiam, vão com a correnteza e se espalham devagar: peixes (em cardume, beliscando), tartarugas, patos, polvos, arraias, águas-vivas e até o tubarão vêm comer, só pela água, e elas somem uma a uma. Só vêm os que estão perto (até uns 10 cm), no máximo 4 de cada vez. As duas comidas podem estar na areia ao mesmo tempo; o painel diz o que caiu e quem vem (e avisa se ninguém come aquilo). Apontar com o indicador (os outros dedos fechados), para cima ou para a areia, também derruba comida no ponto do dedo (na água, flocos de ração; na terra, a tigela), renovada enquanto se aponta: o dedo é reconhecido em 0,2 s, a ração cai do alto (o floco encolhe até pousar na água) e nada mais pisca em volta (a mão parada não vira relevo). A mão aberta sobre a areia só faz chover. Entre eles: o tubarão caça os peixes e o cardume foge, os pássaros mergulham para pescar, os patinhos nadam em fila atrás da mãe e os cachorros correm atrás dos pássaros.
- **Carrinhos e pessoas:** quatro modelos de carro (sedã, picape, van e esportivo; o sedã amarelo vira táxi) seguindo os vales; pessoas de vários jeitos (criança ou adulto, chapéu, boné, mochila, cabelo) passeando pela areia.
- **Água única:** a chuva, o lago dos bichos e o azul do mapa são a mesma água, a da simulação. Com bichos da água, tudo o que fica abaixo do nível do mar enche de verdade (o azul do mapa nunca fica seco, e cavar um buraco faz a água brotar devagar); a chuva escorre em rios até o lago, sobe o nível e a areia absorve a água a mais aos poucos. Uma lâmina fina parece areia molhada e ganha a cor da água conforme fica funda.
- **Chuva:** pela mão aberta sobre a areia (também com bichos; no Mundo LAOB a mão não chove) ou pelo painel, tocando na imagem do projetor ao vivo no ponto onde chover (também chover em tudo e secar; com bichos da água, secar tira a chuva e o lago volta a encher). A água tem a cor do tema: lava no vulcão (azul quando há bichos da água); nos outros temas fica num azul que destaca do relevo (glacial no inverno, ciano em marte e no deserto). A água é mais clara no raso e de um azul firme no fundo, com a margem clara e um brilho que anda com a correnteza. Nos jogos de água, no labirinto e no lago dos bichos ela é azul em todo tema; o que está abaixo de 0 cm vira leito seco, para só a água do jogo parecer água.
- **Sons:** tudo automático, nas caixas de som e no HDMI do projetor ao mesmo tempo, e só do que aparece na areia. Um fundo baixinho segue a animação do tema (brisa no clássico, vento frio com a neve no inverno, vento seco no deserto, lava e roncos no vulcão, ondas no oceano, vento ralo em marte, brilhos com as estrelas no arco-íris); no Mundo LAOB, o som da sala (ar e computadores), alguém digitando e a porta quando um membro entra ou sai. Enquanto chove (mão ou painel) entra o som da chuva. Cada brincadeira põe uma camada por cima (água correndo nos jogos de nascente, lava no vulcão), e os bichos na areia soam de vez em quando (piado, quack, latido, blub), um por vez. Por cima, os efeitos: início, vitória, derrota, alerta, baú aberto no tesouro, pincel e osso no fóssil, plic-plic da ração caindo na água e o puf da tigela na terra. Areia fechada, foto ou escultura: silêncio. No cabeçalho do painel ficam só o mudo 🔇 e o volume.
- **Mundo LAOB:** o laboratório por dentro, visto de cima, desenhado a partir de fotos das salas: a sala principal (cluster com cabos e LEDs piscando, câmeras, scanner corporal com a plataforma girando, arco no piso xadrez, quadro verde, bancada de eletrônica com osciloscópio, mesa de carteado, a própria caixa de areia), a sala de forense (urna eletrônica, armários, mesas com tabuleiro de calibração, TV) e o miniauditório (espuma acústica, tela com slides, sofás, cadeiras embrulhadas), ligadas por portas e um corredor. Cada membro tem um boneco próprio (o Gazziro de chapéu) e uma rotina com sentido: o Gazziro apresenta no miniauditório, dá aula no quadro e trabalha na forense; os outros ficam no próprio lugar e vão ao cluster, às câmeras, ao scanner. Quem o Willbert está vendo no laboratório de verdade fica lá dentro; os outros entram pelo corredor de tempos em tempos. O rosto aparece num balão quando se toca no nome dele no painel. Na aba LAOB dá para escolher o que a areia mostra (tudo, sala principal, forense ou miniauditório) e seguir um membro (👁 seguir: a imagem acompanha o boneco com zoom); pelo terminal, `areia-animais vista principal|forense|mini|tudo|NOME`. Eles também digitam, apontam o slide, bebem água, mexem nos aparelhos (o osciloscópio acelera, a geladeira abre, os LEDs do cluster piscam mais) e conversam em dupla. À noite as salas vazias escurecem e ficam só os LEDs e as telas em espera.
- **Labirinto de água:** leve a água da nascente até o alvo sem encostar nas armadilhas do caminho (encostou, recomeça), com placar.
- **Copie o relevo:** um relevo-alvo sorteado (montanha, duas montanhas, vulcão, cratera, ilha e lago, serra) para fazer no meio da areia. O painel mostra a miniatura do alvo (verde e marrom = monte, azul = buraco); a areia continua com as cores do tema e só ganha, no canto, um cartão claro com a barra e a porcentagem de acerto em algarismos grandes. Acima de 85% vem a comemoração e outro relevo é sorteado.
- **Jogos de água** (fácil, normal, difícil; nova rodada 20 s após o fim):

  | Jogo | Objetivo |
  | :--- | :--- |
  | Corrida de rios | Dois times; ganha quem levar a água primeiro à sua bandeira (pouca água: é preciso cavar) |
  | Salve a vila | A enchente vem de três lados; diques e valas protegem as casinhas (a água desce desde o começo) |
  | Rota com paradas | A água passa pelos portões 1, 2, 3… na ordem até a bandeira; portões espalhados pela caixa, em ordem sorteada |
  | Vulcão em erupção | Jogo de reagir: lava em pulsos do pico mais alto e bocas novas que abrem de surpresa (aviso 6 s antes) |

  Na vila e no vulcão, uma fila de casinhas num canto da areia (e o painel) mostra quantas estão de pé e quantas foram perdidas.
  Todos os jogos (e o labirinto e o Copie) usam as mesmas cores de interface (azul, vermelho, verde, ouro, claro e o escuro dos contornos) e a mesma comemoração: um anel dourado que se abre uma vez, um brilho que some em 2 s e uma coroa dourada sobre o ponto da vitória. Os portões da rota mostram o número em algarismo grande com contorno; o próximo fica dourado, sem piscar.
- **Uma brincadeira por vez:** começar uma encerra as outras; o tema de cor fica.
- **Painel no navegador** (:8090): organizado em abas (Jogos, Vida, Chuva, LAOB, Fotos, Projetor), cada uma cabendo numa tela no computador (1366×768) e no celular (só a Vida, no celular, rola até o quadro da comida); no computador, a aba Vida tem os bichos à esquerda e a imagem ao vivo da comida à direita; uma miniatura ao vivo do projetor fica no cabeçalho; cada botão mostra que está enviando e um ✓ quando a areia aceitou; trocas de brincadeira aparecem na areia com uma transição suave; `#chuva`, `#vida`... no endereço abre direto na aba; controla tudo (modos, temas, brilho da imagem, jogos em cartões com a descrição ao passar o mouse ou tocar, nível, fotos, Willbert, "Reabrir a areia"); a Área de Trabalho tem só o atalho para ele e o `LEIA-ME - caixa de areia.txt`. A foto pela câmera e o ligar/desligar do Willbert rodam em segundo plano (o resultado aparece em alguns segundos e os outros botões não ficam presos); com o Willbert desligado, o botão da câmera fica desabilitado com o motivo e a presença no LAOB fica desconhecida. A aba Projetor tem uma linha de saúde: quadros/s, água %, p99 do quadro, temperatura e uso da GPU, disco livre, tempo ligado, vigia (vigiando ou pausado até HH:MM), areia de reserva e o último aviso do `inicio.log` (também em `GET /saude`, JSON).
- **Trocas rápidas:** trocar de brincadeira, voltar ao relevo ou abrir a foto não recompila o shader (as variantes comuns são compiladas ao abrir a areia); voltar à topografia com a areia aberta leva uns 0,2 s. Uma troca de programa por vez: painel, vigia e terminal esperam a vez.
- **Vigia:** reabre a topografia se ela fechar, se ficar parada (sem desenhar por 2 min) ou se o GNOME travar. Se a versão com Foto não abrir, o SARndbox original entra como **topografia de reserva** (anotado em `~/SARndbox-rosto/reserva`, com o registro da falha em `sandbox.falhou.log`) e o vigia tenta voltar à versão completa a cada 10 min, no máximo 3 vezes por hora.

## Foto e temas

**Modo Foto** (`sarndbox/FaceImage.*`, `patch_surface.py`, `patch_sandbox.py`): a imagem vira
textura com a matriz câmera → imagem; o shader mostra só a imagem, com lente e sombra. Comandos
pelo cano `-cp`: `rosto`, `rostoAjuste`, `esculpir`, `relevo`. A foto ocupa só a área iluminada
(a calibração marca uma caixa maior que a projeção; hoje ~59 × 42 cm). Rosto detectado com YuNet;
recorte com U²-Net de pessoas (~3 s); a preparação (~5 s) fica em `fotos/cache/` (40 últimas) e
trocar para uma já usada leva ~1 s.

Opções da foto: `--sem-fundo` recorta a pessoa mesmo sem rosto; `--com-fundo` nunca recorta. No
painel, "mistura" traz o relevo de volta e deformação 0 + sombra 0 projeta a foto sem efeito
(também por `POST /ajuste`).

**Temas:** a topografia abre com `-uhm temas/atual.cpt -cp topografia.fifo`; `areia-tema` troca na
hora pelo `colorMap` do SARndbox (~0,1 s). Pontos ajustados aos morros de ~8–10 cm do laob.

## Extras e comandos do cano

**Extras** (`sarndbox/Extras.*`): mapa de alturas de 1,5 cm por célula, 2× por segundo, simulação
na CPU e desenho no shader. Comandos pelo cano (`./sandbox.sh cmd "..."`): `tesouro [cm]`, `tesouroFim`,
`tesouroTeste`, `animais peixes|carros|pessoas|todos|nada [n]`, `mar peixe=N tubarao=N …`, `chuva x y s`, `secar`, `aguaNivel auto|mapa|areia|<cm> [absorção cm/s]`, `aguaInfo`,
`mundo …`, `mundoRosto NOME`, `mundoHora <h|auto>`, `labirinto [nível]`, `labirintoNovo`, `labirintoFim`,
`labirintoTeste`, `jogo <nome> [1-3]`, `jogoNovo`, `jogoFim`, `jogoTeste`, `jogoInfo`, `jogoVazao <f>`,
`copia [forma] [1-3]`, `copiaNova`, `copiaFim`, `copiaTeste` (dá a rodada por conseguida), `comida x y s [terra|agua]` (decide pelo ponto; com terra/agua só cai se o ponto for assim), `racao x y s` (ração na água mais perto), `gestoTeste x y [s]` (dedo apontado de mentira),
`gestoDiag [n]` (medidas de cada mão nas próximas n procuras, no log), `gestoAjuste [nome valor]` (limiares do dedo),
`gestoGrava [s]` (grava 1 a 20 s do sensor em `/dev/shm/gesto-*/` para `testes/analisa_gesto.cpp`, ~30 quadros/s, ~400 MB em 20 s; cada gravação nova apaga as anteriores, então analise antes de gravar de novo; precisa de 1 GB livre e para em 600 MB), `luz força [saturação]` (brilho da imagem).

## Cada peça dos Extras

- **Tesouro:** sorteado onde há areia acima da média; ao cavar aparece só o baú (madeira, cintas de ferro, fechadura), sem disco em volta; achado, a tampa abre em 1,4 s, aparecem ouro e joias e moedas saltam para fora; grãozinhos dourados piscam a menos de 2,5 cm; "Tesouro encontrado!" vai para o `sandbox.log`. Achado, reaparece em outro lugar após 20 s.
- **Fósseis:** mesmo mecanismo (`tipoTesouro` 2–5), raio maior; aparecem só os ossos com manchas, poros e rachaduras, em marfim no chão escuro e sépia escuro no chão claro (Deserto, Inverno, topo dos morros), sem curvas de nível por cima (Tiranossauro de corpo inteiro, crânio de Triceratops com escudo, Pterodáctilo de asas abertas, pegada calcada na lama) e, perto, o quadro de escavação com estacas e barbante; `tesouroTeste` mostra o desenho sem cavar; outro aparece em 30 s.
- **Água única:** 4× por segundo o Sandbox lê a água simulada (nível e correnteza) e preenche `aguaSim`, `aguaVx` e `aguaVy` por célula do mapa; `agua()` vale onde há 0,45 cm ou mais (interpolado). Com bichos da água, o lago é mantido na GPU, em todo passo da simulação, dentro do `Water2WaterUpdateShader` (o passo que soma a chuva; `WaterTable2::setLencol` passa o nível do mar e a área iluminada): célula com fundo abaixo do nível do mar recebe 1,6×/s do que falta, e água acima do nível perde 0,02 cm/s (absorção). Nível do mar padrão (`aguaNivel auto`): o maior entre 0 cm (onde o mapa fica azul) e 1,5 cm abaixo da areia mediana. Nada é lido e reescrito pela CPU, então a correnteza nunca para. Ao tirar os bichos da água, o lago é esvaziado. O shader dos extras marca os pixels dos animais (`extrasMarca`) e o da água os deixa à vista. `chuva x y s` cria uma nuvem que chega e se desfaz aos poucos (a água acompanha), com gotas e respingos; no painel, o toque desenha a nuvem com chuva no próprio navegador e a imagem ao vivo vai a 6 quadros/s na aba Chuva; `secar` tira tudo. A imagem ao vivo do painel (`/projetor.jpg`) vem de um `ffmpeg -f x11grab` na saída do projetor, 6 quadros/s, ligado só enquanto alguém olha (desliga 15 s depois e morre junto com o painel).
- **Margem da água:** a altura e a profundidade usadas pelos bichos são interpoladas entre as células, para concordar com a água desenhada. Quem anda em terra precisa de 4 mm de folga acima do lago e evita água da chuva a partir de 0,15 cm; o corpo é conferido em 8 direções.
- **Peixes:** nadam na água simulada e a correnteza os leva (patos, barquinhos e águas-vivas por inteiro, peixes pela metade, o tubarão quase nada); se a água baixa, nadam para o mais fundo e só reaparecem em outro lugar depois de 2 s secos; só nadam onde o corpo inteiro cabe na água (1,8 cm de folga da margem), com rastro e bolhas; cardume com separação de ~5,5 cm, curvas suaves (até 2,8 rad/s); preso numa poça por 4 s, reaparece num lago de verdade.
- **Carrinhos:** só andam onde cabem inteiros em chão firme (2,2 cm de folga do lago e da borda), levantam poeira; seguem a curva de nível dos vales, evitam rampas e lagos, freiam só para o de número menor; travado 3 s, dá meia-volta.
- **Pessoas:** mesmo movimento dos carrinhos com corpo menor (1,7 cm de folga) e passo lento; desenhadas de cima (sapatos, ombros, braços, cabeça).
- **Mundo LAOB** (`sarndbox/Mundo.cpp`, `sarndbox/MundoPlanta.h`): a planta (três salas e o corredor) é desenhada no shader, uma função por sala, cada uma só onde o pixel cai nela (`MUNDO_NIVEL` 2 tira os detalhes menores). `MundoPlanta.h` é a fonte única das posições (paredes, portas, móveis, assentos e pontos), usada pelo shader e pelo C++; as rotas passam pelas portas e contornam os móveis (`testes/teste_mundo.cpp` confere todas). `extrasMundo.x` leva ao shader quem está onde (monitores acendem, slide troca mais rápido quando alguém apresenta). Os bonecos têm visual fixo por nome (`visual[]` em `copiaNomes`). A câmera do mundo é o uniform `extrasVista` (centro e zoom): todo o desenho do mundo sai de `ep.xy` já transformado; `mundoVista tudo|principal|forense|mini|NOME` troca em 0,6 s e segue o boneco (linha `Mundo: vista …`). Lugares fixos: Victor e Mario frente a frente na ilha da direita (3 monitores cada), Lucas na mesinha da parede atrás do carteado, Gazziro no miniauditório. Cada membro tem três estados: fora, dentro e saindo. Presente (visto pelo Willbert) entra logo e não sai; ausente entra, fica de 25 a 55 s e sai por 35 a 105 s. Dentro, alterna entre a própria bancada (60%) e os pontos da sala. O movimento não depende do relevo da areia. As linhas `Mundo: NOME entrou…/está…/foi…/saiu…` do `sandbox.log` alimentam a seção LAOB do painel; com o mundo aberto as linhas de relevo ficam desligadas (`extrasMundo.w`). `extrasMundo2.z` leva a hora local ao shader; `mundoHora <h|auto>` troca a hora para testes. Até 16/11/2026 havia uma cidade em volta do laboratório; foi retirada.
- **Rostos dos membros:** `membros.py` acha o rosto de cada foto de `membros/` (YuNet), recorta em círculo e monta `fotos/membros.rgba` (8 quadros de 128 px, RGBA); o Sandbox lê esse arquivo (textura na unidade 6) e o shader mostra o quadro num balão quando o painel manda `mundoRosto NOME`. O nome é desenhado em letras de 3×5 pontos. O painel lê `present` do `status.json` do Willbert em `/dev/shm/visao/` a cada 5 s (`+` no nome = presente). As fotos ficam só na máquina, fora do repositório.
- **Labirinto:** de 2 a 4 armadilhas (`P_PERIGO`) perto da reta nascente-alvo, medidas como as peças dos jogos; a nascente (disco de 3,2 cm) usa a água do próprio SARndbox (`addWater`); 2× por segundo mede a profundidade no alvo (`glGetTexImage`); com 0,25 cm escreve "Labirinto: a água chegou!", e após 20 s começa rodada nova.
- **Bichos entre si e com a comida:** a mão de verdade não é seguida (desde 18/11/2026; custava CPU e deixava a água lenta); os da água fogem do tubarão (12 cm); o tubarão vai atrás do peixe mais perto (25 cm) e, depois de pegar um, fica satisfeito por 12 a 18 s; o pássaro escolhe um lugar em chão firme perto da comida, pousa e levanta voo se a comida acaba, chega um cachorro (7 cm) ou a água; sem comida, mergulha sobre um peixe (1,8 s, pega em 45% das vezes). O primeiro pato é a mãe e os outros, patinhos menores, seguem em fila. O cachorro senta a 3 cm da comida; sem comida, corre atrás de um pássaro por até 8 s. Ração (até 24 flocos, `extrasRacao`): cada floco vai com 90% da correnteza, se afasta das vizinhas e encosta na margem; se a água some embaixo, procura água por 1,5 s e some. Quem come vê a bolinha mais perto que alcança em linha reta pela água (peixe 12 cm, tartaruga e pato 11, tubarão 10 quando não está caçando, os outros 7 a 9) e belisca com a boca; cada grupo de bolinhas recebe no máximo 4, os mais perto. Na tigela: pássaros de até 10 cm e cachorros de até 8 cm, no máximo 4 juntos, e até 3 pessoas olhando. Encostados, os bichos escorregam de lado em vez de travar (cada espécie tem um corpo, e ninguém entra em 70% dele); na água todos desviam uns dos outros, com preferência pela direita, e rente à margem são puxados para o meio do lago. Linhas `Vida: …` vão para o `sandbox.log`.
- **Copie o relevo:** o alvo é a soma de até 4 gaussianas (morros e buracos) sobre a altura mediana da areia no começo, igual no C++ e no shader. A cada mapa de alturas (2× por segundo), nota = 1 − Σ max(0, |erro| − folga) / Σ max(0, |alvo| − folga): areia lisa dá 0%, cópia perfeita 100%, mexer onde não devia desconta. Folga de 1,5 / 1,1 / 0,8 cm por nível. A nota vai para o `sandbox.log` como `Copia: N%`, e o alvo como `Copia: alvo x,y,altura,largura …` (frações da área iluminada) para a miniatura do painel; o shader só desenha a barra e o número (algarismos 3×5).
- **Transição:** quando a brincadeira muda, o desenho novo entra por cima do relevo em 0,6 s (`extrasMundo2.w`).
- **Jogos de água** (`sarndbox/Jogos.cpp`): até 4 nascentes e 16 peças (casa, portão, bandeira, placar) desenhadas no shader; fazenda, lago, barquinho e resgate foram retirados em 14/11/2026 (o código foi apagado em 18/11/2026). 2× por segundo a GPU devolve a água (`glGetTexImage` só do canal do nível, e do fundo) e mede a maior profundidade em cada peça; com 0,3 cm conta como molhada. Toda nascente é multiplicada por `fatorVazao` (1,32 = o dobro da calibração de 16/11/2026; `jogoVazao <f>` troca); a boca tem 3,2 cm de raio e a água por cm² é reduzida na mesma proporção, para o total não mudar com o tamanho. Vila e vulcão começam sem contagem: a água desce desde o início. Durante os jogos e o labirinto o lago dos bichos fica desligado. No vulcão, `extrasEfeito.w` escurece o relevo e pinta a água de lava em qualquer tema. Os avisos vão para o `sandbox.log` como linhas `Jogo: …`, que o painel e `areia-jogo placar` leem.
- **Medidas:** junto do `Desempenho:`, a linha `Perfil:` (`sarndbox/Perfil.h`) dá o tempo de cada quadro (p50, p95, p99, máximo e quantos passaram de 50 ms), o tempo de CPU do quadro, da água, do relevo e das leituras GPU→CPU, e o tempo de GPU da água e do relevo (consultas `GL_TIME_ELAPSED` lidas sem esperar). A CPU da água e do relevo vem com o máximo da janela; quando o shader da superfície foi compilado, a linha ganha `; shader compilado N vez(es), X ms (max Y)` e o tempo de GPU desse quadro é descartado. Quadro acima de 100 ms vira `Perfil: quadro lento de N ms` (com `(compilando o shader da superficie: M ms)` quando foi isso).
- **Shader por brincadeira:** o shader dos extras só leva o código da brincadeira em uso (`#define EX_JOGOS`, `EX_BICHOS` ou `EX_MUNDO`; tema, tesouro, copie e chuva ficam sempre), sem mudar a imagem. Ao abrir a areia ficam compiladas a variante do estado atual, as 4 comuns (temas/tesouro/copie, jogos, vida, laboratório) e a da foto; a linha `Superficie: N programas prontos ao abrir (X ms)` vai para o `sandbox.log`. Depois de instalar, a 1ª abertura leva uns segundos a mais, uma vez, até o cache do driver guardar. Quem mexer no GLSL dos extras: tempo de `sin`/`cos` em `es` e de `fract`/`floor`/ruído em `et`, com coeficientes de 2 casas, senão a animação pula na virada do tempo (`Extras.h` explica).
- **Brincadeira com o relevo de agora:** com os extras desligados o mapa de alturas é esquecido; a brincadeira nova mede o mapa no mesmo quadro do comando, e Copie, armadilhas do labirinto e bichos esperam por ele.
- **Fluidez:** grade de água em 320×240 (`-wts 320 240`; 640×480 não dava conta na GTX 750 Ti). O log mostra a cada 30 s `Desempenho: N quadros/s` e quanto a água acompanhou o tempo real (`a água andou X% do tempo real`). `AREIA_GRADE_AGUA` e `AREIA_PASSOS_AGUA` trocam a grade e os passos por quadro para testes.

## Chuva, brilho e dedo apontado

**Chuva pela mão:** só com a mão a mais de 4 cm da beirada iluminada (em 16/11/2026 a borda da caixa
era vista como mão e chovia sem parar num canto, deixando a água inteira lenta) e nunca no Mundo LAOB. Quando a
mão começa e para de chover, o log ganha `Chuva: mao comecou|parou` (o som da chuva segue essas linhas).

**Brilho da imagem:** na cor final do shader de superfície (`luzRealce`), os tons médios sobem por
1−(1−c)^(1+força) (o preto quase não sobe: os contornos continuam nítidos). Onde há bicho, pessoa, carro ou comida
(`extrasObjeto`, marcado no `Extras.cpp`) a cor fica bem mais saturada e com mais contraste e o chão fica com
saturação moderada, para cada coisa se destacar do relevo; comando `luz <força 0-1,5> [saturação 0-1]` (normal `0 0`, forte `0.5 0.3`, o padrão,
máximo `0.9 0.45`). O painel (aba Projetor) grava em `~/SARndbox-rosto/luz.txt` e o `sandbox.sh` manda ao abrir.
O brilho da lâmpada em si só muda no menu do projetor (modo ECO desligado / Brilhante).

**Dedo apontado** (`sarndbox/Gesto.*`): o HandExtractor do SARndbox só reconhece a mão aberta (é dela
a chuva), então o dedo apontado tem detector próprio sobre o quadro cru do sensor, em todo quadro (~30× por
segundo; até 3% de um núcleo com a mão na areia) e só com bichos na areia (ou gravando). A areia de referência embaixo da mão fica congelada por até 8 s (o filtro do
SARndbox engole a mão parada em ~1 s). Dois jeitos: **pico** (dedo para cima, até ~45° de inclinação; ponta alta
com tudo em volta bem mais fundo, punho abaixo, nenhum outro pico separado) e **contorno** (dedo apontando para
a areia: silhueta a 1,5 cm acima da areia, o braço vindo da beirada e uma só parte fina de 2,5 a 12 cm no rumo do
braço; a comida cai até 5 cm à frente da ponta). Confirma depois de 0,2 s vendo o dedo (pelo tempo, não pela
contagem de quadros), solta depois de 0,35 s sem ver ou quando a mão abre. **Relevo travado:** a silhueta que
chega pela beirada (o braço) vira uma máscara que o `FrameFilter` do SARndbox respeita (`FrameFilter::trava`,
até 15 s por ponto): a mão parada não vira montanha colorida nem empurra a água; um monte de areia novo não
encosta na beirada e entra no relevo como sempre (a trava só vale quando a borda da silhueta está bem acima da areia, como um braço; areia empurrada contra a beirada entra no relevo em ~1 s). Na areia a comida é a tigela de ração do painel; na água, a ração (se a água sumiu embaixo, cai na água mais perto,
até 6 cm; sem água, vira tigela). Perto do dedo não chove. Linhas `Gesto: dedo em x y (terra|agua)` no
`sandbox.log`. `testes/teste_gesto.cpp` testa com mãos sintéticas; `gestoGrava 20` grava a mão de verdade e
`testes/analisa_gesto.cpp <pasta> --png N` passa o mesmo detector quadro a quadro, com motivo de cada recusa e PNGs.

## Willbert, fotos e registros

**Willbert:** o gesto das duas mãos manda a foto já recortada para `POST /foto` (só aceita a
própria máquina). O painel lê se o Willbert está ligado com `systemctl show visao-detector` (sem sudo)
e liga/desliga com `sudo -n willbert-controle`. Fotos `willbert-*` ficam na subpasta "Fotos do Willbert". O painel responde à
própria máquina, à rede local e à Tailscale; o resto recebe 403. O endereço pedido também é conferido: IP da rede local ou da
Tailscale, `localhost`, nome da máquina ou nome `.local`/`.lan`/`.ts.net`; outro domínio recebe 403. Pedidos de mudança (POST)
só valem vindos da própria página (`Origin`/`Referer`); scripts locais sem navegador (o Willbert, `curl`) continuam funcionando.

**Fotos:** só formatos que abrem são aceitos; um que o OpenCV lê fora da lista (GIF, sem extensão) vira PNG, e o que não abre
(HEIC, arquivo quebrado) é recusado sem gravar nada. O cache em `fotos/cache` (40 últimas) apaga também as cópias de fotos
removidas da pasta (cada pasta do cache tem um `origem.txt`), a cada 10 min. A `fotos/camera.jpg` não fica guardada, e se a
foto projetada for apagada a prévia (`rosto.png`) sai junto.

**Registros:** o `sandbox.log` da vez anterior fica em `sandbox.log.1`; acima de 5 MB o vigia encolhe o `sandbox.log` sem
fechar a areia (a cópia inteira vai para `sandbox.log.1`). O vigia e o papel de parede só escrevem erros no journal. A areia
roda num escopo próprio do systemd do usuário (`areia-rosto-*.scope`): fechar uma sessão ssh ou reiniciar o painel não a derruba.
A saída de som `combinado` (HDMI + caixas) é refeita sozinha se o PulseAudio reiniciar ou se o projetor for ligado depois.

## Comandos internos

**Comandos internos** (`areia/sandbox.sh`):

```bash
./sandbox.sh rosto
./sandbox.sh cmd "relevo"
./sandbox.sh original
./sandbox.sh parar
./sandbox.sh reabrir
```

`parar` fecha a areia; `reabrir` fecha mesmo travada, abre a topografia e religa o vigia. Se outra troca estiver em
andamento, a resposta é `outra troca da areia em andamento; tente de novo`.

## Som e calibração

| Onde | O quê |
| :--- | :--- |
| `areia/som.py` | Sons: efeitos e o fundo automático (motor que mistura as camadas num `pacat`), saída `combinado` (HDMI do projetor + analógica, criada com `module-combine-sink`), volume e mudo em `estado_som.json`; `python som.py cena` mostra o que tocaria agora. Os `.wav` saem de `areia/sons/gerar_sons.py` |
| `/root/etc/SARndbox-2.8` | Calibração real do Kinect e do projetor (`SARNDBOX_CALIB` troca a pasta; `testes/dados/` guarda a do laob, que só vale para a mesma caixa, Kinect e projetor na mesma posição) |

## Testes e medidas

O `medir.sh` passa por cada tema e brincadeira e grava em `~/medidas/<data>/medidas.tsv` o tempo da troca,
quadros/s, tempo de quadro (p50/p95/p99/máx), água em % do tempo real, GPU por passada, CPU e memória. O
`prancha.sh` tira um print de cada tema × brincadeira; `python3 testes/monta_prancha.py <pasta> prancha.jpg`
junta tudo numa grade.

O teste do shader (`testes/teste_shader.cpp`) roda na GPU sem janela e sem Kinect, então pode rodar com a areia
aberta: compila as variantes do shader dos extras e compara cada uma, pixel a pixel, com o shader com tudo ligado
(`--antes` compara também com um `Extras.cpp` antigo; `--gpu` mede o tempo de GPU):

```bash
g++ -O1 -I sarndbox testes/teste_shader.cpp -o /tmp/teste_shader -lEGL -lGL && /tmp/teste_shader sarndbox/Extras.cpp
```

O dedo apontado tem `testes/teste_gesto.cpp` (mãos sintéticas) e `testes/analisa_gesto.cpp` (gravações de
`gestoGrava`); o Mundo LAOB tem `testes/teste_mundo.cpp` (todas as rotas entre os pontos da planta).
