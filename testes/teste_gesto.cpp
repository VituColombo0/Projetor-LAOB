/* Teste do detector do dedo apontado (sarndbox/Gesto.cpp) com mãos SINTÉTICAS, sem Kinect.
   Cada cena é montada em cm (cápsulas = dedos, palma, braço; cúpula = punho; areia com montes) e vira
   um quadro como o do Kinect visto de cima: borrão de ~2 px na borda, sombra do infravermelho ao lado do
   que está alto, ruído gaussiano de 3 mm, degraus de 1,6 mm (a resolução da profundidade) e 1% de pixels
   sem medida. Toda cena roda com 12 sementes de ruído e tem de dar a resposta certa em todas.
   g++ -O2 -Wall -Wextra -I sarndbox testes/teste_gesto.cpp sarndbox/Gesto.cpp -o /tmp/teste_gesto && /tmp/teste_gesto */

#include "Gesto.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <random>
#include <chrono>

namespace {

const float AREIA=4.8f;   // areia a 4,8 cm do plano base (mediana medida no laob)
const float BASE=98.0f;   // plano base a 98 cm do sensor
const float PI=3.14159265f;

struct Capsula {float ax,ay,az,bx,by,bz,r;};
struct Cupula {float cx,cy,a,b,ang,topo,espessura;};
struct Monte {float cx,cy,altura,largura; bool cone;};

struct Cena
	{
	std::vector<Capsula> caps;
	std::vector<Cupula> cups;
	std::vector<Monte> montes;
	bool monteNaAreia=false; // o monte já está no quadro filtrado (velho)
	bool engolida=false;     // a mão parada já virou "areia" no quadro filtrado
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
	/* borrão do Kinect (a correlação do padrão de pontos junta uns 2 px de cada lado): 3x3 duas vezes */
	B=H;
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
	std::normal_distribution<float> ruido(0.0f,0.3f);
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
			float v=B[k]+ruido(rng);
			v=std::round(v/0.16f)*0.16f; // degraus da profundidade
			q.a[k]=(sombra||u01(rng)<0.01f)?NaN:v;
			float fx=(float(x)-w/2)*cp,fy=(float(y)-h/2)*cp;
			q.f[k]=c.engolida?H[k]:areia(c,fx,fy,c.monteNaAreia);
			}
	}

struct Caso
	{
	std::string nome;
	Cena cena;
	bool dedo;        // a resposta certa
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
	std::printf("        ponta %.1f cm (%.1f acima da areia) pico %.2f cm2 raio %.2f cm degrau %.1f cm punho %.1f cm2 alto %.0f%% outro %.2f cm2 -> %s\n",
	            m.ponta,m.sobreAreia,m.picoArea,m.picoRaio,m.degrau,m.punhoArea,100.0f*m.fracAlto,m.outroArea,m.motivo);
	}

/* quantos dedos (e se algum perto da ponta certa) */
bool roda(const Caso& c,unsigned semente,bool mostra)
	{
	const int W=260,H=200;
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
	d.procura(q,2,2,W-2,H-2,rx,ry,ms);
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

}

