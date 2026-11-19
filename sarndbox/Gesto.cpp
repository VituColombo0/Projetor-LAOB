/***********************************************************************
Gesto - o dedo apontado para cima visto de cima (Projetor-LAOB).
Veja Gesto.h. Os números dos limiares saíram das medidas do Kinect do laob
(18/11/2026): a 65-80 cm do sensor, 1 pixel = 1,1-1,4 mm e 1 unidade crua
de profundidade = 1,2-1,9 mm; ruído ~1 unidade numa superfície lisa e borda
"borrada" de 1-3 px. Um dedo de 1,5-2 cm tem 11-18 px de largura.
***********************************************************************/

#include "Gesto.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace Gesto {

namespace {
inline bool ok(float v) {return std::isfinite(v);}
}

/* ------------------------------- limiares */

Limiares::Limiares(void)
	:passo(4.0f),maoMin(3.0f),pontaMin(6.0f),anelGrosso(3.0f),degrauGrosso(3.0f),
	 picoQueda(1.5f),picoAreaMin(0.5f),picoAreaMax(7.0f),picoRaioMax(2.0f),
	 anelDentro(2.5f),anelFora(6.0f),maoAnel(2.0f),altoMax(3.5f),fracAltoMax(0.15f),punhoAreaMin(6.0f),
	 degrauMin(3.5f),degrauMax(14.0f),outroDentro(2.4f),outroQueda(2.5f),outroAreaMax(0.4f),
	 janela(7.0f),candidatosMax(3.0f)
	{
	}

namespace {
struct NomeLimiar {const char* nome; float Limiares::* campo;};
const NomeLimiar nomesLimiares[]={
	{"passo",&Limiares::passo},{"maoMin",&Limiares::maoMin},{"pontaMin",&Limiares::pontaMin},
	{"anelGrosso",&Limiares::anelGrosso},{"degrauGrosso",&Limiares::degrauGrosso},{"picoQueda",&Limiares::picoQueda},
	{"picoAreaMin",&Limiares::picoAreaMin},{"picoAreaMax",&Limiares::picoAreaMax},{"picoRaioMax",&Limiares::picoRaioMax},
	{"anelDentro",&Limiares::anelDentro},{"anelFora",&Limiares::anelFora},{"maoAnel",&Limiares::maoAnel},
	{"altoMax",&Limiares::altoMax},{"fracAltoMax",&Limiares::fracAltoMax},{"punhoAreaMin",&Limiares::punhoAreaMin},
	{"degrauMin",&Limiares::degrauMin},{"degrauMax",&Limiares::degrauMax},{"outroDentro",&Limiares::outroDentro},
	{"outroQueda",&Limiares::outroQueda},{"outroAreaMax",&Limiares::outroAreaMax},{"janela",&Limiares::janela},
	{"candidatosMax",&Limiares::candidatosMax}};
}

