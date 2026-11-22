/***********************************************************************
Gesto - o dedo apontado visto de cima pelo Kinect (Projetor-LAOB,
18/11/2026; refeito em 19/11/2026 depois de o dono apontar sobre a areia
de verdade e nada acontecer). Quem aponta derruba comida para os bichos
(Sandbox.cpp, patch_sandbox.py). O HandExtractor do SARndbox só acha a mão
ABERTA (4 pontas e 3 vãos no contorno: é dela que vem a chuva); o dedo
apontado nunca vira mão para ele, por isso o detector daqui é outro.
Vale apontar de dois jeitos:
  A) PICO: indicador para CIMA (a ponta vai para o sensor). Busca grossa
     numa grade de 4 px (máximos com quase tudo a 3 cm em volta bem mais
     fundo; até 2 dos 8 pontos podem estar altos: é o próprio dedo
     inclinado) e confirmação em resolução cheia: pico estreito (largura
     e comprimento pelos momentos, aguenta dedo inclinado até ~45 graus),
     punho bem abaixo no anel de 2,5 a 6 cm (a fatia do anel por onde o
     dedo inclinado desce não conta como "alto"), nenhum outro pico
     separado (dois dedos, mão aberta).
  B) CONTORNO: indicador para BAIXO ou para a frente (para a areia). Na
     silhueta da mão (o que está 1,5 cm acima da areia), uma "abertura"
     (tira o que tem menos de 3,2 cm de largura) separa o corpo (punho,
     braço) das partes finas; o dedo é UMA parte fina comprida (>= 2,5 cm)
     saindo do corpo na direção do braço para a frente. Duas ou mais
     partes finas compridas = mão aberta ou dois dedos. O braço tem de
     chegar pela beirada do retângulo do detector (monte de areia com uma
     crista tem corpo e parte fina, mas fica solto no meio da caixa) e ser
     alto ali como um braço (bracoAlto: muro de areia saindo da beirada não
     conta); sem ele, só com a ponta bem acima da areia em volta (semBracoAlto).
A areia de referência é a do quadro filtrado do SARndbox, mas CONGELADA
embaixo da mão (até 8 s): parado, o filtro aceita a mão como areia em ~1 s
e a mão sumiria. Para o pico ainda vale a mediana da areia em volta.
Pixels sem medida (0 ou 0x7ff no Kinect v1: borda do dedo, sombra do
infravermelho, superfície inclinada) são ignorados, nunca reprovam.
Tempo (Rastro): visto por 0,2 s (pelo menos 3 acertos) confirma, com limiares
mais folgados perto do dedo já visto (histerese); 0,35 s sem ver solta. Desde
22/11/2026 a procura roda em todo quadro do sensor (~30 Hz; antes 12 Hz e 4
acertos = 0,33 s): confirma mais rápido e com mais quadros de prova.
Relevo travado (22/11/2026, pedido do Victor: "nada piscando em volta"): a mão
que chega pela beirada (silhueta da grade grossa encostando na borda do
retângulo) vira uma máscara (Detector::trava) que o FrameFilter do SARndbox
respeita: ali o relevo não aceita a mão parada como areia (antes, em ~1 s a mão
virava montanha colorida, a água fugia dela e tudo piscava quando ela mexia).
Um monte de areia novo não encosta na borda e entra no relevo como sempre; a
trava de cada ponto vale no máximo travaMax segundos.
Gravação (Gravador): quadro cru + filtrado + matriz + mãos para analisar
depois fora do SARndbox (testes/analisa_gesto.cpp).
Não depende do SARndbox: os testes (testes/teste_gesto.cpp) passam mãos
sintéticas por aqui.
***********************************************************************/

#ifndef GESTO_INCLUDED
#define GESTO_INCLUDED

#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>

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
	virtual bool caixa(float px,float py,float& bx,float& by) const =0; // pixel -> x, y na caixa (cm)
	};

/* O quadro do Kinect de verdade: profundidade crua (0 e >= 0x7ff = sem medida) com a correção por pixel,
   profundidade filtrada (a areia do SARndbox) e a matriz (x, y, profundidade, 1) do pixel -> caixa (cm). Os
   dados podem ser só um recorte do quadro (gravação): o pixel (x, y) fica em [(y-oy)*cw+(x-ox)]. Usado pelo
   Sandbox (Correcao = PixelCorrection do Kinect) e pela análise das gravações. Só as linhas z e w da matriz
   para a altura (2 produtos e 1 divisão por pixel). */
