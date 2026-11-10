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

	/* Área iluminada (cm, espaço da caixa) e mapa de alturas: */
	float bmin[2],bmax[2];
	float celula; // tamanho da célula do mapa (cm)
	int gw,gh;
	std::vector<float> alt; // altura (cm acima do plano base) por célula; <-900 = sem medida
	std::vector<float> soma;std::vector<int> conta; // acumuladores durante a atualização
	bool mapaOk;
	float referencia; // mediana da areia (cm)
	GLfloat caixa[16]; // espaço da câmera -> caixa (cm), coluna-major, para o shader

	/* Caça ao tesouro: */
	bool tesouro,achado;
	float tx,ty,traio,tz; // posição (cm), raio (cm), altura que precisa ser alcançada cavando (cm)
	double quandoAchou;

	/* Animais e objetos: */
	int modo;
	int quantos;
	float ax[MAX_AGENTES],ay[MAX_AGENTES],ang[MAX_AGENTES],vel[MAX_AGENTES],tipo[MAX_AGENTES]; // tipo: 0 nada, 1.xx peixe, 2.xx carro (xx = cor)
	float nivelAgua; // peixes: lago = tudo abaixo desta altura (cm)
	unsigned int semente;
	double tempo;

	Extras(void);

	/* Ligado: o shader precisa do código extra (tesouro, lago ou agentes): */
	bool ativo(void) const
		{
		return tesouro||modo!=NADA;
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

	/* Animais e objetos: */
	void povoa(int novoModo,int quantidade);
	void passo(float dt);

	/* Shader: código a acrescentar e envio dos uniforms (na ordem de localizações): */
	static const char* glslVertexUniforms;
	static const char* glslVertexMain;
	static const char* glslFragmentUniforms;
	static const char* glslFragmentMain;
	static const char* nomesUniforms[4];
	void uploadUniforms(const GLint locations[4]) const;

	private:
	float aleatorio(void); // 0..1
	bool agua(float x,float y) const;
	bool dentro(float x,float y,float margem) const;
	bool sorteiaPonto(bool queAgua,float& x,float& y);
	void nasce(int i);
	};

#endif
