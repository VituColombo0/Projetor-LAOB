/* Teste do detector do dedo apontado (sarndbox/Gesto.cpp) com mãos SINTÉTICAS, sem Kinect.
   Cada cena é montada em cm (cápsulas = dedos, palma, braço; cúpula = punho; areia com montes) e vira
   um quadro como o do Kinect visto de cima: borrão de ~2 px na borda (e, em algumas cenas, a borda
   "engordada" como o Kinect faz), sombra do infravermelho ao lado do que está alto, ruído gaussiano de 3 a
   5 mm, degraus de 1,6 mm (a resolução da profundidade), 1% de pixels sem medida e, em algumas cenas,
   buracos (sem medida) nas bordas e nas superfícies inclinadas, como o Kinect v1 faz de verdade. Toda cena
   roda com 12 sementes de ruído e tem de dar a resposta certa em todas. Depois, sequências no tempo (o
   filtro do SARndbox "engolindo" a mão parada, monte de areia novo, mão aberta mexendo) pelo mesmo
   caminho do Sandbox (Detector::passo + Rastro).
   g++ -O2 -Wall -Wextra -I sarndbox testes/teste_gesto.cpp sarndbox/Gesto.cpp -o /tmp/teste_gesto && /tmp/teste_gesto
   /tmp/teste_gesto --grava DIR   escreve uma GRAVAÇÃO sintética em DIR/gesto-* (para testes/analisa_gesto.cpp) */

#include "Gesto.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <functional>