template<class Correcao>
class QuadroKinect:public Quadro
	{
	public:
	const std::uint16_t* cru;
	const float* filtrado;
	const Correcao* pdc;
	int ox,oy,cw,ch;
	double m[4][4];
	QuadroKinect(const double mat[4][4],const std::uint16_t* sCru,const float* sFiltrado,const Correcao* sPdc,int sOx,int sOy,int sCw,int sCh)
		:cru(sCru),filtrado(sFiltrado),pdc(sPdc),ox(sOx),oy(sOy),cw(sCw),ch(sCh)
		{
		for(int i=0;i<4;++i)
			for(int j=0;j<4;++j)
				m[i][j]=mat[i][j];
		}
	bool dentro(int x,int y) const {return x>=ox&&y>=oy&&x<ox+cw&&y<oy+ch;}
	float profCru(int x,int y) const // profundidade corrigida do quadro cru; <0 = sem medida
		{
		size_t k=size_t(y-oy)*size_t(cw)+size_t(x-ox);
		unsigned int b=cru[k];
		if(b==0||b>=0x07ffU)
			return -1.0f;
		return pdc[k].correct(float(b));
		}
	float profAreia(int x,int y) const // a do quadro filtrado; <0 = sem medida
		{
		float d=filtrado[size_t(y-oy)*size_t(cw)+size_t(x-ox)];
		return std::isfinite(d)&&d>0.0f&&d<2047.0f?d:-1.0f;
		}
	float altDe(int x,int y,float d) const
		{
		double px=x+0.5,py=y+0.5;
		return float((m[2][0]*px+m[2][1]*py+m[2][2]*d+m[2][3])/(m[3][0]*px+m[3][1]*py+m[3][2]*d+m[3][3]));
		}
	void ponto(double px,double py,double d,double& bx,double& by) const // pixel -> x, y na caixa (cm)
		{
		double ww=m[3][0]*px+m[3][1]*py+m[3][2]*d+m[3][3];
		bx=(m[0][0]*px+m[0][1]*py+m[0][2]*d+m[0][3])/ww;
		by=(m[1][0]*px+m[1][1]*py+m[1][2]*d+m[1][3])/ww;
		}
	virtual float alt(int x,int y) const
		{
		float d=profCru(x,y);
		return d<0.0f?std::nanf(""):altDe(x,y,d);
		}
	virtual float fundo(int x,int y) const
		{
		float d=profAreia(x,y);
		return d<0.0f?std::nanf(""):altDe(x,y,d);
		}
	virtual float cmPorPixel(int x,int y) const
		{
		float d=profCru(x,y);
		if(d<0.0f)
			d=profAreia(x,y);
		if(d<0.0f)
			return 0.0f;
		double x0,y0,x1,y1;
		ponto(x+0.5,y+0.5,d,x0,y0);
		ponto(x+1.5,y+0.5,d,x1,y1);
		return float(std::sqrt((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0)));
		}
	virtual bool caixa(float fx,float fy,float& bx,float& by) const
		{
		/* pela profundidade ali perto: a do quadro cru (a mão) ou, sem ela (sombra do infravermelho, a areia
		   para onde o dedo aponta), a da areia */
		int cx=int(fx+0.5f),cy=int(fy+0.5f);
		for(int pass=0;pass<2;++pass)
			for(int r=0;r<=3;++r)
				for(int y=std::max(oy,cy-r);y<=std::min(oy+ch-1,cy+r);++y)
					for(int x=std::max(ox,cx-r);x<=std::min(ox+cw-1,cx+r);++x)
						{
						float d=pass==0?profCru(x,y):profAreia(x,y);
						if(d>=0.0f)
							{
							double dx,dy;
							ponto(fx+0.5,fy+0.5,d,dx,dy);
							bx=float(dx);
							by=float(dy);
							return true;
							}
						}
		return false;
		}
	};

/* Limiares (cm, cm², frações, graus, segundos); comando "gestoAjuste nome valor" muda na hora para calibrar
   com a mão de verdade (e a análise das gravações aceita --ajuste nome=valor) */