bool Limiares::ajusta(const std::string& nome,float valor)
	{
	if(!ok(valor))
		return false;
	for(size_t i=0;i<sizeof(nomesLimiares)/sizeof(nomesLimiares[0]);++i)
		if(nome==nomesLimiares[i].nome)
			{
			/* passo da grade e janela com limites (o custo cresce com eles) */
			if(nome=="passo") valor=std::max(2.0f,std::min(8.0f,valor));
			if(nome=="janela") valor=std::max(3.0f,std::min(10.0f,valor));
			if(nome=="candidatosMax") valor=std::max(1.0f,std::min(6.0f,valor));
			this->*nomesLimiares[i].campo=valor;
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

/* ------------------------------- busca grossa */

bool Detector::anelGrossoOk(const Quadro& q,int x0,int y0,int x1,int y1,int x,int y,float a) const
	{
	/* 8 pontos a anelGrosso cm: todos os que têm medida bem abaixo da ponta (pelo menos 4 com medida).
	   Na mão aberta, no punho, numa cabeça ou num monte de areia há sempre algo alto ali perto. */
	float cp=q.cmPorPixel(x,y);
	if(!(cp>0.0f))
		return false;
	float r=lim.anelGrosso/cp;
	int validos=0;
	for(int k=0;k<8;++k)
		{
		int sx=x+int(std::floor(r*std::cos(float(k)*0.7853982f)+0.5f));
		int sy=y+int(std::floor(r*std::sin(float(k)*0.7853982f)+0.5f));
		if(sx<x0||sy<y0||sx>=x1||sy>=y1)
			continue;
		float b=q.alt(sx,sy);
		if(!ok(b))
			continue; // sombra do infravermelho ao lado do dedo: não conta
		if(a-b<lim.degrauGrosso)
			return false;
		++validos;
		}
	return validos>=4;
	}

void Detector::procura(const Quadro& q,int x0,int y0,int x1,int y1,float rastroX,float rastroY,std::vector<Medida>& saida)
	{
	saida.clear();
	cands.clear();
	int P=std::max(2,int(lim.passo+0.5f));
	if(x1-x0<3*P||y1-y0<3*P)
		return;
	int gw=(x1-x0+P-1)/P,gh=(y1-y0+P-1)/P;
	ga.resize(size_t(gw)*gh);
	gm.resize(size_t(gw)*gh);
	for(int gy=0;gy<gh;++gy)
		for(int gx=0;gx<gw;++gx)
			{
			int x=x0+gx*P,y=y0+gy*P;
			float a=q.alt(x,y);
			size_t k=size_t(gy)*gw+gx;
			ga[k]=a;
			gm[k]=0;
			if(ok(a))
				{
				float f=q.fundo(x,y);
				gm[k]=ok(f)&&a-f>=lim.maoMin;
				}
			}

	/* o dedo já confirmado: o ponto mais alto da grade a até 3 cm de onde ele estava, sem exigir a altura sobre
	   a areia (parado, a mão acaba virando "areia" no quadro filtrado) */
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
			float f=q.fundo(x,y);
			if(!(ok(f)&&a-f>=lim.pontaMin))
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
		Medida m;
		classifica(q,x0,y0,x1,y1,c.x,c.y,c.exige,m);
		m.rastreio=!c.exige;
		saida.push_back(m);
		}
	}

/* ------------------------------- confirmação em resolução cheia */

void Detector::classifica(const Quadro& q,int x0,int y0,int x1,int y1,int cx,int cy,bool exigeAltura,Medida& m)
	{
	const float NaN=std::nanf("");
	m.cx=cx;
	m.cy=cy;
	m.px=float(cx);
	m.py=float(cy);
	m.ponta=m.sobreAreia=NaN;
	m.picoArea=m.picoRaio=m.degrau=m.punhoArea=m.fracAlto=m.outroArea=NaN;
	m.rastreio=!exigeAltura;
	m.dedo=false;
	m.motivo="sem medida";
	float cp=q.cmPorPixel(cx,cy);
	m.cmPx=cp;
	if(!(cp>0.0f))
		return;

	/* janela de "janela" cm em volta do candidato */
	int R=int(lim.janela/cp)+1;
	int wx0=std::max(x0,cx-R),wy0=std::max(y0,cy-R),wx1=std::min(x1,cx+R+1),wy1=std::min(y1,cy+R+1);
	int ww=wx1-wx0,wh=wy1-wy0;
	if(ww<3||wh<3)
		return;
	size_t n=size_t(ww)*size_t(wh);
	ja.resize(n);
	jf.resize(n);
	for(int y=wy0;y<wy1;++y)
		for(int x=wx0;x<wx1;++x)
			{
			size_t k=size_t(y-wy0)*ww+(x-wx0);
			ja[k]=q.alt(x,y);
			jf[k]=exigeAltura&&ok(ja[k])?q.fundo(x,y):NaN;
			}

	/* 1) ponta: o 3º ponto mais alto a até 1 cm do candidato (aguenta 2 pixels de ruído) */
	ordem.clear();
	int rT=std::max(2,int(1.0f/cp+0.5f));
	for(int y=std::max(wy0,cy-rT);y<=std::min(wy1-1,cy+rT);++y)
		for(int x=std::max(wx0,cx-rT);x<=std::min(wx1-1,cx+rT);++x)
			{
			int k=(y-wy0)*ww+(x-wx0);
			if(ok(ja[k])&&(x-cx)*(x-cx)+(y-cy)*(y-cy)<=rT*rT)
				ordem.push_back(k);
			}
	if(ordem.size()<3)
		return;
	std::nth_element(ordem.begin(),ordem.begin()+2,ordem.end(),[this](int u,int v){return ja[u]>ja[v];});
	int semente=ordem[2];
	float ponta=ja[semente];
	m.ponta=ponta;

	/* 2) pico: pixels ligados à ponta (4 vizinhos) a menos de picoQueda abaixo dela */
	marca.assign(n,0);
	pilha.clear();
	pilha.push_back(semente);
	marca[semente]=1;
	int limite=int(lim.picoAreaMax*1.5f/(cp*cp))+1;
	int nP=0;
	double sx=0.0,sy=0.0;
	bool transbordou=false;
	float corte=ponta-lim.picoQueda;
	while(!pilha.empty())
		{
		int k=pilha.back();
		pilha.pop_back();
		++nP;
		int x=k%ww,y=k/ww;
		sx+=x;
		sy+=y;
		if(nP>limite)
			{
			transbordou=true;
			break;
			}
		const int vx[4]={1,-1,0,0},vy[4]={0,0,1,-1};
		for(int v=0;v<4;++v)
			{
			int nx=x+vx[v],ny=y+vy[v];
			if(nx<0||ny<0||nx>=ww||ny>=wh)
				continue;
			int kk=ny*ww+nx;
			if(!marca[kk]&&ok(ja[kk])&&ja[kk]>corte)
				{
				marca[kk]=1;
				pilha.push_back(kk);
				}
			}
		}
	float mx=float(sx/nP),my=float(sy/nP); // centro do pico (na janela)
	m.px=float(wx0)+mx;
	m.py=float(wy0)+my;
	m.picoArea=float(nP)*cp*cp;
	float raio2=0.0f;
	for(size_t k=0;k<n;++k)
		if(marca[k])
			{
			float dx=float(int(k)%ww)-mx,dy=float(int(k)/ww)-my;
			raio2=std::max(raio2,dx*dx+dy*dy);
			}
	m.picoRaio=std::sqrt(raio2)*cp;
	{
	int tx=std::max(wx0,std::min(wx1-1,int(m.px+0.5f))),ty=std::max(wy0,std::min(wy1-1,int(m.py+0.5f)));
	float f=q.fundo(tx,ty);
	m.sobreAreia=ok(f)?ponta-f:NaN;
	}
	if(!medeTudo&&(transbordou||(exigeAltura&&!(m.sobreAreia>=lim.pontaMin))))
		{
		/* já se sabe que não é dedo (mão aberta, punho, cabeça...): o anel em volta fica sem medir */
		m.motivo=transbordou?"pico largo":"perto da areia";
		return;
		}

	/* 3) em volta: anel do punho e outros picos */
	float rIn=lim.anelDentro/cp,rOut=lim.anelFora/cp,rOutro=lim.outroDentro/cp;
	int nMao=0,nAlto=0,nPunho=0,nOutro=0;
	dzs.clear();
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
			if(d2>rOutro*rOutro&&dz<lim.outroQueda)
				++nOutro;
			if(d2<rIn*rIn||d2>rOut*rOut)
				continue;
			if(dz>16.0f)
				continue; // areia ou chão lá embaixo: não é mão
			if(exigeAltura&&!(ok(jf[k])&&a-jf[k]>=lim.maoAnel))
				continue;
			++nMao;
			if(dz<lim.altoMax)
				++nAlto;
			if(dz>=lim.degrauMin&&dz<=15.0f)
				++nPunho;
			dzs.push_back(dz);
			}
	m.punhoArea=float(nPunho)*cp*cp;
	m.fracAlto=nMao>0?float(nAlto)/float(nMao):1.0f;
	m.outroArea=float(nOutro)*cp*cp;
	if(!dzs.empty())
		{
		std::nth_element(dzs.begin(),dzs.begin()+dzs.size()/2,dzs.end());
		m.degrau=dzs[dzs.size()/2];
		}

	/* 4) decisão, na ordem */
	if(exigeAltura&&!(m.sobreAreia>=lim.pontaMin))
		m.motivo="perto da areia";
	else if(transbordou||m.picoArea>lim.picoAreaMax)
		m.motivo="pico largo";
	else if(m.picoArea<lim.picoAreaMin)
		m.motivo="pico pequeno";
	else if(m.picoRaio>lim.picoRaioMax)
		m.motivo="pico comprido";
	else if(m.punhoArea<lim.punhoAreaMin)
		m.motivo="sem punho embaixo";
	else if(m.fracAlto>lim.fracAltoMax)
		m.motivo="alto em volta";
	else if(!(m.degrau>=lim.degrauMin&&m.degrau<=lim.degrauMax))
		m.motivo="degrau";
	else if(m.outroArea>lim.outroAreaMax)
		m.motivo="outro pico";
	else
		{
		m.dedo=true;
		m.motivo="dedo";
		}
	}

/* ------------------------------- tempo */

Rastro::Rastro(void)
	:ativo(false),x(0.0f),y(0.0f),conta(0),acertos(4),raio(5.0f),solta(0.4f),falhas(0),visto(0.0)
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
			}
		else
			{
			x=0.6f*nx+0.4f*x;
			y=0.6f*ny+0.4f*y;
			}
		++conta;
		falhas=0;
		visto=t;
		if(!ativo&&conta>=acertos)
			{
			ativo=true;
			return CONFIRMOU;
			}
		return ativo?CONTINUA:NADA;
		}
	++falhas;
	if(ativo)
		{
		if(t-visto>solta)
			{
			larga();
			return SOLTOU;
			}
		}
	else if(falhas>=2)
		conta=0; // antes de confirmar, só uma falha no meio
	return NADA;
	}

}