namespace {

const float AREIA=4.8f;   // areia a 4,8 cm do plano base (mediana medida no laob)
const float BASE=98.0f;   // plano base a 98 cm do sensor
const float PI=3.14159265f;
const float R=PI/180.0f;

struct Capsula {float ax,ay,az,bx,by,bz,r;};
struct Cupula {float cx,cy,a,b,ang,topo,espessura;};
struct Monte {float cx,cy,altura,largura; bool cone;};

struct Cena
	{
	std::vector<Capsula> caps;
	std::vector<Cupula> cups;
	std::vector<Monte> montes;
	bool monteNaAreia=false; // o monte já está no quadro filtrado (velho)
	float engole=0.0f;       // quanto o filtro já "engoliu" da mão parada (0 = nada, 1 = virou areia)
	float ruido=0.3f;        // desvio do ruído (cm)
	float buracos=0.0f;      // chance de pixel sem medida na borda (metade disso em superfície inclinada)
	int engorda=0;           // a borda do que está alto engordada em tantos px (o Kinect faz)
	float pontaX=0,pontaY=0; // onde está a ponta do dedo (cm), nos casos com dedo
	};

float areia(const Cena& c,float x,float y,bool comMontes)
	{
	float z=AREIA;
	if(comMontes)
		for(const Monte& m:c.montes)
			{
			float r=std::hypot(x-m.cx,y-m.cy);
			z=std::max(z,AREIA+(m.cone?std::max(0.0f,m.altura-r*m.largura):m.altura*std::exp(-r*r/(2.0f*m.largura*m.largura))));
			}
	return z;
	}

float altura(const Cena& c,float x,float y)
	{
	float z=areia(c,x,y,true);
	for(const Capsula& k:c.caps)
		{
		float lo=std::min(k.ax,k.bx)-k.r,hi=std::max(k.ax,k.bx)+k.r;
		if(x<lo||x>hi||y<std::min(k.ay,k.by)-k.r||y>std::max(k.ay,k.by)+k.r)
			continue;
		for(int i=0;i<=60;++i)
			{
			float t=float(i)/60.0f;
			float cx=k.ax+t*(k.bx-k.ax),cy=k.ay+t*(k.by-k.ay),cz=k.az+t*(k.bz-k.az);
			float d2=(x-cx)*(x-cx)+(y-cy)*(y-cy);
			if(d2<k.r*k.r)
				z=std::max(z,cz+std::sqrt(k.r*k.r-d2));
			}
		}
	for(const Cupula& u:c.cups)
		{
		float co=std::cos(u.ang),si=std::sin(u.ang);
		float dx=((x-u.cx)*co+(y-u.cy)*si)/u.a,dy=(-(x-u.cx)*si+(y-u.cy)*co)/u.b;
		float r4=dx*dx*dx*dx+dy*dy*dy*dy;
		if(r4<1.0f)
			z=std::max(z,u.topo-u.espessura+u.espessura*std::sqrt(1.0f-r4));
		}
	return z;
	}

/* ---- peças da mão (cm; ang = para onde a mão aponta no plano; o braço vem do lado oposto) */

struct Giro
	{
	float ox,oy,ang,pitch,roll,xw; // origem, giro no plano, inclinação para cima (em volta do pulso xw) e de lado
	void leva(float x,float y,float z,float& X,float& Y,float& Z) const
		{
		/* roll em volta do eixo x da mão, depois pitch em volta do pulso, depois giro no plano */
		float y1=y*std::cos(roll),z1=z+y*std::sin(roll);
		float x2=xw+(x-xw)*std::cos(pitch),z2=z1+(x-xw)*std::sin(pitch);
		X=ox+x2*std::cos(ang)-y1*std::sin(ang);
		Y=oy+x2*std::sin(ang)+y1*std::cos(ang);
		Z=z2;
		}
	};

void cap(Cena& c,const Giro& g,float ax,float ay,float az,float bx,float by,float bz,float r)
	{
	Capsula k;
	g.leva(ax,ay,az,k.ax,k.ay,k.az);
	g.leva(bx,by,bz,k.bx,k.by,k.bz);
	k.r=r;
	c.caps.push_back(k);
	}

/* punho com 0, 1 ou 2 dedos para cima: topo = altura do alto do punho (cm acima do plano base) */
void punho(Cena& c,float cx,float cy,float ang,float topo,int dedos,float dedoAcima,float inclina,float raioDedo,float e,float abre=0.0f)
	{
	Cupula u={cx,cy,4.0f*e,2.6f*e,ang,topo,4.5f*e};
	c.cups.push_back(u);
	Giro g={cx,cy,ang,0.0f,0.0f,0.0f};
	cap(c,g,-4.5f*e,0.0f,topo-3.2f*e,-40.0f,0.0f,topo-7.0f,3.0f*e);            // braço
	cap(c,g,-1.5f*e,2.2f*e,topo-1.2f,2.0f*e,1.5f*e,topo-1.2f,1.0f*e);           // polegar
	for(int d=0;d<dedos;++d)
		{
		float bx=(d==0?2.8f:0.9f)*e,bz=topo-1.5f;
		float tz=topo+dedoAcima-raioDedo;
		float lado=dedos==2?(d==0?1.0f:-1.0f)*std::tan(abre):0.0f;
		float tx=bx+(tz-bz)*(std::tan(inclina)+lado);
		cap(c,g,bx,0.0f,bz,tx,0.0f,tz,raioDedo);
		if(d==0)
			{
			c.pontaX=cx+tx*std::cos(ang);
			c.pontaY=cy+tx*std::sin(ang);
			}
		}
	}

/* punho de palma para baixo com o indicador esticado para a frente e para BAIXO (baixo = graus abaixo da
   horizontal; 0 = para a frente); topo = alto do punho; dedos = 1 ou 2 (V para baixo); o polegar dobrado */
void aponta(Cena& c,float cx,float cy,float ang,float topo,float baixo,float raioDedo,float e,float comp=7.0f,int dedos=1)
	{
	Cupula u={cx,cy,4.3f*e,3.8f*e,ang,topo,4.5f*e};
	c.cups.push_back(u);
	Giro g={cx,cy,ang,0.0f,0.0f,0.0f};
	cap(c,g,-4.5f*e,0.0f,topo-3.0f*e,-40.0f,0.0f,topo+2.0f,3.0f*e);            // braço (sobe para o ombro)
	cap(c,g,-1.0f*e,-2.6f*e,topo-2.0f*e,2.0f*e,-1.2f*e,topo-2.2f*e,1.0f*e);     // polegar dobrado por baixo
	for(int d=0;d<dedos;++d)
		{
		float lado=d==0?0.0f:-10.0f*R;
		float bx=3.6f*e,by=(d==0?2.0f:0.2f)*e,bz=topo-2.0f*e;
		float tx=bx+comp*e*std::cos(baixo)*std::cos(lado),ty=by+comp*e*std::cos(baixo)*std::sin(lado),tz=bz-comp*e*std::sin(baixo);
		cap(c,g,bx,by,bz,tx,ty,tz,raioDedo);
		if(d==0)
			{
			c.pontaX=cx+tx*std::cos(ang)-ty*std::sin(ang);
			c.pontaY=cy+tx*std::sin(ang)+ty*std::cos(ang);
			}
		}
	}

/* mão aberta: palma (4 cápsulas), 4 dedos abertos, polegar e braço; topo = alto da palma */
void maoAberta(Cena& c,float cx,float cy,float ang,float topo,float pitch,float roll,float abertura=1.0f)
	{
	Giro g={cx,cy,ang,pitch,roll,-4.5f};
	const float ys[4]={-2.7f,-0.9f,0.9f,2.7f};
	const float angs[4]={-24.0f,-8.0f,8.0f,24.0f};
	const float lens[4]={6.0f,7.6f,8.0f,7.0f};
	for(int i=0;i<4;++i)
		{
		cap(c,g,-4.0f,ys[i],topo-1.1f,3.5f,ys[i],topo-1.1f,1.1f);
		float a=angs[i]*abertura*PI/180.0f;
		cap(c,g,3.5f,ys[i],topo-0.8f,3.5f+lens[i]*std::cos(a),ys[i]+lens[i]*std::sin(a),topo-0.8f,0.8f);
		}
	float at=-70.0f*PI/180.0f;
	cap(c,g,-1.5f,-3.2f,topo-1.0f,-1.5f+6.0f*std::cos(at),-3.2f+6.0f*std::sin(at),topo-1.0f,0.9f);  // polegar
	cap(c,g,-4.5f,0.0f,topo-3.2f,-40.0f,0.0f,topo-3.2f-36.0f*std::sin(pitch*0.3f),3.0f);           // braço
	}

/* ---- areia moldada com parte fina: o "dedo de areia" que quem brinca faz sem querer */

/* bolo (monte redondo de altura h, raio ~3 cm) com uma crista saindo de lado: uma fileira de montinhos de
   0,55 h de altura e largura larg (cm), começando na beira do bolo e indo comp cm para fora; afina = a crista
   vai baixando até a ponta (ponta de monte puxada com a mão) */
void bolo(Cena& c,float cx,float cy,float h,float ang,float comp,float larg,bool afina=false)
	{
	Monte b={cx,cy,h,3.0f,false};
	c.montes.push_back(b);
	float hc=0.55f*h;
	float r0=3.0f*std::sqrt(2.0f*std::log(h/hc)); // onde o bolo baixa até a altura da crista
	for(float s=0.0f;s<=comp;s+=0.3f)
		{
		float r=r0+s;
		Monte m={cx+r*std::cos(ang),cy+r*std::sin(ang),afina?hc*(1.0f-0.6f*s/comp):hc,0.5f*larg,false};
		c.montes.push_back(m);
		}
	}

/* muro de areia saindo da beirada da caixa (de -18 cm até fim, na direção ang) que afina numa crista de 0,55 h,
   comp cm de comprido e larg cm de largura: um braço de areia chegando pela borda com um "dedo" na ponta */
void muro(Cena& c,float h,float sigma,float fim,float comp,float larg,float ang,bool afina=false)
	{
	float co=std::cos(ang),si=std::sin(ang);
	for(float s=-18.0f;s<=fim;s+=0.6f)
		{
		Monte m={s*co,s*si,h,sigma,false};
		c.montes.push_back(m);
		}
	float hc=0.55f*h;
	for(float s=0.0f;s<=comp;s+=0.3f)
		{
		float x=fim+1.5f+s;
		Monte m={x*co,x*si,afina?hc*(1.0f-0.6f*s/comp):hc,0.5f*larg,false};
		c.montes.push_back(m);
		}
	}

/* ---- quadro como o Kinect vê */

class QuadroSintetico:public Gesto::Quadro
	{
	public:
	int w,h;
	float cp;
	std::vector<float> a,f;
	float alt(int x,int y) const {return a[size_t(y)*w+x];}
	float fundo(int x,int y) const {return f[size_t(y)*w+x];}
	float cmPorPixel(int,int) const {return cp;}
	bool caixa(float px,float py,float& bx,float& by) const {bx=(px-float(w/2))*cp;by=(py-float(h/2))*cp;return true;}
	};

void monta(const Cena& c,int w,int h,float cp,unsigned semente,QuadroSintetico& q)
	{
	q.w=w;
	q.h=h;
	q.cp=cp;
	std::vector<float> H(size_t(w)*h),B(size_t(w)*h);
	for(int y=0;y<h;++y)
		for(int x=0;x<w;++x)
			H[size_t(y)*w+x]=altura(c,(float(x)-w/2)*cp,(float(y)-h/2)*cp);
	/* borda engordada: o Kinect "espalha" o que está na frente por alguns pixels */
	B=H;
	if(c.engorda>0)
		{
		int r=c.engorda;
		for(int y=0;y<h;++y)
			for(int x=0;x<w;++x)
				{
				float m=H[size_t(y)*w+x];
				for(int dy=-r;dy<=r;++dy)
					for(int dx=-r;dx<=r;++dx)
						if(dx*dx+dy*dy<=r*r&&x+dx>=0&&y+dy>=0&&x+dx<w&&y+dy<h)
							m=std::max(m,H[size_t(y+dy)*w+x+dx]);
				B[size_t(y)*w+x]=m;
				}
		}
	/* borrão do Kinect (a correlação do padrão de pontos junta uns 2 px de cada lado): 3x3 duas vezes */
	for(int pass=0;pass<2;++pass)
		{
		std::vector<float> T=B;
		for(int y=1;y<h-1;++y)
			for(int x=1;x<w-1;++x)
				{
				float s=0.0f;
				for(int dy=-1;dy<=1;++dy)
					for(int dx=-1;dx<=1;++dx)
						s+=T[size_t(y+dy)*w+x+dx];
				B[size_t(y)*w+x]=s/9.0f;
				}
		}
	std::mt19937 rng(semente);
	std::normal_distribution<float> ruido(0.0f,c.ruido);
	std::uniform_real_distribution<float> u01(0.0f,1.0f);
	q.a.assign(size_t(w)*h,0.0f);
	q.f.assign(size_t(w)*h,0.0f);
	const float NaN=std::nanf("");
	for(int y=0;y<h;++y)
		for(int x=0;x<w;++x)
			{
			size_t k=size_t(y)*w+x;
			float z=H[k];
			/* sombra do infravermelho: o projetor de pontos fica a 7,5 cm da câmera, de um lado; o que está
			   alto esconde o que fica atrás dele (s px à esquerda, enquanto s < largura da sombra) */
			bool sombra=false;
			for(int s=1;s<=30&&x-s>=0&&!sombra;++s)
				{
				float zo=H[k-s];
				if(zo>z+1.0f)
					sombra=float(s)<7.5f*575.8f*(1.0f/(BASE-zo)-1.0f/(BASE-z));
				}
			/* buracos do Kinect v1: borda (degrau de mais de 1,5 cm a 2 px) e superfície inclinada (> 45 graus) */
			bool buraco=false;
			if(c.buracos>0.0f&&x>=2&&y>=2&&x<w-2&&y<h-2)
				{
				float degrau=0.0f;
				for(int dy=-2;dy<=2;dy+=2)
					for(int dx=-2;dx<=2;dx+=2)
						degrau=std::max(degrau,std::fabs(H[k+dy*w+dx]-z));
				float incl=(std::fabs(H[k+2]-H[k-2])+std::fabs(H[k+2*w]-H[k-2*w]))/(4.0f*cp);
				float p=degrau>1.5f?c.buracos:(incl>1.0f?0.5f*c.buracos:0.0f);
				buraco=u01(rng)<p;
				}
			float v=B[k]+ruido(rng);
			v=std::round(v/0.16f)*0.16f; // degraus da profundidade
			q.a[k]=(sombra||buraco||u01(rng)<0.01f)?NaN:v;
			float fx=(float(x)-w/2)*cp,fy=(float(y)-h/2)*cp;
			float ar=areia(c,fx,fy,c.monteNaAreia);
			q.f[k]=ar+c.engole*(H[k]-ar);
			}
	}

struct Caso
	{
	std::string nome;
	Cena cena;
	bool dedo;        // a resposta certa (procura inteira)
	int pico;         // a resposta certa só da confirmação do pico no ponto mais alto (-1 = não vale: apontando para baixo)
	bool comRastro;   // procura com o rastro do dedo já confirmado (na ponta)
	float cp;
	};

int falhas=0;

void confere(bool cond,const std::string& o_que)
	{
	std::printf("%s  %s\n",cond?"OK   ":"FALHA",o_que.c_str());
	if(!cond)
		++falhas;
	}

void imprime(const Gesto::Medida& m)
	{
	if(m.tipo==0)
		std::printf("        ponta %.1f cm (%.1f acima da areia) pico %.2f cm2 %.1fx%.1f cm degrau %.1f cm punho %.1f cm2 alto %.0f%% outro %.2f cm2 -> %s\n",
		            m.ponta,m.sobreAreia,m.picoArea,m.picoLarg,m.picoComp,m.degrau,m.punhoArea,100.0f*m.fracAlto,m.outroArea,m.motivo);
	else
		std::printf("        ponta contorno (%.1f acima da areia) sai %.1f cm larg %.1f cm angulo %.0f punho-ponta %.1f cm corpo %.0f cm2 outro %.1f cm braco %.1f cm -> %s\n",
		            m.sobreAreia,m.comp,m.larg,m.angulo,m.degrau,m.punhoArea,m.outroArea,m.braco,m.motivo);
	}

const int W=260,H=200;

/* quantos dedos (e se algum perto da ponta certa) */
bool roda(const Caso& c,unsigned semente,bool mostra)
	{
	QuadroSintetico q;
	monta(c.cena,W,H,c.cp,semente,q);
	Gesto::Detector d;
	std::vector<Gesto::Medida> ms;
	float rx=-1.0f,ry=-1.0f;
	if(c.comRastro)
		{
		rx=c.cena.pontaX/c.cp+W/2;
		ry=c.cena.pontaY/c.cp+H/2;
		}
	d.procura(q,2,2,W-2,H-2,0.0,rx,ry,ms);
	int dedos=0;
	bool perto=false;
	for(const Gesto::Medida& m:ms)
		if(m.dedo)
			{
			++dedos;
			float ex=(m.px-W/2)*c.cp-c.cena.pontaX,ey=(m.py-H/2)*c.cp-c.cena.pontaY;
			perto=perto||std::hypot(ex,ey)<1.5f;
			}
	if(mostra)
		for(const Gesto::Medida& m:ms)
			imprime(m);
	return c.dedo?(dedos==1&&perto):dedos==0;
	}

/* ---- sequências no tempo, pelo caminho do Sandbox (Detector::passo + Rastro), a 12 quadros por segundo (o Sandbox
   roda a ~30 desde 22/11/2026; o Rastro e a areia congelada andam pelo tempo, então valem os dois) */

struct Resultado {int confirmou=0,soltou=0,vistos=0,travados=0; bool ativoNoFim=false; std::vector<int> quando;};

Resultado sequencia(int n,const std::function<Cena(int)>& cena,float cp,unsigned semente)
	{
	Gesto::Detector d;
	Gesto::Rastro r;
	Resultado res;
	std::vector<Gesto::MaoAberta> maos;
	for(int i=0;i<n;++i)
		{
		QuadroSintetico q;
		monta(cena(i),W,H,cp,semente*1000+unsigned(i),q);
		Gesto::Passo p;
		d.passo(q,2,2,W-2,H-2,i/12.0,0,maos,r,p);
		res.vistos+=p.viu;
		res.travados+=!d.trava.empty(); // relevo travado embaixo da mão (FrameFilter)
		if(p.ev==Gesto::Rastro::CONFIRMOU) {++res.confirmou;res.quando.push_back(i);}
		if(p.ev==Gesto::Rastro::SOLTOU) ++res.soltou;
		}
	res.ativoNoFim=r.ativo;
	return res;
	}

/* a mão para cima / para baixo de uma cena de sequência, com um tremido de ~2 mm de um quadro para o outro */
Cena dedoCima(int i,float engole)
	{
	Cena c;
	float tx=0.15f*std::sin(i*0.9f),ty=0.15f*std::cos(i*1.3f);
	punho(c,2.0f+tx,ty,0.0f,AREIA+14.0f,1,6.5f,10.0f*R,0.85f,1.0f);
	c.engole=engole;
	c.ruido=0.35f;
	c.buracos=0.2f;
	c.engorda=1;
	return c;
	}
Cena dedoBaixo(int i,float engole)
	{
	Cena c;
	float tx=0.15f*std::sin(i*0.9f),ty=0.15f*std::cos(i*1.3f);
	aponta(c,-6.0f+tx,-2.0f+ty,0.2f,AREIA+11.0f,40.0f*R,0.85f,1.0f);
	c.engole=engole;
	c.ruido=0.35f;
	c.buracos=0.2f;
	c.engorda=1;
	return c;
	}
/* o filtro do SARndbox: a mão chega no quadro "chega"; 1 s depois ele começa a aceitá-la e em mais 1 s ela virou areia */
float engoleEm(int i,int chega) {return std::max(0.0f,std::min(1.0f,float(i-chega-12)/12.0f));}

/* ---- gravação sintética (para testar a análise): o mesmo formato do comando gestoGrava */
int grava(const std::string& dir)
	{
	/* matriz afim: x, y como no quadro sintético; altura = 0,16 * profundidade - 20 (profundidade crua = (alt + 20) / 0,16) */
	const float cp=0.13f;
	double m[4][4]={{cp,0,0,-cp*(W/2+0.5)},{0,cp,0,-cp*(H/2+0.5)},{0,0,0.16,-20.0},{0,0,0,1}};
	int rec[4]={0,0,W,H},area[4]={2,2,W-2,H-2};
	float caixa[4]={-1000.0f,-1000.0f,1000.0f,1000.0f};
	std::vector<float> corr(size_t(2)*W*H);
	for(size_t i=0;i<corr.size();i+=2) {corr[i]=1.0f;corr[i+1]=0.0f;}
	Gesto::Limiares lim;
	Gesto::Gravador g;
	if(!g.abre(dir,W,H,rec,area,m,caixa,corr,lim.texto(),0.0))
		{
		std::fprintf(stderr,"não deu para gravar em %s\n",dir.c_str());
		return 1;
		}
	std::vector<std::uint16_t> cru(size_t(W)*H);
	std::vector<float> fil(size_t(W)*H);
	std::vector<Gesto::MaoAberta> maos;
	/* 0-9 areia; 10-39 dedo para cima (o filtro engole a mão); 40-51 nada; 52-81 dedo para baixo; 82-99 mão aberta; 100-105 nada */
	for(int i=0;i<106;++i)
		{
		Cena c;
		if(i>=10&&i<40) c=dedoCima(i,engoleEm(i,10));
		else if(i>=52&&i<82) c=dedoBaixo(i,engoleEm(i,52));
		else if(i>=82&&i<100) {maoAberta(c,0.3f*std::sin(i*0.7f),0.0f,0.0f,AREIA+14.0f,0.0f,0.0f);c.buracos=0.2f;}
		QuadroSintetico q;
		monta(c,W,H,cp,77u+unsigned(i),q);
		for(size_t k=0;k<cru.size();++k)
			{
			float a=q.a[k];
			cru[k]=std::isfinite(a)?std::uint16_t(std::lround((a+20.0f)/0.16f)):std::uint16_t(0x07ff);
			fil[k]=(q.f[k]+20.0f)/0.16f;
			}
		maos.clear();
		if(i>=84&&i<100)
			maos.push_back(Gesto::MaoAberta{0.0f,0.0f,AREIA+14.0f,6.0f}); // o HandExtractor vê a mão aberta
		g.quadro(i/12.0,cru.data(),fil.data(),maos,false,0.0f,0.0f);
		}
	int n=g.fecha();
	std::printf("gravação sintética: %d quadros em %s\n",n,g.pasta.c_str());
	return 0;
	}

}

