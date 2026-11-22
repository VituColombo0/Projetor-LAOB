/***********************************************************************
Gesto - o dedo apontado visto de cima (Projetor-LAOB).
Veja Gesto.h. Os números dos limiares saíram das medidas do Kinect do laob
(18/11/2026): plano base a 98 cm do sensor, areia a ~5 cm dele; a 65-80 cm
do sensor 1 pixel = 1,1-1,4 mm e 1 unidade crua de profundidade =
1,2-1,9 mm; ruído ~1 unidade numa superfície lisa e borda "borrada" (e
engordada) de 1-3 px. Um dedo de 1,5-2 cm tem 11-18 px de largura.
***********************************************************************/

#include "Gesto.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>

namespace Gesto {

namespace {
inline bool ok(float v) {return std::isfinite(v);}
const float INF=1e30f;
const float RAIZ2=1.41421356f;
}

/* ------------------------------- limiares */

Limiares::Limiares(void)
	:passo(4.0f),maoMin(3.0f),contornoMin(1.5f),congelaMax(8.0f),libera(1.5f),
	 pontaMin(5.0f),anelGrosso(3.0f),degrauGrosso(3.0f),grossoAltos(2.0f),
	 picoQueda(1.5f),picoAreaMin(0.5f),picoAreaMax(9.0f),picoLargMax(3.2f),picoCompMax(5.5f),altaAreaMax(10.0f),
	 anelDentro(2.5f),anelFora(6.0f),maoAnel(2.0f),altoMax(3.5f),fracAltoMax(0.08f),punhoAreaMin(6.0f),
	 degrauMin(3.5f),degrauMax(16.0f),outroDentro(2.4f),outroQueda(2.5f),outroAreaMax(0.4f),
	 janela(7.0f),candidatosMax(3.0f),
	 maoAreaMin(15.0f),dedoRaio(1.6f),corpoAreaMin(15.0f),dedoCompMin(2.5f),dedoCompMax(12.0f),
	 dedoLargMin(0.7f),outroDedo(2.0f),anguloMax(50.0f),alvoMax(5.0f),semBracoAlto(6.0f),bracoAlto(6.0f),
	 travaMax(15.0f),folga(1.3f)
	{
	}

namespace {
struct NomeLimiar {const char* nome; float Limiares::* campo; float minimo,maximo;};
const NomeLimiar nomesLimiares[]={
	{"passo",&Limiares::passo,2.0f,8.0f},{"maoMin",&Limiares::maoMin,0.5f,20.0f},
	{"contornoMin",&Limiares::contornoMin,0.5f,10.0f},{"congelaMax",&Limiares::congelaMax,0.0f,60.0f},
	{"libera",&Limiares::libera,0.0f,10.0f},{"pontaMin",&Limiares::pontaMin,0.0f,30.0f},
	{"anelGrosso",&Limiares::anelGrosso,1.0f,8.0f},{"degrauGrosso",&Limiares::degrauGrosso,0.5f,10.0f},
	{"grossoAltos",&Limiares::grossoAltos,0.0f,3.0f},{"picoQueda",&Limiares::picoQueda,0.3f,5.0f},
	{"picoAreaMin",&Limiares::picoAreaMin,0.0f,10.0f},{"picoAreaMax",&Limiares::picoAreaMax,1.0f,30.0f},
	{"picoLargMax",&Limiares::picoLargMax,0.5f,8.0f},{"picoCompMax",&Limiares::picoCompMax,0.5f,12.0f},{"altaAreaMax",&Limiares::altaAreaMax,1.0f,60.0f},
	{"anelDentro",&Limiares::anelDentro,0.5f,8.0f},{"anelFora",&Limiares::anelFora,1.0f,10.0f},
	{"maoAnel",&Limiares::maoAnel,0.0f,10.0f},{"altoMax",&Limiares::altoMax,0.5f,10.0f},
	{"fracAltoMax",&Limiares::fracAltoMax,0.0f,1.0f},{"punhoAreaMin",&Limiares::punhoAreaMin,0.0f,50.0f},
	{"degrauMin",&Limiares::degrauMin,0.0f,20.0f},{"degrauMax",&Limiares::degrauMax,1.0f,40.0f},
	{"outroDentro",&Limiares::outroDentro,0.5f,8.0f},{"outroQueda",&Limiares::outroQueda,0.3f,10.0f},
	{"outroAreaMax",&Limiares::outroAreaMax,0.0f,20.0f},{"janela",&Limiares::janela,3.0f,10.0f},
	{"candidatosMax",&Limiares::candidatosMax,1.0f,6.0f},
	{"maoAreaMin",&Limiares::maoAreaMin,1.0f,200.0f},{"dedoRaio",&Limiares::dedoRaio,0.6f,3.0f},
	{"corpoAreaMin",&Limiares::corpoAreaMin,1.0f,200.0f},{"dedoCompMin",&Limiares::dedoCompMin,0.5f,10.0f},
	{"dedoCompMax",&Limiares::dedoCompMax,2.0f,30.0f},{"dedoLargMin",&Limiares::dedoLargMin,0.0f,3.0f},
	{"outroDedo",&Limiares::outroDedo,0.5f,10.0f},{"anguloMax",&Limiares::anguloMax,10.0f,180.0f},
	{"alvoMax",&Limiares::alvoMax,0.0f,15.0f},{"semBracoAlto",&Limiares::semBracoAlto,0.0f,40.0f},{"bracoAlto",&Limiares::bracoAlto,0.0f,40.0f},
	{"travaMax",&Limiares::travaMax,0.0f,120.0f},
	{"folga",&Limiares::folga,1.0f,2.0f}};
}

bool Limiares::ajusta(const std::string& nome,float valor)
	{
	if(!ok(valor))
		return false;
	for(size_t i=0;i<sizeof(nomesLimiares)/sizeof(nomesLimiares[0]);++i)
		if(nome==nomesLimiares[i].nome)
			{
			/* com limites: o custo cresce com o passo fino, a janela e os candidatos */
			this->*nomesLimiares[i].campo=std::max(nomesLimiares[i].minimo,std::min(nomesLimiares[i].maximo,valor));
			return true;
			}
	return false;
	}

std::string Limiares::texto(void) const
	{
	std::string s;
	char b[64];
	for(size_t i=0;i<sizeof(nomesLimiares)/sizeof(nomesLimiares[0]);++i)
		{
		std::snprintf(b,sizeof(b),"%s%s=%g",i?" ":"",nomesLimiares[i].nome,double(this->*nomesLimiares[i].campo));
		s+=b;
		}
	return s;
	}

/* ------------------------------- areia de referência (congelada embaixo da mão) */

template<class Dados>
void Detector::atualizaTrava(const Quadro& q,int x0,int y0,double t,size_t nComps,Dados dados)
	{
	/* Relevo travado (22/11/2026): as silhuetas que encostam na borda do retângulo (o braço entra por ali) e
	   têm tamanho de mão viram a trava, com 2 pontos de folga em volta (a borda da mão é borrada e fica
	   abaixo do contornoMin); cada ponto só fica travado até travaMax segundos de mão em cima */
	int P=refP,gw=refW,gh=refH;
	size_t n=size_t(gw)*size_t(gh);
	trava.assign(n,0);
	bool alguma=false;
	for(size_t i=0;i<nComps;++i)
		{
		int id,nb,sx,sy,cn;
		dados(i,id,nb,sx,sy,cn);
		if(nb<=0)
			continue;
		float cp=q.cmPorPixel(x0+sx*P,y0+sy*P);
		if(!(cp>0.0f)||float(cn)*float(P*P)*cp*cp<0.5f*lim.maoAreaMin)
			continue;
		for(size_t k=0;k<n;++k)
			if(rot[k]==id&&refDesde[k]>=0.0&&t-refDesde[k]<double(lim.travaMax))
				{
				trava[k]=1;
				alguma=true;
				}
		}
	if(!alguma)
		{
		trava.clear();
		return;
		}
	/* folga de 2 pontos (linha e depois coluna) */
	for(int volta=0;volta<2;++volta)
		{
		oc.assign(n,0);
		for(int gy=0;gy<gh;++gy)
			for(int gx=0;gx<gw;++gx)
				{
				size_t k=size_t(gy)*gw+gx;
				oc[k]=trava[k]|(gx>0?trava[k-1]:0)|(gx<gw-1?trava[k+1]:0);
				}
		for(int gy=0;gy<gh;++gy)
			for(int gx=0;gx<gw;++gx)
				{
				size_t k=size_t(gy)*gw+gx;
				trava[k]=oc[k]|(gy>0?oc[k-gw]:0)|(gy<gh-1?oc[k+gw]:0);
				}
		}
	}

void Detector::atualizaAreia(const Quadro& q,int x0,int y0,int x1,int y1,double t)
	{
	/* Cada ponto da grade guarda a areia do quadro filtrado enquanto não há mão em cima. Com mão em cima
	   (ou do lado: 1 ponto de folga, a borda é borrada) fica congelada: o filtro do SARndbox aceita a mão
	   parada como areia em ~1 s (30 quadros com pouca variação) e o dedo sumiria. Mais de congelaMax
	   segundos "com mão" é areia nova (um monte feito agora) ou uma mão esquecida: aceita o filtro. */
	int P=std::max(2,int(lim.passo+0.5f));
	int gw=(x1-x0+P-1)/P,gh=(y1-y0+P-1)/P;
	size_t n=size_t(gw)*size_t(gh);
	if(refP!=P||refX0!=x0||refY0!=y0||refW!=gw||refH!=gh||ref.size()!=n)
		{
		refP=P;
		refX0=x0;
		refY0=y0;
		refW=gw;
		refH=gh;
		ref.assign(n,std::nanf(""));
		refDesde.assign(n,-1.0);
		refVisto.assign(n,-1e9);
		}
	ga.resize(n);
	gf.resize(n);
	gm.resize(n);
	gc.resize(n);
	for(int gy=0;gy<gh;++gy)
		for(int gx=0;gx<gw;++gx)
			{
			int x=x0+gx*P,y=y0+gy*P;
			size_t k=size_t(gy)*gw+gx;
			float a=q.alt(x,y),f=q.fundo(x,y);
			if(!ok(ref[k])&&ok(f))
				ref[k]=f;
			float r=ref[k];
			ga[k]=a;
			gf[k]=f;
			bool tem=ok(a)&&ok(r);
			gm[k]=tem&&a-r>=lim.maoMin;
			gc[k]=tem&&a-r>=lim.contornoMin;
			}
	/* "com mão": o ponto ou um dos 8 vizinhos (OU na linha e depois na coluna) */
	oc.resize(n);
	for(int gy=0;gy<gh;++gy)
		{
		const unsigned char* l=&gc[size_t(gy)*gw];
		unsigned char* o=&oc[size_t(gy)*gw];
		for(int gx=0;gx<gw;++gx)
			o[gx]=l[gx]|(gx>0?l[gx-1]:0)|(gx<gw-1?l[gx+1]:0);
		}
	for(int gy=0;gy<gh;++gy)
		for(int gx=0;gx<gw;++gx)
			{
			size_t k=size_t(gy)*gw+gx;
			bool ocupado=oc[k]||(gy>0&&oc[k-gw])||(gy<gh-1&&oc[k+gw]);
			float f=gf[k];
			if(ocupado)
				{
				if(refDesde[k]<0.0)
					refDesde[k]=t;
				refVisto[k]=t;
				if(t-refDesde[k]>lim.congelaMax&&ok(f))
					ref[k]=f;
				}
			else
				{
				refDesde[k]=-1.0;
				if(t-refVisto[k]>lim.libera&&ok(f))
					ref[k]=f;
				}
			}
	}

float Detector::areia(const Quadro& q,int x,int y) const
	{
	if(refW<2||refH<2)
		return q.fundo(x,y);
	/* bilinear na grade (os pontos sem medida ficam de fora) */
	float gx=float(x-refX0)/float(refP),gy=float(y-refY0)/float(refP);
	int ix=std::max(0,std::min(refW-2,int(std::floor(gx)))),iy=std::max(0,std::min(refH-2,int(std::floor(gy))));
	float fx=std::max(0.0f,std::min(1.0f,gx-float(ix))),fy=std::max(0.0f,std::min(1.0f,gy-float(iy)));
	float s=0.0f,p=0.0f;
	const float pesos[4]={(1.0f-fx)*(1.0f-fy),fx*(1.0f-fy),(1.0f-fx)*fy,fx*fy};
	const size_t ks[4]={size_t(iy)*refW+ix,size_t(iy)*refW+ix+1,size_t(iy+1)*refW+ix,size_t(iy+1)*refW+ix+1};
	for(int i=0;i<4;++i)
		if(ok(ref[ks[i]]))
			{
			s+=pesos[i]*ref[ks[i]];
			p+=pesos[i];
			}
	return p>1e-3f?s/p:q.fundo(x,y);
	}

float Detector::areiaRapida(const Quadro& q,int x,int y) const
	{
	/* o ponto da grade mais perto (para os laços por pixel; a bilinear fica para a ponta) */
	if(refW<1||refH<1)
		return q.fundo(x,y);
	int gx=std::max(0,std::min(refW-1,(x-refX0+refP/2)/refP)),gy=std::max(0,std::min(refH-1,(y-refY0+refP/2)/refP));
	float v=ref[size_t(gy)*refW+gx];
	return ok(v)?v:q.fundo(x,y);
	}

float Detector::medianaEmVolta(const Quadro& q,int x0,int y0,int x1,int y1,float px,float py,float cp)
	{
	/* mediana da areia num anel de 8 a 13 cm em volta: a areia "de verdade" mesmo se a de baixo da mão
	   (do filtro) já subiu até ela; o braço cobre uma fatia pequena do anel */
	vols.clear();
	int P=std::max(2,int(lim.passo+0.5f));
	float r0=8.0f/cp,r1=13.0f/cp;
	int R=int(r1)+1;
	for(int y=std::max(y0,int(py)-R);y<std::min(y1,int(py)+R+1);y+=P)
		for(int x=std::max(x0,int(px)-R);x<std::min(x1,int(px)+R+1);x+=P)
			{
			float d2=(float(x)-px)*(float(x)-px)+(float(y)-py)*(float(y)-py);
			if(d2<r0*r0||d2>r1*r1)
				continue;
			float s=areiaRapida(q,x,y);
			if(ok(s))
				vols.push_back(s);
			}
	if(vols.size()<12)
		return std::nanf("");
	std::nth_element(vols.begin(),vols.begin()+vols.size()/2,vols.end());
	return vols[vols.size()/2];
	}

/* ------------------------------- busca grossa */

bool Detector::anelGrossoOk(const Quadro& q,int x0,int y0,int x1,int y1,int x,int y,float a) const
	{
	/* 8 pontos a anelGrosso cm: os que têm medida bem abaixo da ponta (pelo menos 4 com medida). Até
	   grossoAltos pontos SEGUIDOS podem estar altos (o dedo inclinado desce por ali). Na mão aberta, no
	   punho, numa cabeça ou num monte de areia há algo alto em vários lados. */
	float cp=q.cmPorPixel(x,y);
	if(!(cp>0.0f))
		return false;
	float r=lim.anelGrosso/cp;
	int estado[8]; // 0 = sem medida, 1 = abaixo, 2 = alto
	int baixos=0,altos=0;
	for(int k=0;k<8;++k)
		{
		int sx=x+int(std::floor(r*std::cos(float(k)*0.7853982f)+0.5f));
		int sy=y+int(std::floor(r*std::sin(float(k)*0.7853982f)+0.5f));
		estado[k]=0;
		if(sx<x0||sy<y0||sx>=x1||sy>=y1)
			continue;
		float b=q.alt(sx,sy);
		if(!ok(b))
			continue; // sombra do infravermelho ao lado do dedo: não conta
		if(a-b<lim.degrauGrosso)
			{
			estado[k]=2;
			++altos;
			}
		else
			{
			estado[k]=1;
			++baixos;
			}
		}
	if(baixos<4||altos>int(lim.grossoAltos+0.5f))
		return false;
	/* os altos num trecho só: conta as passagens de "abaixo" para "alto" na volta (sem medida não conta) */
	int passagens=0,ultimo=0;
	for(int k=0;k<16;++k)
		{
		int e=estado[k&7];
		if(e==0)
			continue;
		if(k>=8&&e==2&&ultimo==1)
			++passagens;
		ultimo=e;
		}
	return passagens<=1;
	}

void Detector::procura(const Quadro& q,int x0,int y0,int x1,int y1,double t,float rastroX,float rastroY,std::vector<Medida>& saida)
	{
	saida.clear();
	cands.clear();
	int P=std::max(2,int(lim.passo+0.5f));
	if(x1-x0<3*P||y1-y0<3*P)
		return;
	atualizaAreia(q,x0,y0,x1,y1,t);
	int gw=refW,gh=refH;

	/* ---- A) picos (indicador para cima) */
	/* o dedo já visto: o ponto mais alto da grade a até 3 cm de onde ele estava, sem exigir a altura sobre
	   a areia */
	if(rastroX>=0.0f)
		{
		int rx=int(rastroX+0.5f),ry=int(rastroY+0.5f);
		rx=std::max(x0,std::min(x1-1,rx));
		ry=std::max(y0,std::min(y1-1,ry));
		float cp=q.cmPorPixel(rx,ry);
		if(cp>0.0f)
			{
			int rg=int(3.0f/cp/float(P))+1;
			int bx=-1,by=-1;
			float ba=-1e30f;
			int gx0=(rx-x0)/P,gy0=(ry-y0)/P;
			for(int gy=std::max(0,gy0-rg);gy<=std::min(gh-1,gy0+rg);++gy)
				for(int gx=std::max(0,gx0-rg);gx<=std::min(gw-1,gx0+rg);++gx)
					{
					float a=ga[size_t(gy)*gw+gx];
					if(ok(a)&&a>ba&&(gx-gx0)*(gx-gx0)+(gy-gy0)*(gy-gy0)<=rg*rg)
						{
						ba=a;
						bx=gx;
						by=gy;
						}
					}
			if(bx>=0&&anelGrossoOk(q,x0,y0,x1,y1,x0+bx*P,y0+by*P,ba))
				{
				Candidato c={ba,x0+bx*P,y0+by*P,false};
				cands.push_back(c);
				}
			}
		}

	/* pontos de mão que são máximos locais (8 vizinhos da grade) e passam no anel grosso */
	size_t rastreados=cands.size();
	for(int gy=1;gy<gh-1;++gy)
		for(int gx=1;gx<gw-1;++gx)
			{
			size_t k=size_t(gy)*gw+gx;
			if(!gm[k])
				continue;
			float a=ga[k];
			bool maximo=true;
			for(int dy=-1;dy<=1&&maximo;++dy)
				for(int dx=-1;dx<=1&&maximo;++dx)
					{
					if(dx==0&&dy==0)
						continue;
					float b=ga[k+dy*gw+dx];
					if(!ok(b))
						continue;
					/* empate: fica o primeiro (senão dois pontos iguais vizinhos se anulam) */
					maximo=(dy>0||(dy==0&&dx>0))?a>=b:a>b;
					}
			if(!maximo)
				continue;
			int x=x0+gx*P,y=y0+gy*P;
			if(!(a-ref[k]>=lim.pontaMin))
				continue;
			if(!anelGrossoOk(q,x0,y0,x1,y1,x,y,a))
				continue;
			Candidato c={a,x,y,true};
			cands.push_back(c);
			}

	/* os mais altos primeiro; um só candidato a menos de 5 cm de outro (o do rastro vem antes) */
	std::sort(cands.begin()+rastreados,cands.end(),[](const Candidato& u,const Candidato& v){return u.a>v.a;});
	int maximo=std::max(1,int(lim.candidatosMax+0.5f));
	for(size_t i=0;i<cands.size()&&int(saida.size())<maximo;++i)
		{
		const Candidato& c=cands[i];
		float cp=q.cmPorPixel(c.x,c.y);
		if(!(cp>0.0f))
			continue;
		float r2=(5.0f/cp)*(5.0f/cp);
		bool perto=false;
		for(size_t j=0;j<saida.size()&&!perto;++j)
			perto=(saida[j].cx-c.x)*(saida[j].cx-c.x)+(saida[j].cy-c.y)*(saida[j].cy-c.y)<r2;
		if(perto)
			continue;
		bool pertoRastro=rastroX>=0.0f&&(float(c.x)-rastroX)*(float(c.x)-rastroX)+(float(c.y)-rastroY)*(float(c.y)-rastroY)<(3.0f/cp)*(3.0f/cp);
		Medida m;
		classifica(q,x0,y0,x1,y1,c.x,c.y,c.exige,m,!c.exige||pertoRastro);
		saida.push_back(m);
		}

	/* ---- B) contorno (indicador para baixo / para a frente): silhuetas da grade, as maiores primeiro */
	size_t n=size_t(gw)*size_t(gh);
	rot.assign(n,-1);
	struct Comp {int n,x0,y0,x1,y1,sx,sy,id,nb; float bx,by;}; // nb, bx, by: células na borda do retângulo e o meio delas
	std::vector<Comp> comps;
	for(size_t k0=0;k0<n;++k0)
		{
		if(!gc[k0]||rot[k0]>=0)
			continue;
		int id=int(comps.size());
		Comp c={0,gw,gh,-1,-1,int(k0%gw),int(k0/gw),id,0,0.0f,0.0f};
		pilha.clear();
		pilha.push_back(int(k0));
		rot[k0]=id;
		while(!pilha.empty())
			{
			int k=pilha.back();
			pilha.pop_back();
			int x=k%gw,y=k/gw;
			++c.n;
			c.x0=std::min(c.x0,x);
			c.y0=std::min(c.y0,y);
			c.x1=std::max(c.x1,x);
			c.y1=std::max(c.y1,y);
			if(x==0||y==0||x==gw-1||y==gh-1)
				{
				++c.nb;
				c.bx+=float(x);
				c.by+=float(y);
				}
			for(int dy=-1;dy<=1;++dy)
				for(int dx=-1;dx<=1;++dx)
					{
					int ux=x+dx,uy=y+dy;
					if(ux<0||uy<0||ux>=gw||uy>=gh)
						continue;
					size_t kk=size_t(uy)*gw+ux;
					if(gc[kk]&&rot[kk]<0)
						{
						rot[kk]=id;
						pilha.push_back(int(kk));
						}
					}
			}
		comps.push_back(c);
		}
	std::sort(comps.begin(),comps.end(),[](const Comp& u,const Comp& v){return u.n>v.n;});
	atualizaTrava(q,x0,y0,t,comps.size(),[&](size_t i,int& id,int& nb,int& sx,int& sy,int& cn){const Comp& c=comps[i];id=c.id;nb=c.nb;sx=c.sx;sy=c.sy;cn=c.n;});
	for(size_t i=0;i<comps.size()&&i<4;++i)
		{
		const Comp& c=comps[i];
		int sx=x0+c.sx*P,sy=y0+c.sy*P;
		float cp=q.cmPorPixel(sx,sy);
		if(!(cp>0.0f)||float(c.n)*float(P*P)*cp*cp<lim.maoAreaMin)
			break; // as outras são menores ainda
		/* braço comprido entrando pela beirada: só o pedaço de 21 cm do lado da mão (a ponta mais longe de
		   onde o braço entra: dedo + punho + um pouco do braço) vai para a silhueta fina; o custo fica limitado */
		int ex0=c.x0,ey0=c.y0,ex1=c.x1+1,ey1=c.y1+1;
		bool corte[4]={false,false,false,false};
		int L=int(21.0f/(cp*float(P)))+1;
		if(c.nb>0&&(ex1-ex0>L||ey1-ey0>L))
			{
			float bx=c.bx/float(c.nb),by=c.by/float(c.nb),longe=-1.0f;
			int fx=c.sx,fy=c.sy;
			for(int gy=c.y0;gy<=c.y1;++gy)
				for(int gx=c.x0;gx<=c.x1;++gx)
					if(rot[size_t(gy)*gw+gx]==c.id)
						{
						float d=(float(gx)-bx)*(float(gx)-bx)+(float(gy)-by)*(float(gy)-by);
						if(d>longe)
							{
							longe=d;
							fx=gx;
							fy=gy;
							}
						}
			/* o centro da janela fica 8 cm para dentro do braço a partir da ponta */
			float dl=std::sqrt(longe),av=std::min(dl,8.0f/(cp*float(P)));
			int mx=dl>0.0f?int(float(fx)+(bx-float(fx))/dl*av+0.5f):fx,my=dl>0.0f?int(float(fy)+(by-float(fy))/dl*av+0.5f):fy;
			int nx0=std::max(ex0,mx-L/2),ny0=std::max(ey0,my-L/2),nx1=std::min(ex1,mx+L/2+1),ny1=std::min(ey1,my+L/2+1);
			corte[0]=nx0>ex0;
			corte[1]=nx1<ex1;
			corte[2]=ny0>ey0;
			corte[3]=ny1<ey1;
			ex0=nx0;
			ey0=ny0;
			ex1=nx1;
			ey1=ny1;
			}
		contorno(q,x0,y0,x1,y1,x0+ex0*P,y0+ey0*P,x0+ex1*P,y0+ey1*P,corte,rastroX,rastroY,saida);
		}

	/* ---- um só por dedo: o pico e o contorno podem achar o mesmo (dedo inclinado); fica o que é dedo */
	for(size_t i=0;i<saida.size();++i)
		for(size_t j=i+1;j<saida.size();)
			{
			float cp=saida[i].cmPx>0.0f?saida[i].cmPx:0.13f;
			float dx=saida[i].px-saida[j].px,dy=saida[i].py-saida[j].py;
			if(dx*dx+dy*dy<(4.0f/cp)*(4.0f/cp)&&(saida[i].dedo||saida[j].dedo))
				{
				if(!saida[i].dedo)
					saida[i]=saida[j];
				saida.erase(saida.begin()+j);
				}
			else
				++j;
			}
	}

/* ------------------------------- A) confirmação do pico em resolução cheia */

void Detector::classifica(const Quadro& q,int x0,int y0,int x1,int y1,int cx,int cy,bool exigeAltura,Medida& m,bool rastreio)
	{
	const float NaN=std::nanf("");
	m.tipo=0;
	m.cx=cx;
	m.cy=cy;
	m.px=m.ax=float(cx);
	m.py=m.ay=float(cy);
	m.bx=m.by=NaN;
	m.ponta=m.sobreAreia=NaN;
	m.picoArea=m.picoLarg=m.picoComp=m.altaArea=m.degrau=m.punhoArea=m.fracAlto=m.outroArea=NaN;
	m.comp=m.larg=m.angulo=m.braco=NaN;
	m.rastreio=rastreio;
	m.dedo=false;
	m.motivo="sem medida";
	float cp=q.cmPorPixel(cx,cy);
	m.cmPx=cp;
	if(!(cp>0.0f))
		return;
	float fg=rastreio?lim.folga:1.0f; // histerese: perto do dedo já visto, mais folga

	/* a areia: a de referência (congelada embaixo da mão) e, se ela já subiu até a mão, a mediana em volta
	   (só medida quando a ponta parece perto demais da areia) */
	float tetoAreia=INF;

	/* janela de "janela" cm em volta do candidato */
	int R=int(lim.janela/cp)+1;
	int wx0=std::max(x0,cx-R),wy0=std::max(y0,cy-R),wx1=std::min(x1,cx+R+1),wy1=std::min(y1,cy+R+1);
	int ww=wx1-wx0,wh=wy1-wy0;
	if(ww<3||wh<3)
		return;
	size_t n=size_t(ww)*size_t(wh);
	/* lido do quadro só quando preciso (mão aberta, punho: o pico transborda logo e o resto nem é lido) */
	const float NAOLIDO=-1e30f;
	ja.assign(n,NAOLIDO);
	auto A=[&](size_t k)->float
		{
		float v=ja[k];
		if(v==NAOLIDO)
			ja[k]=v=q.alt(wx0+int(k%ww),wy0+int(k/ww));
		return v;
		};

	/* 1) ponta: o 3º ponto mais alto a até 1 cm do candidato (aguenta 2 pixels de ruído) */
	ordem.clear();
	int rT=std::max(2,int(1.0f/cp+0.5f));
	for(int y=std::max(wy0,cy-rT);y<=std::min(wy1-1,cy+rT);++y)
		for(int x=std::max(wx0,cx-rT);x<=std::min(wx1-1,cx+rT);++x)
			{
			int k=(y-wy0)*ww+(x-wx0);
			if(ok(A(k))&&(x-cx)*(x-cx)+(y-cy)*(y-cy)<=rT*rT)
				ordem.push_back(k);
			}
	if(ordem.size()<3)
		return;
	std::nth_element(ordem.begin(),ordem.begin()+2,ordem.end(),[this](int u,int v){return ja[u]>ja[v];});
	int semente=ordem[2];
	float ponta=ja[semente];
	m.ponta=ponta;

	/* 2) pico: pixels ligados à ponta (4 vizinhos) a menos de picoQueda abaixo dela; sem medida no meio
	   não corta (é só não contar) */
	marca.assign(n,0);
	pilha.clear();
	pilha.push_back(semente);
	marca[semente]=1;
	int limite=int(lim.picoAreaMax*fg*1.5f/(cp*cp))+1;
	int nP=0;
	double sx=0.0,sy=0.0,sxx=0.0,syy=0.0,sxy=0.0;
	bool transbordou=false;
	float corte=ponta-lim.picoQueda;
	const int vx[4]={1,-1,0,0},vy[4]={0,0,1,-1};
	while(!pilha.empty())
		{
		int k=pilha.back();
		pilha.pop_back();
		++nP;
		double x=k%ww,y=k/ww;
		sx+=x;
		sy+=y;
		sxx+=x*x;
		syy+=y*y;
		sxy+=x*y;
		if(nP>limite)
			{
			transbordou=true;
			break;
			}
		for(int v=0;v<4;++v)
			{
			int nx=int(x)+vx[v],ny=int(y)+vy[v];
			if(nx<0||ny<0||nx>=ww||ny>=wh)
				continue;
			int kk=ny*ww+nx;
			if(!marca[kk]&&A(kk)>corte)
				{
				marca[kk]=1;
				pilha.push_back(kk);
				}
			}
		}
	float mx=float(sx/nP),my=float(sy/nP); // centro do pico (na janela)
	m.px=m.ax=float(wx0)+mx;
	m.py=m.ay=float(wy0)+my;
	m.picoArea=float(nP)*cp*cp;
	float eixoX=1.0f,eixoY=0.0f; // direção do comprimento do pico
	{
	/* largura e comprimento pelos momentos (disco de diâmetro D: 4 desvios = D); o dedo inclinado fica comprido */
	double cxx=sxx/nP-double(mx)*mx,cyy=syy/nP-double(my)*my,cxy=sxy/nP-double(mx)*my;
	double tr=0.5*(cxx+cyy),dt=std::sqrt(std::max(0.0,0.25*(cxx-cyy)*(cxx-cyy)+cxy*cxy));
	m.picoLarg=4.0f*float(std::sqrt(std::max(0.0,tr-dt)+1.0/12.0))*cp;
	m.picoComp=4.0f*float(std::sqrt(std::max(0.0,tr+dt)+1.0/12.0))*cp;
	float th=0.5f*float(std::atan2(2.0*cxy,cxx-cyy));
	eixoX=std::cos(th);
	eixoY=std::sin(th);
	}
	{
	int tx=std::max(wx0,std::min(wx1-1,int(m.px+0.5f))),ty=std::max(wy0,std::min(wy1-1,int(m.py+0.5f)));
	float f=areia(q,tx,ty);
	if(exigeAltura&&!(ok(f)&&ponta-f>=lim.pontaMin))
		{
		float emVolta=medianaEmVolta(q,x0,y0,x1,y1,m.px,m.py,cp);
		if(ok(emVolta))
			{
			tetoAreia=emVolta+1.5f;
			f=ok(f)?std::min(f,tetoAreia):tetoAreia;
			}
		}
	m.sobreAreia=ok(f)?ponta-f:NaN;
	}
	if(!medeTudo&&(transbordou||(exigeAltura&&!(m.sobreAreia>=lim.pontaMin))))
		{
		/* já se sabe que não é dedo (mão aberta, punho, cabeça...): o anel em volta fica sem medir */
		m.motivo=transbordou?"pico largo":"perto da areia";
		return;
		}

	/* daqui em diante a janela inteira: lê o que falta, linha por linha */
	for(int y=0;y<wh;++y)
		for(int x=0;x<ww;++x)
			{
			float& v=ja[size_t(y)*ww+x];
			if(v==NAOLIDO)
				v=q.alt(wx0+x,wy0+y);
			}

	/* 3) a parte alta ligada à ponta (a menos de outroQueda abaixo): o dedo inclinado inteiro fica nela;
	   um "outro pico" é alto e SEPARADO dela (outro dedo, a mão aberta) */
	marca2.assign(n,0);
	pilha.clear();
	pilha.push_back(semente);
	marca2[semente]=1;
	float corte2=ponta-lim.outroQueda;
	int limite2=int(4.0f*lim.picoAreaMax/(cp*cp))+1,n2=0;
	while(!pilha.empty()&&n2<limite2)
		{
		int k=pilha.back();
		pilha.pop_back();
		++n2;
		int x=k%ww,y=k/ww;
		for(int v=0;v<4;++v)
			{
			int nx=x+vx[v],ny=y+vy[v];
			if(nx<0||ny<0||nx>=ww||ny>=wh)
				continue;
			int kk=ny*ww+nx;
			if(!marca2[kk]&&ja[kk]>corte2)
				{
				marca2[kk]=1;
				pilha.push_back(kk);
				}
			}
		}
	/* encosta: a parte alta inteira de um dedo é pequena (o lado dele é um paredão); a de um monte de areia
	   liso, que desce devagar, é larga */
	m.altaArea=float(n2)*cp*cp;

	/* 4) em volta: anel do punho e outros picos. Pico comprido (dedo inclinado): a fatia do anel ao longo
	   dele, do lado por onde o dedo desce (o com mais "alto"), é o próprio dedo e não conta como "alto" */
	float rIn=lim.anelDentro/cp,rOut=lim.anelFora/cp,rOutro=lim.outroDentro/cp;
	float dMin=lim.degrauMin/fg;
	bool inclinado=m.picoComp>1.3f*m.picoLarg;
	int nMao=0,nAlto=0,nPunho=0,nOutro=0;
	int altoF[2]={0,0},maoF[2]={0,0}; // nas duas fatias ao longo do pico (+ e -)
	dzs.clear();
	vols.clear();
	for(int y=0;y<wh;++y)
		for(int x=0;x<ww;++x)
			{
			size_t k=size_t(y)*ww+x;
			float a=ja[k];
			if(!ok(a))
				continue;
			float dx=float(x)-mx,dy=float(y)-my;
			float d2=dx*dx+dy*dy;
			float dz=ponta-a;
			if(d2>rOutro*rOutro&&dz<lim.outroQueda&&!marca2[k])
				++nOutro;
			if(d2<rIn*rIn||d2>rOut*rOut)
				continue;
			if(dz>16.0f)
				continue; // areia ou chão lá embaixo: não é mão
			if(exigeAltura)
				{
				float s=std::min(areiaRapida(q,wx0+x,wy0+y),tetoAreia);
				if(!(ok(s)&&a-s>=lim.maoAnel))
					continue;
				}
			/* na fatia de +-30 graus em volta do eixo do pico? (f = 0 de um lado, 1 do outro) */
			int f=-1;
			if(inclinado)
				{
				float ao=dx*eixoX+dy*eixoY,pe=std::fabs(dy*eixoX-dx*eixoY);
				if(pe<=0.577f*std::fabs(ao))
					f=ao>=0.0f?0:1;
				}
			++nMao;
			if(f>=0)
				++maoF[f];
			if(dz<lim.altoMax)
				{
				++nAlto;
				if(f>=0)
					++altoF[f];
				}
			if(dz>=dMin&&dz<=15.0f)
				++nPunho;
			if(f<0)
				dzs.push_back(dz); // o degrau é medido fora da fatia do dedo
			else
				vols.push_back(dz);
			}
	int fd=altoF[1]>altoF[0]?1:0; // o lado por onde o dedo desce
	int exAlto=inclinado?altoF[fd]:0,exMao=inclinado?maoF[fd]:0;
	m.punhoArea=float(nPunho)*cp*cp;
	/* quase nada fora da fatia (dedo bem inclinado, o punho fica longe): não há o que estar "alto" */
	int fora=nMao-exMao;
	m.fracAlto=fora*cp*cp>=1.0f?float(nAlto-exAlto)/float(fora):(nMao>0?0.0f:1.0f);
	m.outroArea=float(nOutro)*cp*cp;
	if(dzs.size()*cp*cp<1.0f)
		dzs.insert(dzs.end(),vols.begin(),vols.end());
	if(!dzs.empty())
		{
		std::nth_element(dzs.begin(),dzs.begin()+dzs.size()/2,dzs.end());
		m.degrau=dzs[dzs.size()/2];
		}

	/* 5) decisão, na ordem */
	if(exigeAltura&&!(m.sobreAreia>=lim.pontaMin))
		m.motivo="perto da areia";
	else if(transbordou||m.picoArea>lim.picoAreaMax*fg)
		m.motivo="pico largo";
	else if(m.picoArea<lim.picoAreaMin)
		m.motivo="pico pequeno";
	else if(m.picoLarg>lim.picoLargMax*fg)
		m.motivo="pico grosso";
	else if(m.picoComp>lim.picoCompMax*fg)
		m.motivo="pico comprido";
	else if(m.altaArea>lim.altaAreaMax*fg)
		m.motivo="encosta suave (monte?)";
	else if(m.punhoArea<lim.punhoAreaMin/fg)
		m.motivo="sem punho embaixo";
	else if(m.fracAlto>lim.fracAltoMax+(rastreio?0.1f:0.0f))
		m.motivo="alto em volta";
	else if(!(m.degrau>=dMin&&m.degrau<=lim.degrauMax))
		m.motivo="degrau";
	else if(m.outroArea>lim.outroAreaMax*fg)
		m.motivo="outro pico";
	else
		{
		m.dedo=true;
		m.motivo="dedo";
		}
	}

/* ------------------------------- B) contorno: o dedo saindo do punho na silhueta */

namespace {
/* distância (em células) por chanfro 1 / raiz de 2, em passadas de ida e volta; d < 0 = célula bloqueada
   (fora da silhueta), d = 0 = origem; fora da grade: borda[0..3] (esquerda, direita, cima, baixo) diz se vale
   "longe" (a mão continua além do retângulo do detector) ou, com fora0, 0 (fundo). Numa grade com moldura
   de 1 célula (pad), sem conferir limites a cada vizinho. */
void chanfro(std::vector<float>& d,int w,int h,const bool borda[4],bool fora0,int voltas,std::vector<float>& pad)
	{
	int pw=w+2,ph=h+2;
	pad.resize(size_t(pw)*size_t(ph));
	float fora=fora0?0.0f:INF;
	for(int x=0;x<pw;++x)
		{
		pad[x]=borda[2]?INF:fora;
		pad[size_t(ph-1)*pw+x]=borda[3]?INF:fora;
		}
	for(int y=0;y<ph;++y)
		{
		pad[size_t(y)*pw]=borda[0]?INF:fora;
		pad[size_t(y)*pw+pw-1]=borda[1]?INF:fora;
		}
	for(int y=0;y<h;++y)
		for(int x=0;x<w;++x)
			{
			float v=d[size_t(y)*w+x];
			pad[size_t(y+1)*pw+x+1]=v<0.0f?INF:v;
			}
	for(int it=0;it<voltas;++it)
		{
		for(int y=0;y<h;++y)
			for(int x=0;x<w;++x)
				{
				if(d[size_t(y)*w+x]<=0.0f)
					continue;
				size_t k=size_t(y+1)*pw+x+1;
				float v=std::min(pad[k],std::min(std::min(pad[k-1]+1.0f,pad[k-pw]+1.0f),std::min(pad[k-pw-1]+RAIZ2,pad[k-pw+1]+RAIZ2)));
				pad[k]=v;
				}
		for(int y=h-1;y>=0;--y)
			for(int x=w-1;x>=0;--x)
				{
				if(d[size_t(y)*w+x]<=0.0f)
					continue;
				size_t k=size_t(y+1)*pw+x+1;
				float v=std::min(pad[k],std::min(std::min(pad[k+1]+1.0f,pad[k+pw]+1.0f),std::min(pad[k+pw+1]+RAIZ2,pad[k+pw-1]+RAIZ2)));
				pad[k]=v;
				}
		}
	for(int y=0;y<h;++y)
		for(int x=0;x<w;++x)
			if(d[size_t(y)*w+x]>0.0f)
				d[size_t(y)*w+x]=pad[size_t(y+1)*pw+x+1];
	}
}

void Detector::contorno(const Quadro& q,int x0,int y0,int x1,int y1,int gx0,int gy0,int gx1,int gy1,const bool corte[4],float rastroX,float rastroY,std::vector<Medida>& saida)
	{
	/* silhueta fina (passo de 2 px) em volta da mão achada na grade; corte: lados onde a janela cortou o braço */
	const int S=2;
	int P=std::max(2,refP);
	int bx0=std::max(x0,gx0-(corte[0]?0:2*P)),by0=std::max(y0,gy0-(corte[2]?0:2*P));
	int bx1=std::min(x1,gx1+(corte[1]?0:2*P)),by1=std::min(y1,gy1+(corte[3]?0:2*P));
	const bool borda[4]={bx0<=x0||corte[0],bx1>=x1||corte[1],by0<=y0||corte[2],by1>=y1||corte[3]};
	int fw=(bx1-bx0+S-1)/S,fh=(by1-by0+S-1)/S;
	if(fw<4||fh<4)
		return;
	size_t n=size_t(fw)*size_t(fh);
	fm.assign(n,0);
	fa.resize(n);
	for(int j=0;j<fh;++j)
		for(int i=0;i<fw;++i)
			{
			int x=bx0+i*S,y=by0+j*S;
			size_t k=size_t(j)*fw+i;
			float a=q.alt(x,y),r=areiaRapida(q,x,y);
			fa[k]=a;
			fm[k]=!ok(a)||!ok(r)?2:(a-r>=lim.contornoMin?1:0);
			}
	/* sem medida: vira mão se 3 dos 4 vizinhos são mão (buraco no dedo); senão fundo (a sombra do lado) */
	for(int it=0;it<2;++it)
		for(int j=0;j<fh;++j)
			for(int i=0;i<fw;++i)
				{
				size_t k=size_t(j)*fw+i;
				if(fm[k]!=2)
					continue;
				int v=(i>0&&fm[k-1]==1)+(i<fw-1&&fm[k+1]==1)+(j>0&&fm[k-fw]==1)+(j<fh-1&&fm[k+fw]==1);
				if(v>=3)
					fm[k]=1;
				else if(it==1)
					fm[k]=0;
				}
	/* a maior silhueta (8 vizinhos) */
	frot.assign(n,-1);
	int melhor=-1,melhorN=0,id=0,semente=-1;
	for(size_t k0=0;k0<n;++k0)
		{
		if(fm[k0]!=1||frot[k0]>=0)
			continue;
		int cnt=0;
		pilha.clear();
		pilha.push_back(int(k0));
		frot[k0]=id;
		while(!pilha.empty())
			{
			int k=pilha.back();
			pilha.pop_back();
			++cnt;
			int x=k%fw,y=k/fw;
			for(int dy=-1;dy<=1;++dy)
				for(int dx=-1;dx<=1;++dx)
					{
					int ux=x+dx,uy=y+dy;
					if(ux<0||uy<0||ux>=fw||uy>=fh)
						continue;
					size_t kk=size_t(uy)*fw+ux;
					if(fm[kk]==1&&frot[kk]<0)
						{
						frot[kk]=id;
						pilha.push_back(int(kk));
						}
					}
			}
		if(cnt>melhorN)
			{
			melhorN=cnt;
			melhor=id;
			semente=int(k0);
			}
		++id;
		}
	if(melhor<0)
		return;
	float cp=q.cmPorPixel(bx0+(semente%fw)*S,by0+(semente/fw)*S);
	if(!(cp>0.0f))
		return;
	float cs=float(S)*cp; // cm por célula
	if(float(melhorN)*cs*cs<lim.maoAreaMin)
		return;

	/* distância ao fundo; corpo = o que fica a mais de dedoRaio da beira (punho, braço) */
	d1.assign(n,0.0f);
	for(size_t k=0;k<n;++k)
		if(frot[k]==melhor)
			d1[k]=INF;
	chanfro(d1,fw,fh,borda,true,1,dp);
	float rA=lim.dedoRaio/cs;
	int nCorpo=0;
	d2.assign(n,-1.0f);
	for(size_t k=0;k<n;++k)
		if(frot[k]==melhor)
			{
			if(d1[k]>rA)
				{
				d2[k]=0.0f;
				++nCorpo;
				}
			else
				d2[k]=INF;
			}
	if(float(nCorpo)*cs*cs<lim.corpoAreaMin)
		return; // fino inteiro (um braço de longe, um pedaço): sem punho
	/* distância ao corpo andando por dentro da silhueta; o que fica a mais de dedoRaio dele é parte fina */
	const bool semBorda[4]={false,false,false,false};
	chanfro(d2,fw,fh,semBorda,false,2,dp);

	/* partes finas (8 vizinhos): quanto saem do corpo, onde fica a ponta e onde nascem */
	struct Parte {float comp,larg; int ponta,base,ini,fim;};
	std::vector<Parte> partes;
	frot2.assign(n,-1);
	lista.clear();
	float fino=rA+0.5f; // a mais disso do corpo: parte fina
	for(size_t k0=0;k0<n;++k0)
		{
		if(frot[k0]!=melhor||!(d2[k0]>fino)||d2[k0]>=INF||frot2[k0]>=0)
			continue;
		Parte pt={0.0f,0.0f,int(k0),int(k0),int(lista.size()),0};
		int pid=int(partes.size());
		pilha.clear();
		pilha.push_back(int(k0));
		frot2[k0]=pid;
		while(!pilha.empty())
			{
			int k=pilha.back();
			pilha.pop_back();
			lista.push_back(k);
			if(d2[k]>d2[pt.ponta])
				pt.ponta=k;
			if(d2[k]<d2[pt.base])
				pt.base=k;
			int x=k%fw,y=k/fw;
			for(int dy=-1;dy<=1;++dy)
				for(int dx=-1;dx<=1;++dx)
					{
					int ux=x+dx,uy=y+dy;
					if(ux<0||uy<0||ux>=fw||uy>=fh)
						continue;
					size_t kk=size_t(uy)*fw+ux;
					if(frot[kk]==melhor&&d2[kk]>fino&&d2[kk]<INF&&frot2[kk]<0)
						{
						frot2[kk]=pid;
						pilha.push_back(int(kk));
						}
					}
			}
		pt.fim=int(lista.size());
		pt.comp=d2[pt.ponta]*cs-lim.dedoRaio;
		pt.larg=float(pt.fim-pt.ini)*cs*cs/std::max(cs,d2[pt.ponta]*cs-fino*cs);
		partes.push_back(pt);
		}
	if(partes.empty())
		return; // punho fechado, mão chata de dedos juntos: nada fino saindo
	std::sort(partes.begin(),partes.end(),[](const Parte& u,const Parte& v){return u.comp>v.comp;});
	const Parte& d=partes[0];
	if(d.comp<1.0f)
		return; // fiapos da borda

	const float NaN=std::nanf("");
	Medida m;
	m.tipo=1;
	m.bx=m.by=NaN;
	m.picoArea=m.picoLarg=m.picoComp=m.altaArea=m.fracAlto=NaN;
	m.braco=NaN;
	m.cmPx=cp;
	/* ponta: o meio das células a menos de 4 mm da mais longe do corpo */
	double tx=0.0,ty=0.0;
	int nt=0;
	vols.clear();
	float dTopo=d2[d.ponta]-0.4f/cs;
	for(int i=d.ini;i<d.fim;++i)
		{
		int k=lista[i];
		if(d2[k]<dTopo)
			continue;
		tx+=k%fw;
		ty+=k/fw;
		++nt;
		if(ok(fa[k]))
			vols.push_back(fa[k]);
		}
	tx/=nt;
	ty/=nt;
	m.px=float(bx0)+float(tx)*S;
	m.py=float(by0)+float(ty)*S;
	m.cx=int(m.px+0.5f);
	m.cy=int(m.py+0.5f);
	if(!vols.empty())
		{
		std::nth_element(vols.begin(),vols.begin()+vols.size()/2,vols.end());
		m.ponta=vols[vols.size()/2];
		}
	else
		m.ponta=NaN;
	{
	float s=areia(q,m.cx,m.cy);
	m.sobreAreia=ok(s)&&ok(m.ponta)?m.ponta-s:NaN;
	}
	/* punho: mediana da altura do corpo a até 3 cm de onde o dedo nasce */
	int bi=d.base%fw,bj=d.base/fw;
	int rb=int(3.0f/cs)+1;
	vols.clear();
	for(int j=std::max(0,bj-rb);j<=std::min(fh-1,bj+rb);++j)
		for(int i=std::max(0,bi-rb);i<=std::min(fw-1,bi+rb);++i)
			{
			size_t k=size_t(j)*fw+i;
			if(d2[k]==0.0f&&ok(fa[k])&&(i-bi)*(i-bi)+(j-bj)*(j-bj)<=rb*rb)
				vols.push_back(fa[k]);
			}
	float punho=NaN;
	if(!vols.empty())
		{
		std::nth_element(vols.begin(),vols.begin()+vols.size()/2,vols.end());
		punho=vols[vols.size()/2];
		}
	m.degrau=ok(punho)&&ok(m.ponta)?punho-m.ponta:NaN; // > 0: apontando para baixo
	m.punhoArea=float(nCorpo)*cs*cs;
	m.outroArea=partes.size()>1?partes[1].comp:0.0f;
	m.comp=d.comp;
	m.larg=d.larg;
	/* de onde vem o braço: o meio da silhueta na borda do retângulo do detector (ou no lado onde a janela
	   cortou o braço comprido); sem borda, o corpo mais longe da ponta. A mão de verdade SEMPRE chega com o
	   braço pela beirada da caixa; um monte de areia com uma crista (ou uma ponta puxada de lado) tem corpo e
	   parte fina como o punho com o dedo, e a areia congelada o segura por até congelaMax, mas fica solto no
	   meio da caixa */
	double rx=0.0,ry=0.0;
	int nr=0;
	vols.clear();
	auto naBorda=[&](int i,int j)
		{
		size_t k=size_t(j)*fw+i;
		if(frot[k]==melhor)
			{
			rx+=i;
			ry+=j;
			++nr;
			float r=areiaRapida(q,bx0+i*S,by0+j*S);
			if(ok(fa[k])&&ok(r))
				vols.push_back(fa[k]-r);
			}
		};
	for(int j=0;j<fh;++j)
		{
		if(borda[0]) naBorda(0,j);
		if(borda[1]&&fw>1) naBorda(fw-1,j);
		}
	for(int i=1;i<fw-1;++i)
		{
		if(borda[2]) naBorda(i,0);
		if(borda[3]&&fh>1) naBorda(i,fh-1);
		}
	/* e o braço tem de ser alto como um braço: a mediana da silhueta na borda pelo menos bracoAlto acima da
	   areia (congelada) embaixo dela. Um muro ou uma península de areia que encosta na beirada e afina numa
	   crista contava como braço e virava dedo durante os 8 s de areia congelada (revisão de 19/11: 21 de 24
	   cenas); areia moldada perto da beirada raramente passa de uns 6 cm, o braço de quem aponta passa */
	m.braco=NaN;
	if(!vols.empty())
		{
		std::nth_element(vols.begin(),vols.begin()+vols.size()/2,vols.end());
		m.braco=vols[vols.size()/2];
		}
	bool naBordaVista=nr>=3; // havia silhueta na beirada (o nr é trocado abaixo)
	bool braco=naBordaVista&&m.braco>=lim.bracoAlto;
	if(!braco)
		{
		float longe=-1.0f;
		for(int j=0;j<fh;++j)
			for(int i=0;i<fw;++i)
				{
				size_t k=size_t(j)*fw+i;
				float dd=(float(i)-float(tx))*(float(i)-float(tx))+(float(j)-float(ty))*(float(j)-float(ty));
				if(d2[k]==0.0f&&dd>longe)
					{
					longe=dd;
					rx=i;
					ry=j;
					}
				}
		nr=1;
		}
	rx/=nr;
	ry/=nr;
	float ux=float(tx)-float(bi),uy=float(ty)-float(bj); // base -> ponta
	float vx=float(bi)-float(rx),vy=float(bj)-float(ry); // braço -> base
	float nu=std::sqrt(ux*ux+uy*uy),nv=std::sqrt(vx*vx+vy*vy);
	m.angulo=nu>0.0f&&nv>0.0f?std::acos(std::max(-1.0f,std::min(1.0f,(ux*vx+uy*vy)/(nu*nv))))*57.29578f:0.0f;
	m.rastreio=rastroX>=0.0f&&(m.px-rastroX)*(m.px-rastroX)+(m.py-rastroY)*(m.py-rastroY)<(3.0f/cp)*(3.0f/cp);
	float fg=m.rastreio?lim.folga:1.0f;
	m.ax=m.px;
	m.ay=m.py;

	/* decisão, na ordem */
	m.dedo=false;
	if(d.comp<lim.dedoCompMin/fg)
		m.motivo="dedo curto";
	else if(d.comp>lim.dedoCompMax)
		m.motivo="comprido demais";
	else if(d.larg<lim.dedoLargMin)
		m.motivo="fiapo";
	else if(partes.size()>1&&partes[1].comp>=std::max(lim.outroDedo*fg,0.6f*d.comp))
		m.motivo=partes.size()>2?"mao aberta (contorno)":"dois dedos (contorno)";
	else if(m.angulo>lim.anguloMax)
		m.motivo="dedo de lado";
	else if(!braco&&!(m.ponta-medianaEmVolta(q,x0,y0,x1,y1,m.px,m.py,cp)>=lim.semBracoAlto))
		m.motivo=naBordaVista?"braco baixo (areia?)":"sem braco (areia?)"; // solto no meio e baixo: areia moldada; alto o bastante, só a mão (o braço sem medida na beirada)
	else
		{
		m.dedo=true;
		m.motivo="dedo";
		/* apontando para baixo: a comida cai um pouco à frente, onde o dedo aponta (no máximo alvoMax) */
		if(nu>0.0f&&ok(m.degrau)&&ok(m.sobreAreia)&&m.degrau>1.0f&&m.sobreAreia>0.0f)
			{
			float horiz=std::max(0.5f,nu*cs);
			float inclina=m.degrau/horiz;
			if(inclina>=0.4f)
				{
				float e=std::min(lim.alvoMax,m.sobreAreia/inclina)/cs*float(S);
				m.ax=std::max(float(x0),std::min(float(x1-1),m.px+ux/nu*e));
				m.ay=std::max(float(y0),std::min(float(y1-1),m.py+uy/nu*e));
				}
			}
		}
	saida.push_back(m);
	}

/* ------------------------------- o passo do Sandbox */

void Detector::passo(const Quadro& q,int x0,int y0,int x1,int y1,double t,const float area[4],const std::vector<MaoAberta>& maos,Rastro& r,Passo& p)
	{
	procura(q,x0,y0,x1,y1,t,r.conta>0?rastroPx:-1.0f,rastroPy,medidas);
	int melhor=-1;
	double melhorD=1e30;
	for(size_t i=0;i<medidas.size();++i)
		{
		Medida& md=medidas[i];
		float bx,by;
		if(q.caixa(md.ax,md.ay,bx,by))
			{
			md.bx=bx;
			md.by=by;
			}
		else
			{
			if(md.dedo)
				md.motivo="sem medida na ponta";
			md.dedo=false;
			continue;
			}
		/* longe da beirada (como a chuva) e de uma mão ABERTA achada pelo SARndbox (essa é da chuva) */
		if(md.dedo&&area!=0&&(bx<area[0]+4.0f||bx>area[2]-4.0f||by<area[1]+4.0f||by>area[3]-4.0f))
			{
			md.dedo=false;
			md.motivo="na beirada";
			}
		for(size_t j=0;md.dedo&&j<maos.size();++j)
			if((maos[j].x-bx)*(maos[j].x-bx)+(maos[j].y-by)*(maos[j].y-by)<(maos[j].raio+2.0f)*(maos[j].raio+2.0f))
				{
				md.dedo=false;
				md.motivo="mao aberta (chuva)";
				}
		if(!md.dedo)
			continue;
		/* com um dedo já visto, fica o mais perto dele; senão, o mais alto */
		double dd=r.conta>0?double(bx-r.x)*(bx-r.x)+double(by-r.y)*(by-r.y):-double(md.ponta);
		if(dd<melhorD)
			{
			melhorD=dd;
			melhor=int(i);
			}
		}
	/* abriu a mão em cima do dedo confirmado: solta na hora (a chuva volta sem esperar os 0,4 s) */
	p.soltouMao=false;
	if(r.ativo)
		for(size_t j=0;j<maos.size();++j)
			if((maos[j].x-r.x)*(maos[j].x-r.x)+(maos[j].y-r.y)*(maos[j].y-r.y)<(maos[j].raio+2.0f)*(maos[j].raio+2.0f))
				{
				r.larga();
				rastroPx=rastroPy=-1.0f;
				melhor=-1;
				p.soltouMao=true;
				break;
				}
	p.escolhido=melhor;
	p.viu=melhor>=0;
	p.x=p.viu?medidas[melhor].bx:0.0f;
	p.y=p.viu?medidas[melhor].by:0.0f;
	if(p.viu)
		{
		rastroPx=medidas[melhor].px;
		rastroPy=medidas[melhor].py;
		}
	p.ev=r.passo(t,p.viu,p.x,p.y);
	if(r.conta==0)
		rastroPx=rastroPy=-1.0f;
	}

/* ------------------------------- tempo */

Rastro::Rastro(void)
	:ativo(false),x(0.0f),y(0.0f),conta(0),acertosMin(3),confirma(0.2f),raio(5.0f),solta(0.35f),suave(0.09f),visto(-1e9),primeiro(0.0)
	{
	}

Rastro::Evento Rastro::passo(double t,bool viu,float nx,float ny)
	{
	if(viu&&conta>0&&(nx-x)*(nx-x)+(ny-y)*(ny-y)>raio*raio)
		{
		if(ativo)
			viu=false; // outro dedo longe do confirmado: conta como falha deste (uma vaga só)
		else
			conta=0;   // começou em outro lugar
		}
	if(viu)
		{
		if(conta==0)
			{
			x=nx;
			y=ny;
			primeiro=t;
			}
		else
			{
			/* suavização pelo tempo (a mesma a 12 ou a 30 quadros por segundo) */
			float a=1.0f-std::exp(-float(std::max(0.0,t-visto))/suave);
			x=a*nx+(1.0f-a)*x;
			y=a*ny+(1.0f-a)*y;
			}
		++conta;
		visto=t;
		if(!ativo&&conta>=acertosMin&&t-primeiro>=double(confirma)-1e-6)
			{
			ativo=true;
			return CONFIRMOU;
			}
		return ativo?CONTINUA:NADA;
		}
	if(ativo)
		{
		if(t-visto>solta)
			{
			larga();
			return SOLTOU;
			}
		}
	else if(conta>0&&t-visto>0.15)
		conta=0; // antes de confirmar, só falhas curtas (até 0,15 s) no meio
	return NADA;
	}

/* ------------------------------- gravação */

bool Gravador::abre(const std::string& base,int sw,int sh,const int recorte[4],const int area[4],const double m[4][4],
                    const float caixa[4],const std::vector<float>& correcao,const std::string& limiares,double t)
	{
	fecha();
	char nome[64];
	std::time_t agora=std::time(0);
	std::tm lt;
	localtime_r(&agora,&lt);
	std::strftime(nome,sizeof(nome),"gesto-%Y%m%d-%H%M%S",&lt);
	pasta=base+"/"+nome;
	for(int i=2;mkdir(pasta.c_str(),0755)!=0;++i)
		{
		if(errno!=EEXIST||i>9)
			return false;
		pasta=base+"/"+nome+"-"+std::to_string(i);
		}
	w=sw;
	h=sh;
	for(int i=0;i<4;++i)
		{
		rec[i]=recorte[i];
		are[i]=area[i];
		cai[i]=caixa[i];
		for(int j=0;j<4;++j)
			mat[i][j]=m[i][j];
		}
	lims=limiares;
	porQuadro.clear();
	n=0;
	t0=t;
	size_t np=size_t(rec[2]-rec[0])*size_t(rec[3]-rec[1]);
	std::FILE* fc=std::fopen((pasta+"/correcao.f32").c_str(),"wb");
	bool certo=fc!=0&&correcao.size()==2*np&&std::fwrite(correcao.data(),sizeof(float),correcao.size(),fc)==correcao.size();
	if(fc!=0)
		certo=std::fclose(fc)==0&&certo;
	fCru=std::fopen((pasta+"/cru.u16").c_str(),"wb");
	fAreia=std::fopen((pasta+"/areia.u16").c_str(),"wb");
	if(!certo||fCru==0||fAreia==0)
		{
		fecha();
		return false;
		}
	escreveJson(false);
	return true;
	}

void Gravador::quadro(double t,const std::uint16_t* cru,const float* filtrado,const std::vector<MaoAberta>& maos,bool viu,float vx,float vy)
	{
	if(fCru==0)
		return;
	int cw=rec[2]-rec[0];
	bool certo=true;
	for(int y=rec[1];y<rec[3];++y)
		certo=std::fwrite(cru+size_t(y)*w+rec[0],sizeof(std::uint16_t),size_t(cw),fCru)==size_t(cw)&&certo;
	linha.resize(size_t(cw));
	for(int y=rec[1];y<rec[3];++y)
		{
		const float* f=filtrado+size_t(y)*w+rec[0];
		for(int x=0;x<cw;++x)
			linha[x]=std::isfinite(f[x])&&f[x]>0.0f&&f[x]<2047.0f?std::uint16_t(std::lround(f[x]*32.0f)):std::uint16_t(0);
		certo=std::fwrite(linha.data(),sizeof(std::uint16_t),size_t(cw),fAreia)==size_t(cw)&&certo;
		}
	if(!certo)
		{
		/* tmpfs cheio: para aqui, com o que já foi */
		fecha();
		return;
		}
	char b[160];
	std::snprintf(b,sizeof(b),"%s\n  {\"t\": %.3f, \"maos\": [",n?",":"",t-t0);
	porQuadro+=b;
	for(size_t i=0;i<maos.size();++i)
		{
		std::snprintf(b,sizeof(b),"%s[%.2f, %.2f, %.2f, %.2f]",i?", ":"",maos[i].x,maos[i].y,maos[i].z,maos[i].raio);
		porQuadro+=b;
		}
	std::snprintf(b,sizeof(b),"], \"vivo\": [%d, %.2f, %.2f]}",viu?1:0,viu?vx:0.0f,viu?vy:0.0f);
	porQuadro+=b;
	++n;
	}

int Gravador::fecha(void)
	{
	if(fCru==0&&fAreia==0)
		return n;
	if(fCru!=0)
		std::fclose(fCru);
	if(fAreia!=0)
		std::fclose(fAreia);
	fCru=fAreia=0;
	escreveJson(true);
	return n;
	}

void Gravador::escreveJson(bool pronta)
	{
	std::FILE* f=std::fopen((pasta+"/gesto.json").c_str(),"w");
	if(f==0)
		return;
	std::fprintf(f,"{\n \"versao\": 1,\n \"pronta\": %s,\n \"largura\": %d,\n \"altura\": %d,\n",pronta?"true":"false",w,h);
	std::fprintf(f," \"recorte\": [%d, %d, %d, %d],\n \"area\": [%d, %d, %d, %d],\n",rec[0],rec[1],rec[2],rec[3],are[0],are[1],are[2],are[3]);
	std::fprintf(f," \"matriz\": [");
	for(int i=0;i<16;++i)
		std::fprintf(f,"%s%.12g",i?", ":"",mat[i/4][i%4]);
	std::fprintf(f,"],\n \"caixa\": [%.3f, %.3f, %.3f, %.3f],\n",cai[0],cai[1],cai[2],cai[3]);
	std::fprintf(f," \"limiares\": \"%s\",\n",lims.c_str());
	std::fprintf(f," \"cru\": \"cru.u16\",\n \"areia\": \"areia.u16\",\n \"areiaEscala\": 32,\n \"correcao\": \"correcao.f32\",\n");
	std::fprintf(f," \"quadros\": [%s\n ]\n}\n",porQuadro.c_str());
	std::fclose(f);
	}

}
