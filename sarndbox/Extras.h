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
#include <GL/GLObject.h>

#include "Types.h"

class Extras:public GLObject
	{
	/* A folha do desenho fica numa textura (uma por contexto OpenGL): */
	private:
	struct DataItem:public GLObject::DataItem
		{
		public:
		GLuint textura;
		unsigned int versao;
		DataItem(void);
		virtual ~DataItem(void);
		};
	
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
	float nivelAgua; // peixes: lago = tudo abaixo desta altura (cm)
	unsigned int semente;
	
	/* Desenho na areia: folha branca; o dedo encostando (ou quase) na areia pinta: */
	enum {DW=160,DH=120}; // células da folha (~0,37 cm cada na área iluminada do lab2)
	bool desenho;
	unsigned char tela[DH][DW][4]; // RGBA; alfa 0 = papel
	unsigned int telaVersao;
	float corCaneta[3];
	bool arcoIris,borracha;
	int raioCaneta; // em células
	std::vector<unsigned short> toques; // pontos do dedo por célula no quadro atual
	void limpaDesenho(void);
	bool escolheCor(const char* nome); // preto, azul, vermelho, verde, laranja, roxo, rosa, amarelo, arco-iris, borracha
	void comecaToque(void);
	void toque(float x,float y,float z); // ponto da imagem crua do Kinect, já no espaço da caixa (cm)
	int fechaToque(void); // pinta onde o dedo encostou; devolve quantas células pintou
	double tempo;

	Extras(void);

	/* Ligado: o shader precisa do código extra (tesouro, lago ou agentes): */
	bool ativo(void) const
		{
		return tesouro||modo!=NADA||efeito!=SEM_EFEITO||desenho;
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
	static int efeitoDoTema(const char* nome); // vulcao -> LAVA, inverno -> NEVE...; -1 se não existe
	void passo(float dt);

	/* Shader: código a acrescentar e envio dos uniforms (na ordem de localizações): */
	static const char* glslVertexUniforms;
	static const char* glslVertexMain;
	static const char* glslFragmentUniforms;
	static const char* glslFragmentMain;
	static const char* nomesUniforms[NUM_UNIFORMS];
	void uploadUniforms(const GLint locations[NUM_UNIFORMS]) const;
	virtual void initContext(GLContextData& contextData) const;
	void bindDesenho(GLContextData& contextData) const; // liga a textura da folha (envia se mudou)

	private:
	float aleatorio(void); // 0..1
	bool agua(float x,float y) const;
	bool dentro(float x,float y,float margem) const;
	bool sorteiaPonto(bool queAgua,float& x,float& y);
	void nasce(int i);
	};

#endif