int main(int argc,char** argv)
	{
	if(argc>=3&&std::strcmp(argv[1],"--grava")==0)
		return grava(argv[2]);
	std::vector<Caso> casos;
	auto novo=[&](const std::string& nome,bool dedo,float cp,bool rastro=false,int pico=-2)->Cena&
		{
		Caso c;
		c.nome=nome;
		c.dedo=dedo;
		c.pico=pico==-2?(dedo?1:0):pico;
		c.cp=cp;
		c.comRastro=rastro;
		casos.push_back(c);
		return casos.back().cena;
		};
	/* ---- com o dedo apontado para CIMA */
	punho(novo("punho + indicador para cima",true,0.13f),2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	punho(novo("indicador inclinado 25 graus",true,0.13f),2.0f,1.0f,0.6f,AREIA+14.0f,1,6.5f,25.0f*R,0.85f,1.0f);
	punho(novo("indicador inclinado 30 graus, de lado",true,0.13f),0.0f,0.0f,2.3f,AREIA+12.0f,1,6.5f,30.0f*R,0.85f,1.0f);
	punho(novo("indicador inclinado 40 graus",true,0.13f),-2.0f,0.0f,0.4f,AREIA+13.0f,1,5.1f,40.0f*R,0.85f,1.0f); // dedo de ~7,5 cm
	punho(novo("indicador inclinado 45 graus, de lado",true,0.13f),0.0f,1.0f,-2.0f,AREIA+13.0f,1,4.7f,45.0f*R,0.85f,1.0f); // dedo de ~7,5 cm
	punho(novo("mão de criança (dedo curto e fino)",true,0.14f),1.0f,0.0f,-0.8f,AREIA+10.0f,1,4.5f,0.0f,0.65f,0.75f);
	punho(novo("mão alta (ponta a 35 cm da areia)",true,0.10f),2.0f,0.0f,1.2f,AREIA+28.0f,1,6.5f,10.0f*R,0.85f,1.0f);
	punho(novo("mão baixa (punho quase na areia)",true,0.15f),2.0f,0.0f,0.0f,AREIA+5.5f,1,6.0f,0.0f,0.85f,1.0f);
	punho(novo("mão a 8 cm da areia",true,0.14f),1.0f,0.0f,0.5f,AREIA+8.0f,1,6.5f,15.0f*R,0.85f,1.0f);
	punho(novo("mão a 15 cm da areia",true,0.13f),1.0f,0.0f,-0.5f,AREIA+15.0f,1,6.5f,15.0f*R,0.85f,1.0f);
	punho(novo("mão a 25 cm da areia",true,0.11f),1.0f,0.0f,2.8f,AREIA+25.0f,1,6.5f,15.0f*R,0.85f,1.0f);
	{
	Cena& c=novo("indicador para cima, ruído de 5 mm",true,0.13f);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,10.0f*R,0.85f,1.0f);
	c.ruido=0.5f;
	}
	{
	Cena& c=novo("indicador para cima, buracos nas bordas e borda engordada",true,0.13f);
	punho(c,2.0f,0.0f,0.3f,AREIA+14.0f,1,6.5f,10.0f*R,0.85f,1.0f);
	c.buracos=0.4f;
	c.engorda=2;
	}
	{
	Cena& c=novo("inclinado 35 graus, buracos, ruído 4 mm",true,0.13f);
	punho(c,0.0f,0.0f,2.0f,AREIA+12.0f,1,6.5f,35.0f*R,0.85f,1.0f);
	c.buracos=0.4f;
	c.ruido=0.4f;
	c.engorda=1;
	}
	{
	Cena& c=novo("filtro engoliu metade da mão (a areia de referência subindo)",true,0.13f);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	c.engole=0.5f;
	}
	{
	Cena& c=novo("dedo parado: a mão já virou areia no filtro (com rastro)",true,0.13f,true);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	c.engole=1.0f;
	}
	{
	/* sem história nenhuma (o programa acabou de abrir com a mão parada ali): a busca não acha; a confirmação
	   do pico sozinha acha pela mediana da areia em volta */
	Cena& c=novo("mão parada desde antes de abrir (virou areia, sem história)",false,0.13f,false,1);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	c.engole=1.0f;
	}
	{
	Cena& c=novo("indicador para cima com mão aberta de outra pessoa ao lado",true,0.13f);
	punho(c,-6.0f,-5.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	maoAberta(c,8.0f,8.0f,PI,AREIA+13.0f,0.0f,0.0f);
	}
	/* ---- com o dedo apontado para BAIXO / para a frente (o contorno) */
	aponta(novo("apontando para baixo 45 graus",true,0.13f,false,-1),-6.0f,-2.0f,0.0f,AREIA+12.0f,45.0f*R,0.85f,1.0f);
	aponta(novo("apontando para baixo 30 graus, mão a 8 cm",true,0.13f,false,-1),-6.0f,0.0f,0.3f,AREIA+8.0f,30.0f*R,0.85f,1.0f);
	aponta(novo("apontando para baixo 60 graus, mão a 15 cm",true,0.13f,false,-1),-6.0f,0.0f,-0.3f,AREIA+15.0f,60.0f*R,0.85f,1.0f);
	aponta(novo("apontando para a frente (reto)",true,0.13f,false,-1),-7.0f,0.0f,0.0f,AREIA+10.0f,0.0f,0.85f,1.0f);
	aponta(novo("apontando para baixo, ponta quase na areia",true,0.14f,false,-1),-6.0f,0.0f,0.0f,AREIA+7.0f,45.0f*R,0.85f,1.0f);
	aponta(novo("apontando para baixo, mão a 25 cm",true,0.11f,false,-1),-8.0f,0.0f,0.5f,AREIA+25.0f,40.0f*R,0.85f,1.0f);
	aponta(novo("apontando para baixo, mão de criança",true,0.14f,false,-1),-5.0f,0.0f,0.0f,AREIA+9.0f,40.0f*R,0.65f,0.75f,5.5f);
	aponta(novo("apontando para baixo, braço do outro lado",true,0.13f,false,-1),6.0f,2.0f,2.6f,AREIA+12.0f,40.0f*R,0.85f,1.0f);
	{
	Cena& c=novo("apontando para baixo, buracos, ruído 4 mm, borda engordada",true,0.13f,false,-1);
	aponta(c,-6.0f,-2.0f,0.0f,AREIA+12.0f,40.0f*R,0.85f,1.0f);
	c.buracos=0.4f;
	c.ruido=0.4f;
	c.engorda=2;
	}
	{
	Cena& c=novo("apontando para baixo, ruído 5 mm",true,0.13f,false,-1);
	aponta(c,-6.0f,1.0f,-0.2f,AREIA+12.0f,40.0f*R,0.85f,1.0f);
	c.ruido=0.5f;
	}
	{
	Cena& c=novo("apontando para baixo, filtro engoliu metade",true,0.13f,false,-1);
	aponta(c,-6.0f,-2.0f,0.0f,AREIA+12.0f,40.0f*R,0.85f,1.0f);
	c.engole=0.5f;
	}
	/* ---- sem dedo apontado */
	novo("areia lisa",false,0.15f);
	punho(novo("punho fechado",false,0.13f),0.0f,0.0f,0.0f,AREIA+14.0f,0,0.0f,0.0f,0.85f,1.0f);
	{
	Cena& c=novo("punho fechado baixo, buracos",false,0.13f);
	punho(c,0.0f,0.0f,0.4f,AREIA+8.0f,0,0.0f,0.0f,0.85f,1.0f);
	c.buracos=0.4f;
	c.engorda=1;
	}
	{
	Cena& c=novo("punho de palma para baixo (sem dedo)",false,0.13f);
	aponta(c,-6.0f,0.0f,0.0f,AREIA+12.0f,40.0f*R,0.85f,1.0f,0.0f,0);
	}
	maoAberta(novo("mão aberta chata (chuva)",false,0.13f),0.0f,0.0f,0.0f,AREIA+14.0f,0.0f,0.0f);
	{
	Cena& c=novo("mão aberta chata, buracos, ruído 4 mm",false,0.13f);
	maoAberta(c,0.0f,0.0f,0.4f,AREIA+12.0f,0.0f,0.0f);
	c.buracos=0.4f;
	c.ruido=0.4f;
	c.engorda=1;
	}
	maoAberta(novo("mão aberta chata, dedos juntos",false,0.13f),0.0f,0.0f,0.5f,AREIA+14.0f,0.0f,0.0f,0.2f);
	maoAberta(novo("mão aberta inclinada 35 graus (pontas para cima)",false,0.13f),-2.0f,0.0f,0.0f,AREIA+12.0f,35.0f*R,0.0f);
	maoAberta(novo("mão aberta inclinada para baixo 30 graus (pontas para a areia)",false,0.13f),-4.0f,0.0f,0.2f,AREIA+14.0f,-30.0f*R,0.0f);
	maoAberta(novo("mão aberta inclinada de lado 45 graus",false,0.13f),0.0f,0.0f,0.3f,AREIA+12.0f,10.0f*R,45.0f*R);
	maoAberta(novo("mão aberta de pé (dedos para cima)",false,0.12f),-6.0f,0.0f,0.0f,AREIA+5.0f,80.0f*R,0.0f);
	maoAberta(novo("mão de pé, de lado (cutelo)",false,0.13f),0.0f,0.0f,0.0f,AREIA+12.0f,0.0f,90.0f*R,0.3f);
	punho(novo("dois dedos (V)",false,0.13f),2.0f,0.0f,0.0f,AREIA+14.0f,2,6.5f,0.0f,0.85f,1.0f,8.0f*R);
	aponta(novo("dois dedos para baixo (V)",false,0.13f,false,-1),-6.0f,0.0f,0.0f,AREIA+12.0f,35.0f*R,0.85f,1.0f,7.0f,2);
	{
	Cena& c=novo("só o braço entrando (mão fora)",false,0.13f);
	Giro g={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
	cap(c,g,2.0f,0.0f,AREIA+12.0f,-40.0f,0.0f,AREIA+16.0f,3.0f);
	}
	{
	Cena& c=novo("monte de areia novo (o filtro ainda não pegou)",false,0.15f);
	Monte m={0.0f,0.0f,10.0f,4.0f,false};
	c.montes.push_back(m);
	}
	{
	Cena& c=novo("monte de areia pontudo novo (cone de 12 cm)",false,0.15f);
	Monte m={0.0f,0.0f,12.0f,0.8f,true};
	c.montes.push_back(m);
	}
	/* monte com crista feito agora (o filtro ainda não pegou): a silhueta tem corpo e uma parte fina, como
	   o punho com o dedo, mas nada chega à borda (não há braço) */
	bolo(novo("monte de 5 cm com crista novo (o filtro ainda não pegou)",false,0.15f),0.0f,0.0f,5.0f,0.3f,5.5f,0.9f);
	bolo(novo("monte de 8 cm com crista fina novo",false,0.15f),-2.0f,1.0f,8.0f,2.0f,6.0f,0.6f);
	bolo(novo("monte de 5 cm com ponta afinando, novo",false,0.13f),1.0f,0.0f,5.0f,-1.0f,7.0f,1.2f,true);
	{
	Cena& c=novo("monte de areia velho (já no filtro)",false,0.15f);
	Monte m={0.0f,0.0f,12.0f,0.8f,true};
	c.montes.push_back(m);
	c.monteNaAreia=true;
	}
	{
	Cena& c=novo("cabeça de quem se debruça",false,0.10f);
	Giro g={0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
	cap(c,g,0.0f,0.0f,AREIA+26.0f,0.0f,0.0f,AREIA+26.0f,9.0f);
	}

	std::printf("== classificação (12 sementes de ruído por cena)\n");
	for(const Caso& c:casos)
		{
		int certos=0;
		int errada=0;
		for(unsigned s=1;s<=12;++s)
			{
			bool certo=roda(c,s,false);
			certos+=certo;
			if(!certo&&!errada)
				errada=int(s);
			}
		confere(certos==12,c.nome+(c.dedo?" -> dedo":" -> não é dedo")+" ("+std::to_string(certos)+"/12)");
		roda(c,errada?unsigned(errada):1u,true); // as medidas (da semente que errou, se errou)
		}

	std::printf("== só a confirmação do pico (sem a busca grossa), no ponto mais alto de cada cena\n");
	for(const Caso& c:casos)
		{
		if(c.pico<0)
			continue;
		int certos=0;
		Gesto::Medida m1;
		for(unsigned s=1;s<=12;++s)
			{
			QuadroSintetico q;
			monta(c.cena,W,H,c.cp,s,q);
			int bx=0,by=0;
			float ba=-1e30f;
			for(int y=4;y<H-4;++y)
				for(int x=4;x<W-4;++x)
					if(std::isfinite(q.alt(x,y))&&q.alt(x,y)>ba)
						{
						ba=q.alt(x,y);
						bx=x;
						by=y;
						}
			Gesto::Detector d;
			d.medeTudo=true;
			Gesto::Medida m;
			d.classifica(q,2,2,W-2,H-2,bx,by,!c.comRastro,m,c.comRastro);
			certos+=m.dedo==(c.pico==1);
			if(s==1)
				m1=m;
			}
		confere(certos==12,c.nome+(c.pico==1?" -> dedo":" -> não é dedo")+" ("+std::to_string(certos)+"/12)");
		imprime(m1);
		}

	std::printf("== no tempo (caminho do Sandbox: passo + rastro, 12 quadros por segundo)\n");
	{
	/* chega no quadro 12 e fica 5 s parado; o filtro engole a mão do 24 ao 36 */
	for(int tipo=0;tipo<2;++tipo)
		{
		int ok=0;
		Resultado r1;
		for(unsigned s=1;s<=4;++s)
			{
			Resultado r=sequencia(72,[&](int i){return i<12?Cena():(tipo==0?dedoCima(i,engoleEm(i,12)):dedoBaixo(i,engoleEm(i,12)));},0.13f,s);
			ok+=r.confirmou==1&&r.soltou==0&&r.ativoNoFim&&r.quando[0]<=12+8&&r.travados>=55;
			if(s==1) r1=r;
			}
		confere(ok==4,std::string(tipo==0?"dedo para cima":"dedo para baixo")+" parado 5 s com o filtro engolindo a mão: confirma 1 vez em até 0,7 s, não solta e trava o relevo ("
		        +std::to_string(ok)+"/4; confirmou no quadro "+(r1.quando.empty()?std::string("-"):std::to_string(r1.quando[0]))+", visto em "+std::to_string(r1.vistos)
		        +", relevo travado em "+std::to_string(r1.travados)+" de 60)");
		}
	}
	{
	/* aponta para cima num lugar, tira a mão, aponta para baixo em outro: 2 vezes */
	Resultado r=sequencia(84,[&](int i){return i<30?dedoCima(i,engoleEm(i,0)):(i<42?Cena():dedoBaixo(i,engoleEm(i,42)));},0.13f,5);
	confere(r.confirmou==2&&r.soltou==1,"aponta, tira a mão, aponta em outro lugar: confirma 2 vezes, solta 1 ("+std::to_string(r.confirmou)+", "+std::to_string(r.soltou)+")");
	}
	{
	/* monte de areia feito agora e que fica: o filtro aceita em 1 s; a areia congelada aceita depois de 8 s */
	Resultado r=sequencia(150,[&](int i){Cena c;if(i>=12){Monte m={0.0f,0.0f,10.0f,3.0f,false};c.montes.push_back(m);c.monteNaAreia=i>=24;}return c;},0.15f,6);
	confere(r.confirmou==0&&r.vistos==0&&r.travados==0,"monte de areia novo que fica 11 s: nunca vira dedo nem trava o relevo (visto "+std::to_string(r.vistos)+", travado "+std::to_string(r.travados)+")");
	}
	{
	/* monte com crista (ou ponta puxada de lado) feito agora e que fica: a areia congelada segura o monte
	   inteiro por 8 s; com corpo e parte fina ele parece um punho com o dedo. Alturas de 3, 5 e 8 cm */
	struct Crista {float comp,larg; bool afina;};
	const Crista cs[4]={{4.0f,0.6f,false},{5.5f,0.9f,false},{7.0f,1.2f,false},{7.0f,1.0f,true}};
	const float alturas[3]={3.0f,5.0f,8.0f};
	for(float h:alturas)
		{
		int ok=0,vistos=0,total=0;
		for(int k=0;k<4;++k)
			for(unsigned s=1;s<=2;++s)
				{
				float ang=0.4f+1.7f*float(k)+0.9f*float(s);
				Resultado r=sequencia(150,[&](int i){Cena c;if(i>=12){bolo(c,1.0f,-1.0f,h,ang,cs[k].comp,cs[k].larg,cs[k].afina);c.monteNaAreia=i>=24;}return c;},0.13f,20u+unsigned(k)*7u+s);
				ok+=r.confirmou==0;
				vistos+=r.vistos;
				++total;
				}
		confere(ok==total,"monte de "+std::to_string(int(h))+" cm com crista feito agora, fica 11 s: nunca vira dedo ("+std::to_string(ok)+"/"
		        +std::to_string(total)+", visto em "+std::to_string(vistos)+" quadros)");
		}
	}
	{
	/* muro de areia feito agora saindo da beirada e afinando numa crista: chega pela borda como um braço, mas é
	   baixo ali (bracoAlto); a areia congelada o segura por 8 s. Alturas de 5 a 8 cm (revisão de 19/11: antes
	   virava dedo em 21 de 24 cenas) */
	struct Muro {float h,sigma,fim,comp,larg; bool afina;};
	const Muro ms[6]={{5.0f,1.8f,-4.0f,5.5f,0.9f,false},{5.0f,2.0f,-3.0f,7.0f,1.0f,true},{6.0f,2.2f,-4.0f,8.0f,1.4f,false},
	                  {7.0f,2.0f,-4.0f,6.0f,1.0f,false},{8.0f,2.0f,-4.0f,6.0f,0.9f,false},{8.0f,2.5f,-5.0f,6.0f,1.2f,true}};
	int ok=0,vistos=0,total=0;
	for(int k=0;k<6;++k)
		for(unsigned s=1;s<=2;++s)
			{
			float ang=0.15f*float(s)-0.3f+(k%2?PI:0.0f); // da beirada esquerda e da direita
			Resultado r=sequencia(150,[&](int i){Cena c;if(i>=12){muro(c,ms[k].h,ms[k].sigma,ms[k].fim,ms[k].comp,ms[k].larg,ang,ms[k].afina);c.monteNaAreia=i>=24;}return c;},0.13f,100u+unsigned(k)*7u+s);
			ok+=r.confirmou==0;
			vistos+=r.vistos;
			++total;
			}
	confere(ok==total,"muro de areia de 5 a 8 cm saindo da beirada com crista, feito agora, fica 11 s: nunca vira dedo ("+std::to_string(ok)+"/"
	        +std::to_string(total)+", visto em "+std::to_string(vistos)+" quadros)");
	}
	{
	/* a trava do relevo não se alimenta sozinha (23/11/2026): muro de areia baixo empurrado contra a beirada, sem
	   mão nenhuma, com o filtro como no SARndbox (só aceita a areia nova 1 s depois de ela aparecer, e não onde
	   está travado). Antes a trava pegava o muro e o relevo ficava preso os 15 s inteiros (travaMax) */
	const float hs[3]={2.5f,3.5f,5.0f};
	int ok=0,total=0,travadosSoma=0,aceitoMax=0;
	for(int k=0;k<3;++k)
		for(unsigned s=1;s<=2;++s)
			{
			float ang=s==1?0.0f:PI; // da beirada esquerda e da direita
			Gesto::Detector d;
			Gesto::Rastro r;
			std::vector<Gesto::MaoAberta> maos;
			int ultimaTrava=-1000,aceito=-1,travados=0;
			for(int i=0;i<12*20;++i)
				{
				Cena c;
				if(i>=12)
					{
					muro(c,hs[k],2.2f,-4.0f,8.0f,1.4f,ang,false);
					if(aceito<0&&i>=24&&i-ultimaTrava>=12)
						aceito=i;
					c.monteNaAreia=aceito>=0;
					}
				QuadroSintetico q;
				monta(c,W,H,0.13f,(700u+unsigned(k)*31u+s)*1000u+unsigned(i),q);
				Gesto::Passo p;
				d.passo(q,2,2,W-2,H-2,i/12.0,0,maos,r,p);
				if(!d.trava.empty())
					{
					++travados;
					ultimaTrava=i;
					}
				}
			ok+=travados==0&&aceito>=0&&aceito<=24+2;
			travadosSoma+=travados;
			aceitoMax=std::max(aceitoMax,aceito-12);
			++total;
			}
	confere(ok==total,"muro de areia de 2,5 a 5 cm empurrado contra a beirada, sem mão: não trava o relevo, que aceita a areia em ~1 s ("
	        +std::to_string(ok)+"/"+std::to_string(total)+"; travado em "+std::to_string(travadosSoma)+" quadros; aceita em até "
	        +std::to_string(aceitoMax)+" quadros de 1/12 s)");
	}
	{
	Resultado r=sequencia(36,[&](int i){Cena c;maoAberta(c,0.4f*std::sin(i*0.7f),0.3f*std::cos(i*0.5f),0.1f*std::sin(i*0.3f),AREIA+14.0f,0.0f,0.0f);c.buracos=0.2f;return c;},0.13f,7);
	confere(r.confirmou==0,"mão aberta mexendo por 3 s: nunca vira dedo (visto "+std::to_string(r.vistos)+")");
	}
	{
	Resultado r=sequencia(36,[&](int i){Cena c;punho(c,0.3f*std::sin(i*0.7f),0.0f,0.0f,AREIA+12.0f,0,0.0f,0.0f,0.85f,1.0f);c.buracos=0.2f;c.engorda=1;return c;},0.13f,8);
	confere(r.confirmou==0,"punho fechado mexendo por 3 s: nunca vira dedo (visto "+std::to_string(r.vistos)+")");
	}

	std::printf("== tempo (Rastro)\n");
	{
	Gesto::Rastro r;
	int confirmou=-1;
	for(int i=0;i<8;++i)
		if(r.passo(i/12.0,true,10.0f+0.3f*i,5.0f)==Gesto::Rastro::CONFIRMOU)
			confirmou=i;
	confere(confirmou==3,"visto por 0,2 s confirma (4º quadro a 12 por segundo)");
	int soltou=-1;
	for(int i=8;i<20&&soltou<0;++i)
		if(r.passo(i/12.0,false,0,0)==Gesto::Rastro::SOLTOU)
			soltou=i;
	confere(soltou>=8+4&&soltou<=8+6,"sem ver por ~0,4 s solta ("+std::to_string(soltou-8+1)+" quadros)");
	}
	{
	Gesto::Rastro r;
	bool conf=false;
	for(int i=0;i<40;++i)
		conf=conf||r.passo(i/12.0,i%3==0,10.0f,5.0f)==Gesto::Rastro::CONFIRMOU;
	confere(!conf,"visto um quadro em cada três: não confirma");
	}
	{
	Gesto::Rastro r;
	bool conf=false;
	for(int i=0;i<40;++i)
		conf=conf||r.passo(i/12.0,true,(i%2)?-10.0f:10.0f,5.0f)==Gesto::Rastro::CONFIRMOU;
	confere(!conf,"pulando entre dois lugares longe: não confirma");
	}
	{
	Gesto::Rastro r;
	int ev[6];
	const bool viu[6]={true,true,false,true,true,true};
	for(int i=0;i<6;++i)
		ev[i]=r.passo(i/12.0,viu[i],0.0f,0.0f);
	confere(ev[3]==Gesto::Rastro::CONFIRMOU,"uma falha no meio é tolerada");
	}
	{
	/* a 30 quadros por segundo (o Sandbox desde 22/11/2026): confirma em 0,2 s, tolera 3 quadros perdidos,
	   solta em ~0,35 s */
	Gesto::Rastro r;
	int confirmou=-1,soltou=-1;
	for(int i=0;i<12;++i)
		if(r.passo(i/30.0,!(i>=2&&i<=4),0.2f*i,0.0f)==Gesto::Rastro::CONFIRMOU)
			confirmou=i;
	for(int i=12;i<40&&soltou<0;++i)
		if(r.passo(i/30.0,false,0,0)==Gesto::Rastro::SOLTOU)
			soltou=i;
	confere(confirmou==6&&soltou>=21&&soltou<=23,"a 30 por segundo: confirma em 0,2 s com 3 quadros perdidos, solta em ~0,35 s (confirmou no "+
	        std::to_string(confirmou)+", soltou no "+std::to_string(soltou)+")");
	}
	{
	/* a mão aberta que o SARndbox acha em cima do dedo confirmado solta na hora */
	Gesto::Detector d;
	Gesto::Rastro r;
	std::vector<Gesto::MaoAberta> nenhuma,aberta;
	bool soltou=false,confirmou=false;
	for(int i=0;i<12;++i)
		{
		Cena c=dedoCima(i,0.0f);
		QuadroSintetico q;
		monta(c,W,H,0.13f,900u+unsigned(i),q);
		if(i==8)
			aberta.push_back(Gesto::MaoAberta{c.pontaX,c.pontaY,AREIA+14.0f,6.0f});
		Gesto::Passo p;
		d.passo(q,2,2,W-2,H-2,i/12.0,0,i>=8?aberta:nenhuma,r,p);
		confirmou=confirmou||p.ev==Gesto::Rastro::CONFIRMOU;
		soltou=soltou||p.soltouMao;
		}
	confere(confirmou&&soltou&&!r.ativo,"abriu a mão (o SARndbox achou) em cima do dedo: solta na hora");
	}
	{
	/* beirada: o dedo a menos de 4 cm da borda da área iluminada não vale */
	Gesto::Detector d;
	Gesto::Rastro r;
	std::vector<Gesto::MaoAberta> nenhuma;
	Cena c=dedoCima(0,0.0f);
	QuadroSintetico q;
	monta(c,W,H,0.13f,950u,q);
	float area[4]={c.pontaX-3.0f,-100.0f,100.0f,100.0f};
	Gesto::Passo p;
	d.passo(q,2,2,W-2,H-2,0.0,area,nenhuma,r,p);
	bool beirada=false;
	for(const Gesto::Medida& m:d.medidas)
		beirada=beirada||std::string(m.motivo)=="na beirada";
	confere(!p.viu&&beirada,"dedo a 3 cm da beirada iluminada: recusado (na beirada)");
	}

	std::printf("== custo\n");
	{
	/* quadro do tamanho da área iluminada do laob (~368 x 260 px), com uma mão aberta, um dedo para cima e um para baixo */
	Cena c;
	punho(c,-10.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	maoAberta(c,12.0f,2.0f,PI,AREIA+13.0f,0.0f,0.0f);
	aponta(c,-4.0f,-11.0f,-0.4f,AREIA+11.0f,40.0f*R,0.85f,1.0f);
	QuadroSintetico q;
	monta(c,368,260,0.13f,7,q);
	Gesto::Detector d;
	std::vector<Gesto::Medida> ms;
	const int N=200;
	auto t0=std::chrono::steady_clock::now();
	for(int i=0;i<N;++i)
		d.procura(q,0,0,368,260,i/12.0,-1.0f,-1.0f,ms);
	double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/N;
	int dedos=0;
	for(const Gesto::Medida& m:ms)
		dedos+=m.dedo;
	std::printf("        %.0f us por procura (%zu candidatos); a 30 por segundo = %.2f%% de um núcleo\n",us,ms.size(),us*30.0/1e4);
	confere(dedos==2,"no quadro inteiro, com a mão aberta ao lado, acha os 2 dedos ("+std::to_string(dedos)+")");
	}
	{
	Cena c;
	QuadroSintetico q;
	monta(c,368,260,0.15f,8,q);
	Gesto::Detector d;
	std::vector<Gesto::Medida> ms;
	const int N=200;
	auto t0=std::chrono::steady_clock::now();
	for(int i=0;i<N;++i)
		d.procura(q,0,0,368,260,i/12.0,-1.0f,-1.0f,ms);
	double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/N;
	std::printf("        %.0f us por procura sem ninguém na areia; a 30 por segundo = %.2f%% de um núcleo\n",us,us*30.0/1e4);
	confere(ms.empty(),"areia sem ninguém: nenhum candidato");
	}

	std::printf(falhas?"HÁ FALHAS (%d)\n":"TUDO OK\n",falhas);
	return falhas?1:0;
	}
