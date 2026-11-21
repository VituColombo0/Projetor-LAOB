/***********************************************************************
Extras - Brincadeiras sobre o relevo do AR Sandbox (Projetor-LAOB):
  - caça ao tesouro: um baú escondido "embaixo" da areia aparece quando
    cavam até a profundidade certa (e brilha quando estão perto);
  - animais e objetos vivos: peixes nadando no lago (as partes baixas
    viram água) e desviando dos morros; carrinhos andando pelos vales.
A simulação roda na CPU sobre um mapa de alturas grosseiro (1,5 cm por
célula, atualizado 2x por segundo); o desenho é feito no shader da
superfície (SurfaceRenderer), que recebe posições e estado em uniforms.
Não existe no SARndbox original.
***********************************************************************/

#ifndef EXTRAS_INCLUDED
#define EXTRAS_INCLUDED

#include <vector>
#include <string>
#include <GL/gl.h>

#include "Types.h"

class Extras
	{
	public:
	enum {MAX_AGENTES=32};
	enum Modo {NADA=0,PEIXES=1,CARROS=2,TODOS=3,PESSOAS=4,MUNDO=6}; // TODOS = peixes, carrinhos e pessoas; MUNDO: o laboratório (Mundo.cpp); 5 era o modo membros (tirado em 18/11/2026)
	enum {NUM_UNIFORMS=20}; // cabe em heightMapShaderUniforms[48] do SurfaceRenderer (static_assert no patch_surface.py)
	enum {MAX_NOMES=8,MAX_LETRAS=8}; // membros do laboratório (Mundo), com o nome em cima
	/* Efeito animado de cada tema de cor: */
	enum Efeito {SEM_EFEITO=0,LAVA=1,NEVE=2,POEIRA=3,AREIA=4,ONDAS=5,BRILHO=6}; // (a chuva fina do clássico saiu em 13/11)
	int efeito;

	/* Área iluminada (cm, espaço da caixa) e mapa de alturas: */
	float bmin[2],bmax[2];
	float celula; // tamanho da célula do mapa (cm)
	int gw,gh;
	std::vector<float> alt; // altura (cm acima do plano base) por célula; <-900 = sem medida
	std::vector<float> soma;std::vector<int> conta; // acumuladores durante a atualização
	bool mapaOk;
	float referencia; // mediana da areia (cm)
	GLfloat caixa[16]; // espaço da câmera -> caixa (cm), coluna-major, para o shader

	/* Caça ao tesouro e fósseis: */
	bool tesouro,achado;
	int tipoTesouro; // 1 = baú, 2 = T-Rex, 3 = Triceratops, 4 = Pterodáctilo, 5 = Pegada
	float tx,ty,traio,tz; // posição (cm), raio (cm), altura que precisa ser alcançada cavando (cm)
	double quandoAchou;
	float profundidade; // cm de areia em cima do tesouro (vale também para os próximos)
	float reaparece; // segundos depois de achado até esconder outro em outro lugar (0 = não reaparece)
	int quantosAchados; // tesouros achados desde o último "tesouro"

	/* Animais e objetos: */
	int modo;
	int quantos;
	float ax[MAX_AGENTES],ay[MAX_AGENTES],ang[MAX_AGENTES],vel[MAX_AGENTES],tipo[MAX_AGENTES]; // tipo: 0 nada, 1.xx peixe, 2.xx carro (xx = cor)
	float vb[MAX_AGENTES]; // velocidade de cruzeiro (vel varia em volta dela: nadadas, rampas, curvas)
	float parado[MAX_AGENTES]; // segundos travado (carro dá meia-volta; peixe preso numa poça renasce)
	float nivelAgua; // com bichos da água: nível do mar (cm, espaço da caixa); a simulação mantém cheio tudo abaixo dele (SEM = sem lago)
	/* nível do mar: 0 auto = o maior entre o azul do mapa (0 cm) e 1,5 cm abaixo da areia mediana (lago do tamanho
	   de sempre, e o azul do mapa nunca fica seco); 1 fixo (nivelFixo); 2 só pela areia. absorcao: cm/s de água a
	   mais (acima do nível) que a areia absorve com o lago ligado */
	int modoNivel; float nivelFixo; float absorcao;
	float nivelLago(void) const {return modoNivel==1?nivelFixo:modoNivel==2?referencia-1.5f:(referencia-1.5f>0.0f?referencia-1.5f:0.0f);}
	float encharcado[MAX_AGENTES]; // segundos com água embaixo (bicho de terra) ou fora da água (bicho da água)
	unsigned int semente;
	double tempo;

	Extras(void);

	/* Ligado: o shader precisa do código extra (tesouro, lago ou agentes): */
	bool ativo(void) const
		{
		return tesouro||modo!=NADA||efeito!=SEM_EFEITO||labirinto||jogo!=SEM_JOGO||temChuva||copia;
		}

	void setArea(const double areaMin[2],const double areaMax[2],const OGTransform& cameraToBox);

	/* Atualização do mapa: comecaMapa, amostra (pontos já no espaço da caixa), fechaMapa: */
	void comecaMapa(void);
	void amostra(float x,float y,float z);
	void fechaMapa(void);
	float altura(float x,float y) const; // <-900 se não há medida ou está fora da área

	/* Caça ao tesouro: */
	bool novoTesouro(float profundidade); // false se ainda não há mapa
	bool confereTesouro(void); // true no instante em que foi achado
	bool horaDeReesconder(void) const; // achado há mais de "reaparece" segundos

	/* Animais e objetos: */
	void povoa(int novoModo,int quantidade);
	
	/* Labirinto de água: uma nascente joga água de verdade (simulação do SARndbox) e a turma cava
	   canais até a água chegar ao alvo, do outro lado. Chegou: comemora e, depois, rodada nova: */
	bool labirinto;
	int labEstado; // 1 = água correndo, 2 = chegou ao alvo
	float lsx,lsy,ltx,lty,lraio; // nascente e alvo (cm, espaço da caixa); raio do alvo (cm)
	float labVazao; // quanto de água a nascente joga
	double labInicio,labChegou; // tempo de início da rodada e da chegada
	int labRodadas; // rodadas vencidas desde o "labirinto"
	int labDificuldade; // 1 fácil (perto), 2 normal (lados opostos), 3 difícil (cantos opostos, menos água)
	bool labZerarAgua; // pedido para esvaziar a água (feito no contexto OpenGL)
	void novoLabirinto(int dificuldade);
	bool aguaNoAlvo(float nivel); // nível de água medido no alvo (cm); true no instante da chegada
	static int efeitoDoTema(const char* nome); // vulcao -> LAVA, inverno -> NEVE...; -1 se não existe

	/* Jogos de água (Jogos.cpp): como o labirinto, a água é a da simulação do SARndbox e a GPU mede
	   quanta água há em cada peça (casa, portão, bandeira...) duas vezes por segundo.
	   Os números são fixos (3, 5, 7 e 8 eram jogos tirados): vão para o log e para o shader, que separa
	   as peças por faixas de tipo (placar 9,5-10,5, aviso 10,5-11,5, armadilha >11,5). */
	enum Jogo {SEM_JOGO=0,CORRIDA=1,VILA=2,ROTA=4,VULCAO=6,NUM_JOGOS=9};
	enum TipoPeca {P_NADA=0,P_NASCENTE=1,P_ALVO=2,P_CASA=3,P_PORTAO=5,P_PLACAR=10,P_AVISO=11,P_PERIGO=12};
	enum {MAX_PECAS=16,MAX_FONTES=4};
	struct Peca
		{
		int tipo;
		float x,y,raio; // cm, espaço da caixa
		int time; // corrida: 0 azul, 1 vermelho
		int ordem; // rota: número do portão
		int estado; // 0 esperando/seco, 1 conseguiu (passou, chegou, a salvo), 2 perdeu (casa alagada)
		};
	struct Fonte {float x,y,vazao; bool ativa;};
	int jogo,jogoFase; // fase 0 desligado, 1 jogando, 2 terminou (comemoração, rodada nova depois)
	int jogoNivel; // 1 fácil, 2 normal, 3 difícil
	double jogoInicio,jogoFim; // início da rodada e do fim (fase 2)
	float jogoDuracao; // segundos de chuva/rodada (0 = sem limite)
	int jogoPontos,jogoVencedor,proximoPortao;
	bool jogoPedido; // começar assim que houver mapa de alturas
	Peca pecas[MAX_PECAS];
	int nPecas;
	Fonte fontes[MAX_FONTES];
	int nFontes;
	std::vector<std::string> mensagens; // eventos para o log ("Jogo: ..."); o Sandbox imprime e limpa
	static int jogoDoNome(const char* nome); // "vila" -> VILA; -1 se não existe
	static const char* nomeDoJogo(int j);
	void pedeJogo(int qual,int nivel); // começa já ou quando o mapa estiver pronto
	void fimDoJogo(void); // desliga o jogo (a água dele é esvaziada)
	/* Medição da GPU: para cada peça, a maior profundidade de água no raio dela (cm): */
	void jogoLeitura(const float* maxima);
	void jogoPasso(float dt);
	/* Animais marinhos: espécie = parte inteira do tipo (1 peixe, 4 tubarão, 5 tartaruga, 6 água-viva,
	   7 polvo, 8 arraia). Nadam no lago (areia baixa) e na água da chuva (aguaSim, medida pelo Sandbox). */
	enum {NUM_ESPECIES=12}; // seis marinhos, pato e barquinho (na água), carro, pessoa e cachorro (em terra), pássaro (no ar)
	static const char* nomeEspecie(int k);    // "peixe", "tubarao", ..., "carro", "pessoa"
	static int codigoEspecie(int k);          // 1, 4, 5, 6, 7, 8, 2 (carro), 3 (pessoa)
	void mar(const int quantidades[NUM_ESPECIES]);
	/* Água única (17/11/2026): a água da chuva, a do lago dos bichos e o azul do mapa são a MESMA, a da
	   simulação do SARndbox. O Sandbox lê 4x por segundo a profundidade e a correnteza em cada célula do mapa. */
	std::vector<float> aguaSim;               // profundidade da água por célula do mapa (cm)
	std::vector<float> aguaVx,aguaVy;         // correnteza por célula (cm/s, espaço da caixa)
	bool querAguaSim(void) const {return mapaOk&&(modo==PEIXES||modo==TODOS||modo==CARROS||modo==PESSOAS);}
	bool lencol(void) const {return jogo==SEM_JOGO&&!labirinto&&nivelAgua>SEM_NIVEL&&(modo==PEIXES||modo==TODOS);} // manter o lago cheio (jogos desligam o lençol)
	enum {SEM_NIVEL=-999};
	float profAgua(float x,float y) const;    // profundidade interpolada (cm); 0 = seco
	void correnteza(float x,float y,float& vx,float& vy) const; // velocidade da água no ponto (cm/s)
	int celulasX(void) const {return gw;} int celulasY(void) const {return gh;}
	float celulaCm(void) const {return celula;} const float* cantoMin(void) const {return bmin;}
	/* Chuva pelo painel: até quatro nuvens que jogam água num ponto por alguns segundos */
	enum {MAX_CHUVAS=4};
	struct Chuva {float x,y; double ate,desde;};
	float chuvaForca(int k) const; // 0..1: a nuvem chega e vai embora aos poucos (água e desenho acompanham)
	Chuva chuvas[MAX_CHUVAS]; bool temChuva;
	void chove(float fx,float fy,float segundos); // fx, fy = 0..1 dentro da área iluminada
	char nomes[MAX_NOMES][MAX_LETRAS+1]; int nNomes,giroNomes;
	bool presente[MAX_NOMES]; // o Willbert está vendo esse membro no laboratório agora
	int visual[MAX_NOMES]; // visual próprio do boneco (copiaNomes: 1 Gazziro, 2 Lucas, 3 Mario, 4 Victor; 0 = sorteado pelo nome)
	int nFotos; // os primeiros nFotos nomes têm rosto no painel de rostos (na mesma ordem)
	enum {ATLAS_LADO=128};
	std::vector<unsigned char> atlas; // painel de rostos: MAX_NOMES quadros RGBA lado a lado
	mutable bool atlasNovo; mutable GLuint atlasTextura; // textura (criada no contexto OpenGL, no envio)
	bool carregaAtlas(const char* arquivo);
	/* Mundo LAOB (Mundo.cpp, o laboratório): destino e rotina de cada boneco; alvoX/temAlvo também
	   servem ao pássaro e ao cachorro */
	float alvoX[MAX_AGENTES],alvoY[MAX_AGENTES],mAte[MAX_AGENTES],mTroca[MAX_AGENTES];
	bool temAlvo[MAX_AGENTES],escondido[MAX_AGENTES];
	int mEstado[MAX_AGENTES]; // 0 dentro da sala, 1 fora (esperando a hora de entrar), 2 saindo pela porta
	double rostoAte[MAX_NOMES]; // até quando mostrar o rosto no balão
	float wpX[MAX_AGENTES],wpY[MAX_AGENTES]; // para onde o boneco está andando (bancada ou ponto da sala)
	float horaFixa; // hora do laboratório para testes (0-24); <0 = a hora de verdade
	/* 19/11/2026: três salas (MundoPlanta.h) e rotas pelas portas, sem atravessar móveis */
	enum {MAX_ROTA=14};
	float mRota[MAX_AGENTES][MAX_ROTA][2]; int nRota[MAX_AGENTES],kRota[MAX_AGENTES]; // pontos da rota (F) e o próximo
	int mDest[MAX_AGENTES]; // destino: -1 o próprio assento, 0.. um ponto (Mundo.cpp), -2 a saída
	int mVaga[MAX_AGENTES]; // qual lugar do ponto (sofá, plateia, aula no quadro têm 3)
	float mAlvoAng[MAX_AGENTES]; // para onde fica virado ao chegar
	double mChegou[MAX_AGENTES]; // quando chegou ao destino (<0 = ainda andando)
	int mSocial[MAX_AGENTES]; // ponto escolhido pela rotina social (aula do Gazziro), -1 nenhum
	double mundoArcoAte,mundoMusicaAte,mundoTVAte; // até quando o arco, a música e a TV da forense ficam ligados
	float mundoBits; // estado para o shader (extrasMundo.x): salas ocupadas, portas, tela, scanner... (Mundo.cpp)
	/* 20/11/2026: o que cada um faz com as mãos (extrasAg2.x no laboratório: 0 andando, 1 parado, 2 trabalhando,
	   3 apontando, 4 bebendo, 5 falando, 6 mexendo em aparelho), a conversa em dupla e mais estado para o
	   shader (extrasMundo.w = 1 + 2*mundoBits2: osciloscópio, caixa de areia, cluster, geladeira) */
	float mAtiv[MAX_AGENTES];
	double mConversaAte[MAX_AGENTES]; int mConversaCom[MAX_AGENTES];
	float mundoBits2;
	float mundoAtividade(int i) const; // o código de mAtiv do boneco i, pelo destino e pela conversa
	void mundoConversa(int i); // ao chegar: puxa conversa com quem está parado perto (às vezes)
	void mundoRota(int i,float fx,float fy); // planeja a rota do boneco i até (fx, fy) (F)
	void mundoEscolhe(int i,int ponto,int vaga,const char* texto); // manda o boneco i a um ponto (-1 = assento)
	void mundoEstado(void); // monta mundoBits
	/* Copie o relevo: o alvo é a soma de até 4 morros/buracos (gaussianas: x, y, altura cm, largura cm)
	   sobre a altura da areia no começo; a nota (0-100) compara o alvo com o relevo medido, 2x por segundo */
	enum {COPIA_MAX=4,NUM_FORMAS=6};
	bool copia; int copiaForma,copiaNivel,copiaRodadas; float copiaG[COPIA_MAX][4]; float copiaRef,copiaTol,copiaNota,copiaAnunciada;
	double copiaInicio,copiaConseguiu,copiaAnuncio;
	static const char* nomeForma(int f); static int formaDoNome(const char* nome);
	void copiaNova(int forma,int nivel); // forma <0 = sorteio
	float copiaAlvo(float x,float y) const;
	void copiaMede(void); // depois de cada mapa de alturas
	void copiaPasso(void);
	/* Transição: quando a brincadeira muda, o desenho novo aparece aos poucos (0,6 s) */
	long long assinaturaAntes; double trocaTempo;
	float fatorVazao; // multiplica a água de todas as nascentes dos jogos e do labirinto (calibração)
	bool mundoPedido,mundoPronto; // pedido antes de haver mapa; sala montada
	void mundo(const std::vector<std::string>& lista,int giro,int fotos); // "NOME+" = presente agora
	void mundoMonta(void);
	void mundoPasso(float dt);
	bool mundoRosto(const char* nome,float segundos=12.0f);
	/* Vista do Mundo (20/11/2026, pedido do Victor: pelo painel escolher a área que aparece e seguir um membro):
	   uma câmera sobre a planta, com centro em F e zoom (1 = o laboratório inteiro); a troca desliza em 0,6 s.
	   O shader recebe a câmera em extrasVista e passa por ela tudo o que é do laboratório (Mundo.cpp) */
	enum {VISTA_TUDO=0,VISTA_PRINCIPAL=1,VISTA_FORENSE=2,VISTA_MINI=3,VISTA_SEGUE=4};
	int vistaModo; std::string vistaNome;  // VISTA_SEGUE: o nome de quem está sendo seguido
	float vistaC[2],vistaZ;                // câmera de agora: centro (F) e zoom
	float vistaDe[3]; double vistaDesde;   // de onde a troca partiu (centro e zoom) e quando
	float vistaSeg[2]; bool vistaSegOk,vistaFora; // posição suavizada de quem é seguido (F); ele está fora da sala
	bool mundoVista(const char* qual);     // tudo, principal, forense, mini ou o NOME de um membro; false se não existe
	void mundoVistaPasso(float dt);
	void armadilhasDoLabirinto(void); // peças P_PERIGO entre a nascente e o alvo
	/* Comida na terra (painel, comando "comida", ou dedo apontado): os pássaros pousam em volta, os cachorros sentam do lado
	   e quem passeia para um pouco para olhar; só vêm os que estão perto, no máximo COMEDORES de uma vez (19/11/2026).
	   O shader desenha um pão com migalhas que somem conforme ela acaba (extrasComida). A mão aberta NÃO é seguida (18/11: só faz chover);
	   o dedo apontado para cima derruba comida embaixo dele (jogarComidaEm, Gesto.cpp). */
	struct Comida {float x,y,r; double ate,dura;}; // centro e raio (cm); até quando fica e quanto durava
	Comida comida;
	bool querComida(void) const {return mapaOk&&modo!=NADA&&modo!=MUNDO;}
	bool comidaPerto(float x,float y,float& borda,float folga=0.6f) const; // comida na areia (ou acabada há menos de "folga" s); borda = cm até a beira dela
	/* Comida num ponto (dedo apontado para cima, Gesto.cpp, e painel; 18/11/2026): x, y em cm no espaço da caixa.
	   onde: -1 = decide pela água no ponto (aguaNoPonto), 0 = terra, 1 = água (cai na água mais perto, até 6 cm;
	   sem água por perto, secou: vira comida de terra). Devolve true se caiu na água.
	   Na terra é a mesma comida do painel (pão com migalhas): chamar de novo perto do mesmo lugar só renova,
	   o pão não muda de lugar nem recomeça. Na água caem bolinhas de ração (quantas; 0 = 5 na primeira vez
	   e 2 enquanto o dedo continua ali). As duas comidas são independentes e podem estar na areia juntas. */
	bool jogarComidaEm(float x,float y,float segundos,int onde=-1,int quantas=0);
	bool comem(bool naAgua) const // há na areia quem coma aquilo? água: peixe, tubarão, tartaruga, água-viva, polvo, arraia, pato; terra: pássaro, cachorro
		{
		for(int j=0;j<quantos;++j)
			{
			int t=int(tipo[j]);
			if(vel[j]!=0.0f&&(naAgua?(t==1||(t>=4&&t<=9)):(t==11||t==12)))
				return true;
			}
		return false;
		}
	bool aguaNoPonto(float x,float y) const {return agua(x,y);} // água funda o bastante para os bichos da água
	/* Ração na água: bolinhas que boiam, vão com a correnteza de verdade (aguaVx/Vy), se espalham devagar e
	   somem uma a uma (comidas ou no fim do tempo; se a água some embaixo, procuram água e, sem achar, somem).
	   Peixes, tartarugas, patos, polvos, arraias, águas-vivas e o tubarão (quando não está caçando) vêm
	   comer, só pela água. O shader desenha as bolinhas, o respingo ao cair e a beliscada (extrasRacao). */
	enum {MAX_RACAO=24};
	struct Racao {float x,y,vx,vy,seco; double desde,ate,mordida; int mordidas;}; // mordidas<=0: comida (só o respingo da última beliscada)
	Racao racao[MAX_RACAO];
	int nRacao; // bolinhas ainda boiando (contadas a cada passo)
	double racaoUltima; float racaoUltX,racaoUltY; // onde e quando caiu a última (o dedo parado solta menos)
	double vidaRacaoAnuncio; // "Vida: ... comendo a racao" no máximo a cada 10 s
	int jogarRacao(float x,float y,int n,float espalha,float segundos); // devolve quantas caíram
	/* Estado de cada bicho: 0 normal, 1 pousado/sentado/parado olhando, 2 mergulhando, 3 indo pousar/indo até a comida, 4 perseguindo */
	int acao[MAX_AGENTES],presa[MAX_AGENTES]; float acaoT[MAX_AGENTES],voo[MAX_AGENTES],escala[MAX_AGENTES]; double acaoAte[MAX_AGENTES];
 // relógios dos jogos e passo dos bichos
	void passo(float dt);

	/* Shader: código a acrescentar e envio dos uniforms (na ordem de localizações): */
	static const char* glslVertexUniforms;
	static const char* glslVertexMain;
	static const char* glslFragmentUniforms;
	static const char* glslFragmentMain;
	static const char* nomesUniforms[NUM_UNIFORMS];
	void uploadUniforms(const GLint locations[NUM_UNIFORMS]) const;

	private:
	void novoJogo(void);
	bool pontoLivre(float& x,float& y,float margem,float distMin,float hMin,float hMax,int tentativas=600);
	void addPeca(int tipo,float x,float y,float raio,int time=0,int ordem=0);
	void addFonte(float x,float y,float vazao);
	void anuncia(const std::string& m) {mensagens.push_back(m);}
	void terminaRodada(const std::string& resultado,int vencedor=-1);
	float aleatorio(void); // 0..1
	bool agua(float x,float y) const;
	bool dentro(float x,float y,float margem) const;
	bool aguaLarga(float x,float y,float raio) const; // água no ponto e em volta (cabe o peixe)
	bool molhado(float x,float y) const; // água ou quase (para quem anda em terra)
	bool chaoLivre(float x,float y,float raio) const; // chão firme no ponto e em volta (cabe o carrinho)
	bool sorteiaPonto(bool queAgua,float& x,float& y);
	void nasce(int i);
	void passaro(int i,float dt);
	bool cachorro(int i,float dt); // true = sentado do lado da comida
	bool lugarDePouso(int i);
	void passoRacao(float dt);
	int racaoAlcance(int i,float vista,float boca) const; // bolinha mais perto que o bicho i alcança pela água (-1 nenhuma)
	double belisca[MAX_AGENTES]; // até quando o bicho espera para beliscar de novo
	/* Comida só para quem está perto (19/11/2026): na ração, cada bicho da água tem a sua bolinha (comeRacao,
	   -1 nenhuma) e cada grupo de bolinhas recebe no máximo COMEDORES de uma vez, os mais perto; na terra, o pão
	   chama no máximo COMEDORES (pássaros e cachorros) e OLHANDO pessoas, só dentro do alcance de cada um */
	enum {COMEDORES=4,OLHANDO=3};
	int comeRacao[MAX_AGENTES];
	void escolheComedores(const int patoDaFrente[MAX_AGENTES]);
	bool vagaNaComida(int i,float alcance) const; // o bicho i (pássaro, cachorro ou pessoa) pode ir até o pão agora?
	/* Encostar sem travar (19/11/2026): cada bicho tem um corpo (raio, cm) e desvia dos outros antes de encostar;
	   encostado, escorrega pelo lado em vez de empurrar (sem tremer e sem atravessar) */
	float corpo(int i) const;
	void escorrega(int i,bool naAgua,int ignora,float& mx,float& my) const; // tira do passo (mx, my) o que entra em outro corpo
	float apertado[MAX_AGENTES]; // segundos nadando sem folga (poça pequena demais: renasce num lago de verdade)
	void copiaNomes(const std::vector<std::string>& lista,int giro,int fotos); // nomes, cores e rostos dos membros (para o mundo)
	};

#endif
