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
#include <GL/gl.h>

#include "Types.h"

class Extras
	{
	public:
	enum {MAX_AGENTES=24};
	enum Modo {NADA=0,PEIXES=1,CARROS=2,TODOS=3};
	enum {NUM_UNIFORMS=7};
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
		return tesouro||modo!=NADA||efeito!=SEM_EFEITO||labirinto;
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
	void passo(float dt);

	/* Shader: código a acrescentar e envio dos uniforms (na ordem de localizações): */
	static const char* glslVertexUniforms;
	static const char* glslVertexMain;
	static const char* glslFragmentUniforms;
	static const char* glslFragmentMain;
	static const char* nomesUniforms[NUM_UNIFORMS];
	void uploadUniforms(const GLint locations[NUM_UNIFORMS]) const;

	private:
	float aleatorio(void); // 0..1
	bool agua(float x,float y) const;
	bool dentro(float x,float y,float margem) const;
	bool sorteiaPonto(bool queAgua,float& x,float& y);
	void nasce(int i);
	};

#endif
