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
	enum Modo {NADA=0,PEIXES=1,CARROS=2,TODOS=3,PESSOAS=4,MEMBROS=5,MUNDO=6}; // MUNDO: a cidade LAOB (Mundo.cpp) // TODOS = peixes, carrinhos e pessoas
	enum {NUM_UNIFORMS=19};
	enum {MAX_NOMES=8,MAX_LETRAS=8}; // membros do laboratório andando na areia, com o nome em cima
	/* Efeito animado de cada tema de cor: */
	enum Efeito {SEM_EFEITO=0,LAVA=1,NEVE=2,POEIRA=3,AREIA=4,ONDAS=5,BRILHO=6,CHUVA=7};
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
	   quanta água há em cada peça (casa, planta, portão...) duas vezes por segundo. */
	enum Jogo {SEM_JOGO=0,CORRIDA=1,VILA=2,FAZENDA=3,ROTA=4,LAGO=5,VULCAO=6,BARCO=7,RESGATE=8,NUM_JOGOS=9};
	enum TipoPeca {P_NADA=0,P_NASCENTE=1,P_ALVO=2,P_CASA=3,P_PLANTA=4,P_PORTAO=5,P_LAGO=6,P_BARCO=7,P_PORTO=8,P_PEIXE=9,P_PLACAR=10,P_AVISO=11,P_PERIGO=12,P_LAB=13,P_PRACA=14,P_CAFE=15,P_PARQUE=16,P_CAMPO=17,P_MERCADO=18,P_PESCA=19};
	enum {MAX_PECAS=16,MAX_FONTES=4};
	struct Peca
		{
		int tipo;
		float x,y,raio; // cm, espaço da caixa
		int time; // corrida: 0 azul, 1 vermelho
		int ordem; // rota: número do portão
		int estado; // 0 esperando/seco, 1 conseguiu (molhado, salvo, chegou), 2 perdeu (inundada, murchou...)
		float nivel; // planta/peixe: água ou ar que resta (0..1); lago: profundidade média (cm)
		float ang; // barco: direção
		};
	struct Fonte {float x,y,vazao; bool ativa;};
	int jogo,jogoFase; // fase 0 desligado, 1 jogando, 2 terminou (comemoração, rodada nova depois)
	int jogoNivel; // 1 fácil, 2 normal, 3 difícil
	double jogoInicio,jogoFim; // início da rodada e do fim (fase 2)
	float jogoDuracao; // segundos de chuva/rodada (0 = sem limite)
	int jogoPontos,jogoTotal,jogoRodadas,jogoVencedor,proximoPortao;
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
	/* Medição da GPU: para cada peça, profundidade máxima e média de água no raio (cm) e, no barco,
	   a velocidade da água (cm/s, espaço da caixa): */
	void jogoLeitura(const float* maxima,const float* media,float vx,float vy);
	bool jogoPrecisaVelocidade(void) const {return jogo==BARCO&&jogoFase==1;}
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
	bool lencol(void) const {return nivelAgua>SEM_NIVEL&&(modo==PEIXES||modo==TODOS);} // manter o lago cheio
	enum {SEM_NIVEL=-999};
	float profAgua(float x,float y) const;    // profundidade interpolada (cm); 0 = seco
	void correnteza(float x,float y,float& vx,float& vy) const; // velocidade da água no ponto (cm/s)
	int celulasX(void) const {return gw;} int celulasY(void) const {return gh;}
	float celulaCm(void) const {return celula;} const float* cantoMin(void) const {return bmin;}
	/* Chuva pelo painel: até quatro nuvens que jogam água num ponto por alguns segundos */
	enum {MAX_CHUVAS=4};
	struct Chuva {float x,y; double ate;};
	Chuva chuvas[MAX_CHUVAS]; bool temChuva;
	void chove(float fx,float fy,float segundos); // fx, fy = 0..1 dentro da área iluminada
	char nomes[MAX_NOMES][MAX_LETRAS+1]; int nNomes,giroNomes;
	bool presente[MAX_NOMES]; // o Willbert está vendo esse membro no laboratório agora
	int nFotos; // os primeiros nFotos nomes têm rosto no painel de rostos (na mesma ordem)
	enum {ATLAS_LADO=128};
	std::vector<unsigned char> atlas; // painel de rostos: MAX_NOMES quadros RGBA lado a lado
	mutable bool atlasNovo; mutable GLuint atlasTextura; // textura (criada no contexto OpenGL, no envio)
	bool carregaAtlas(const char* arquivo);
	/* Mundo LAOB (Mundo.cpp): destino e rotina de cada boneco */
	float alvoX[MAX_AGENTES],alvoY[MAX_AGENTES],mAte[MAX_AGENTES],mTroca[MAX_AGENTES];
	bool temAlvo[MAX_AGENTES],escondido[MAX_AGENTES];
	int mEstado[MAX_AGENTES],mLugar[MAX_AGENTES],mCasa[MAX_AGENTES]; // 0 no lugar / 1 a caminho; peça atual ou destino; casa
	double rostoAte[MAX_NOMES]; // até quando mostrar o rosto no balão
	float wpX[MAX_AGENTES][5],wpY[MAX_AGENTES][5]; int wpN[MAX_AGENTES],wpI[MAX_AGENTES],wpCarro[MAX_AGENTES]; // pontos de passagem
	bool noLab[MAX_AGENTES]; int mDest[MAX_AGENTES]; // dentro do laboratório (4ª tela); destino já escolhido ao sair
	float horaFixa; // hora da cidade para testes (0-24); <0 = a hora de verdade
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
	float camX,camAlvo; double camTroca; // a cidade tem 3 telas de largura: deslocamento da imagem (cm)
	bool mundoPedido,mundoPronto; // pedido antes de haver mapa; sala montada
	void mundo(const std::vector<std::string>& lista,int giro,int fotos,int moradores);
	void mundoMonta(void);
	int mundoEscolhe(int i);
	void mundoPasso(float dt);
	bool mundoRosto(const char* nome,float segundos=12.0f);
	void membros(const std::vector<std::string>& lista,int giro,int fotos=0); // uma pessoinha por nome (mantém quem já estava)
	void armadilhasDoLabirinto(void); // peças P_PERIGO entre a nascente e o alvo
	float vbx,vby; // última velocidade da água no barco
	/* Mãos sobre a areia (os bichos reagem a elas). O Sandbox compara o quadro CRU do sensor com o mapa de
	   alturas: o que está mais de 1,2 cm acima da areia é mão (sem ninguém, o ruído medido não passa de 0,5 cm). Embaixo da mão a areia de REFERÊNCIA (fundo)
	   fica congelada (a mão parada perto da areia continua mão); o mapa dos bichos é sempre o atual. Mão = mancha que entra pela beirada (o braço) ou que
	   apareceu há pouco (um monte de areia novo para de contar em 4 s). */
	enum {MAX_MAOS=4}; // a última vaga é a mão de teste (maoTeste)
	struct Mao {float x,y,r; double visto,desde;};
	Mao maos[MAX_MAOS];
	std::vector<float> fundo; // areia de referência só para achar a mão (congelada embaixo dela; os bichos usam o mapa atual)
	std::vector<float> somaM; std::vector<int> contaM; std::vector<double> congela; std::vector<float> maoSeg; std::vector<unsigned char> eraMao;
	int maosAntes; double maoFalsaAte; bool maoDiag; // maoDiag: imprime no log a diferença cru x mapa (calibração)
	bool querMaos(void) const {return mapaOk&&modo!=NADA&&modo!=MUNDO&&modo!=MEMBROS;}
	void comecaMaos(void);
	void amostraMao(float x,float y,float z);
	void fechaMaos(float dt);
	int maoPerto(float x,float y,float& borda,float idade=0.6f) const; // mão vista há menos de "idade" s mais perto; borda = cm até a beira dela; -1 = nenhuma
	void maoTeste(float fx,float fy,float segundos); // mão de mentira (testes): fx, fy = 0..1 na área iluminada
	/* Estado de cada bicho: 0 normal, 1 pousado/sentado, 2 mergulhando, 3 indo pousar/indo até a mão, 4 perseguindo */
	int acao[MAX_AGENTES],presa[MAX_AGENTES]; float acaoT[MAX_AGENTES],voo[MAX_AGENTES],escala[MAX_AGENTES]; double acaoAte[MAX_AGENTES];
 // relógios, plantas secando, ar dos peixes, barco andando
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
	bool cachorro(int i,float dt); // true = sentado do lado da mão
	bool lugarDePouso(int i,int m);
	};

#endif