struct Limiares
	{
	/* busca grossa e areia */
	float passo;          // grade da busca grossa (px)
	float maoMin;         // ponto da grade conta como mão (para o pico): pelo menos isso acima da areia
	float contornoMin;    // silhueta da mão (contorno, areia congelada): pelo menos isso acima da areia
	float congelaMax;     // areia congelada embaixo da mão no máximo esse tempo (s); depois aceita o filtro
	float libera;         // a mão saiu: espera isso (s) para a areia voltar a seguir o filtro
	/* A) pico (indicador para cima) */
	float pontaMin;       // na 1ª detecção a ponta fica pelo menos isso acima da areia
	float anelGrosso;     // busca grossa: 8 pontos a essa distância da ponta...
	float degrauGrosso;   // ...pelo menos isso abaixo dela...
	float grossoAltos;    // ...menos até tantos vizinhos seguidos (o dedo inclinado passa por eles)
	float picoQueda;      // pico = o que está a menos disso abaixo da ponta (a ponta do dedo)
	float picoAreaMin,picoAreaMax; // área do pico (cm²): dedo visto de cima ~2,5 a 5 (o Kinect engorda a borda)
	float picoLargMax;    // largura do pico (eixo menor)
	float picoCompMax;    // comprimento do pico (eixo maior; dedo inclinado é mais comprido)
	float altaAreaMax;    // a parte alta ligada à ponta (a menos de outroQueda abaixo): dedo tem lado de paredão, monte de areia não
	float anelDentro,anelFora; // anel em volta da ponta onde fica o punho
	float maoAnel;        // no anel, ponto de mão = pelo menos isso acima da areia (só na 1ª detecção)
	float altoMax;        // no anel, "alto" = a menos disso abaixo da ponta
	float fracAltoMax;    // fração máxima de "alto" entre os pontos de mão do anel (fora da fatia do dedo)
	float punhoAreaMin;   // área de mão no anel entre degrauMin e 15 cm abaixo da ponta
	float degrauMin,degrauMax; // mediana de quanto a mão do anel está abaixo da ponta (= dedo acima do punho)
	float outroDentro,outroQueda,outroAreaMax; // outro pico SEPARADO: a mais de outroDentro da ponta, a menos de outroQueda abaixo
	float janela;         // raio da janela de confirmação
	float candidatosMax;  // quantos picos confirmar por quadro
	/* B) contorno (indicador para baixo / para a frente) */
	float maoAreaMin;     // silhueta com menos área que isso não é mão
	float dedoRaio;       // meia largura máxima do dedo (a abertura tira o que é mais fino que 2x isso)
	float corpoAreaMin;   // o corpo (punho, braço) tem pelo menos isso
	float dedoCompMin,dedoCompMax; // quanto o dedo sai do corpo
	float dedoLargMin;    // largura média mínima (menos que isso é fiapo de borda)
	float outroDedo;      // outra parte fina com pelo menos isso (e 60% do dedo) = mão aberta / dois dedos
	float anguloMax;      // dedo no máximo isso (graus) fora da direção braço -> mão (polegar de lado não vale)
	float alvoMax;        // apontando para baixo: a comida cai até isso à frente da ponta, onde o dedo aponta
	float semBracoAlto;   // silhueta sem o braço chegando à borda (monte de areia com crista?): só vale com a ponta isso acima da areia em volta
	float bracoAlto;      // o braço na borda: mediana da silhueta ali pelo menos isso acima da areia (muro de areia saindo da beirada não conta)
	/* relevo travado embaixo da mão (Detector::trava) */
	float travaMax;       // a trava de um ponto dura no máximo isso (s) de mão em cima; depois o relevo aceita (mão esquecida, areia nova na beirada)
	/* tempo */
	float folga;          // perto do dedo já visto os limiares ficam essa vez mais folgados (histerese)
	Limiares(void);
	bool ajusta(const std::string& nome,float valor); // false se não existe
	std::string texto(void) const; // "passo=4 maoMin=3 ..."
	};

/* Medidas de um candidato (o comando "gestoDiag" e a análise das gravações imprimem) */
struct Medida
	{
	int tipo;         // 0 = pico (para cima), 1 = contorno (para baixo / para a frente)
	int cx,cy;        // candidato (px)
	float px,py;      // ponta (px)
	float ax,ay;      // onde a comida cai (px): a ponta ou, apontando para baixo, um pouco à frente
	float bx,by;      // ax, ay na caixa (cm; NaN sem medida; quem chama preenche: Detector::passo)
	float ponta;      // altura da ponta (cm acima do plano base)
	float sobreAreia; // ponta acima da areia embaixo dela (cm; NaN sem medida)
	float cmPx;       // tamanho do pixel na ponta (cm)
	/* pico */
	float picoArea,picoLarg,picoComp,altaArea; // cm², cm, cm, cm² (a parte alta ligada à ponta)
	float degrau;     // pico: mediana de quanto a mão no anel está abaixo da ponta; contorno: punho - ponta
	float punhoArea;  // pico: cm² de punho no anel; contorno: área do corpo
	float fracAlto;   // 0..1
	float outroArea;  // pico: cm² de outro pico; contorno: comprimento da 2ª parte fina (cm)
	/* contorno */
	float comp,larg,angulo; // quanto o dedo sai do corpo (cm), largura média (cm), desvio da direção do braço (graus)
	float braco;      // contorno: mediana da silhueta na borda acima da areia (cm; NaN sem silhueta na borda)
	bool rastreio;    // perto do dedo já visto (limiares folgados; o pico do rastro nem exige altura sobre a areia)
	bool dedo;
	const char* motivo; // por que não é dedo ("dedo" quando é)
	};

