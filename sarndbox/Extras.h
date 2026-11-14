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
	enum {MAX_AGENTES=24};
	enum Modo {NADA=0,PEIXES=1,CARROS=2,TODOS=3};
	enum {NUM_UNIFORMS=9};
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
	float nivelAgua; // peixes: lago = tudo abaixo desta altura (cm)
	unsigned int semente;
	double tempo;

	Extras(void);

	/* Ligado: o shader precisa do código extra (tesouro, lago ou agentes): */
	bool ativo(void) const
		{
		return tesouro||modo!=NADA||efeito!=SEM_EFEITO||labirinto||jogo!=SEM_JOGO;
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
	enum TipoPeca {P_NADA=0,P_NASCENTE=1,P_ALVO=2,P_CASA=3,P_PLANTA=4,P_PORTAO=5,P_LAGO=6,P_BARCO=7,P_PORTO=8,P_PEIXE=9,P_PLACAR=10};
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
	float vbx,vby; // última velocidade da água no barco
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
	bool chaoLivre(float x,float y,float raio) const; // chão firme no ponto e em volta (cabe o carrinho)
	bool sorteiaPonto(bool queAgua,float& x,float& y);
	void nasce(int i);
	};

#endif
