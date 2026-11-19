/***********************************************************************
Gesto - o dedo apontado para cima (indicador levantado, os outros dedos
fechados) visto de cima pelo Kinect (Projetor-LAOB, 18/11/2026).
Quem aponta derruba comida embaixo do dedo para os bichos (Sandbox.cpp,
patch_sandbox.py). O HandExtractor do SARndbox só acha a mão ABERTA (4
pontas e 3 vãos no contorno: é dela que vem a chuva); o dedo apontado
nunca vira mão para ele, por isso o detector daqui é outro:
  1) busca grossa, numa grade de 4 px da área iluminada: pontos de mão
     (acima da areia), máximos locais com tudo a 3 cm em volta bem mais
     fundo (o resto da mão aberta, do punho ou de um monte fica perto);
  2) confirmação em resolução cheia numa janela de 7 cm: pico estreito
     (o que está a menos de 1,5 cm abaixo da ponta tem a área e a largura
     de um dedo), punho bem abaixo em volta (anel de 2,5 a 6 cm), nada
     alto no anel e nenhum outro pico (dois dedos, mão aberta);
  3) tempo (Rastro): confirma com 4 acertos seguidos (~0,3 s a 12 Hz) e
     solta depois de 0,4 s sem ver.
Não depende do SARndbox: os testes (testes/teste_gesto.cpp) passam mãos
sintéticas por aqui.
***********************************************************************/

#ifndef GESTO_INCLUDED
#define GESTO_INCLUDED

#include <vector>
#include <string>

namespace Gesto {

/* O quadro de profundidade como o detector vê: altura em cm acima do plano base de cada pixel no quadro
   CRU do sensor (a mão aparece na hora) e no quadro filtrado do SARndbox (a areia: o filtro demora a aceitar
   o que se mexe). NaN = sem medida. */
class Quadro
	{
	public:
	virtual ~Quadro(void) {}
	virtual float alt(int x,int y) const =0;
	virtual float fundo(int x,int y) const =0;
	virtual float cmPorPixel(int x,int y) const =0; // tamanho do pixel nesse ponto (cm)
	};

/* Limiares (cm, cm², frações); comando "gestoAjuste nome valor" muda na hora para calibrar com a mão de verdade */
struct Limiares
	{
	float passo;          // grade da busca grossa (px)
	float maoMin;         // ponto da grade conta como mão: pelo menos isso acima da areia
	float pontaMin;       // na 1ª detecção a ponta fica pelo menos isso acima da areia
	float anelGrosso;     // busca grossa: 8 pontos a essa distância da ponta...
	float degrauGrosso;   // ...todos pelo menos isso abaixo dela
	float picoQueda;      // pico = o que está a menos disso abaixo da ponta (a ponta do dedo)
	float picoAreaMin,picoAreaMax; // área do pico (cm²): dedo visto de cima ~2,5 a 3
	float picoRaioMax;    // maior distância do centro do pico até a beira dele
	float anelDentro,anelFora; // anel em volta da ponta onde fica o punho
	float maoAnel;        // no anel, ponto de mão = pelo menos isso acima da areia (só na 1ª detecção)
	float altoMax;        // no anel, "alto" = a menos disso abaixo da ponta
	float fracAltoMax;    // fração máxima de "alto" entre os pontos de mão do anel
	float punhoAreaMin;   // área de mão no anel entre degrauMin e 15 cm abaixo da ponta
	float degrauMin,degrauMax; // mediana de quanto a mão do anel está abaixo da ponta (= dedo acima do punho)
	float outroDentro,outroQueda,outroAreaMax; // outro pico: a mais de outroDentro da ponta, a menos de outroQueda abaixo
	float janela;         // raio da janela de confirmação
	float candidatosMax;  // quantos candidatos confirmar por quadro
	Limiares(void);
	bool ajusta(const std::string& nome,float valor); // false se não existe
	std::string texto(void) const; // "passo=4 maoMin=3 ..."
	};

/* Medidas de um candidato (o comando "gestoDiag" imprime) */
struct Medida
	{
	int cx,cy;        // candidato da busca grossa (px)
	float px,py;      // ponta: centro do pico (px)
	float ponta;      // altura da ponta (cm acima do plano base)
	float sobreAreia; // ponta acima da areia embaixo dela (cm; NaN sem medida)
	float cmPx;       // tamanho do pixel na ponta (cm)
	float picoArea,picoRaio; // cm², cm
	float degrau;     // mediana: quanto a mão no anel está abaixo da ponta (cm), a altura do dedo acima do punho
	float punhoArea;  // cm²
	float fracAlto;   // 0..1
	float outroArea;  // cm²
	bool rastreio;    // veio da vaga do dedo já confirmado (sem exigir altura sobre a areia)
	bool dedo;
	const char* motivo; // por que não é dedo ("dedo" quando é)
	};

class Detector
	{
	public:
	Limiares lim;
	bool medeTudo; // mede tudo mesmo depois de saber que não é dedo (para o "gestoDiag"); senão para no 1º motivo
	Detector(void):medeTudo(false) {}
	/* Procura dedos no retângulo [x0,x1) x [y0,y1) do quadro (px). rastroX >= 0: onde estava o dedo já
	   confirmado (px); ali procura também sem exigir altura sobre a areia (o filtro do SARndbox acaba
	   aceitando a mão parada como areia). Em "saida" vão todos os candidatos medidos, dedo ou não. */
	void procura(const Quadro& q,int x0,int y0,int x1,int y1,float rastroX,float rastroY,std::vector<Medida>& saida);
	/* A confirmação de um candidato (cx, cy), em resolução cheia */
	void classifica(const Quadro& q,int x0,int y0,int x1,int y1,int cx,int cy,bool exigeAltura,Medida& m);
	private:
	bool anelGrossoOk(const Quadro& q,int x0,int y0,int x1,int y1,int x,int y,float a) const;
	/* reaproveitados de uma chamada para outra (nada de alocar a cada quadro) */
	std::vector<float> ga;             // altura na grade grossa
	std::vector<unsigned char> gm;     // ponto de mão na grade grossa
	std::vector<float> ja,jf;          // janela: altura e areia
	std::vector<unsigned char> marca;  // janela: pixels do pico
	std::vector<int> pilha,ordem;
	std::vector<float> dzs;
	struct Candidato {float a; int x,y; bool exige;};
	std::vector<Candidato> cands;
	};

/* Tempo: 4 acertos no mesmo lugar (uma falha no meio é tolerada) confirmam; 0,4 s sem ver solta */
class Rastro
	{
	public:
	enum Evento {NADA=0,CONFIRMOU,CONTINUA,SOLTOU};
	bool ativo;     // dedo confirmado
	float x,y;      // posição suavizada (cm, espaço da caixa)
	int conta;      // acertos seguidos
	int acertos;    // quantos acertos confirmam
	float raio;     // acerto a menos disso do anterior = mesmo dedo (cm)
	float solta;    // segundos sem ver para soltar
	Rastro(void);
	Evento passo(double t,bool viu,float nx,float ny);
	void larga(void) {ativo=false;conta=0;falhas=0;}
	private:
	int falhas;
	double visto;
	};

}

#endif