/* Mão aberta achada pelo HandExtractor do SARndbox (é a da chuva), na caixa (cm) */
struct MaoAberta {float x,y,z,raio;};

class Rastro;

/* Um passo inteiro como no Sandbox: procura, posição na caixa, escolha e tempo */
struct Passo
	{
	int ev;          // Rastro::Evento
	bool viu;        // algum dedo escolhido neste quadro
	float x,y;       // onde (caixa, cm)
	int escolhido;   // índice em Detector::medidas (-1 nenhum)
	bool soltouMao;  // abriu a mão em cima do dedo confirmado (solta na hora)
	};

class Detector
	{
	public:
	Limiares lim;
	bool medeTudo; // mede tudo mesmo depois de saber que não é dedo (para o "gestoDiag"); senão para no 1º motivo
	std::vector<Medida> medidas; // as da última procura do passo()
	float rastroPx,rastroPy;     // onde está o dedo seguido no quadro (px; <0 = nenhum)
	Detector(void):medeTudo(false),rastroPx(-1.0f),rastroPy(-1.0f),refP(0),refX0(0),refY0(0),refW(0),refH(0) {}
	/* Procura dedos no retângulo [x0,x1) x [y0,y1) do quadro (px), no instante t (s; para a areia congelada).
	   rastroX >= 0: onde estava o dedo já visto (px); ali procura também sem exigir altura sobre a areia e
	   com limiares folgados. Em "saida" vão todos os candidatos medidos, dedo ou não. */
	void procura(const Quadro& q,int x0,int y0,int x1,int y1,double t,float rastroX,float rastroY,std::vector<Medida>& saida);
	/* A confirmação de um pico (cx, cy), em resolução cheia */
	void classifica(const Quadro& q,int x0,int y0,int x1,int y1,int cx,int cy,bool exigeAltura,Medida& m,bool rastreio=false);
	/* O passo do Sandbox: procura, leva cada candidato para a caixa, recusa os da beirada da área iluminada
	   (area = x mín, y mín, x máx, y máx em cm; margem 4 cm, como a chuva) e os em cima de uma mão aberta,
	   escolhe um (o mais perto do rastro ou o mais alto) e passa pelo Rastro */
	void passo(const Quadro& q,int x0,int y0,int x1,int y1,double t,const float area[4],const std::vector<MaoAberta>& maos,Rastro& r,Passo& p);
	/* A areia de referência no pixel (cm acima do plano base; congelada embaixo da mão); sem procura ainda, a
	   do quadro filtrado */
	float areia(const Quadro& q,int x,int y) const;
	void esquece(void) {ref.clear();trava.clear();refW=refH=0;rastroPx=rastroPy=-1.0f;} // volta a areia ao filtro
	/* Relevo travado: na grade da areia (gradeP px a partir de gradeX0, gradeY0; gradeW x gradeH pontos), 1 = mão
	   chegando pela beirada em cima (o relevo do SARndbox não aceita). Vazia = nada travado. */
	std::vector<unsigned char> trava;
	int gradeP(void) const {return refP;}
	int gradeX0(void) const {return refX0;}
	int gradeY0(void) const {return refY0;}
	int gradeW(void) const {return refW;}
	int gradeH(void) const {return refH;}
	private:
	bool anelGrossoOk(const Quadro& q,int x0,int y0,int x1,int y1,int x,int y,float a) const;
	float areiaRapida(const Quadro& q,int x,int y) const; // o ponto da grade mais perto (laços por pixel)
	float medianaEmVolta(const Quadro& q,int x0,int y0,int x1,int y1,float px,float py,float cp);
	void contorno(const Quadro& q,int x0,int y0,int x1,int y1,int gx0,int gy0,int gx1,int gy1,const bool corte[4],float rastroX,float rastroY,std::vector<Medida>& saida);
	void atualizaAreia(const Quadro& q,int x0,int y0,int x1,int y1,double t);
	template<class Dados> void atualizaTrava(const Quadro& q,int x0,int y0,double t,size_t nComps,Dados dados); // a trava das silhuetas que vêm da beirada
	/* areia congelada, numa grade de refP px a partir de (refX0, refY0) */
	int refP,refX0,refY0,refW,refH;
	std::vector<float> ref;
	std::vector<double> refDesde,refVisto; // desde quando a mão está em cima; quando foi vista por último
	/* reaproveitados de uma chamada para outra (nada de alocar a cada quadro) */
	std::vector<float> ga,gf;          // altura e areia do filtro na grade grossa
	std::vector<unsigned char> gm,gc,oc; // ponto de mão na grade (pico; contorno; com mão do lado)
	std::vector<int> rot;              // componentes da grade
	std::vector<float> ja;             // janela: altura
	std::vector<unsigned char> marca,marca2; // janela: pixels do pico; da parte alta ligada à ponta
	std::vector<int> pilha,ordem;
	std::vector<float> dzs,vols;
	std::vector<unsigned char> fm;     // contorno: silhueta fina
	std::vector<float> fa,d1,d2,dp;    // contorno: altura, distância ao fundo e ao corpo, moldura do chanfro
	std::vector<int> frot,frot2,lista; // contorno: componentes da silhueta e das partes finas
	struct Candidato {float a; int x,y; bool exige;};
	std::vector<Candidato> cands;
	};