int main(void)
	{
	std::vector<Caso> casos;
	auto novo=[&](const std::string& nome,bool dedo,float cp,bool rastro=false)->Cena&
		{
		Caso c;
		c.nome=nome;
		c.dedo=dedo;
		c.cp=cp;
		c.comRastro=rastro;
		casos.push_back(c);
		return casos.back().cena;
		};
	const float R=PI/180.0f;
	/* com o dedo apontado */
	punho(novo("punho + indicador para cima",true,0.13f),2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	punho(novo("indicador inclinado 25 graus",true,0.13f),2.0f,1.0f,0.6f,AREIA+14.0f,1,6.5f,25.0f*R,0.85f,1.0f);
	punho(novo("indicador inclinado 30 graus, de lado",true,0.13f),0.0f,0.0f,2.3f,AREIA+12.0f,1,6.5f,30.0f*R,0.85f,1.0f);
	punho(novo("mão de criança (dedo curto e fino)",true,0.14f),1.0f,0.0f,-0.8f,AREIA+10.0f,1,4.5f,0.0f,0.65f,0.75f);
	punho(novo("mão alta (ponta a 35 cm da areia)",true,0.10f),2.0f,0.0f,1.2f,AREIA+28.0f,1,6.5f,10.0f*R,0.85f,1.0f);
	punho(novo("mão baixa (punho quase na areia)",true,0.15f),2.0f,0.0f,0.0f,AREIA+5.5f,1,6.0f,0.0f,0.85f,1.0f);
	{
	Cena& c=novo("dedo parado: a mão já virou areia no filtro (com rastro)",true,0.13f,true);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	c.engolida=true;
	}
	{
	Cena& c=novo("dedo parado: a mão já virou areia, sem rastro (não confirma sozinho)",false,0.13f);
	punho(c,2.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	c.engolida=true;
	}
	{
	Cena& c=novo("indicador para cima com mão aberta de outra pessoa ao lado",true,0.13f);
	punho(c,-6.0f,-5.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	maoAberta(c,8.0f,8.0f,PI,AREIA+13.0f,0.0f,0.0f);
	}
	/* sem dedo apontado */
	novo("areia lisa",false,0.15f);
	punho(novo("punho fechado",false,0.13f),0.0f,0.0f,0.0f,AREIA+14.0f,0,0.0f,0.0f,0.85f,1.0f);
	maoAberta(novo("mão aberta chata (chuva)",false,0.13f),0.0f,0.0f,0.0f,AREIA+14.0f,0.0f,0.0f);
	maoAberta(novo("mão aberta chata, dedos juntos",false,0.13f),0.0f,0.0f,0.5f,AREIA+14.0f,0.0f,0.0f,0.2f);
	maoAberta(novo("mão aberta inclinada 35 graus (pontas para cima)",false,0.13f),-2.0f,0.0f,0.0f,AREIA+12.0f,35.0f*R,0.0f);
	maoAberta(novo("mão aberta inclinada de lado 45 graus",false,0.13f),0.0f,0.0f,0.3f,AREIA+12.0f,10.0f*R,45.0f*R);
	maoAberta(novo("mão aberta de pé (dedos para cima)",false,0.12f),-6.0f,0.0f,0.0f,AREIA+5.0f,80.0f*R,0.0f);
	maoAberta(novo("mão de pé, de lado (cutelo)",false,0.13f),0.0f,0.0f,0.0f,AREIA+12.0f,0.0f,90.0f*R,0.3f);
	punho(novo("dois dedos (V)",false,0.13f),2.0f,0.0f,0.0f,AREIA+14.0f,2,6.5f,0.0f,0.85f,1.0f,8.0f*R);
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
		for(unsigned s=1;s<=12;++s)
			certos+=roda(c,s,false);
		confere(certos==12,c.nome+(c.dedo?" -> dedo":" -> não é dedo")+" ("+std::to_string(certos)+"/12)");
		roda(c,1,true);
		}

	std::printf("== só a confirmação (sem a busca grossa), no ponto mais alto de cada cena\n");
	for(const Caso& c:casos)
		{
		int certos=0;
		Gesto::Medida m1;
		for(unsigned s=1;s<=12;++s)
			{
			const int W=260,H=200;
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
			d.classifica(q,2,2,W-2,H-2,bx,by,!c.comRastro,m);
			certos+=m.dedo==c.dedo;
			if(s==1)
				m1=m;
			}
		confere(certos==12,c.nome+(c.dedo?" -> dedo":" -> não é dedo")+" ("+std::to_string(certos)+"/12)");
		imprime(m1);
		}
	
	std::printf("== tempo (Rastro)\n");
	{
	Gesto::Rastro r;
	int confirmou=-1;
	for(int i=0;i<8;++i)
		if(r.passo(i/12.0,true,10.0f+0.3f*i,5.0f)==Gesto::Rastro::CONFIRMOU)
			confirmou=i;
	confere(confirmou==3,"4 acertos seguidos confirmam (~0,3 s a 12 por segundo)");
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
	confere(ev[4]==Gesto::Rastro::CONFIRMOU,"uma falha no meio é tolerada");
	}
	{
	/* mão aberta passando por várias sementes como uma sequência: nunca confirma */
	Gesto::Rastro r;
	bool conf=false;
	const Caso* aberta=0;
	for(const Caso& c:casos)
		if(c.nome=="mão aberta chata (chuva)")
			aberta=&c;
	for(unsigned s=1;s<=24&&aberta;++s)
		{
		QuadroSintetico q;
		monta(aberta->cena,260,200,aberta->cp,100+s,q);
		Gesto::Detector d;
		std::vector<Gesto::Medida> ms;
		d.procura(q,2,2,258,198,-1.0f,-1.0f,ms);
		bool viu=false;
		for(const Gesto::Medida& m:ms)
			viu=viu||m.dedo;
		conf=conf||r.passo(s/12.0,viu,0.0f,0.0f)==Gesto::Rastro::CONFIRMOU;
		}
	confere(aberta&&!conf,"mão aberta por 2 s: nunca vira dedo");
	}

	std::printf("== custo\n");
	{
	/* quadro do tamanho da área iluminada do laob (~368 x 260 px), com uma mão aberta e um dedo apontado */
	Cena c;
	punho(c,-10.0f,0.0f,0.0f,AREIA+14.0f,1,6.5f,0.0f,0.85f,1.0f);
	maoAberta(c,12.0f,2.0f,PI,AREIA+13.0f,0.0f,0.0f);
	QuadroSintetico q;
	monta(c,368,260,0.13f,7,q);
	Gesto::Detector d;
	std::vector<Gesto::Medida> ms;
	const int N=200;
	auto t0=std::chrono::steady_clock::now();
	for(int i=0;i<N;++i)
		d.procura(q,0,0,368,260,-1.0f,-1.0f,ms);
	double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/N;
	int dedos=0;
	for(const Gesto::Medida& m:ms)
		dedos+=m.dedo;
	std::printf("        %.0f us por procura (%zu candidatos); a 12 por segundo = %.2f%% de um núcleo\n",us,ms.size(),us*12.0/1e4);
	confere(dedos==1,"no quadro inteiro, com a mão aberta ao lado, acha só o dedo");
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
		d.procura(q,0,0,368,260,-1.0f,-1.0f,ms);
	double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/N;
	std::printf("        %.0f us por procura sem ninguém na areia; a 12 por segundo = %.2f%% de um núcleo\n",us,us*12.0/1e4);
	confere(ms.empty(),"areia sem ninguém: nenhum candidato");
	}

	std::printf(falhas?"HÁ FALHAS (%d)\n":"TUDO OK\n",falhas);
	return falhas?1:0;
	}