/* Tempo: visto no mesmo lugar por "confirma" segundos (pelo menos acertosMin vezes; falhas curtas, até 0,15 s,
   são toleradas) confirma; "solta" segundos sem ver solta. Pelo tempo e não pela contagem: vale igual a 12 ou a
   30 quadros por segundo. */
class Rastro
	{
	public:
	enum Evento {NADA=0,CONFIRMOU,CONTINUA,SOLTOU};
	bool ativo;     // dedo confirmado
	float x,y;      // posição suavizada (cm, espaço da caixa)
	int conta;      // acertos seguidos
	int acertosMin; // pelo menos tantos acertos para confirmar
	float confirma; // visto por tanto tempo (s) confirma
	float raio;     // acerto a menos disso do anterior = mesmo dedo (cm)
	float solta;    // segundos sem ver para soltar
	float suave;    // constante de tempo (s) da suavização da posição
	Rastro(void);
	Evento passo(double t,bool viu,float nx,float ny);
	void larga(void) {ativo=false;conta=0;}
	private:
	double visto,primeiro;
	};

/* Gravação para analisar a mão de verdade depois (comando "gestoGrava <s>"): numa pasta nova
   <base>/gesto-AAAAMMDD-HHMMSS/ ficam cru.u16 (quadros crus do recorte, um atrás do outro, 16 bits),
   areia.u16 (o filtrado x 32 em 16 bits; 0 = sem medida), correcao.f32 (escala e deslocamento por pixel do
   recorte, uma vez) e gesto.json (tamanhos, recorte, área do detector, matriz pixel -> caixa, área iluminada,
   limiares e, por quadro, o tempo, as mãos abertas e o que o detector achou ao vivo). Barato: só copia o
   recorte (~0,5 MB por quadro). */
class Gravador
	{
	public:
	std::string pasta;
	Gravador(void):fCru(0),fAreia(0),n(0),w(0),h(0),t0(0.0) {}
	~Gravador(void) {fecha();}
	bool gravando(void) const {return fCru!=0;}
	int quadros(void) const {return n;}
	/* recorte e area = x0, y0, x1, y1 (px); caixa = área iluminada (cm); correcao = 2 floats por pixel do recorte */
	bool abre(const std::string& base,int w,int h,const int recorte[4],const int area[4],const double m[4][4],
	          const float caixa[4],const std::vector<float>& correcao,const std::string& limiares,double t);
	void quadro(double t,const std::uint16_t* cru,const float* filtrado,const std::vector<MaoAberta>& maos,bool viu,float vx,float vy);
	int fecha(void); // escreve o gesto.json; devolve quantos quadros
	private:
	void escreveJson(bool pronta);
	std::FILE* fCru;
	std::FILE* fAreia;
	int n,w,h,rec[4],are[4];
	double mat[4][4],t0;
	float cai[4];
	std::string lims,porQuadro;
	std::vector<std::uint16_t> linha;
	};

}

#endif
