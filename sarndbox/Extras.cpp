/***********************************************************************
Extras - caça ao tesouro, peixes e carrinhos sobre o relevo (Projetor-LAOB).
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <cstdio>
#include <ctime>
#include <Geometry/ProjectiveTransformation.h>

namespace {
const float SEM=-1000.0f; // célula sem medida
const float PI=3.14159265f;
}

Extras::Extras(void)
	:celula(1.5f),gw(0),gh(0),mapaOk(false),referencia(0.0f),
	 tesouro(false),achado(false),tipoTesouro(1),tx(0),ty(0),traio(5.0f),tz(0),quandoAchou(0.0),
	 profundidade(3.0f),reaparece(20.0f),quantosAchados(0),
	 modo(NADA),quantos(0),nivelAgua(SEM),modoNivel(0),nivelFixo(0.0f),absorcao(0.02f),semente(12345u),tempo(0.0),efeito(SEM_EFEITO),
	 labirinto(false),labEstado(0),lsx(0),lsy(0),ltx(0),lty(0),lraio(4.0f),labVazao(1.0f),
	 labInicio(0.0),labChegou(0.0),labRodadas(0),labDificuldade(2),labZerarAgua(false)
	{
	bmin[0]=bmin[1]=-10.0f;
	bmax[0]=bmax[1]=10.0f;
	for(int i=0;i<16;++i)
		caixa[i]=(i%5==0)?1.0f:0.0f;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		ax[i]=ay[i]=ang[i]=vel[i]=vb[i]=parado[i]=encharcado[i]=0.0f;
		tipo[i]=0.0f;
		}
	nNomes=giroNomes=nFotos=0;
	temChuva=false;
	for(int k=0;k<MAX_CHUVAS;++k)
		{
		chuvas[k].x=chuvas[k].y=0.0f;
		chuvas[k].ate=-1.0;
		}
	mundoPedido=mundoPronto=false;
	camX=camAlvo=0.0f;
	camTroca=0.0;
	horaFixa=-1.0f;
	fatorVazao=0.66f;
	copia=false;
	copiaForma=copiaNivel=copiaRodadas=0;
	copiaRef=0.0f;
	copiaTol=1.1f;
	copiaNota=copiaAnunciada=0.0f;
	copiaInicio=copiaConseguiu=copiaAnuncio=0.0;
	for(int k=0;k<COPIA_MAX;++k)
		copiaG[k][0]=copiaG[k][1]=copiaG[k][2]=copiaG[k][3]=0.0f;
	assinaturaAntes=-1;
	trocaTempo=-100.0; // calibrado em 16/11/2026: com a água a 100% do tempo real, 0,66 devolve o ritmo dos jogos de quando ela andava a 66%
	for(int i=0;i<MAX_AGENTES;++i)
		{
		noLab[i]=false;
		mDest[i]=-1;
		wpN[i]=wpI[i]=wpCarro[i]=0;
		for(int k=0;k<5;++k)
			wpX[i][k]=wpY[i][k]=0.0f;
		alvoX[i]=alvoY[i]=mAte[i]=mTroca[i]=0.0f;
		temAlvo[i]=escondido[i]=false;
		mEstado[i]=0;
		mLugar[i]=mCasa[i]=-1;
		}
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	atlasNovo=false;
	atlasTextura=0;
	for(int i=0;i<MAX_NOMES;++i)
		{
		nomes[i][0]=0;
		presente[i]=false;
		}
	maosAntes=0;
	maoFalsaAte=-1.0;
	maoDiag=false;
	for(int k=0;k<MAX_MAOS;++k)
		{
		maos[k].x=maos[k].y=maos[k].r=0.0f;
		maos[k].visto=maos[k].desde=-100.0;
		}
	for(int i=0;i<MAX_AGENTES;++i)
		{
		acao[i]=0;
		presa[i]=-1;
		acaoT[i]=0.0f;
		voo[i]=escala[i]=1.0f;
		acaoAte[i]=0.0;
		}
	jogo=SEM_JOGO;
	jogoFase=0;
	jogoNivel=2;
	jogoInicio=jogoFim=0.0;
	jogoDuracao=0.0f;
	jogoPontos=jogoTotal=jogoRodadas=0;
	jogoVencedor=-1;
	proximoPortao=1;
	jogoPedido=false;
	nPecas=nFontes=0;
	vbx=vby=0.0f;
	for(int i=0;i<MAX_PECAS;++i)
		{
		pecas[i].tipo=P_NADA;
		pecas[i].x=pecas[i].y=pecas[i].raio=pecas[i].nivel=pecas[i].ang=0.0f;
		pecas[i].time=pecas[i].ordem=pecas[i].estado=0;
		}
	}

void Extras::setArea(const double areaMin[2],const double areaMax[2],const OGTransform& cameraToBox)
	{
	for(int i=0;i<2;++i)
		{
		bmin[i]=float(areaMin[i]);
		bmax[i]=float(areaMax[i]);
		}
	gw=std::max(4,int((bmax[0]-bmin[0])/celula)+1);
	gh=std::max(4,int((bmax[1]-bmin[1])/celula)+1);
	alt.assign(gw*gh,SEM);
	congela.assign(gw*gh,-1.0);
	fundo.assign(gw*gh,SEM);
	maoSeg.assign(gw*gh,0.0f);
	eraMao.assign(gw*gh,0);
	PTransform pb(cameraToBox);
	GLfloat* p=caixa;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++p)
			*p=GLfloat(pb.getMatrix()(i,j));
	}

void Extras::comecaMapa(void)
	{
	soma.assign(gw*gh,0.0f);
	conta.assign(gw*gh,0);
	}

void Extras::amostra(float x,float y,float z)
	{
	int cx=int((x-bmin[0])/celula),cy=int((y-bmin[1])/celula);
	if(cx<0||cy<0||cx>=gw||cy>=gh||std::abs(z)>80.0f)
		return;
	soma[cy*gw+cx]+=z;
	++conta[cy*gw+cx];
	}

void Extras::fechaMapa(void)
	{
	std::vector<float> todas;
	bool comMaos=congela.size()==alt.size()&&fundo.size()==alt.size();
	for(int i=0;i<gw*gh;++i)
		if(conta[i]>0)
			{
			alt[i]=soma[i]/float(conta[i]); // os bichos sempre usam a areia de agora (mexeram nela, eles veem na hora)
			todas.push_back(alt[i]);
			/* a referência das mãos não muda embaixo de uma mão: o filtro do sensor acaba aceitando a mão
			   parada como areia, e ela sumiria */
			if(comMaos&&!(congela[i]>tempo&&fundo[i]>SEM+1.0f))
				fundo[i]=alt[i];
			}
	if(todas.size()<10)
		return;
	std::nth_element(todas.begin(),todas.begin()+todas.size()/2,todas.end());
	referencia=todas[todas.size()/2];
	mapaOk=true;
	/* O lago dos bichos é água de verdade até o nível do mar (cavar um buraco faz a água brotar): */
	nivelAgua=(modo==PEIXES||modo==TODOS)?nivelLago():SEM;
	}

float Extras::altura(float x,float y) const
	{
	int cx=int((x-bmin[0])/celula),cy=int((y-bmin[1])/celula);
	if(!mapaOk||cx<0||cy<0||cx>=gw||cy>=gh)
		return SEM;
	/* interpolada entre os centros das células vizinhas: o desenho (que pinta o lago ponto a ponto)
	   e os bichos concordam sobre onde fica a margem; antes, com a célula inteira (1,5 cm) valendo uma
	   altura só, um carrinho podia ficar com metade dentro da água desenhada */
	float fx=(x-bmin[0])/celula-0.5f,fy=(y-bmin[1])/celula-0.5f;
	int x0=int(std::floor(fx)),y0=int(std::floor(fy));
	float ux=fx-float(x0),uy=fy-float(y0);
	float soma=0.0f,peso=0.0f;
	for(int j=0;j<2;++j)
		for(int i=0;i<2;++i)
			{
			int qx=std::max(0,std::min(gw-1,x0+i)),qy=std::max(0,std::min(gh-1,y0+j));
			float h=alt[qy*gw+qx];
			if(h<SEM+1.0f)
				continue;
			float w=(i?ux:1.0f-ux)*(j?uy:1.0f-uy);
			soma+=h*w;
			peso+=w;
			}
	return peso>1e-4f?soma/peso:alt[cy*gw+cx];
	}

/* ------------------------------------------------------------- mãos */

void Extras::comecaMaos(void)
	{
	somaM.assign(gw*gh,0.0f);
	contaM.assign(gw*gh,0);
	if(congela.size()!=alt.size()||fundo.size()!=alt.size())
		{
		fundo=alt;
		congela.assign(gw*gh,-1.0);
		maoSeg.assign(gw*gh,0.0f);
		eraMao.assign(gw*gh,0);
		}
	}

void Extras::amostraMao(float x,float y,float z)
	{
	int cx=int((x-bmin[0])/celula),cy=int((y-bmin[1])/celula);
	if(cx<0||cy<0||cx>=gw||cy>=gh||std::abs(z)>150.0f||contaM.empty())
		return;
	somaM[cy*gw+cx]+=z;
	++contaM[cy*gw+cx];
	}

namespace {
struct Mancha {std::vector<int> cel; float ex,ey,braco; int ne; float idade;};
bool maior(const Mancha& a,const Mancha& b) {return a.cel.size()>b.cel.size();}
}

void Extras::fechaMaos(float dt)
	{
	int n=gw*gh;
	if(int(contaM.size())!=n||int(alt.size())!=n||int(fundo.size())!=n||!mapaOk)
		return;
	/* 1. o que está acima da areia (média do quadro cru na célula contra o mapa de alturas) */
	const int beira=int(3.0f/celula+0.5f); // 3 cm da beirada iluminada: lá o sensor vê a borda da caixa
	std::vector<unsigned char> cru(n,0);
	std::vector<float> acima(n,0.0f);
	for(int cy=beira;cy<gh-beira;++cy)
		for(int cx=beira;cx<gw-beira;++cx)
			{
			int k=cy*gw+cx;
			if(contaM[k]<2||fundo[k]<SEM+1.0f)
				continue;
			acima[k]=somaM[k]/float(contaM[k])-fundo[k];
			float limiar=eraMao[k]?0.8f:1.2f; // histerese: a mão que já estava é seguida mesmo mais baixa
			cru[k]=acima[k]>limiar&&acima[k]<90.0f;
			}
	for(int k=0;k<n;++k)
		maoSeg[k]=cru[k]?maoSeg[k]+dt:0.0f;
	if(maoDiag)
		{
		/* calibração: sem ninguém na caixa a diferença fica perto de 0 e quase nenhuma célula passa do limiar */
		int nc=0,nAcima=0;
		double sm=0.0,sa=0.0;
		float mx=-1e9f,mn=1e9f;
		for(int k=0;k<n;++k)
			if(contaM[k]>=2&&fundo[k]>SEM+1.0f)
				{
				++nc;
				sm+=acima[k];
				sa+=std::abs(acima[k]);
				mx=std::max(mx,acima[k]);
				mn=std::min(mn,acima[k]);
				nAcima+=cru[k];
				}
		char t[200];
		std::snprintf(t,sizeof(t),"Maos diag: %d celulas, media %.2f cm, media abs %.2f cm, min %.1f max %.1f, acima do limiar %d",
		              nc,nc?sm/nc:0.0,nc?sa/nc:0.0,mn,mx,nAcima);
		anuncia(t);
		maoDiag=false;
		}
	/* 2. manchas ligadas (8 vizinhos) */
	std::vector<int> visto(n,0);
	std::vector<int> pilha;
	std::vector<Mancha> manchas;
	for(int k=0;k<n;++k)
		{
		if(!cru[k]||visto[k])
			continue;
		Mancha m;
		m.ex=m.ey=0.0f;
		m.ne=0;
		m.braco=0.0f;
		m.idade=0.0f;
		pilha.clear();
		pilha.push_back(k);
		visto[k]=1;
		while(!pilha.empty())
			{
			int q=pilha.back();
			pilha.pop_back();
			m.cel.push_back(q);
			m.idade+=maoSeg[q];
			int qx=q%gw,qy=q/gw;
			if(qx<=beira+1||qy<=beira+1||qx>=gw-beira-2||qy>=gh-beira-2)
				{
				m.ex+=float(qx);
				m.ey+=float(qy);
				++m.ne;
				m.braco=std::max(m.braco,acima[q]);
				}
			for(int oy=-1;oy<=1;++oy)
				for(int ox=-1;ox<=1;++ox)
					{
					int nx=qx+ox,ny=qy+oy;
					if(nx<0||ny<0||nx>=gw||ny>=gh)
						continue;
					int nq=ny*gw+nx;
					if(cru[nq]&&!visto[nq])
						{
						visto[nq]=1;
						pilha.push_back(nq);
						}
					}
			}
		m.idade/=float(m.cel.size());
		/* mão: o braço entra pela beirada (alto: mais de 4 cm acima da areia) ou a mancha é nova (monte de
		   areia recém-feito também aparece aqui, mas o filtro do sensor o aceita e ele some em ~1 s) */
		bool braco=m.ne>0&&m.braco>4.0f;
		if(m.cel.size()>=6&&(braco||m.idade<4.0f))
			{
			if(!braco)
				m.ne=0;
			manchas.push_back(m);
			}
		}
	std::sort(manchas.begin(),manchas.end(),maior);
	/* 3. posição da mão: com braço, a ponta (os últimos 9 cm da mancha, longe da beirada); sem, o meio */
	Mao velhas[MAX_MAOS-1],novas[MAX_MAOS-1];
	bool usada[MAX_MAOS-1];
	for(int k=0;k<MAX_MAOS-1;++k)
		{
		velhas[k]=maos[k];
		usada[k]=false;
		novas[k].x=novas[k].y=novas[k].r=0.0f;
		novas[k].visto=novas[k].desde=-100.0;
		}
	std::fill(eraMao.begin(),eraMao.end(),0);
	int nn=0;
	for(size_t b=0;b<manchas.size()&&nn<MAX_MAOS-1;++b)
		{
		const Mancha& m=manchas[b];
		float sx=0.0f,sy=0.0f;
		int c=0;
		if(m.ne>0)
			{
			float ex=m.ex/float(m.ne),ey=m.ey/float(m.ne),dmax=0.0f;
			for(size_t j=0;j<m.cel.size();++j)
				dmax=std::max(dmax,std::sqrt((m.cel[j]%gw-ex)*(m.cel[j]%gw-ex)+(m.cel[j]/gw-ey)*(m.cel[j]/gw-ey)));
			float corte=dmax-9.0f/celula;
			for(size_t j=0;j<m.cel.size();++j)
				if(std::sqrt((m.cel[j]%gw-ex)*(m.cel[j]%gw-ex)+(m.cel[j]/gw-ey)*(m.cel[j]/gw-ey))>=corte)
					{
					sx+=float(m.cel[j]%gw);
					sy+=float(m.cel[j]/gw);
					++c;
					}
			}
		else
			for(size_t j=0;j<m.cel.size();++j)
				{
				sx+=float(m.cel[j]%gw);
				sy+=float(m.cel[j]/gw);
				++c;
				}
		if(c==0)
			continue;
		float x=bmin[0]+(sx/float(c)+0.5f)*celula,y=bmin[1]+(sy/float(c)+0.5f)*celula;
		float r=std::max(2.5f,std::min(m.ne>0?7.0f:10.0f,std::sqrt(float(c)/PI)*celula));
		/* a mesma mão do quadro anterior: suaviza (a medida treme um pouco) */
		int perto=-1;
		float dmin=12.0f;
		for(int k=0;k<MAX_MAOS-1;++k)
			if(!usada[k]&&tempo-velhas[k].visto<1.0)
				{
				float d=std::sqrt((velhas[k].x-x)*(velhas[k].x-x)+(velhas[k].y-y)*(velhas[k].y-y));
				if(d<dmin)
					{
					dmin=d;
					perto=k;
					}
				}
		Mao h;
		h.x=x;
		h.y=y;
		h.r=r;
		h.visto=tempo;
		h.desde=tempo;
		if(perto>=0)
			{
			usada[perto]=true;
			h.x=0.35f*velhas[perto].x+0.65f*x;
			h.y=0.35f*velhas[perto].y+0.65f*y;
			h.r=0.5f*velhas[perto].r+0.5f*r;
			h.desde=velhas[perto].desde;
			}
		novas[nn++]=h;
		/* embaixo da mão (e em volta) a referência fica congelada por 3 s */
		for(size_t j=0;j<m.cel.size();++j)
			{
			int q=m.cel[j],qx=q%gw,qy=q/gw;
			eraMao[q]=1;
			for(int oy=-1;oy<=1;++oy)
				for(int ox=-1;ox<=1;++ox)
					if(qx+ox>=0&&qy+oy>=0&&qx+ox<gw&&qy+oy<gh)
						congela[(qy+oy)*gw+qx+ox]=tempo+3.0;
			}
		}
	/* mão que sumiu agora continua valendo por 0,6 s (um quadro ruim do sensor não espanta ninguém) */
	for(int k=0;k<MAX_MAOS-1&&nn<MAX_MAOS-1;++k)
		if(!usada[k]&&tempo-velhas[k].visto<0.6)
			novas[nn++]=velhas[k];
	for(int k=0;k<MAX_MAOS-1;++k)
		maos[k]=novas[k];
	int agora=0;
	for(int k=0;k<MAX_MAOS-1;++k)
		if(maos[k].visto==tempo)
			++agora;
	if(agora!=maosAntes)
		{
		char t[64];
		std::snprintf(t,sizeof(t),"Maos: %d",agora);
		anuncia(t);
		maosAntes=agora;
		}
	}

int Extras::maoPerto(float x,float y,float& borda,float idade) const
	{
	int m=-1;
	borda=1e9f;
	for(int k=0;k<MAX_MAOS;++k)
		{
		if(tempo-maos[k].visto>idade)
			continue;
		float d=std::sqrt((maos[k].x-x)*(maos[k].x-x)+(maos[k].y-y)*(maos[k].y-y))-maos[k].r;
		if(d<borda)
			{
			borda=d;
			m=k;
			}
		}
	return m;
	}

void Extras::maoTeste(float fx,float fy,float segundos)
	{
	Mao& h=maos[MAX_MAOS-1];
	h.x=bmin[0]+std::max(0.0f,std::min(1.0f,fx))*(bmax[0]-bmin[0]);
	h.y=bmin[1]+std::max(0.0f,std::min(1.0f,fy))*(bmax[1]-bmin[1]);
	h.r=4.5f;
	h.desde=h.visto=tempo;
	maoFalsaAte=tempo+std::max(0.0f,std::min(120.0f,segundos));
	}

bool Extras::dentro(float x,float y,float margem) const
	{
	return x>bmin[0]+margem&&x<bmax[0]-margem&&y>bmin[1]+margem&&y<bmax[1]-margem;
	}

namespace {
/* parte inteira do tipo: 1 peixe, 2 carro, 3 pessoa, 4 tubarão, 5 tartaruga, 6 água-viva, 7 polvo, 8 arraia,
   9 pato, 10 barquinho, 11 pássaro, 12 cachorro (a fração é a cor) */
inline bool aquatico(float t) {return t>0.5f&&(t<2.0f||(t>=4.0f&&t<11.0f));} // na água: peixes, bichos do mar, pato, barco
inline bool voador(float t) {return t>=11.0f&&t<12.0f;}                      // pássaro: voa por cima de tudo
inline bool ehPessoa(float t) {return (t>=3.0f&&t<4.0f)||(t>=12.0f&&t<13.0f);} // a pé (pessoa ou cachorro)
inline bool terrestre(float t) {return (t>=2.0f&&t<4.0f)||(t>=12.0f&&t<13.0f);} // em terra: carro, pessoa, cachorro
}

const char* Extras::nomeEspecie(int k)
	{
	static const char* nomes[NUM_ESPECIES]={"peixe","tubarao","tartaruga","aguaviva","polvo","arraia","pato","barco",
	                                        "carro","pessoa","cachorro","passaro"};
	return k>=0&&k<NUM_ESPECIES?nomes[k]:"";
	}

int Extras::codigoEspecie(int k)
	{
	static const int codigos[NUM_ESPECIES]={1,4,5,6,7,8,9,10,2,3,12,11};
	return k>=0&&k<NUM_ESPECIES?codigos[k]:1;
	}

void Extras::mar(const int quantidades[NUM_ESPECIES])
	{
	/* A escolha do painel: tantos de cada (animais marinhos, carrinhos e pessoas; até MAX_AGENTES no
	   total). Havendo algum marinho, existe o lago; só carros e pessoas, não. */
	int marinhos=0,deTerra=0;
	for(int k=0;k<NUM_ESPECIES;++k)
		(aquatico(float(codigoEspecie(k)))?marinhos:deTerra)+=quantidades[k];
	povoa(marinhos>0?(deTerra>0?TODOS:PEIXES):CARROS,0);
	int n=0;
	for(int k=0;k<NUM_ESPECIES;++k)
		for(int c=0;c<quantidades[k]&&n<MAX_AGENTES;++c,++n)
			{
			tipo[n]=float(codigoEspecie(k))+0.05f+0.9f*aleatorio();
			vel[n]=0.0f;
			if(mapaOk)
				nasce(n);
			}
	quantos=n;
	}

void Extras::chove(float fx,float fy,float segundos)
	{
	/* uma nuvem nova no lugar da que acaba primeiro */
	int livre=0;
	for(int k=1;k<MAX_CHUVAS;++k)
		if(chuvas[k].ate<chuvas[livre].ate)
			livre=k;
	chuvas[livre].x=bmin[0]+std::max(0.05f,std::min(0.95f,fx))*(bmax[0]-bmin[0]);
	chuvas[livre].y=bmin[1]+std::max(0.05f,std::min(0.95f,fy))*(bmax[1]-bmin[1]);
	chuvas[livre].ate=tempo+std::max(1.0f,std::min(60.0f,segundos));
	temChuva=true;
	}

/* ---------------------------------------------------------- copie o relevo */

const char* Extras::nomeForma(int f)
	{
	static const char* nomes[NUM_FORMAS]={"montanha","duas montanhas","vulcao","cratera","ilha e lago","serra"};
	return f>=0&&f<NUM_FORMAS?nomes[f]:"?";
	}

int Extras::formaDoNome(const char* nome)
	{
	static const char* curtos[NUM_FORMAS]={"montanha","duas","vulcao","cratera","ilha","serra"};
	for(int f=0;f<NUM_FORMAS;++f)
		if(std::string(nome)==curtos[f]||std::string(nome)==nomeForma(f))
			return f;
	return -1;
	}

void Extras::copiaNova(int forma,int nivel)
	{
	/* Sorteia o relevo-alvo em volta do meio da área iluminada. Fácil: mais baixo e com mais folga. */
	copia=true;
	copiaNivel=std::max(1,std::min(3,nivel));
	int f=forma>=0&&forma<NUM_FORMAS?forma:int(aleatorio()*NUM_FORMAS)%NUM_FORMAS;
	if(forma<0&&f==copiaForma&&copiaRodadas>0)
		f=(f+1+int(aleatorio()*(NUM_FORMAS-1)))%NUM_FORMAS; // não repete a mesma em seguida
	copiaForma=f;
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1];
	const float cx=(bmin[0]+bmax[0])*0.5f+(aleatorio()-0.5f)*0.25f*w,cy=(bmin[1]+bmax[1])*0.5f+(aleatorio()-0.5f)*0.2f*h;
	const float alto=copiaNivel==1?4.0f:copiaNivel==2?5.5f:7.0f; // cm acima da areia
	const float L=0.11f*w;                                     // largura típica de um morro
	for(int k=0;k<COPIA_MAX;++k)
		copiaG[k][0]=copiaG[k][1]=copiaG[k][2]=copiaG[k][3]=0.0f;
	auto poe=[&](int k,float x,float y,float a,float l){copiaG[k][0]=x; copiaG[k][1]=y; copiaG[k][2]=a; copiaG[k][3]=l;};
	float ang=aleatorio()*6.2831853f,dx=std::cos(ang),dy=std::sin(ang);
	switch(f)
		{
		case 0: poe(0,cx,cy,alto,L*1.2f); break;                                       // montanha
		case 1: poe(0,cx-dx*L*1.4f,cy-dy*L*1.0f,alto,L); poe(1,cx+dx*L*1.4f,cy+dy*L*1.0f,alto*0.75f,L*0.85f); break;
		case 2: poe(0,cx,cy,alto*1.15f,L*1.3f); poe(1,cx,cy,-alto*0.75f,L*0.42f); break;   // vulcão: morro com cratera
		case 3: poe(0,cx,cy,-alto*0.6f,L*1.1f); poe(1,cx,cy,alto*0.35f,L*1.9f); poe(2,cx,cy,-alto*0.2f,L*0.6f); break;
		case 4: poe(0,cx-dx*L*1.2f,cy-dy*L*0.9f,alto,L); poe(1,cx+dx*L*1.4f,cy+dy*L*1.0f,-alto*0.55f,L*0.9f); break;
		default: for(int k=0;k<3;++k) poe(k,cx+dx*L*1.3f*(k-1),cy+dy*L*0.9f*(k-1),alto*(k==1?1.0f:0.7f),L*0.8f); break;
		}
	copiaTol=copiaNivel==1?1.5f:copiaNivel==2?1.1f:0.8f;
	copiaRef=referencia;
	copiaNota=copiaAnunciada=0.0f;
	copiaInicio=tempo;
	copiaConseguiu=0.0;
	copiaAnuncio=tempo;
	char m[120];
	std::snprintf(m,sizeof(m),"Copia: forma %s (nível %d)",nomeForma(f),copiaNivel);
	anuncia(m);
	}

float Extras::copiaAlvo(float x,float y) const
	{
	float t=0.0f;
	for(int k=0;k<COPIA_MAX;++k)
		if(copiaG[k][3]>0.0f)
			{
			float dx=x-copiaG[k][0],dy=y-copiaG[k][1],l=copiaG[k][3];
			t+=copiaG[k][2]*std::exp(-(dx*dx+dy*dy)/(2.0f*l*l));
			}
	return t;
	}

void Extras::copiaMede(void)
	{
	/* nota: quanto do relevo-alvo já está feito. Só conta o erro que passa da folga (tol), dividido pelo
	   tamanho do alvo: areia lisa no começo = 0%, cópia perfeita = 100%; mexer onde não devia desconta. */
	if(!copia||!mapaOk)
		return;
	double erro=0.0,alvoTotal=0.0;
	for(int cy=0;cy<gh;++cy)
		for(int cx=0;cx<gw;++cx)
			{
			float hh=alt[cy*gw+cx];
			if(hh<SEM+1.0f)
				continue;
			float x=bmin[0]+(cx+0.5f)*celula,y=bmin[1]+(cy+0.5f)*celula;
			if(!dentro(x,y,3.0f))
				continue;
			float t=copiaAlvo(x,y);
			erro+=std::max(0.0f,std::abs(hh-(copiaRef+t))-copiaTol);
			alvoTotal+=std::max(0.0f,std::abs(t)-copiaTol);
			}
	if(alvoTotal<1.0)
		return;
	float nova=100.0f*float(std::max(0.0,std::min(1.0,1.0-erro/alvoTotal)));
	copiaNota=copiaNota*0.6f+nova*0.4f; // suaviza (mãos na areia e ruído do sensor)
	}

void Extras::copiaPasso(void)
	{
	if(!copia)
		return;
	char m[120];
	if(copiaConseguiu==0.0&&copiaNota>=85.0f&&tempo-copiaInicio>5.0)
		{
		copiaConseguiu=tempo;
		++copiaRodadas;
		std::snprintf(m,sizeof(m),"Copia: conseguiu! %d%% em %d s",int(copiaNota+0.5f),int(tempo-copiaInicio));
		anuncia(m);
		}
	else if(copiaConseguiu==0.0&&(std::abs(copiaNota-copiaAnunciada)>=5.0f||(tempo-copiaAnuncio>10.0&&std::abs(copiaNota-copiaAnunciada)>=1.0f)))
		{
		copiaAnunciada=copiaNota;
		copiaAnuncio=tempo;
		std::snprintf(m,sizeof(m),"Copia: %d%%",int(copiaNota+0.5f));
		anuncia(m);
		}
	if(copiaConseguiu>0.0&&tempo-copiaConseguiu>20.0)
		copiaNova(-1,copiaNivel); // outro relevo, sorteado
	}

float Extras::profAgua(float x,float y) const
	{
	/* profundidade da água da simulação, interpolada entre os centros das células (margem suave) */
	if(aguaSim.size()!=size_t(gw)*size_t(gh)||gw<1||gh<1)
		return 0.0f;
	float fx=(x-bmin[0])/celula-0.5f,fy=(y-bmin[1])/celula-0.5f;
	int x0=int(std::floor(fx)),y0=int(std::floor(fy));
	float ux=fx-float(x0),uy=fy-float(y0),soma=0.0f;
	for(int j=0;j<2;++j)
		for(int i=0;i<2;++i)
			{
			int qx=std::max(0,std::min(gw-1,x0+i)),qy=std::max(0,std::min(gh-1,y0+j));
			soma+=aguaSim[qy*gw+qx]*(i?ux:1.0f-ux)*(j?uy:1.0f-uy);
			}
	return soma;
	}

void Extras::correnteza(float x,float y,float& vx,float& vy) const
	{
	vx=vy=0.0f;
	if(aguaVx.size()!=size_t(gw)*size_t(gh))
		return;
	int cx=int((x-bmin[0])/celula),cy=int((y-bmin[1])/celula);
	if(cx<0||cy<0||cx>=gw||cy>=gh)
		return;
	vx=aguaVx[cy*gw+cx];
	vy=aguaVy[cy*gw+cx];
	float v=std::sqrt(vx*vx+vy*vy);
	if(v>12.0f) // onda forte de uma chuva em cima: não arremessa ninguém
		{
		vx*=12.0f/v;
		vy*=12.0f/v;
		}
	}

bool Extras::agua(float x,float y) const
	{
	/* Água para os bichos da água: a mesma da simulação (lago, rio de chuva, poça funda), com uns 4,5 mm */
	if(altura(x,y)<SEM+1.0f||!dentro(x,y,1.0f))
		return false;
	return profAgua(x,y)>=0.45f;
	}

bool Extras::aguaLarga(float x,float y,float raio) const
	{
	/* água no ponto e em volta dele: o peixe inteiro (uns 4,5 cm) cabe, sem a cauda na margem */
	if(!agua(x,y))
		return false;
	for(int k=0;k<8;++k)
		if(!agua(x+raio*std::cos(k*PI/4.0f),y+raio*std::sin(k*PI/4.0f)))
			return false;
	return true;
	}

bool Extras::molhado(float x,float y) const
	{
	/* para quem anda em terra: água ou quase (4 mm de folga acima do lago; água da chuva já rasa) */
	float h=altura(x,y);
	if(h<SEM+1.0f)
		return false;
	if(nivelAgua>SEM+1.0f&&h<nivelAgua+0.4f)
		return true;
	return profAgua(x,y)>=0.15f;
	}

bool Extras::chaoLivre(float x,float y,float raio) const
	{
	/* chão firme no ponto e em volta: areia medida, fora do lago e longe da borda iluminada
	   (antes só o centro do carrinho era conferido e metade dele entrava no lago ou saía da areia) */
	if(!dentro(x,y,raio+1.0f)||altura(x,y)<SEM+1.0f||molhado(x,y))
		return false;
	for(int k=0;k<8;++k) // 8 direções: o carrinho é comprido, 6 deixavam escapar a quina
		{
		float px=x+raio*std::cos(k*PI/4.0f),py=y+raio*std::sin(k*PI/4.0f);
		if(altura(px,py)<SEM+1.0f||molhado(px,py))
			return false;
		}
	return true;
	}

float Extras::aleatorio(void)
	{
	semente=semente*1664525u+1013904223u;
	return float((semente>>8)&0xffffff)/float(0x1000000);
	}

bool Extras::sorteiaPonto(bool queAgua,float& x,float& y)
	{
	for(int t=0;t<400;++t)
		{
		x=bmin[0]+3.0f+aleatorio()*(bmax[0]-bmin[0]-6.0f);
		y=bmin[1]+3.0f+aleatorio()*(bmax[1]-bmin[1]-6.0f);
		float h=altura(x,y);
		if(h<SEM+1.0f)
			continue;
		bool ok=queAgua?agua(x,y):((nivelAgua<SEM+1.0f||h>=nivelAgua+0.5f)&&chaoLivre(x,y,3.0f));
		if(ok&&queAgua)
			for(int k=0;k<8&&ok;++k) // lago de pelo menos uns 7 cm, não uma poça
				ok=agua(x+3.5f*std::cos(k*PI/4.0f),y+3.5f*std::sin(k*PI/4.0f));
		for(int j=0;j<quantos&&ok&&t<300;++j)
			if(vel[j]!=0.0f&&(ax[j]-x)*(ax[j]-x)+(ay[j]-y)*(ay[j]-y)<36.0f)
				ok=false; // não nasce em cima de outro
		if(ok)
			return true;
		}
	return false;
	}

/* ---------------------------------------------------------------- tesouro */

bool Extras::novoTesouro(float novaProfundidade)
	{
	if(!mapaOk)
		return false;
	profundidade=novaProfundidade;
	/* De preferência num lugar com areia acima da média (dá para cavar), longe da borda e,
	   quando é um tesouro novo depois de achar outro, longe de onde estava (mínimo 15 cm): */
	float x=0,y=0;
	bool ok=false,tinhaOutro=tesouro&&achado;
	float vx=tx,vy=ty;
	for(int t=0;t<800&&!ok;++t)
		{
		x=bmin[0]+6.0f+aleatorio()*(bmax[0]-bmin[0]-12.0f);
		y=bmin[1]+6.0f+aleatorio()*(bmax[1]-bmin[1]-12.0f);
		float h=altura(x,y);
		bool longe=!tinhaOutro||t>600||(x-vx)*(x-vx)+(y-vy)*(y-vy)>=15.0f*15.0f;
		ok=h>SEM+1.0f&&longe&&(h>=referencia||t>400);
		}
	if(!ok)
		return false;
	tx=x;
	ty=y;
	traio=(tipoTesouro>=2)?8.5f:5.0f;
	tz=altura(x,y)-profundidade;
	tesouro=true;
	achado=false;
	return true;
	}

bool Extras::confereTesouro(void)
	{
	if(!tesouro||achado||!mapaOk)
		return false;
	/* Achou quando alguma célula dentro do raio chegou à profundidade (folga de 0,3 cm): */
	for(float dy=-traio;dy<=traio;dy+=celula*0.5f)
		for(float dx=-traio;dx<=traio;dx+=celula*0.5f)
			if(dx*dx+dy*dy<=traio*traio*0.5f)
				{
				float h=altura(tx+dx,ty+dy);
				if(h>SEM+1.0f&&h<=tz+0.3f)
					{
					achado=true;
					quandoAchou=tempo;
					++quantosAchados;
					return true;
					}
				}
	return false;
	}

int Extras::efeitoDoTema(const char* nome)
	{
	static const struct {const char* tema;int efeito;} tabela[]={
		{"classico",SEM_EFEITO},{"vulcao",LAVA},{"inverno",NEVE},{"marte",POEIRA},{"deserto",AREIA},
		{"oceano",ONDAS},{"arco-iris",BRILHO},{"nada",SEM_EFEITO}};
	for(unsigned i=0;i<sizeof(tabela)/sizeof(tabela[0]);++i)
		if(std::strcmp(nome,tabela[i].tema)==0)
			return tabela[i].efeito;
	return -1;
	}

/* --------------------------------------------------------------- labirinto */

void Extras::novoLabirinto(int dificuldade)
	{
	labDificuldade=std::max(1,std::min(3,dificuldade));
	jogo=SEM_JOGO; // um jogo de água por vez
	jogoFase=0;
	nPecas=nFontes=0;
	float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],m=5.0f;
	/* Nascente num lado (ou canto) sorteado; alvo do lado oposto. Fácil: alvo no meio do caminho. */
	float ax,ay,bx,by;
	int lado=int(aleatorio()*4.0f)%4;
	if(labDificuldade==3)
		{
		ax=(lado&1)?bmax[0]-m:bmin[0]+m;
		ay=(lado&2)?bmax[1]-m:bmin[1]+m;
		bx=(lado&1)?bmin[0]+m:bmax[0]-m;
		by=(lado&2)?bmin[1]+m:bmax[1]-m;
		}
	else if(lado<2)
		{
		ax=lado==0?bmin[0]+m:bmax[0]-m;
		bx=lado==0?bmax[0]-m:bmin[0]+m;
		ay=bmin[1]+m+aleatorio()*(h-2.0f*m);
		by=bmin[1]+m+aleatorio()*(h-2.0f*m);
		}
	else
		{
		ay=lado==2?bmin[1]+m:bmax[1]-m;
		by=lado==2?bmax[1]-m:bmin[1]+m;
		ax=bmin[0]+m+aleatorio()*(w-2.0f*m);
		bx=bmin[0]+m+aleatorio()*(w-2.0f*m);
		}
	if(labDificuldade==1)
		{
		bx=ax+(bx-ax)*0.55f;
		by=ay+(by-ay)*0.55f;
		}
	lsx=ax;
	lsy=ay;
	ltx=bx;
	lty=by;
	lraio=labDificuldade==1?5.0f:4.0f;
	labVazao=labDificuldade==3?0.6f:1.0f;
	labirinto=true;
	labEstado=1;
	labInicio=tempo;
	labZerarAgua=true;
	armadilhasDoLabirinto();
	}

bool Extras::aguaNoAlvo(float nivel)
	{
	if(!labirinto||labEstado!=1||nivel<0.25f)
		return false;
	labEstado=2;
	labChegou=tempo;
	++labRodadas;
	return true;
	}

bool Extras::horaDeReesconder(void) const
	{
	return tesouro&&achado&&reaparece>0.0f&&tempo-quandoAchou>=reaparece;
	}

/* ------------------------------------------------------ peixes e carrinhos */

void Extras::nasce(int i)
	{
	acao[i]=0;
	presa[i]=-1;
	acaoT[i]=0.0f;
	voo[i]=1.0f;
	acaoAte[i]=tempo+2.0; // acabou de chegar: não sai caçando na hora
	if(modo!=MUNDO)
		temAlvo[i]=false;
	bool peixe=aquatico(tipo[i]);
	float x,y;
	if(!sorteiaPonto(peixe,x,y))
		{
		vel[i]=0.0f; // fica escondido até haver lugar (ex.: cavar um lago)
		return;
		}
	ax[i]=x;
	ay[i]=y;
	ang[i]=aleatorio()*2.0f*PI;
	int esp=int(tipo[i]);
	/* cada um no seu ritmo: tubarão rápido, tartaruga e polvo devagar, água-viva quase parada */
	vb[i]=esp==4?(5.0f+aleatorio()*1.8f):esp==5?(1.7f+aleatorio()*0.9f):esp==6?(0.8f+aleatorio()*0.6f):
	      esp==7?(1.5f+aleatorio()*1.0f):esp==8?(2.6f+aleatorio()*1.0f):
	      esp==9?(1.6f+aleatorio()*0.8f):esp==10?(2.2f+aleatorio()*0.8f):esp==11?(5.0f+aleatorio()*2.0f):
	      esp==12?(3.4f+aleatorio()*1.2f):
	      peixe?(4.0f+aleatorio()*3.0f):ehPessoa(tipo[i])?(2.2f+aleatorio()*1.4f):(6.0f+aleatorio()*4.0f);
	vel[i]=vb[i];
	parado[i]=0.0f;
	}

void Extras::povoa(int novoModo,int quantidade)
	{
	if(modo==MUNDO)
		nPecas=0; // os lugares da cidade somem com ela
	mundoPedido=mundoPronto=false;
	for(int i=0;i<MAX_AGENTES;++i)
		temAlvo[i]=escondido[i]=false;
	bool tinhaLago=lencol();
	modo=novoModo;
	nNomes=0;
	quantos=std::max(0,std::min(int(MAX_AGENTES),quantidade));
	if(modo==PEIXES||modo==TODOS)
		nivelAgua=nivelLago();
	else
		nivelAgua=SEM;
	/* o lago dos bichos é água de verdade: quando ele acaba (bichos tirados ou só de terra), a água dele vai
	   embora junto, como a dos jogos */
	if(tinhaLago&&!lencol())
		labZerarAgua=true;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		tipo[i]=0.0f;
		vel[i]=0.0f;
		if(i>=quantos||modo==NADA)
			continue;
		bool peixe=(modo==PEIXES)||(modo==TODOS&&i%3==0);
		bool pessoa=(modo==PESSOAS)||(modo==TODOS&&i%3==2);
		tipo[i]=(peixe?1.0f:pessoa?3.0f:2.0f)+0.05f+0.9f*aleatorio(); // a fração é a cor
		if(mapaOk)
			nasce(i);
		}
	}

bool Extras::carregaAtlas(const char* arquivo)
	{
	/* painel de rostos montado pelo membros.py: MAX_NOMES quadros de ATLAS_LADO, RGBA cru */
	size_t tam=size_t(ATLAS_LADO)*ATLAS_LADO*MAX_NOMES*4;
	std::vector<unsigned char> novo(tam);
	FILE* f=std::fopen(arquivo,"rb");
	if(f==0)
		return false;
	size_t lido=std::fread(&novo[0],1,tam,f);
	std::fclose(f);
	if(lido!=tam)
		return false;
	atlas.swap(novo);
	atlasNovo=true;
	return true;
	}

void Extras::membros(const std::vector<std::string>& lista,int giro,int fotos)
	{
	/* Uma pessoinha para cada membro presente (o painel manda a lista de quem o Willbert está
	   vendo no laboratório). Quem já estava continua andando de onde estava; quem chegou nasce
	   num lugar livre; quem saiu some. A cor da camisa sai do nome: é sempre a mesma. */
	int n=std::min(int(lista.size()),int(MAX_NOMES));
	float nx[MAX_NOMES],ny[MAX_NOMES],na[MAX_NOMES],nv[MAX_NOMES],nb[MAX_NOMES],nt[MAX_NOMES];
	bool novo[MAX_NOMES];
	char nn[MAX_NOMES][MAX_LETRAS+1];
	nFotos=atlas.empty()?0:std::max(0,std::min(fotos,n));
	for(int i=0;i<n;++i)
		{
		presente[i]=!lista[i].empty()&&lista[i][lista[i].size()-1]=='+'; // "NOME+" = presente agora
		int k=0;
		unsigned int soma=7u;
		for(size_t c=0;c<lista[i].size()&&k<MAX_LETRAS;++c)
			{
			char ch=lista[i][c];
			if(ch>='a'&&ch<='z')
				ch=char(ch-'a'+'A');
			if(ch>='A'&&ch<='Z')
				{
				nn[i][k++]=ch;
				soma=soma*31u+(unsigned int)ch;
				}
			}
		nn[i][k]=0;
		novo[i]=true;
		nt[i]=3.05f+0.9f*float(soma%97u)/97.0f;
		nx[i]=ny[i]=na[i]=nv[i]=nb[i]=0.0f;
		if(modo==MEMBROS)
			for(int j=0;j<nNomes&&j<quantos;++j)
				if(std::string(nomes[j])==nn[i])
					{
					nx[i]=ax[j]; ny[i]=ay[j]; na[i]=ang[j]; nv[i]=vel[j]; nb[i]=vb[j];
					novo[i]=false;
					}
		}
	if(lencol())
		labZerarAgua=true; // o lago dos bichos vai embora
	modo=MEMBROS;
	nivelAgua=SEM;
	giroNomes=((giro%4)+4)%4;
	nNomes=quantos=n;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		tipo[i]=0.0f;
		vel[i]=0.0f;
		parado[i]=0.0f;
		if(i>=n)
			continue;
		for(int c=0;c<=MAX_LETRAS;++c)
			nomes[i][c]=nn[i][c];
		tipo[i]=nt[i];
		if(novo[i])
			{
			if(mapaOk)
				nasce(i);
			}
		else
			{
			ax[i]=nx[i]; ay[i]=ny[i]; ang[i]=na[i]; vel[i]=nv[i]; vb[i]=nb[i];
			}
		}
	}

static float difAng(float a) // ângulo em -pi..pi
	{
	while(a>PI) a-=2.0f*PI;
	while(a<-PI) a+=2.0f*PI;
	return a;
	}

void Extras::passo(float dt)
	{
	tempo+=dt;
	/* troca de brincadeira: o desenho novo aparece aos poucos */
	long long sig=(long long)modo+10LL*jogo+1000LL*(labirinto?1:0)+10000LL*(tesouro?tipoTesouro:0)
	             +100000LL*(copia?1+copiaForma:0)+10000000LL*efeito;
	if(sig!=assinaturaAntes)
		{
		if(assinaturaAntes>=0)
			trocaTempo=tempo;
		assinaturaAntes=sig;
		}
	copiaPasso();
	if(maoFalsaAte>tempo)
		maos[MAX_MAOS-1].visto=tempo;
	if(temChuva)
		{
		temChuva=false;
		for(int k=0;k<MAX_CHUVAS;++k)
			temChuva=temChuva||chuvas[k].ate>tempo;
		}
	if(!mapaOk||modo==NADA)
		return;
	dt=std::min(dt,0.1f);
	if(modo==MUNDO)
		{
		mundoPasso(dt); // a cidade tem ruas e rotina próprias (Mundo.cpp): não usa o movimento pelo relevo
		return;
		}
	/* Patos: o primeiro é a mãe; os outros são patinhos, em fila atrás dela (cada um atrás do da frente) */
	int patoDaFrente[MAX_AGENTES];
	int ultimoPato=-1;
	for(int i=0;i<quantos;++i)
		{
		patoDaFrente[i]=-1;
		escala[i]=1.0f;
		if(int(tipo[i])==9&&vel[i]!=0.0f)
			{
			patoDaFrente[i]=ultimoPato;
			if(ultimoPato>=0)
				escala[i]=0.72f;
			ultimoPato=i;
			}
		}
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		if(modo==MUNDO&&(escondido[i]||!mundoPronto))
			continue; // dentro de uma construção (ou a cidade ainda não foi montada)
		bool peixe=aquatico(tipo[i]);
		if(vel[i]==0.0f)
			{
			if(aleatorio()<dt) // tenta aparecer de vez em quando
				nasce(i);
			continue;
			}
		float c0=std::cos(ang[i]),s0=std::sin(ang[i]);
		if(voador(tipo[i]))
			{
			passaro(i,dt);
			continue;
			}
		if(peixe)
			{
			const float raioA=int(tipo[i])==4?2.4f:1.8f*std::max(0.65f,escala[i]); // tubarão (comprido) precisa de água larga; patinho cabe na estreita
			bool folgado=aguaLarga(ax[i],ay[i],raioA);
			if(!agua(ax[i],ay[i]))
				{
				/* a água baixou embaixo dele (escorreu, foi absorvida): nada para onde está mais fundo; só
				   reaparece em outro lugar se ficar 2 s sem achar água */
				encharcado[i]+=dt;
				float p0=profAgua(ax[i],ay[i]),melhor=p0;
				int k0=-1;
				for(int k=0;k<12;++k)
					{
					float a=float(k)*PI/6.0f;
					float pk=profAgua(ax[i]+std::cos(a)*3.0f,ay[i]+std::sin(a)*3.0f);
					if(pk>melhor+0.02f)
						{
						melhor=pk;
						k0=k;
						}
					}
				if(encharcado[i]>2.0f||(p0<0.05f&&k0<0))
					{
					encharcado[i]=0.0f;
					nasce(i);
					continue;
					}
				if(k0>=0)
					{
					float a=float(k0)*PI/6.0f;
					ang[i]=difAng(ang[i]+std::max(-6.0f*dt,std::min(6.0f*dt,difAng(a-ang[i]))));
					ax[i]+=std::cos(a)*vb[i]*1.3f*dt;
					ay[i]+=std::sin(a)*vb[i]*1.3f*dt;
					}
				continue;
				}
			encharcado[i]=0.0f;
			/* Cardume: afasta dos muito perto, nada junto com os vizinhos e puxa para o grupo */
			float vx=c0,vy=s0,sepX=0.0f,sepY=0.0f,aliX=0.0f,aliY=0.0f,cx=0.0f,cy=0.0f;
			int n=0;
			for(int j=0;j<quantos;++j)
				{
				if(j==i||!aquatico(tipo[j])||int(tipo[j])!=int(tipo[i])||vel[j]==0.0f) // cardume só com os da mesma espécie
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i],d2=dx*dx+dy*dy;
				if(d2<0.01f||d2>100.0f)
					continue;
				if(d2<30.0f) // ~5,5 cm: um peixe tem ~4,5 cm; antes (3,5 cm) eles se empilhavam
					{
					float d=std::sqrt(d2);
					float f=(5.5f-d)/5.5f;
					sepX-=dx/d*f*1.5f;
					sepY-=dy/d*f*1.5f;
					}
				aliX+=std::cos(ang[j]);
				aliY+=std::sin(ang[j]);
				cx+=dx;
				cy+=dy;
				++n;
				}
			if(n>0)
				{
				vx+=0.45f*aliX/n+0.012f*cx/n;  // atração fraca: nadam juntos, sem amontoar
				vy+=0.45f*aliY/n+0.012f*cy/n;
				}
			vx+=2.0f*sepX;
			vy+=2.0f*sepY;
			/* Vida mais viva: patinhos em fila, o tubarão caça, fogem do perigo (da mão não) */
			const int esp=int(tipo[i]);
			float susto=0.0f,fila=-1.0f;
			bool cacando=false;
			if(esp==9&&patoDaFrente[i]>=0)
				{
				int f=patoDaFrente[i];
				float atras=1.75f*escala[f]+1.1f*escala[i]+0.3f; // logo atrás do rabo do pato da frente
				float tx=ax[f]-std::cos(ang[f])*atras,ty=ay[f]-std::sin(ang[f])*atras;
				float dx=tx-ax[i],dy=ty-ay[i];
				fila=std::sqrt(dx*dx+dy*dy);
				vx=dx/std::max(0.3f,fila)*2.0f+0.6f*std::cos(ang[f])+0.5f*sepX;
				vy=dy/std::max(0.3f,fila)*2.0f+0.6f*std::sin(ang[f])+0.5f*sepY;
				}
			if(esp==4&&acaoAte[i]<tempo)
				{
				/* tubarão com fome: vai atrás do peixe mais perto (a boca fica 2,8 cm à frente do meio) */
				int alvo=-1;
				float melhor=25.0f;
				for(int j=0;j<quantos;++j)
					if(int(tipo[j])==1&&vel[j]!=0.0f)
						{
						float d=std::sqrt((ax[j]-ax[i])*(ax[j]-ax[i])+(ay[j]-ay[i])*(ay[j]-ay[i]));
						if(d<melhor)
							{
							melhor=d;
							alvo=j;
							}
						}
				if(alvo>=0)
					{
					cacando=true;
					vx+=(ax[alvo]-ax[i])/std::max(0.5f,melhor)*3.0f;
					vy+=(ay[alvo]-ay[i])/std::max(0.5f,melhor)*3.0f;
					float bx=ax[i]+c0*2.8f-ax[alvo],by=ay[i]+s0*2.8f-ay[alvo];
					if(bx*bx+by*by<1.8f*1.8f)
						{
						nasce(alvo); // pegou: outro peixe aparece em outro lugar
						acaoAte[i]=tempo+12.0+6.0*aleatorio(); // satisfeito por um tempo
						anuncia("Vida: o tubarao pegou um peixe");
						}
					}
				}
			/* a mão NÃO espanta os bichos da água (Victor, 17/11/2026: tirar o espantar) */
			if(esp==1||esp==9||esp==5)
				for(int j=0;j<quantos;++j)
					{
					/* peixe, pato e tartaruga fogem do tubarão; o peixe foge também do pássaro que mergulha */
					bool perigo=(int(tipo[j])==4&&vel[j]!=0.0f)||(esp==1&&voador(tipo[j])&&acao[j]==2);
					if(!perigo)
						continue;
					float dx=ax[i]-ax[j],dy=ay[i]-ay[j],d=std::sqrt(dx*dx+dy*dy),raio=int(tipo[j])==4?12.0f:8.0f;
					if(d<raio&&d>0.01f)
						{
						float f=1.0f-d/raio;
						vx+=dx/d*4.0f*f;
						vy+=dy/d*4.0f*f;
						susto=std::max(susto,0.8f*f);
						}
					}
			/* Passeio: cada peixe vira devagar para um lado e para o outro */
			float vaga=0.8f*std::sin(float(tempo)*0.6f+float(i)*1.7f)*dt;
			float desejo=std::atan2(vy,vx)+vaga;
			/* Desvia das margens: a primeira direção livre perto da desejada */
			const float desvios[]={0.0f,0.4f,-0.4f,0.8f,-0.8f,1.3f,-1.3f,1.9f,-1.9f,2.6f,-2.6f,PI};
			bool livre=false;
			for(int passada=0;passada<2&&!livre;++passada) // 1ª: com folga da margem; 2ª (encostado): qualquer água
				for(int k=0;k<12;++k)
					{
					float a=desejo+desvios[k];
					float x1=ax[i]+std::cos(a)*1.5f,y1=ay[i]+std::sin(a)*1.5f;
					float x3=ax[i]+std::cos(a)*3.5f,y3=ay[i]+std::sin(a)*3.5f;
					if(passada==0?(aguaLarga(x3,y3,1.8f)&&aguaLarga(x1,y1,1.8f)):(agua(x3,y3)&&agua(x1,y1)))
						{
						desejo=a;
						livre=true;
						break;
						}
					}
			float giro=livre?difAng(desejo-ang[i]):2.0f;
			const float virar=(fila>=0.0f?4.0f:2.8f)*(1.0f+0.8f*susto);
			ang[i]=difAng(ang[i]+std::max(-virar*dt,std::min(virar*dt,giro)));
			/* Nadadas: acelera e desacelera de leve, mais devagar quando está virando */
			float alvoVel=vb[i]*(0.85f+0.25f*std::sin(float(tempo)*2.3f+float(i)))*(1.0f-0.35f*std::min(1.0f,std::abs(giro)));
			if(fila>=0.0f) // patinho: acompanha a mãe; atrasado, corre
				alvoVel=std::max(0.5f,std::min(4.5f,vel[patoDaFrente[i]]*0.9f+1.4f*fila));
			if(cacando)
				alvoVel=vb[i]*1.6f;
			alvoVel*=1.0f+0.9f*susto;
			vel[i]+=(alvoVel-vel[i])*std::min(1.0f,2.0f*dt);
			vel[i]=std::max(vel[i],0.5f);
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			/* a correnteza (rio de chuva, água entrando no lago) leva junto: quem boia (pato, barquinho,
			   água-viva) vai inteiro com ela; quem nada, só um pouco; o tubarão quase nada */
			float cvx,cvy;
			correnteza(ax[i],ay[i],cvx,cvy);
			const float leva=(esp==9||esp==10||esp==6)?0.9f:esp==4?0.15f:0.45f;
			nx+=cvx*leva*dt;
			ny+=cvy*leva*dt;
			/* só avança se o corpo inteiro continuar na água (encostado na margem, qualquer água serve
			   para ele conseguir sair dali) */
			if(aguaLarga(nx,ny,raioA)||(!folgado&&agua(nx,ny)))
				{
				ax[i]=nx;
				ay[i]=ny;
				parado[i]=folgado?0.0f:parado[i]+dt;
				}
			else
				parado[i]+=dt;
			if(parado[i]>4.0f) // preso numa poça ou espremido na margem: aparece num lago de verdade
				nasce(i);
			}
		else
			{
			/* Carrinho: entre as direções possíveis, a que segue mais baixo (o vale), sem subir
			   rampa íngreme, sem entrar no lago e sem sair da área iluminada: */
			float h0=altura(ax[i],ay[i]);
			if(h0<SEM+1.0f||!dentro(ax[i],ay[i],1.0f))
				{
				nasce(i); // a areia sumiu dali: aparece em chão firme
				continue;
				}
			if(molhado(ax[i],ay[i]))
				{
				/* a água chegou (chuva escorrendo, lago subindo): corre para o seco mais perto, na direção
				   em que está andando se der; só reaparece em outro lugar se ficar 4 s na água ou se afundar */
				encharcado[i]+=dt;
				float melhorA=0.0f,melhorD=1e9f;
				bool achou=false;
				for(int r=1;r<=4&&!achou;++r)
					for(int k=0;k<16;++k)
						{
						float a=ang[i]+float((k+1)/2)*((k%2)?0.39f:-0.39f);
						float d=2.5f*float(r);
						if(chaoLivre(ax[i]+std::cos(a)*d,ay[i]+std::sin(a)*d,1.2f))
							{
							float custo=d+1.5f*std::abs(difAng(a-ang[i]));
							if(custo<melhorD)
								{
								melhorD=custo;
								melhorA=a;
								achou=true;
								}
							}
						}
				if(!achou||encharcado[i]>4.0f||profAgua(ax[i],ay[i])>1.5f)
					{
					encharcado[i]=0.0f;
					nasce(i);
					continue;
					}
				ang[i]=difAng(ang[i]+std::max(-5.0f*dt,std::min(5.0f*dt,difAng(melhorA-ang[i]))));
				vel[i]+=(1.6f*vb[i]-vel[i])*std::min(1.0f,4.0f*dt);
				float cvx,cvy;
				correnteza(ax[i],ay[i],cvx,cvy); // a enxurrada empurra
				ax[i]+=(std::cos(ang[i])*vel[i]+0.3f*cvx)*dt;
				ay[i]+=(std::sin(ang[i])*vel[i]+0.3f*cvy)*dt;
				parado[i]=0.0f;
				continue;
				}
			encharcado[i]=0.0f;
			if(int(tipo[i])==12&&cachorro(i,dt))
				{
				vel[i]=std::max(0.05f,vel[i]*(1.0f-std::min(1.0f,4.0f*dt)));
				parado[i]=0.0f;
				continue; // sentado do lado da mão, abanando o rabo
				}
			const float desvios[]={0.0f,0.35f,-0.35f,0.7f,-0.7f,1.1f,-1.1f};
			/* Pessoa (tipo 3): mesmo jeito de andar do carrinho, com corpo menor, passo curto e um
			   vai-e-vem de quem passeia */
			const bool pessoa=ehPessoa(tipo[i]);
			const float rc=pessoa?1.7f:2.2f,olha=pessoa?3.5f:4.0f,perto=pessoa?5.5f:7.0f;
			float melhor=1e9f,escolha=PI,hEscolha=h0;
			for(int k=0;k<7;++k)
				{
				float a=ang[i]+desvios[k];
				float px=ax[i]+std::cos(a)*olha,py=ay[i]+std::sin(a)*olha;
				float h=altura(px,py);
				/* à frente tem que caber o carrinho inteiro em chão firme, sem paredão nem barranco */
				if(modo==MEMBROS&&!dentro(px,py,6.0f)&&dentro(ax[i],ay[i],6.0f))
					continue; // membro: longe da beirada, para a plaquinha do nome caber inteira
				if(!chaoLivre(px,py,rc)||!chaoLivre(ax[i]+std::cos(a)*olha*0.5f,ay[i]+std::sin(a)*olha*0.5f,rc)
				   ||((h-h0>2.0f||h0-h>3.0f)&&!(modo==MUNDO&&pessoa)))
					continue; // (no mundo LAOB quem está a pé sobe e desce ladeira: precisa chegar aos lugares)
				/* vale: baixo, mas sem despencar (segue a curva de nível); longe dos outros carros */
				float nota=0.35f*h+0.6f*std::abs(h-h0)+0.8f*std::abs(desvios[k]);
				if(pessoa) // passeia: de tempos em tempos prefere virar para um lado
					nota=0.5f*std::abs(h-h0)+0.6f*std::abs(desvios[k]-0.7f*std::sin(float(tempo)*0.35f+float(i)*2.3f));
				if(temAlvo[i]) // (cidade/laboratório, ou o cachorro indo até a mão ou atrás de um pássaro)
					{
					/* tem destino: prefere a direção que aponta para ele (o relevo ainda pesa) */
					float rumo=difAng(std::atan2(alvoY[i]-ay[i],alvoX[i]-ax[i])-ang[i]);
					rumo=std::max(-1.1f,std::min(1.1f,rumo));
					nota=0.4f*std::abs(h-h0)+0.9f*std::abs(desvios[k]-rumo);
					}
				for(int j=0;j<quantos;++j)
					if(j!=i&&terrestre(tipo[j])&&vel[j]!=0.0f)
						{
						float d=std::sqrt((ax[j]-px)*(ax[j]-px)+(ay[j]-py)*(ay[j]-py));
						if(d<perto)
							nota+=2.5f*(1.0f-d/perto);
						}
				if(nota<melhor)
					{
					melhor=nota;
					escolha=desvios[k];
					hEscolha=h;
					}
				}
			/* Outro carrinho logo à frente: freia e desvia para o lado (só para os de número menor:
			   assim dois nunca ficam esperando um pelo outro) */
			float frente=1e9f;
			for(int j=0;j<i;++j)
				{
				if(!terrestre(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i];
				float adiante=dx*c0+dy*s0,lado=-dx*s0+dy*c0;
				if(adiante>0.0f&&adiante<perto&&std::abs(lado)<(pessoa?1.8f:3.0f))
					{
					frente=std::min(frente,adiante);
					if(escolha!=PI)
						escolha+=(lado>0.0f?-0.4f:0.4f);
					}
				}
			if(escolha==PI) // sem saída: vira no lugar até achar caminho
				{
				ang[i]=difAng(ang[i]+2.5f*dt);
				vel[i]+=(0.3f*vb[i]-vel[i])*std::min(1.0f,3.0f*dt);
				continue;
				}
			ang[i]=difAng(ang[i]+std::max(-1.8f*dt,std::min(1.8f*dt,escolha)));
			/* Desce mais rápido, sobe devagar; curva e carro à frente freiam */
			float rampa=h0>SEM+1.0f?(hEscolha-h0):0.0f;
			float alvoVel=vb[i]*std::max(0.45f,std::min(1.35f,1.0f-0.25f*rampa))*(1.0f-0.3f*std::min(1.0f,std::abs(escolha)));
			if(int(tipo[i])==12&&(acao[i]==3||acao[i]==4))
				alvoVel*=acao[i]==4?1.8f:1.5f; // corre até a mão / atrás do pássaro
			if(frente<perto)
				alvoVel*=std::max(0.0f,(frente-perto*0.43f)/(perto*0.57f));
			vel[i]+=(alvoVel-vel[i])*std::min(1.0f,2.5f*dt);
			vel[i]=std::max(vel[i],0.05f); // 0 é "escondido"
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			bool andou=chaoLivre(nx,ny,rc)||(!chaoLivre(ax[i],ay[i],rc)&&dentro(nx,ny,1.5f)&&!agua(nx,ny)&&altura(nx,ny)>SEM+1.0f);
			if(andou)
				{
				ax[i]=nx;
				ay[i]=ny;
				}
			/* Carrinhos lado a lado (o freio só vale para o da frente): empurra para se separarem */
			for(int j=0;j<quantos;++j)
				{
				if(j==i||!terrestre(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[i]-ax[j],dy=ay[i]-ay[j],d2=dx*dx+dy*dy;
				float junto=(pessoa&&ehPessoa(tipo[j]))?3.8f:4.5f; // duas pessoas chegam mais perto que dois carros
				if(d2<junto*junto&&d2>1e-4f) // ~4,5 cm (um carrinho tem ~4,3 x 2,1 cm)
					{
					float d=std::sqrt(d2),empurra=std::min(0.5f,(junto-d)*0.25f);
					float px=ax[i]+dx/d*empurra,py=ay[i]+dy/d*empurra;
					if(chaoLivre(px,py,rc))
						{
						ax[i]=px;
						ay[i]=py;
						}
					}
				}
						/* Travado (devagar ou sem andar) por 3 s: dá meia-volta */
			parado[i]=(!andou||vel[i]<0.3f*vb[i])?parado[i]+dt:0.0f;
			if(parado[i]>3.0f)
				{
				ang[i]=difAng(ang[i]+PI*(0.75f+0.5f*aleatorio()));
				vel[i]=0.5f*vb[i];
				parado[i]=0.0f;
				}
			}
		}
	}

/* ------------------------------------------------- pássaros e cachorros */

bool Extras::lugarDePouso(int i,int m)
	{
	/* um lugar no chão perto da mão, do lado de onde o pássaro vem e longe dos outros que vão pousar */
	const Mao& h=maos[m];
	float base=std::atan2(ay[i]-h.y,ax[i]-h.x);
	for(int k=0;k<13;++k)
		{
		float a=base+((k%2)?1.0f:-1.0f)*float((k+1)/2)*0.45f;
		float d=h.r+3.5f+1.5f*aleatorio();
		float x=h.x+std::cos(a)*d,y=h.y+std::sin(a)*d;
		if(!dentro(x,y,2.5f)||!chaoLivre(x,y,1.5f))
			continue; // chão firme, sem água e sem paredão
		bool ocupado=false;
		for(int j=0;j<quantos&&!ocupado;++j)
			if(j!=i&&voador(tipo[j])&&(acao[j]==1||acao[j]==3))
				ocupado=(alvoX[j]-x)*(alvoX[j]-x)+(alvoY[j]-y)*(alvoY[j]-y)<7.0f;
		if(ocupado)
			continue;
		alvoX[i]=x;
		alvoY[i]=y;
		temAlvo[i]=true;
		return true;
		}
	return false;
	}

void Extras::passaro(int i,float dt)
	{
	/* Pássaro: voa em curvas largas por cima de tudo. Vê uma mão: desce e pousa perto dela (cada um do seu
	   lado) e fica bicando o chão; sai voando quando a mão some ou chega um cachorro (a mão não espanta).
	   Vê um peixe: mergulha para pescar (às vezes consegue). voo = altura (1 alto, 0 no chão). */
	acaoT[i]+=dt;
	float borda;
	int m=maoPerto(ax[i],ay[i],borda,(acao[i]==1||acao[i]==3)?2.0f:0.6f);
	int cao=-1;
	float dCao=1e9f;
	for(int j=0;j<quantos;++j)
		if(int(tipo[j])==12&&vel[j]!=0.0f)
			{
			float d=std::sqrt((ax[j]-ax[i])*(ax[j]-ax[i])+(ay[j]-ay[i])*(ay[j]-ay[i]));
			if(d<dCao)
				{
				dCao=d;
				cao=j;
				}
			}
	float c0=std::cos(ang[i]),s0=std::sin(ang[i]);
	if(acao[i]==1||acao[i]==3)
		{
		bool sai=m<0||dCao<7.0f||(acao[i]==1&&(acaoT[i]>25.0f||agua(ax[i],ay[i])));
		if(!sai)
			{
			/* a mão andou: escolhe outro lugar perto dela */
			const Mao& h=maos[m];
			float dl=std::sqrt((alvoX[i]-h.x)*(alvoX[i]-h.x)+(alvoY[i]-h.y)*(alvoY[i]-h.y))-h.r;
			if(dl<2.0f||dl>9.0f)
				{
				if(lugarDePouso(i,m))
					acao[i]=3;
				else
					sai=true;
				}
			}
		if(sai)
			{
			/* levanta voo para longe de quem assustou */
			float fx=std::cos(ang[i]+2.0f),fy=std::sin(ang[i]+2.0f);
			if(dCao<7.0f)
				{
				fx=ax[i]-ax[cao];
				fy=ay[i]-ay[cao];
				}
			else if(m>=0)
				{
				fx=ax[i]-maos[m].x;
				fy=ay[i]-maos[m].y;
				}
			ang[i]=std::atan2(fy,fx);
			acao[i]=0;
			acaoT[i]=0.0f;
			temAlvo[i]=false;
			acaoAte[i]=tempo+6.0;
			}
		else if(acao[i]==1)
			{
			/* pousado: olha para a mão de vez em quando e bica o chão (o desenho faz a bicada) */
			voo[i]=std::max(0.0f,voo[i]-2.0f*dt);
			float olha=std::atan2(maos[m].y-ay[i],maos[m].x-ax[i])+0.9f*std::sin(float(tempo)*0.5f+float(i)*1.3f);
			ang[i]=difAng(ang[i]+std::max(-1.5f*dt,std::min(1.5f*dt,difAng(olha-ang[i]))));
			return;
			}
		else
			{
			/* descendo para o lugar escolhido */
			float dx=alvoX[i]-ax[i],dy=alvoY[i]-ay[i],d=std::sqrt(dx*dx+dy*dy);
			if(d<0.6f)
				{
				acao[i]=1;
				acaoT[i]=0.0f;
				voo[i]=0.0f;
				return;
				}
			float giro=difAng(std::atan2(dy,dx)-ang[i]);
			ang[i]=difAng(ang[i]+std::max(-3.5f*dt,std::min(3.5f*dt,giro)));
			float v=vb[i]*std::max(0.25f,std::min(1.0f,d/6.0f))*(std::abs(giro)>1.2f?0.6f:1.0f);
			ax[i]+=std::cos(ang[i])*v*dt;
			ay[i]+=std::sin(ang[i])*v*dt;
			voo[i]=std::min(1.0f,d/8.0f);
			return;
			}
		}
	if(acao[i]==2)
		{
		/* mergulho de 1,8 s: desce até a água em cima do peixe, encosta (pega ou não) e sobe */
		float t=acaoT[i]/1.8f;
		int p=presa[i];
		if(t<0.5f&&p>=0&&vel[p]!=0.0f)
			{
			float giro=difAng(std::atan2(ay[p]-ay[i],ax[p]-ax[i])-ang[i]);
			ang[i]=difAng(ang[i]+std::max(-4.0f*dt,std::min(4.0f*dt,giro)));
			}
		if(t>=0.5f&&p>=0)
			{
			float dx=ax[p]-ax[i],dy=ay[p]-ay[i];
			if(vel[p]!=0.0f&&dx*dx+dy*dy<2.5f*2.5f&&aleatorio()<0.45f)
				{
				nasce(p);
				anuncia("Vida: um passaro pescou um peixe");
				}
			presa[i]=-1;
			}
		voo[i]=std::min(1.0f,std::abs(1.0f-2.0f*t));
		float v=vb[i]*(0.25f+0.75f*voo[i]);
		ax[i]+=std::cos(ang[i])*v*dt;
		ay[i]+=std::sin(ang[i])*v*dt;
		if(t>=1.0f)
			{
			acao[i]=0;
			acaoT[i]=0.0f;
			voo[i]=1.0f;
			acaoAte[i]=tempo+10.0+10.0*aleatorio();
			}
		}
	else
		{
		voo[i]=std::min(1.0f,voo[i]+dt);
		if(m>=0&&acaoAte[i]<tempo&&lugarDePouso(i,m))
			{
			acao[i]=3;
			acaoT[i]=0.0f;
			return;
			}
		if(acaoAte[i]<tempo)
			{
			/* um peixe embaixo, um pouco à frente: mergulha */
			for(int j=0;j<quantos;++j)
				{
				if(int(tipo[j])!=1||vel[j]==0.0f)
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i];
				if(dx*dx+dy*dy<10.0f*10.0f&&dx*c0+dy*s0>0.0f&&agua(ax[j],ay[j]))
					{
					acao[i]=2;
					acaoT[i]=0.0f;
					presa[i]=j;
					break;
					}
				}
			if(acao[i]!=2)
				acaoAte[i]=tempo+0.5; // olha de novo daqui a pouco
			}
		/* passeio: curvas largas; perto da beirada vira de volta para o meio */
		float vira=0.8f*std::sin(float(tempo)*0.37f+float(i)*2.1f)+0.35f*std::sin(float(tempo)*1.13f+float(i)*0.7f);
		if(!dentro(ax[i]+c0*7.0f,ay[i]+s0*7.0f,3.0f))
			{
			float cx=(bmin[0]+bmax[0])*0.5f-ax[i],cy=(bmin[1]+bmax[1])*0.5f-ay[i];
			vira=difAng(std::atan2(cy,cx)-ang[i])>0.0f?1.6f:-1.6f;
			}
		ang[i]=difAng(ang[i]+vira*dt);
		ax[i]+=std::cos(ang[i])*vb[i]*dt;
		ay[i]+=std::sin(ang[i])*vb[i]*dt;
		}
	if(!dentro(ax[i],ay[i],0.5f))
		{
		ax[i]=std::max(bmin[0]+1.0f,std::min(bmax[0]-1.0f,ax[i]));
		ay[i]=std::max(bmin[1]+1.0f,std::min(bmax[1]-1.0f,ay[i]));
		}
	}

bool Extras::cachorro(int i,float dt)
	{
	/* Cachorro: vê uma mão, corre até ela e senta do lado, olhando para ela e abanando o rabo; sem mão, de
	   vez em quando corre atrás de um pássaro (o pousado sai voando). true = está sentado. */
	acaoT[i]+=dt;
	float borda;
	int m=maoPerto(ax[i],ay[i],borda,(acao[i]==1||acao[i]==3)?2.0f:0.6f);
	if(m>=0)
		{
		const Mao& h=maos[m];
		float dx=ax[i]-h.x,dy=ay[i]-h.y,d=std::max(0.1f,std::sqrt(dx*dx+dy*dy));
		if(acao[i]==1&&borda>0.8f&&borda<7.0f)
			{
			float giro=difAng(std::atan2(-dy,-dx)-ang[i]);
			ang[i]=difAng(ang[i]+std::max(-3.0f*dt,std::min(3.0f*dt,giro)));
			return true;
			}
		if(acao[i]!=3)
			{
			acao[i]=3;
			acaoT[i]=0.0f;
			presa[i]=-1;
			}
		if(borda<4.0f&&borda>0.8f)
			{
			acao[i]=1;
			acaoT[i]=0.0f;
			temAlvo[i]=false;
			return true;
			}
		temAlvo[i]=true;
		alvoX[i]=h.x+dx/d*(h.r+3.0f);
		alvoY[i]=h.y+dy/d*(h.r+3.0f);
		return false;
		}
	if(acao[i]==1||acao[i]==3)
		{
		acao[i]=0;
		temAlvo[i]=false;
		acaoAte[i]=tempo+3.0;
		}
	if(acao[i]==4)
		{
		int p=presa[i];
		bool segue=p>=0&&p<quantos&&voador(tipo[p])&&vel[p]!=0.0f&&acaoT[i]<8.0f;
		if(segue)
			segue=(ax[p]-ax[i])*(ax[p]-ax[i])+(ay[p]-ay[i])*(ay[p]-ay[i])<35.0f*35.0f;
		if(!segue)
			{
			acao[i]=0;
			presa[i]=-1;
			temAlvo[i]=false;
			acaoAte[i]=tempo+8.0+8.0*aleatorio(); // cansou: passeia um pouco
			return false;
			}
		temAlvo[i]=true;
		alvoX[i]=ax[p];
		alvoY[i]=ay[p];
		return false;
		}
	if(acaoAte[i]<tempo)
		{
		int p=-1;
		float melhor=28.0f*28.0f;
		for(int j=0;j<quantos;++j)
			if(voador(tipo[j])&&vel[j]!=0.0f)
				{
				float d2=(ax[j]-ax[i])*(ax[j]-ax[i])+(ay[j]-ay[i])*(ay[j]-ay[i]);
				if(d2<melhor)
					{
					melhor=d2;
					p=j;
					}
				}
		if(p>=0)
			{
			acao[i]=4;
			acaoT[i]=0.0f;
			presa[i]=p;
			temAlvo[i]=true;
			alvoX[i]=ax[p];
			alvoY[i]=ay[p];
			}
		else
			acaoAte[i]=tempo+1.0;
		}
	return false;
	}

/* ----------------------------------------------------------------- shader */

const char* Extras::nomesUniforms[NUM_UNIFORMS]={"extrasCaixa","extrasAg","extrasTesouro","extrasInfo","extrasEfeito",
	"extrasLab","extrasLab2","extrasJogo","extrasJogoInfo","extrasNomes","extrasNomesCfg","extrasRostos","extrasMundo","extrasMundo2","extrasChuva","extrasCopia","extrasCopiaInfo",
	"extrasAg2","extrasMaos"};

const char* Extras::glslVertexUniforms=R"GLSL(
uniform mat4 extrasCaixa; // espaço da câmera -> caixa (cm)
varying vec3 extrasPos;
)GLSL";

const char* Extras::glslVertexMain=R"GLSL(
extrasPos=(extrasCaixa*vertexCc).xyz;
)GLSL";

const char* Extras::glslFragmentUniforms=R"GLSL(
uniform vec4 extrasAg[32];   // x, y (cm), ângulo, tipo (1.xx peixe, 2.xx carrinho; xx = cor)
uniform vec4 extrasTesouro;  // x, y, raio (cm), altura a alcançar cavando (cm)
uniform vec4 extrasInfo;     // tempo (s), achou, tesouro ligado, nível do lago (cm; -1000 = sem lago)
uniform vec4 extrasEfeito;   // efeito do tema (0 nenhum, 1 lava, 2 neve, 3 poeira, 4 areia, 5 ondas, 6 brilho, 7 chuva), altura média da areia (cm)
uniform vec4 extrasLab;      // labirinto: nascente (x, y) e alvo (x, y), cm
uniform vec4 extrasLab2;     // labirinto: raio do alvo, estado (0 desligado, 1 correndo, 2 chegou), segundos no estado
uniform vec4 extrasJogo[32]; // jogos de água: por peça (x, y, raio, tipo) e (estado, nível, ordem, time ou ângulo)
uniform vec4 extrasJogoInfo; // jogo (0 nenhum), fase, segundos na fase, vencedor ou próximo portão
uniform float extrasNomes[72]; // membros: por pessoa, nº de letras e 8 letras (desenho 3x5 em bits)
uniform vec4 extrasNomesCfg;  // quantos nomes, giro do texto (quartos de volta), quantos têm rosto no painel
uniform vec4 extrasCopia[4];    // copie o relevo: morros/buracos do alvo (x, y, altura cm, largura cm)
uniform vec4 extrasCopiaInfo;   // ligado, folga (cm), altura de referência (cm), nota (+1000 = conseguiu)
uniform vec4 extrasChuva[4]; // nuvens de chuva do painel: x, y (cm), ligada (1)
uniform vec4 extrasAg2[32];  // por bicho: estado (0 normal, 1 pousado/sentado, 2 mergulho, 3 indo, 4 caçando), altura do voo (0-1), tamanho, andamento do mergulho (0-1)
uniform vec4 extrasMaos[4];  // mãos sobre a areia: x, y, raio (cm), vista agora (1)
float extrasMarca;          // 1 onde há um animal marinho (o shader da água o deixa à vista)
uniform vec4 extrasMundo;   // cidade LAOB: deslocamento da imagem (cm), largura e altura da caixa, ligada (1)
uniform vec4 extrasMundo2;  // canto da caixa (x, y) e a hora local (0-24)
uniform sampler2D extrasRostos; // painel de rostos dos membros (8 quadros lado a lado, com transparência)
float exCaixa(vec2 p,vec2 metade,float raio)
	{
	vec2 q=abs(p)-metade+vec2(raio);
	return length(max(q,0.0))+min(max(q.x,q.y),0.0)-raio;
	}
varying vec3 extrasPos;
float exHash(vec2 p)
	{
	return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);
	}
float exRuido(vec2 p)
	{
	vec2 i=floor(p);
	vec2 f=fract(p);
	f=f*f*(3.0-2.0*f);
	return mix(mix(exHash(i),exHash(i+vec2(1.0,0.0)),f.x),mix(exHash(i+vec2(0.0,1.0)),exHash(i+vec2(1.0,1.0)),f.x),f.y);
	}
/* Pássaro visto de cima, voando para +x: 1 = corpo e cabeça, 2 = asa, 3 = cauda, 0 = nada.
   abre = 0..1 (asas abertas no plano ou levantadas, parecendo mais curtas) */
float exPassaro(vec2 p,float abre)
	{
	if(length(p/vec2(0.95,0.3))<1.0||length(p-vec2(0.85,0.0))<0.24)
		return 1.0;
	float alc=2.3*(0.45+0.55*abre);
	float ay=abs(p.y);
	if(ay>0.18&&ay<alc)
		{
		float t=ay/alc;
		float frente=0.32-0.55*t*t;
		float tras=frente-0.55*(1.0-t)-0.12;
		if(p.x<frente&&p.x>tras)
			return 2.0;
		}
	if(p.x<-0.75&&p.x>-1.45&&ay<(-p.x-0.75)*0.65+0.08)
		return 3.0;
	return 0.0;
	}
/* Um osso: haste entre dois pontos, mais grossa nas juntas */
float exOsso(vec2 p,vec2 a,vec2 b,float w)
	{
	vec2 pa=p-a;
	vec2 ba=b-a;
	float d=length(pa-ba*clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0));
	return max(smoothstep(w,w*0.4,d),smoothstep(w*1.8,w*0.9,min(length(pa),length(p-b))));
	}
/* Pontinhos que andam (neve, brilhos): grade de células com no máximo um ponto cada */
float exPontos(vec2 p,float tamanho,float chance)
	{
	vec2 c=floor(p);
	float h=exHash(c);
	if(h>chance)
		return 0.0;
	vec2 centro=vec2(exHash(c+vec2(3.1,7.7)),exHash(c+vec2(9.2,1.3)))*0.7+0.15;
	return smoothstep(tamanho,tamanho*0.3,length(fract(p)-centro));
	}
)GLSL";

const char* Extras::glslFragmentMain=R"GLSL(
	{
	/* Projetor-LAOB: efeito do tema, lago, tesouro e animais sobre o relevo */
	vec3 ep=extrasPos;
	float et=extrasInfo.x;
	extrasMarca=0.0;
	vec4 exAntes=baseColor;     // para a transição suave quando a brincadeira muda
	float exNoite=0.0;          // 0 de dia .. 1 de noite (só na cidade LAOB)
	float ef=extrasEfeito.x;
	float ref=extrasEfeito.y;
	if(extrasEfeito.w>0.5)
		{
		/* Jogo do vulcão: nada de lava de enfeite (não dava para ver onde estava a lava de
		   verdade); o relevo fica escuro, como rocha à noite, e só a lava do jogo brilha */
		float cinza=dot(baseColor.rgb,vec3(0.3,0.5,0.2));
		baseColor=vec4(mix(vec3(cinza),baseColor.rgb,0.45)*0.5,1.0);
		}
	else if(ef>0.5&&ef<1.5)
		{
		/* Lava: faixas incandescentes que descem dos morros (a fase acompanha a altura) */
		float n=exRuido(ep.xy*0.5+vec2(0.0,et*0.25));
		float faixa=smoothstep(0.55,0.95,fract(ep.z*0.8+et*0.45+n*0.7));
		float onde=smoothstep(ref-1.0,ref+2.5,ep.z);
		float topo=smoothstep(ref+3.0,ref+6.0,ep.z)*(0.6+0.4*sin(et*4.0+n*6.0));
		vec3 lava=mix(vec3(0.9,0.2,0.0),vec3(1.0,0.85,0.2),faixa);
		baseColor=vec4(mix(baseColor.rgb,lava,clamp(faixa*onde*0.85+topo*0.5,0.0,1.0)),1.0);
		}
	else if(ef>1.5&&ef<2.5)
		{
		/* Neve caindo: três camadas de flocos, cada uma numa velocidade, com vento leve */
		float flocos=0.0;
		for(int k=0;k<3;++k)
			{
			float fk=float(k);
			vec2 p=ep.xy*(0.35+0.15*fk)+vec2(sin(et*0.4+fk*2.0)*1.5+et*0.3,et*(0.9+0.5*fk));
			flocos=max(flocos,exPontos(p,0.13-0.03*fk,0.35));
			}
		baseColor=vec4(mix(baseColor.rgb,vec3(1.0),flocos*0.9),1.0);
		}
	else if(ef>2.5&&ef<3.5)
		{
		/* Tempestade de poeira marciana */
		float n=exRuido(ep.xy*0.12+vec2(et*0.35,et*0.08))+0.5*exRuido(ep.xy*0.35-vec2(et*0.5,0.0));
		baseColor=vec4(mix(baseColor.rgb,vec3(0.85,0.48,0.28),smoothstep(0.7,1.3,n)*0.5),1.0);
		}
	else if(ef>3.5&&ef<4.5)
		{
		/* Areia levada pelo vento: riscos que correm de um lado para o outro */
		float n=exRuido(vec2(ep.x*0.07-et*1.2,ep.y*1.1));
		float risco=smoothstep(0.62,0.9,n)*(0.6+0.4*exRuido(ep.xy*0.2+et*0.2));
		baseColor=vec4(mix(baseColor.rgb,vec3(1.0,0.92,0.72),risco*0.45),1.0);
		}
	else if(ef>4.5&&ef<5.5)
		{
		/* Mar: luz dançando no fundo e espuma na beira das ilhas */
		float c=abs(sin(ep.x*0.7+et*1.2+sin(ep.y*0.45+et*0.8))*sin(ep.y*0.8-et*1.0+sin(ep.x*0.55-et*0.6)));
		float agua=1.0-smoothstep(ref+2.5,ref+3.6,ep.z);
		float espuma=smoothstep(0.45,0.0,abs(ep.z-(ref+3.6)+0.25*sin(et*2.0+ep.x*0.8)));
		baseColor=vec4(baseColor.rgb+vec3(0.5,0.8,0.9)*pow(1.0-c,10.0)*0.5*agua+vec3(0.9)*espuma*0.55,1.0);
		}
	else if(ef>5.5&&ef<6.5)
		{
		/* Arco-íris: estrelinhas piscando */
		vec2 p=ep.xy*0.45;
		float b=exPontos(p,0.12,0.25)*pow(0.5+0.5*sin(et*3.0+exHash(floor(p))*6.283),3.0);
		baseColor=vec4(mix(baseColor.rgb,vec3(1.0),b),1.0);
		}
	else if(ef>6.5&&ef<7.5)
		{
		/* Chuva fina: gotas caindo e fazendo ondinhas que se abrem */
		vec2 p=ep.xy/5.0;
		vec2 c=floor(p);
		float fase=fract(et*0.6+exHash(c)*7.0);
		vec2 centro=(c+vec2(exHash(c+vec2(2.0,5.0)),exHash(c+vec2(8.0,1.0)))*0.8+0.1)*5.0;
		float d=distance(ep.xy,centro);
		float anel=smoothstep(0.35,0.0,abs(d-fase*2.5))*(1.0-fase);
		baseColor=vec4(mix(baseColor.rgb,vec3(0.85,0.93,1.0),anel*0.55),1.0);
		}
	/* (o lago dos bichos não é mais desenhado aqui: é a água de verdade da simulação, pintada pelo
	   SurfaceAddWaterColor com o mesmo visual, junto com a chuva) */
	if(extrasCopiaInfo.x>0.5)
		{
		/* Copie o relevo: o relevo fica desbotado; vermelho = sobra areia (tirar), azul = falta (pôr),
		   verde = já está certo; linhas brancas tracejadas = curvas de nível do alvo (a cada 1,5 cm) */
		float T=0.0;
		for(int k=0;k<4;++k)
			{
			vec4 g=extrasCopia[k];
			if(g.w>0.0)
				{
				vec2 dg=ep.xy-g.xy;
				T+=g.z*exp(-dot(dg,dg)/(2.0*g.w*g.w));
				}
			}
		float tol=extrasCopiaInfo.y;
		float err=ep.z-(extrasCopiaInfo.z+T);
		float nota=mod(extrasCopiaInfo.w,1000.0);
		bool feito=extrasCopiaInfo.w>=1000.0;
		vec3 c=mix(vec3(dot(exAntes.rgb,vec3(0.3,0.5,0.2))),exAntes.rgb,0.35)*0.7+vec3(0.12); // relevo sem a animação do tema (lê melhor as cores)
		float sobra=smoothstep(tol,tol+2.5,err);
		float falta=smoothstep(tol,tol+2.5,-err);
		c=mix(c,vec3(0.95,0.25,0.15),sobra*0.85);
		c=mix(c,vec3(0.15,0.45,0.98),falta*0.85);
		if(abs(err)<tol&&abs(T)>0.6)
			c=mix(c,vec3(0.25,0.88,0.4),0.5);
		float nivelT=T/1.5;
		float iso=smoothstep(0.1,0.0,abs(fract(nivelT+0.5)-0.5))*step(0.5,abs(T))*step(0.5,fract((ep.x+ep.y)*0.55));
		c=mix(c,vec3(1.0),iso*0.7);
		if(feito)
			c+=vec3(1.0,0.85,0.3)*smoothstep(0.6,0.0,abs(fract(length(ep.xy-extrasCopia[0].xy)*0.25-et*0.8)-0.5))*0.35*step(0.6,abs(T));
		baseColor=vec4(c,1.0);
		/* barra da nota no canto de baixo, à esquerda (de quem olha a imagem do projetor) */
		vec2 pb=ep.xy-(extrasMundo2.xy+vec2(3.0,2.5));
		if(pb.x>-0.35&&pb.x<20.35&&pb.y>-0.35&&pb.y<1.85)
			{
			vec3 corB=vec3(0.06,0.07,0.1);
			if(pb.x>0.0&&pb.x<20.0&&pb.y>0.0&&pb.y<1.5&&pb.x<nota*0.2)
				corB=feito?mix(vec3(1.0,0.8,0.2),vec3(1.0,0.97,0.7),0.5+0.5*sin(et*6.0)):
				     mix(mix(vec3(0.9,0.2,0.15),vec3(0.95,0.8,0.15),smoothstep(20.0,55.0,nota)),vec3(0.25,0.85,0.35),smoothstep(55.0,85.0,nota));
			if(abs(pb.x-17.0)<0.08&&pb.y>-0.2&&pb.y<1.7)
				corB=vec3(1.0);                                     // marca dos 85%
			baseColor=vec4(corB,1.0);
			}
		}
	if(extrasInfo.z>0.5)
		{
		/* Tesouro e fósseis: só o objeto aparece onde a areia foi cavada até ele (sem disco em volta) */
		vec2 q=(ep.xy-extrasTesouro.xy)/extrasTesouro.z;
		float r=length(q);
		float falta=ep.z-extrasTesouro.w;
		bool achou=extrasInfo.y>0.5;
		float ta=extrasInfo.y-1.0;          // segundos desde que foi achado
		bool visto=falta<=0.0||achou;
		if(max(abs(q.x),abs(q.y))<1.6)
			{
			if(extrasInfo.z<1.5)
				{
				/* Baú de madeira visto de cima: tábuas, cintas de ferro com rebites, cantoneiras e fechadura.
				   Achado, a tampa se abre devagar, o ouro e as joias aparecem brilhando e moedas saltam para fora. */
				vec2 p=q*1.0;                                                   // baú de uns 8 x 5 cm
				float corpo=exCaixa(p,vec2(0.8,0.52),0.07);
				float ab=achou?clamp(ta/1.4,0.0,1.0):0.0;
				ab=ab*ab*(3.0-2.0*ab);                                          // tampa: 0 fechada .. 1 aberta
				if(visto)
					{
					/* luz dourada saindo do baú aberto */
					if(ab>0.0)
						{
						float raios=pow(max(0.0,sin(atan(p.y-0.3,p.x)*9.0+et*1.3)),5.0)*smoothstep(1.9,0.4,length(p-vec2(0.0,0.3)));
						baseColor.rgb+=vec3(1.0,0.82,0.35)*(raios*0.35+0.25*smoothstep(1.6,0.2,length(p-vec2(0.0,0.3))))*ab;
						}
					/* a tampa aberta fica em pé atrás do baú (vê-se o lado de dentro) */
					bool tampaEmPe=ab>0.02&&abs(p.x)<0.8&&p.y>0.5&&p.y<0.5+0.46*ab;
					bool dentro=corpo<0.0&&p.y>0.04&&p.y<0.04+0.5*ab;         // o que a tampa já descobriu
					if(tampaEmPe)
						{
						vec3 cor=mix(vec3(0.24,0.13,0.05),vec3(0.42,0.25,0.1),exRuido(vec2(p.x*4.0,p.y*40.0)));
						if(abs(abs(p.x)-0.5)<0.075)
							cor=vec3(0.3,0.3,0.28);
						cor*=0.75+0.25*smoothstep(0.5,0.5+0.46*ab,p.y);
						baseColor=vec4(cor,1.0);
						}
					else if(dentro)
						{
						/* ouro: duas camadas de moedas, e por cima as joias */
						float m1=exPontos(p*11.0,0.44,0.95);
						float m2=exPontos(p*17.0+vec2(3.3,1.7),0.4,0.8);
						vec3 cor=mix(vec3(0.5,0.31,0.05),vec3(0.93,0.7,0.16),m1);
						cor=mix(cor,vec3(1.0,0.9,0.42),m2*0.9);
						cor*=0.82+0.18*sin(et*6.0+p.x*31.0+p.y*23.0);
						for(int k=0;k<6;++k)
							{
							float fk=float(k);
							vec2 pj=vec2(-0.6+0.24*fk,0.17+0.2*exHash(vec2(fk,2.0)));
							float dj=abs(p.x-pj.x)+abs(p.y-pj.y);                     // losango lapidado
							if(dj<0.075)
								{
								vec3 gema=k==0||k==3?vec3(0.9,0.08,0.15):(k==1||k==4?vec3(0.1,0.75,0.35):(k==2?vec3(0.15,0.4,0.95):vec3(0.7,0.3,0.9)));
								cor=gema*(0.75+0.5*step(p.x+p.y,pj.x+pj.y))+vec3(1.0)*pow(0.5+0.5*sin(et*5.0+fk*2.1),8.0)*smoothstep(0.075,0.0,dj);
								}
							}
						float colar=abs(length(p-vec2(0.3,0.42))-0.2);              // colar de pérolas
						if(colar<0.03&&p.y<0.42&&fract(atan(p.y-0.42,p.x-0.3)*4.5)<0.6)
							cor=vec3(0.97,0.95,0.9);
						cor*=mix(0.45,1.0,smoothstep(0.04,0.1,p.y))*mix(0.55,1.0,smoothstep(0.0,-0.06,corpo));   // sombra das paredes
						baseColor=vec4(cor,1.0);
						}
					else if(corpo<0.0)
						{
						float veio=exRuido(vec2(p.x*4.0,p.y*46.0))*0.6+exRuido(p*90.0)*0.4;
						vec3 cor=mix(vec3(0.33,0.18,0.07),vec3(0.56,0.34,0.15),veio);
						cor*=mix(0.62,1.0,smoothstep(0.0,0.04,abs(fract(p.y*4.8+0.5)-0.5)-0.02));     // frestas entre as tábuas
						float cinta=abs(abs(p.x)-0.5);
						if(cinta<0.075)
							{
							cor=mix(vec3(0.2,0.2,0.19),vec3(0.46,0.45,0.42),exRuido(p*34.0));              // ferro batido
							if(distance(vec2(cinta,fract(p.y*3.4)),vec2(0.0,0.5))<0.035)
								cor=vec3(0.72,0.68,0.55);                                                  // rebites
							}
						if(abs(p.x)>0.66&&abs(p.y)>0.38)
							cor=mix(vec3(0.5,0.38,0.12),vec3(0.82,0.66,0.26),exRuido(p*40.0));             // cantoneiras de latão
						if(abs(p.y-0.04)<0.018)
							cor*=0.4;                                                                      // linha da tampa
						if(abs(p.x)<0.1&&abs(p.y+0.03)<0.13)
							cor=length(p-vec2(0.0,-0.05))<0.036?vec3(0.08):mix(vec3(0.6,0.46,0.14),vec3(0.9,0.74,0.3),exRuido(p*50.0)); // fechadura
						cor*=mix(1.0,0.55,smoothstep(-0.06,0.0,corpo));                                    // contorno
						cor*=0.82+0.3*smoothstep(-0.6,0.5,p.y-p.x*0.3);                                    // luz de um lado
						baseColor=vec4(cor,1.0);
						}
					else if(exCaixa(p-vec2(0.08,-0.08),vec2(0.8,0.52),0.07)<0.0)
						baseColor.rgb*=0.55;                                                               // sombra na areia
					/* moedas que saltam do baú e ficam espalhadas em volta */
					if(achou&&ta>1.0)
						for(int k=0;k<9;++k)
							{
							float fk=float(k);
							float vai=min((ta-1.0)*(1.4+0.5*exHash(vec2(fk,5.0))),0.95+0.45*exHash(vec2(fk,7.0)));
							float ak=fk*0.698+0.4;
							vec2 pm=vec2(0.0,0.25)+vec2(cos(ak),sin(ak)*0.8)*vai;
							float dm=distance(p,pm);
							if(dm<0.07)
								baseColor=vec4(mix(vec3(1.0,0.86,0.3),vec3(0.7,0.48,0.08),smoothstep(0.03,0.07,dm))
								               +vec3(0.5)*pow(0.5+0.5*sin(et*6.0+fk*1.7),10.0),1.0);
							}
					if(achou)
						baseColor.rgb+=vec3(1.0,0.9,0.5)*pow(max(0.0,sin(atan(q.y,q.x)*7.0+et*2.0)),40.0)*smoothstep(1.25,0.3,r)*ab;
					}
				else if(falta<2.5&&corpo<0.25)
					{
					/* perto: grãozinhos dourados piscando na areia em cima dele */
					float pisca=exPontos(ep.xy*1.4+vec2(floor(et*2.5)*3.7,floor(et*2.5)*1.3),0.16,0.3);
					baseColor.rgb=mix(baseColor.rgb,vec3(1.0,0.88,0.4),pisca*(1.0-falta/2.5));
					}
				}
			else
				{
				/* Fóssil: só os ossos, cor de marfim manchado, meio presos na areia */
				if(visto)
					{
					float osso=0.0;
					float furo=0.0;                                              // buracos do crânio (órbita, narina)
					int tf=int(extrasInfo.z+0.5);
					if(tf<=2)
						{
						/* Tiranossauro de lado: crânio com janelas e dentes, pescoço, espinha com espinhos, cauda fina,
						   costelas curvas, bacia, as duas pernas com garras e os bracinhos */
						vec2 qc=(q-vec2(0.6,0.27))*2.3;
						float cabeca=smoothstep(0.42,0.34,length(qc*vec2(1.15,1.7)));
						float focinho=smoothstep(0.2,0.14,abs(qc.y-0.03))*step(0.1,qc.x)*step(qc.x,0.5);
						float boca=step(qc.y,-0.03)*step(-0.17,qc.y)*step(-0.2,qc.x)*step(qc.x,0.5);
						float dentes=step(0.35,abs(sin(qc.x*42.0)))*step(-0.17,qc.y)*step(qc.y,-0.05)*step(-0.15,qc.x)*step(qc.x,0.48);
						float mand=exOsso(qc,vec2(-0.26,-0.14),vec2(0.46,-0.25),0.045);
						osso=max(max(cabeca,focinho)*(1.0-boca),max(dentes*0.9,mand));
						furo=max(smoothstep(0.1,0.07,length((qc-vec2(0.0,0.12))*vec2(1.0,1.2))),smoothstep(0.05,0.03,length(qc-vec2(0.36,0.08))));
						furo=max(furo,smoothstep(0.08,0.05,length((qc-vec2(-0.2,0.05))*vec2(1.3,1.0))));
						osso=max(osso,exOsso(q,vec2(0.3,0.1),vec2(0.46,0.22),0.042));                      // pescoço
						for(int k=0;k<16;++k)
							{
							float fk=float(k);
							vec2 v=vec2(0.3-0.085*fk,0.1+0.02*sin(fk*0.5)-0.0028*fk*fk);                   // vértebras até a ponta da cauda
							float tamV=0.05-0.0022*fk;
							osso=max(osso,smoothstep(tamV,tamV*0.45,distance(q,v)));
							if(k<10)
								osso=max(osso,exOsso(q,v,v+vec2(-0.02,0.075-0.004*fk),0.011));            // espinhos de cima
							}
						for(int k=0;k<7;++k)
							{
							float fk=float(k);
							float x0=0.27-0.075*fk;
							vec2 topo=vec2(x0,0.08-0.003*fk);
							vec2 meio=vec2(x0-0.05,-0.08);
							vec2 ponta=vec2(x0-0.01,-0.22+0.03*abs(fk-2.5));
							osso=max(osso,max(exOsso(q,topo,meio,0.016),exOsso(q,meio,ponta,0.013)));      // costelas curvas
							}
						osso=max(osso,smoothstep(0.15,0.09,length((q-vec2(-0.26,0.0))*vec2(1.0,1.5))));     // bacia
						furo=max(furo,smoothstep(0.045,0.025,length(q-vec2(-0.27,0.0))));
						for(int k=0;k<2;++k)
							{
							float dx=float(k)*0.13;                                                        // perna de trás e a da frente
							osso=max(osso,exOsso(q,vec2(-0.27,-0.03),vec2(-0.15+dx,-0.36),0.042-0.008*float(k)));
							osso=max(osso,exOsso(q,vec2(-0.15+dx,-0.36),vec2(-0.3+dx,-0.66),0.03));
							osso=max(osso,exOsso(q,vec2(-0.3+dx,-0.66),vec2(-0.21+dx,-0.76),0.022));
							for(int d=0;d<3;++d)
								osso=max(osso,exOsso(q,vec2(-0.21+dx,-0.76),vec2(-0.1+dx+0.03*float(d),-0.8+0.035*float(d)),0.012));  // dedos com garra
							}
						osso=max(osso,max(exOsso(q,vec2(0.2,-0.04),vec2(0.29,-0.17),0.016),exOsso(q,vec2(0.29,-0.17),vec2(0.36,-0.13),0.012)));  // bracinho
						}
					else if(tf==3)
						{
						/* Triceratops de cima: escudo com borda em babados e duas janelas, três chifres, bico, e um pedaço do pescoço e das costelas */
						float aE=atan(q.y,q.x+0.2);
						float escudo=smoothstep(0.02,-0.02,length((q+vec2(0.2,0.0))*vec2(1.25,1.0))-0.52-0.03*cos(aE*11.0))*step(q.x,0.05);
						furo=max(smoothstep(0.11,0.08,length((q-vec2(-0.38,0.2))*vec2(1.0,0.8))),smoothstep(0.11,0.08,length((q-vec2(-0.38,-0.2))*vec2(1.0,0.8))));
						float cranio=smoothstep(0.3,0.24,length((q-vec2(0.15,0.0))*vec2(0.8,1.25)));
						float bico=smoothstep(0.02,-0.01,abs(q.y)-(0.62-q.x)*0.45)*step(0.3,q.x)*step(q.x,0.62);
						osso=max(max(escudo,cranio),bico);
						osso=max(osso,exOsso(q,vec2(0.12,0.16),vec2(0.6,0.42),0.035)*smoothstep(0.75,0.3,q.x));   // chifres compridos
						osso=max(osso,exOsso(q,vec2(0.12,-0.16),vec2(0.6,-0.42),0.035)*smoothstep(0.75,0.3,q.x));
						osso=max(osso,smoothstep(0.06,0.03,length((q-vec2(0.42,0.0))*vec2(0.6,1.0))));             // chifre do nariz
						furo=max(furo,max(smoothstep(0.05,0.03,distance(q,vec2(0.2,0.13))),smoothstep(0.05,0.03,distance(q,vec2(0.2,-0.13)))));
						for(int k=0;k<5;++k)
							{
							float fk=float(k);
							osso=max(osso,smoothstep(0.045,0.02,distance(q,vec2(-0.75-0.045*fk,0.0))));              // pescoço saindo de baixo do escudo
							}
						}
					else if(tf==4)
						{
						/* Pterodáctilo de asas abertas: crânio com bico comprido e crista, corpo pequeno com costelas,
						   braços longos em "M" com o dedo da asa, perninhas e rabo curto */
						osso=max(exOsso(q,vec2(0.12,0.0),vec2(0.72,0.03),0.022),smoothstep(0.11,0.07,length((q-vec2(0.16,0.0))*vec2(1.0,1.5))));  // bico e cabeça
						osso=max(osso,exOsso(q,vec2(0.12,0.03),vec2(-0.08,0.2),0.02));                             // crista
						furo=smoothstep(0.035,0.02,distance(q,vec2(0.2,0.0)));
						osso=max(osso,smoothstep(0.13,0.08,length((q-vec2(-0.12,0.0))*vec2(0.75,1.3))));            // tronco
						for(int k=0;k<4;++k)
							osso=max(osso,exOsso(q,vec2(-0.02-0.06*float(k),0.0),vec2(-0.05-0.06*float(k),0.11),0.01)
							               +exOsso(q,vec2(-0.02-0.06*float(k),0.0),vec2(-0.05-0.06*float(k),-0.11),0.01));
						for(int lado=0;lado<2;++lado)
							{
							float sy=lado==0?1.0:-1.0;
							osso=max(osso,exOsso(q,vec2(-0.03,0.08*sy),vec2(0.06,0.4*sy),0.024));                   // úmero
							osso=max(osso,exOsso(q,vec2(0.06,0.4*sy),vec2(-0.12,0.72*sy),0.02));                    // antebraço
							osso=max(osso,exOsso(q,vec2(-0.12,0.72*sy),vec2(-0.55,1.05*sy),0.014));                 // dedo da asa
							osso=max(osso,exOsso(q,vec2(-0.12,0.72*sy),vec2(-0.03,0.8*sy),0.01));                   // dedinhos
							osso=max(osso,exOsso(q,vec2(-0.26,0.05*sy),vec2(-0.5,0.16*sy),0.016));                  // perna
							osso=max(osso,exOsso(q,vec2(-0.5,0.16*sy),vec2(-0.72,0.12*sy),0.012));
							}
						osso=max(osso,exOsso(q,vec2(-0.28,0.0),vec2(-0.48,0.0),0.012));                            // rabo
						}
					else
						{
						/* Pegada de três dedos calcada na lama endurecida: o fundo é mais escuro e a borda tem luz */
						float marca=smoothstep(0.26,0.2,length((q+vec2(0.0,0.3))*vec2(1.0,0.85)));                 // calcanhar
						marca=max(marca,exOsso(q,vec2(0.0,-0.15),vec2(0.0,0.62),0.1));                             // dedo do meio
						marca=max(marca,exOsso(q,vec2(-0.06,-0.15),vec2(-0.42,0.42),0.085));
						marca=max(marca,exOsso(q,vec2(0.06,-0.15),vec2(0.42,0.42),0.085));
						float garras=max(smoothstep(0.05,0.02,distance(q,vec2(0.0,0.76))),max(smoothstep(0.05,0.02,distance(q,vec2(-0.5,0.53))),smoothstep(0.05,0.02,distance(q,vec2(0.5,0.53)))));
						marca=clamp(max(marca,garras),0.0,1.0);
						vec3 lama=mix(vec3(0.33,0.25,0.17),vec3(0.2,0.15,0.1),exRuido(q*13.0));
						baseColor.rgb=mix(baseColor.rgb,lama,smoothstep(0.35,0.7,marca)*0.9);
						baseColor.rgb+=vec3(0.22,0.18,0.12)*smoothstep(0.1,0.35,marca)*(1.0-smoothstep(0.35,0.6,marca))*step(0.0,q.x+q.y);  // borda iluminada
						baseColor.rgb*=1.0-0.35*smoothstep(0.1,0.35,marca)*(1.0-smoothstep(0.35,0.6,marca))*step(q.x+q.y,0.0);            // borda em sombra
						if(achou)
							baseColor.rgb+=vec3(0.25,0.2,0.08)*(0.5+0.5*sin(et*5.0))*smoothstep(0.35,0.7,marca);
						}
					osso=clamp(osso,0.0,1.0)*(1.0-0.9*clamp(furo,0.0,1.0));
					float miolo=smoothstep(0.35,0.62,osso);
					/* marfim com manchas, poros e rachaduras finas; mais claro no meio de cada osso (volume) */
					float mancha=exRuido(q*20.0)*0.5+exRuido(q*70.0)*0.5;
					vec3 marfim=mix(vec3(0.92,0.87,0.72),vec3(0.62,0.52,0.38),mancha*0.75);
					marfim*=mix(0.6,1.08,smoothstep(0.4,0.95,osso));
					marfim*=1.0-0.35*smoothstep(0.86,0.93,exRuido(q*46.0+vec2(1.7,9.2)));                      // poros
					marfim*=1.0-0.4*smoothstep(0.03,0.0,abs(exRuido(q*9.0)-0.5))*step(0.5,exRuido(q*3.0));     // rachaduras
					marfim+=vec3(0.12)*smoothstep(0.8,1.0,osso)*smoothstep(0.3,0.7,exRuido(q*30.0+vec2(4.0)));  // brilho do polimento
					baseColor.rgb*=1.0-0.45*smoothstep(0.02,0.3,osso)*(1.0-miolo);                          // sombra em volta de cada osso
					baseColor.rgb=mix(baseColor.rgb,marfim,miolo);
					if(achou)
						baseColor.rgb+=vec3(0.3,0.24,0.1)*(0.6+0.4*sin(et*5.0))*miolo;
					}
				else if(falta<3.0)
					{
					/* perto: o quadro de escavação (estacas nos cantos e barbante esticado) */
					float prox=1.0-falta/3.0;
					float lado=max(abs(q.x),abs(q.y));
					float corda=smoothstep(0.03,0.0,abs(lado-0.95));
					corda=max(corda,0.5*step(lado,0.95)*max(smoothstep(0.02,0.0,abs(abs(q.x)-0.32)),smoothstep(0.02,0.0,abs(abs(q.y)-0.32))));
					baseColor.rgb=mix(baseColor.rgb,vec3(0.97,0.93,0.8),corda*prox*0.75);
					if(distance(abs(q),vec2(0.95))<0.07)
						baseColor.rgb=mix(baseColor.rgb,vec3(0.5,0.32,0.14),prox);
					}
				}
			}
		}
	/* Mundo LAOB: o chão deixa de ser o relevo colorido e vira o laboratório por dentro, visto de cima
	   (planta aproximada, tirada do vídeo da sala; as posições são as mesmas do Mundo.cpp): janelas em
	   cima, porta azul e quadro verde embaixo, bancada de parede com computadores, bancada comprida no
	   meio, mesas perto das janelas, arco branco, manequim e a caixa de areia. À noite (hora de
	   verdade) as janelas escurecem. */
	if(extrasMundo.w>0.5)
		{
		vec2 u=vec2(ep.x-extrasMundo2.x,ep.y-extrasMundo2.y);
		float mw=extrasMundo.y;
		float mh=extrasMundo.z;
			{

			/* Quarta tela: o laboratório por dentro, visto de cima (planta aproximada, tirada do vídeo da sala):
			   janelas em cima, porta azul e quadro verde embaixo, bancada de parede com computadores, bancada
			   comprida no meio, mesas perto das janelas, arco branco, manequim e a caixa de areia. */
			vec2 f=vec2(u.x/mw,u.y/mh);                                  // 0..1 dentro da sala
			vec2 cm=vec2(f.x*mw,f.y*mh);                                 // em cm, para as texturas
			vec3 g=mix(vec3(0.60,0.61,0.60),vec3(0.66,0.67,0.66),step(0.5,fract(cm.x/4.0))*step(0.5,fract(cm.y/4.0))
			           +(1.0-step(0.5,fract(cm.x/4.0)))*(1.0-step(0.5,fract(cm.y/4.0))));     // piso
			g+=vec3(0.05)*exRuido(cm*0.25);
			float dia=smoothstep(5.3,6.8,extrasMundo2.z)*(1.0-smoothstep(17.5,19.2,extrasMundo2.z));
			g+=vec3(0.07,0.07,0.05)*dia*smoothstep(0.45,0.95,f.y);       // claridade que entra pelas janelas
			/* paredes */
			float borda=min(min(f.x,1.0-f.x)*mw,min(f.y,1.0-f.y)*mh);
			if(borda<2.6)
				{
				g=borda<1.4?vec3(0.1,0.11,0.13):vec3(0.86,0.85,0.8);
				if(f.y>0.5&&borda>=1.4&&f.x>0.06&&f.x<0.94)
					g=fract(f.x*13.0)<0.08?vec3(0.5,0.52,0.55):mix(vec3(0.1,0.14,0.28),vec3(0.62,0.82,0.95),dia);   // janelas
				if(f.y<0.5&&borda>=1.4&&f.x>0.405&&f.x<0.465)
					g=vec3(0.13,0.3,0.72);                                // porta azul
				if(f.y<0.5&&borda>=1.4&&f.x>0.18&&f.x<0.34)
					g=vec3(0.16,0.4,0.3);                                 // quadro verde
				if(f.y<0.5&&borda>=1.4&&f.x>0.6&&f.x<0.66)
					g=vec3(0.85,0.2,0.2);                                 // cartaz vermelho
				if(f.x>0.5&&borda>=1.4&&((f.y>0.34&&f.y<0.43)||(f.y>0.5&&f.y<0.59)))
					g=vec3(0.12,0.12,0.16);                               // as duas portas escuras do fundo
				if(f.x<0.5&&borda>=1.4&&f.y>0.4&&f.y<0.7)
					g=vec3(0.2,0.22,0.3);                                 // cartaz escuro
				}
			else
				{
				/* armário elétrico ao lado da porta */
				if(f.x>0.345&&f.x<0.395&&f.y<0.13)
					g=mix(vec3(0.6,0.61,0.62),vec3(0.9,0.2,0.15),step(distance(f,vec2(0.37,0.1)),0.008));
				/* coluna com o extintor */
				if(abs(f.x-0.36)<0.022&&f.y>0.86)
					g=vec3(0.7,0.7,0.68);
				if(distance(cm,vec2(0.385*mw,0.9*mh))<0.8)
					g=vec3(0.85,0.1,0.1);
				/* bancada de parede com computadores, e as cadeiras */
				if(f.x>0.5&&f.x<0.92&&f.y>0.075&&f.y<0.17)
					{
					g=vec3(0.9,0.9,0.88);
					if(abs(fract((f.x-0.56)*10.0+0.5)-0.5)<0.24&&f.y<0.12)
						g=vec3(0.08,0.09,0.12);                           // monitores
					if(abs(fract((f.x-0.56)*10.0+0.5)-0.5)<0.2&&f.y>0.135&&f.y<0.155)
						g=vec3(0.25);                                     // teclados
					}
				for(int k=0;k<4;++k)
					if(distance(cm,vec2((0.56+0.1*float(k))*mw,0.215*mh))<1.15)
						g=k==3?vec3(0.9,0.75,0.1):(k==2?vec3(0.85,0.2,0.15):vec3(0.13));   // cadeiras (uma amarela e uma vermelha)
				/* bancada comprida do meio, com prateleira e cabos azuis */
				if(f.x>0.42&&f.x<0.8&&f.y>0.44&&f.y<0.56)
					{
					g=vec3(0.93,0.93,0.91);
					if(abs(f.y-0.5)<0.022)
						g=vec3(0.45,0.47,0.5);
					if(fract(f.x*23.0)<0.3&&abs(f.y-0.47)<0.012)
						g=vec3(0.15,0.4,0.85);
					g*=mix(1.0,0.75,smoothstep(0.05,0.06,abs(f.y-0.5)));
					}
				/* mesas perto das janelas, com monitores e cadeiras */
				if(f.x>0.1&&f.x<0.3&&f.y>0.7&&f.y<0.8)
					g=abs(fract((f.x-0.14)*16.7+0.5)-0.5)<0.22&&f.y>0.75?vec3(0.08,0.09,0.12):vec3(0.86,0.82,0.74);
				if(f.x>0.06&&f.x<0.13&&f.y>0.3&&f.y<0.56)
					g=abs(f.y-0.42)<0.04&&f.x<0.09?vec3(0.08,0.09,0.12):vec3(0.86,0.82,0.74);
				for(int k=0;k<3;++k)
					if(distance(cm,vec2((0.14+0.06*float(k))*mw,0.65*mh))<1.1)
						g=vec3(0.13);
				if(distance(cm,vec2(0.17*mw,0.42*mh))<1.1)
					g=vec3(0.13);
				/* arco branco */
				float arco=abs(length((cm-vec2(0.3*mw,0.5*mh))/vec2(1.0,1.0))-3.2);
				if(arco<0.28&&cm.y>0.5*mh)
					g=vec3(0.97);
				if(distance(cm,vec2(0.3*mw-3.2,0.5*mh))<0.7)
					g=vec3(0.95);
				/* manequim */
				if(length((cm-vec2(0.9*mw,0.62*mh))/vec2(0.7,1.3))<1.0||distance(cm,vec2(0.9*mw,0.62*mh))<0.62)
					g=vec3(0.96,0.96,0.94);
				/* a caixa de areia: moldura de madeira e o relevo colorido dentro */
				vec2 ca=(f-vec2(0.64,0.77))/vec2(0.075,0.08);
				if(max(abs(ca.x),abs(ca.y))<1.0)
					{
					float rel=exRuido(ca*2.2+vec2(et*0.05,0.0));
					g=max(abs(ca.x),abs(ca.y))>0.86?vec3(0.5,0.33,0.16)
					  :(rel<0.35?vec3(0.2,0.5,0.85):(rel<0.5?vec3(0.3,0.7,0.35):(rel<0.68?vec3(0.92,0.84,0.5):vec3(0.75,0.4,0.2))));
					}
				/* a câmera do Willbert, olhando para a porta */
				vec2 cw2=cm-vec2(0.44*mw,mh-3.4);
				if(length(cw2)<0.6)
					g=vec3(0.98);
				else if(cw2.y<0.0&&cw2.y>-9.0&&abs(cw2.x)<-cw2.y*0.45)
					g+=vec3(0.06,0.08,0.1)*(0.6+0.4*sin(et*2.0));
				}
			g*=mix(0.86,1.0,dia);                                        // à noite, só a luz de dentro
			baseColor=vec4(g,1.0);
			}
		}
	/* (as peças vêm antes dos bonecos: no mundo LAOB eles andam por cima da praça e do campinho) */
	/* Jogos de água (Jogos.cpp): peças desenhadas sobre o relevo */
	if(extrasJogoInfo.x>0.5)
		{
		float jfase=extrasJogoInfo.y;
		float jt=extrasJogoInfo.z;
		float jaux=extrasJogoInfo.w; // corrida/outros: vencedor (-1 nenhum); rota: próximo portão
		for(int i=0;i<16;++i)
			{
			vec4 a=extrasJogo[2*i];
			vec4 b=extrasJogo[2*i+1];
			if(a.w<0.5)
				continue;
			vec2 q=ep.xy-a.xy;
			float d=length(q);
			float r=a.z;
			if(d>r*2.2+2.5)
				continue;
			float tipo=a.w;
			float est=b.x;
			float ang=atan(q.y,q.x);
			vec3 c=baseColor.rgb;
			float ouro=(jfase>1.5&&est>0.5&&est<1.5)?(0.6+0.4*sin(et*5.0)):0.0;
			if(tipo<1.5)
				{
				/* nascente: ondas azuis (some quando a rodada termina) */
				float on=smoothstep(r*1.6,0.0,d)*(jfase<1.5?1.0:0.3);
				float ondas=0.5+0.5*sin(d*3.0-et*6.0);
				c=mix(c,mix(vec3(0.15,0.45,0.95),vec3(0.75,0.95,1.0),ondas),on*0.85);
				}
			else if(tipo<2.5)
				{
				/* bandeira (corrida: azul ou vermelha; rota: vermelha) */
				vec3 cor=b.w<0.5?vec3(0.15,0.45,1.0):vec3(0.95,0.15,0.15);
				float venceu=(jfase>1.5&&((b.w<1.5&&abs(jaux-b.w)<0.5)||est>0.5))?1.0:0.0;
				float anel=smoothstep(0.35,0.0,abs(d-r))*step(0.0,sin(ang*8.0+et*2.5));
				float pano=(q.x>0.0&&q.x<2.2&&q.y>0.3&&q.y<1.5-q.x*0.4)?1.0:0.0;
				float mastro=(abs(q.x)<0.12&&q.y>-0.6&&q.y<1.6)?1.0:0.0;
				c=mix(c,venceu>0.5?vec3(1.0,0.82,0.2):cor,max(anel,smoothstep(1.0,0.6,d)*0.8));
				c=mix(c,cor,pano*(0.85+0.15*sin(et*6.0+q.x*3.0)));
				c=mix(c,vec3(0.95),mastro);
				if(venceu>0.5)
					{
					float onda=smoothstep(0.6,0.0,abs(d-mod(jt*7.0,14.0)))*smoothstep(8.0,1.0,jt);
					c=mix(c,vec3(1.0,0.85,0.25),onda);
					}
				}
			else if(tipo<3.5)
				{
				/* casa: paredes, telhado e porta; alagada = água por cima; salva = brilho verde */
				float s=r/2.6;
				vec2 p=q/s;
				bool corpo=abs(p.x)<1.1&&p.y>-1.2&&p.y<0.3;
				bool telhado=p.y>=0.3&&p.y<1.5&&abs(p.x)<(1.5-p.y)*1.3;
				bool porta=abs(p.x)<0.28&&p.y>-1.2&&p.y<-0.45;
				if(corpo||telhado)
					{
					vec3 telha=b.w>0.5?vec3(0.5)+0.45*cos(6.2832*(vec3(b.w/100.0)+vec3(0.0,0.33,0.67))):vec3(0.78,0.2,0.14);
					vec3 cc=corpo?vec3(0.96,0.9,0.74):telha;
					if(corpo&&b.w>0.5&&abs(abs(p.x)-0.68)<0.2&&abs(p.y+0.3)<0.22)
						cc=mix(vec3(0.25,0.3,0.4),vec3(1.0,0.9,0.4),max(b.y,exNoite*step(0.35,exHash(a.xy+vec2(extrasMundo.x,0.0))))); // janelas: acesas com o dono em casa (e à noite, na maioria)
					if(porta)
						cc=vec3(0.45,0.28,0.15);
					if(est>1.5)
						cc=mix(cc,vec3(0.2,0.45,0.9),0.55+0.15*sin(et*4.0+p.x*3.0));
					c=cc*mix(1.0,0.7,smoothstep(0.85,1.1,max(abs(p.x)/1.1,0.0)));
					}
				else if(est>0.5&&est<1.5&&d<r*1.5&&b.w<0.5)
					c=mix(c,vec3(0.3,0.95,0.4),smoothstep(r*1.5,r*1.1,d)*0.6);
				}
			else if(tipo<4.5)
				{
				/* plantação: broto com duas folhas; seca devagar (verde -> marrom); anel = água que resta */
				float s=r/2.8;
				vec2 p=q/s;
				vec3 verde=mix(vec3(0.55,0.4,0.15),vec3(0.2,0.75,0.2),clamp(b.y,0.0,1.0));
				if(est>1.5)
					verde=vec3(0.45,0.38,0.3);
				vec2 f1=p-vec2(0.55,0.35),f2=p+vec2(0.55,-0.35);
				bool folha=(f1.x*f1.x/0.36+f1.y*f1.y/0.1<1.0)||(f2.x*f2.x/0.36+f2.y*f2.y/0.1<1.0);
				bool caule=abs(p.x)<0.1&&p.y>-0.9&&p.y<0.4;
				bool terra=length(p+vec2(0.0,0.95))<0.45;
				if(folha||caule)
					c=verde;
				else if(terra)
					c=vec3(0.4,0.26,0.14);
				float fr=fract(ang/6.2831853+1.0);
				float arco=smoothstep(0.3,0.0,abs(d-r*1.15))*(fr<b.y?1.0:0.25);
				c=mix(c,mix(vec3(0.9,0.25,0.15),vec3(0.2,0.6,1.0),b.y),arco*(est>1.5?0.0:1.0));
				c=mix(c,vec3(0.3,0.95,0.4),smoothstep(0.4,0.0,abs(d-r*1.3))*ouro);
				}
			else if(tipo<5.5)
				{
				/* portão numerado: anel com o número em bolinhas; o próximo pisca amarelo */
				vec3 cor=est>0.5?vec3(0.2,0.9,0.35):(abs(b.z-jaux)<0.5?mix(vec3(1.0,0.85,0.1),vec3(1.0),0.5+0.5*sin(et*6.0)):vec3(0.95));
				float anel=smoothstep(0.45,0.15,abs(d-r));
				float bolas=0.0;
				for(int k=0;k<4;++k)
					{
					if(float(k)>=b.z)
						break;
					vec2 cb=vec2((float(k)-(b.z-1.0)*0.5)*0.9,0.0);
					bolas=max(bolas,smoothstep(0.38,0.28,length(q-cb)));
					}
				c=mix(c,cor,max(anel,bolas));
				}
			else if(tipo<6.5)
				{
				/* lago: contorno tracejado girando; cor pela água que tem (azul falta, verde na medida, vermelho demais) */
				vec3 cor=b.z<0.5?vec3(0.3,0.55,1.0):(b.z<1.5?vec3(0.25,0.95,0.4):vec3(1.0,0.3,0.2));
				float tracos=step(0.0,sin(ang*12.0-et*1.5));
				float anel=smoothstep(0.35,0.0,abs(d-r))*tracos;
				float dentroL=smoothstep(r,r-0.6,d);
				c=mix(c,cor,max(anel,dentroL*0.18));
				if(jfase>1.5)
					c=mix(c,est<1.5?vec3(1.0,0.85,0.25):vec3(1.0,0.3,0.2),smoothstep(0.6,0.0,abs(d-mod(jt*6.0,r*2.0)))*0.8);
				}
			else if(tipo<7.5)
				{
				/* barquinho: casco de madeira e vela branca, apontando para onde a água leva */
				float ca=cos(b.w),sa=sin(b.w);
				vec2 p=vec2(ca*q.x+sa*q.y,-sa*q.x+ca*q.y)/(r/1.8);
				bool casco=(p.x*p.x/2.2+p.y*p.y/0.35<1.0)&&p.x>-1.4;
				bool vela=p.x>-0.6&&p.x<0.6&&p.y>0.05&&p.y<(0.6-p.x)*1.4&&p.y<1.3;
				if(vela)
					c=vec3(0.97);
				else if(casco)
					c=mix(vec3(0.55,0.32,0.15),vec3(0.75,0.5,0.25),smoothstep(0.6,0.0,abs(p.y)));
				c=mix(c,vec3(1.0,0.82,0.2),smoothstep(0.5,0.0,abs(d-r*1.5))*ouro);
				}
			else if(tipo<8.5)
				{
				/* porto: tábuas de madeira dentro de um anel */
				float anel=smoothstep(0.35,0.0,abs(d-r))*step(0.0,sin(ang*10.0+et*1.5));
				bool tabuas=abs(q.x)<r*0.5&&abs(q.y)<r*0.25;
				float linhas=step(0.82,fract(q.x*1.2));
				if(tabuas)
					c=mix(vec3(0.62,0.42,0.22),vec3(0.35,0.22,0.1),linhas);
				c=mix(c,est>0.5?vec3(1.0,0.82,0.2):vec3(0.85,0.65,0.4),anel);
				}
			else if(tipo>11.5)
				{
				/* armadilha do labirinto: disco vermelho com espinhos girando; ao ser tocada, pisca */
				float espinhos=r*(0.78+0.22*abs(sin(ang*5.0+et*1.5)));
				float dentroA=smoothstep(espinhos,espinhos-0.5,d);
				float xis=smoothstep(0.35,0.1,abs(abs(q.x)-abs(q.y)))*step(d,r*0.55);
				vec3 cor=mix(vec3(0.75,0.05,0.05),vec3(1.0,0.35,0.1),0.5+0.5*sin(et*3.0));
				if(est>0.5)
					cor=mix(cor,vec3(1.0),0.5+0.5*sin(et*25.0));
				c=mix(c,cor,dentroA*0.85);
				c=mix(c,vec3(1.0,0.95,0.6),xis*dentroA);
				}
			else if(tipo>10.5)
				{
				/* aviso do vulcão: aqui vai abrir uma boca de lava (anéis que fecham, piscando) */
				float pisca=0.5+0.5*sin(et*12.0);
				float anel=smoothstep(0.5,0.0,abs(d-r*(1.6-fract(et*1.2))));
				float miolo=smoothstep(r*0.5,0.0,d);
				c=mix(c,mix(vec3(1.0,0.9,0.1),vec3(1.0,0.2,0.0),pisca),clamp(anel+miolo*pisca,0.0,1.0)*0.9);
				}
			else if(tipo>9.5)
				{
				/* placar: casinha pequena; branca = de pé, vermelha com X = perdida, verde = salva */
				vec2 p=q/(r/1.3);
				bool corpo=abs(p.x)<0.8&&p.y>-0.9&&p.y<0.15;
				bool telhado=p.y>=0.15&&p.y<1.0&&abs(p.x)<(1.0-p.y)*1.25;
				vec3 cor=est>1.5?vec3(0.95,0.15,0.1):(est>0.5?vec3(0.25,0.95,0.4):vec3(0.97));
				if(d<r*1.25)
					c=mix(c,vec3(0.0),0.75);
				if(corpo||telhado)
					c=cor;
				if(est>1.5&&abs(abs(p.x)-abs(p.y+0.2))<0.16&&abs(p.x)<0.9)
					c=vec3(0.1,0.0,0.0);
				}
			else
				{
				/* peixe preso na poça: anel = ar que resta; salvo = nada e brilha; perdido = cinza parado */
				float s=r/2.4;
				float onda=est<1.5?0.25*sin(et*(est>0.5?10.0:4.0)+q.x*2.0):0.0;
				vec2 p=vec2(q.x,q.y+onda)/s;
				bool corpo=p.x*p.x/1.4+p.y*p.y/0.4<1.0;
				bool rabo=p.x<-0.9&&p.x>-1.8&&abs(p.y)<(-p.x-0.9)*0.9;
				vec3 cor=est>1.5?vec3(0.55):vec3(1.0,0.55,0.15);
				if(corpo||rabo)
					c=corpo&&distance(p,vec2(0.6,0.15))<0.15?vec3(0.05):cor;
				else if(d<r)
					c=mix(c,vec3(0.25,0.55,0.95),0.35);
				float fr=fract(ang/6.2831853+1.0);
				float arco=smoothstep(0.3,0.0,abs(d-r*1.15))*(fr<b.y?1.0:0.2);
				c=mix(c,mix(vec3(0.9,0.25,0.15),vec3(0.35,0.75,1.0),b.y),arco*(est<0.5?1.0:0.0));
				c=mix(c,vec3(0.3,0.95,0.6),smoothstep(0.4,0.0,abs(d-r*1.2))*(est>0.5&&est<1.5?0.8:0.0));
				}
			baseColor.rgb=c;
			}
		}
	/* Mão sobre a areia: ondas de luz saindo dela (os bichos estão sentindo a mão) */
	for(int k=0;k<4;++k)
		{
		vec4 mh=extrasMaos[k];
		if(mh.w<0.5)
			continue;
		float dm=distance(ep.xy,mh.xy)-mh.z;
		if(dm<0.0||dm>8.0)
			continue;
		float o=fract(et*0.6+float(k)*0.3);
		float o2=fract(o+0.5);
		float anel=smoothstep(0.5,0.0,abs(dm-(0.5+o*7.0)))*(1.0-o)*0.85
		          +smoothstep(0.5,0.0,abs(dm-(0.5+o2*7.0)))*(1.0-o2)*0.85
		          +smoothstep(1.8,0.0,dm)*0.45;                                                  // brilho colado na mão
		baseColor.rgb=mix(baseColor.rgb,vec3(1.0,0.97,0.75),clamp(anel,0.0,0.9));
		}
	/* Peixes e carrinhos (vistos de cima, com sombra): */
	for(int i=0;i<32;++i)
		{
		vec4 a=extrasAg[i];
		if(a.w<0.5)
			continue;
		vec2 r=ep.xy-a.xy;
		if(dot(r,r)>49.0)
			continue;
		float c=cos(a.z);
		float s=sin(a.z);
		vec2 l=vec2(c*r.x+s*r.y,-s*r.x+c*r.y);
		float hue=fract(a.w);
		vec3 cor=vec3(0.55)+0.45*cos(6.2832*(vec3(hue)+vec3(0.0,0.33,0.67)));
		vec2 rs=r-vec2(0.35,-0.35);                                   // sombra: luz vinda do canto, igual para todos
		vec2 ls=vec2(c*rs.x+s*rs.y,-s*rs.x+c*rs.y);
		if(a.w<2.0)
			{
			/* Peixe: corpo que ondula, cauda bifurcada, nadadeiras, olhos e listras */
			float fase=float(i)*1.7;
			float onda=0.32*sin(et*8.0+l.x*1.6+fase)*smoothstep(1.2,-3.2,l.x);
			vec2 f=vec2(l.x,l.y+onda);
			vec2 fs=vec2(ls.x,ls.y+onda);
			float corpo=(f.x*f.x)/5.3+(f.y*f.y)/(0.95*0.95*(1.0-0.25*clamp(-f.x/2.3,0.0,1.0)));
			bool cauda=f.x<-1.9&&f.x>-3.5&&abs(f.y)<(-f.x-1.9)*0.95&&!(f.x<-2.85&&abs(f.y)<(-f.x-2.85)*0.7);
			float aba=0.25*sin(et*10.0+fase);
			bool nad=f.x>0.0&&f.x<0.9&&abs(f.y)>0.75&&abs(f.y)<1.45+aba&&abs(f.y)-0.75<(0.9-f.x)*0.9;
			float sombra=(fs.x*fs.x)/5.3+(fs.y*fs.y)/0.9;
			if(corpo<1.0||cauda||nad)
				{
				extrasMarca=1.0;
				vec3 pc=cor;
				if(corpo<1.0)
					{
					pc=mix(cor*1.25,cor*0.7,smoothstep(0.0,0.9,abs(f.y)));      // lombo claro, bordas escuras
					if(hue>0.5)
						pc*=0.85+0.15*step(0.0,sin(f.x*4.5));                    // listras em metade dos peixes
					pc+=vec3(0.35)*smoothstep(0.35,0.0,distance(f,vec2(0.9,0.25))); // brilho
					pc*=0.9+0.16*smoothstep(0.25,0.5,abs(fract(f.x*2.6+0.5*step(0.5,fract(f.y*2.2)))-0.5))*step(f.x,0.9); // escamas
					pc=mix(pc,vec3(1.0),0.25*smoothstep(0.55,0.95,abs(f.y))*step(-1.2,f.x));                          // barriga clara
					pc*=mix(1.0,0.55,smoothstep(0.8,1.0,corpo));               // contorno
					vec2 olho=vec2(1.45,0.42*sign(f.y));
					float dOlho=distance(f,olho);
					if(dOlho<0.24)
						pc=dOlho<0.12?vec3(0.03):vec3(0.97);
					}
				else
					pc=cor*0.75+vec3(0.08);                                       // cauda e nadadeiras
				baseColor=vec4(pc,1.0);
				}
			else
				{
				if(sombra<1.0)
					baseColor.rgb*=0.72;
				/* rastro em V atrás da cauda e bolhinhas que saem da boca e somem */
				float rastro=smoothstep(0.2,0.0,abs(abs(l.y)-(-l.x-3.2)*0.42))*smoothstep(-6.4,-3.6,l.x)*step(l.x,-3.4)
				             *(0.5+0.5*sin(l.x*3.0+et*8.0+fase));
				float bolhas=0.0;
				for(int k=0;k<3;++k)
					{
					float ph=fract(et*0.45+fase*0.13+float(k)/3.0);
					vec2 pb=vec2(2.7+ph*2.4,(float(k)-1.0)*0.55+0.3*sin(ph*6.0+float(k)*2.0));
					float rb=0.14+0.22*ph;
					bolhas=max(bolhas,smoothstep(0.1,0.0,abs(distance(l,pb)-rb))*(1.0-ph));
					}
				baseColor.rgb=mix(baseColor.rgb,vec3(0.9,0.97,1.0),clamp(rastro*0.35+bolhas*0.8,0.0,1.0));
				}
			}
		else if(a.w>=9.0)
			{
			/* Pato, barquinho, pássaro e cachorro */
			float fase=float(i)*1.7;
			vec3 cb=baseColor.rgb;
			bool desenhou=true;
			if(a.w<10.0)
				{
				/* pato: corpo, cabeça, bico laranja, asas dobradas e a esteira em V atrás; branco ou marreco */
				float esc=max(0.3,extrasAg2[i].z);
				vec2 p=l/esc+vec2(0.0,0.06*sin(et*3.0+fase));
				float corpoP=length(p/vec2(1.35,0.82));
				float cabP=length((p-vec2(1.15,0.0))/vec2(0.5,0.46));
				float bico=length((p-vec2(1.66,0.0))/vec2(0.3,0.17));
				bool rabo=p.x<-1.1&&p.x>-1.75&&abs(p.y)<(p.x+1.75)*0.45;
				bool branco=hue<0.45;
				vec3 corC=branco?vec3(0.97,0.96,0.92):vec3(0.56,0.46,0.37);
				vec3 corH=branco?vec3(0.99):vec3(0.08,0.45,0.22);
				if(esc<0.9)
					{
					corC=vec3(1.0,0.86,0.3);                                                               // patinho amarelo
					corH=vec3(1.0,0.9,0.4);
					branco=true;
					}
				if(bico<1.0)
					cb=vec3(1.0,0.6,0.1);
				else if(cabP<1.0)
					{
					cb=corH*mix(1.15,0.8,cabP);
					if(distance(p,vec2(1.3,0.24*sign(p.y)))<0.08)
						cb=vec3(0.02);
					}
				else if(corpoP<1.0)
					{
					cb=corC*mix(1.15,0.72,corpoP);
					if(abs(abs(p.y)-0.45)<0.2&&p.x<0.6&&p.x>-0.95)
						cb*=0.8;                                                                         // asas dobradas
					if(!branco&&abs(p.x-0.72)<0.07)
						cb=vec3(0.95);                                                                   // colarinho branco
					}
				else if(rabo)
					cb=corC*0.78;
				else
					{
					float v=smoothstep(0.12,0.0,abs(abs(p.y)-(-p.x-1.6)*0.5))*step(p.x,-1.6)*smoothstep(-5.0,-1.7,p.x)
					        *(0.5+0.5*sin(p.x*3.0+et*6.0));
					if(v>0.05)
						cb=mix(cb,vec3(0.92,0.97,1.0),v*0.6);
					else
						desenhou=false;
					}
				}
			else if(a.w<11.0)
				{
				/* barquinho a vela: casco de madeira com tábuas, borda escura, mastro, vela branca que enche e a esteira */
				vec2 p=l;
				float meia=p.x>0.6?0.78*clamp((1.95-p.x)/1.35,0.0,1.0):0.78;
				bool casco=p.x>-1.6&&p.x<1.95&&abs(p.y)<meia&&!(p.x<-1.3&&length((p-vec2(-1.3,0.0))/vec2(0.3,0.78))>1.0);
				bool vela=p.x<0.3&&p.x>-1.25&&p.y>0.05&&p.y<(0.3-p.x)*0.62+0.07*sin(et*3.0+p.x*2.0+fase);
				if(vela)
					cb=mix(vec3(0.99),vec3(0.82,0.86,0.9),smoothstep(0.0,0.9,p.y))*(0.92+0.08*sin(p.x*9.0));
				else if(casco)
					{
					cb=mix(vec3(0.58,0.38,0.2),vec3(0.75,0.52,0.3),step(0.5,fract(p.y*3.2)));
					if(abs(p.y)>meia-0.14)
						cb=vec3(0.32,0.2,0.1);                                                           // borda
					if(fract(a.w)>0.5&&abs(p.y)>meia-0.3&&abs(p.y)<=meia-0.14)
						cb=vec3(0.75,0.15,0.15);                                                         // faixa colorida
					}
				else
					{
					float v=smoothstep(0.13,0.0,abs(abs(p.y)-(-p.x-1.6)*0.45))*step(p.x,-1.6)*smoothstep(-6.0,-1.7,p.x)
					        *(0.5+0.5*sin(p.x*2.6+et*5.0));
					if(v>0.05)
						cb=mix(cb,vec3(0.92,0.97,1.0),v*0.65);
					else
						desenhou=false;
					}
				if(distance(p,vec2(0.3,0.0))<0.13)
					cb=vec3(0.25,0.16,0.08);                                                             // mastro
				}
			else if(a.w<12.0)
				{
				/* pássaro: a sombra no chão (longe, porque ele está alto) e a ave batendo as asas; gaivota ou andorinha */
				vec4 e2=extrasAg2[i];
				float h=e2.y;                                                                            // altura: 1 alto, 0 no chão
				bool mergulho=e2.x>1.5&&e2.x<2.5;
				float abre=mergulho?0.08:0.5+0.5*sin(et*(e2.x>2.5?11.0:7.0)+fase);
				vec2 rsb=r-vec2(0.25+1.25*h,-0.25-1.25*h);                                              // a sombra chega perto ao descer
				vec2 lsb=vec2(c*rsb.x+s*rsb.y,-s*rsb.x+c*rsb.y);
				float tam=0.8+0.2*h;                                                                     // lá embaixo parece menor
				float ave=exPassaro(l/tam,abre);
				bool gaivota=hue<0.55;
				vec3 corpoA=gaivota?vec3(0.97):vec3(0.12,0.16,0.3);
				vec3 asaA=gaivota?vec3(0.72,0.75,0.8):vec3(0.1,0.12,0.22);
				if(e2.x>0.5&&e2.x<1.5)
					{
					/* pousado: asas fechadas, bicando o chão de vez em quando */
					float bica=pow(max(0.0,sin(et*2.3+fase)),10.0)*0.28;
					vec2 l0=l;
					l=l/1.45;                                                                        // no chão ele parece maior (está mais perto)
					float corpoP=length(l/vec2(0.8,0.36));
					float cab=length(l-vec2(0.62+bica,0.0));
					bool bicoP=l.x>0.8+bica&&l.x<1.08+bica&&abs(l.y)<0.07;
					bool caudaP=l.x<-0.6&&l.x>-1.15&&abs(l.y)<0.17-0.1*(l.x+0.6);
					if(bicoP)
						cb=gaivota?vec3(1.0,0.75,0.1):vec3(0.05);
					else if(cab<0.25)
						cb=distance(l,vec2(0.7+bica,0.12*sign(l.y)))<0.06?vec3(0.02):corpoA;
					else if(corpoP<1.0)
						cb=(abs(l.y)>0.14&&l.x<0.45)?asaA:(gaivota?corpoA:vec3(0.9,0.5,0.35));
					else if(caudaP)
						cb=asaA*0.9;
					else if(length(ls/vec2(1.3,0.6))<1.0)
						cb*=0.72;
					else
						desenhou=false;
					if(desenhou&&(corpoP<1.0||cab<0.25)&&(corpoP>0.82&&cab>0.19))
						cb=gaivota?cb*0.6:vec3(0.85,0.88,0.95);                                         // contorno: a andorinha escura aparece na areia
					l=l0;
					}
				else if(ave>0.5)
					{
					cb=ave<1.5?corpoA:(ave<2.5?asaA:corpoA*0.85);
					if(ave>1.5&&ave<2.5&&abs(l.y/tam)>2.3*(0.45+0.55*abre)*0.78)
						cb=gaivota?vec3(0.1):vec3(0.05);                                                 // pontas das asas
					if(ave<1.5&&l.x/tam>0.95&&abs(l.y/tam)<0.08)
						cb=gaivota?vec3(1.0,0.75,0.1):vec3(0.05);                                        // bico
					if(!gaivota&&ave<1.5&&l.x/tam<0.5&&l.x/tam>-0.4&&abs(l.y/tam)<0.14)
						cb=vec3(0.9,0.5,0.35);                                                           // peito
					}
				else if(exPassaro(lsb/tam,abre)>0.5)
					cb*=0.75;
				else if(mergulho&&e2.w>0.45)
					{
					/* tocou a água: respingo e ondinhas */
					float t2=(e2.w-0.45)/0.55;
					float onda=smoothstep(0.18,0.0,abs(length(r)-(0.4+t2*2.6)))*(1.0-t2)
					          +smoothstep(0.15,0.0,abs(length(r)-(0.2+t2*1.5)))*(1.0-t2)*0.7;
					if(onda>0.05)
						cb=mix(cb,vec3(0.92,0.97,1.0),clamp(onda,0.0,1.0));
					else
						desenhou=false;
					}
				else
					desenhou=false;
				}
			else
				{
				/* cachorro andando: corpo, cabeça com orelhas e focinho, rabo abanando, patas que alternam */
				bool senta=extrasAg2[i].x>0.5&&extrasAg2[i].x<1.5;
				float passo=senta?0.0:sin(et*(extrasAg2[i].x>2.5?14.0:9.0)+fase);                         // correndo: passo rápido
				vec2 p=l;
				float corpoD=senta?length((p-vec2(0.15,0.0))/vec2(0.85,0.58)):length(p/vec2(1.1,0.5));     // sentado: mais curto
				float cabD=length(p-vec2(1.05,0.0));
				float foc=length((p-vec2(1.45,0.0))/vec2(0.25,0.18));
				float orelha=min(length((p-vec2(0.9,0.36))/vec2(0.24,0.13)),length((p-vec2(0.9,-0.36))/vec2(0.24,0.13)));
				vec2 rr=p-vec2(senta?-0.68:-1.05,0.0);
				float abana=senta?0.55*sin(et*24.0+fase):0.35*sin(et*12.0+fase);                         // sentado do lado da mão: abana muito
				bool rabo=rr.x<0.0&&rr.x>-0.7&&abs(rr.y-abana*(-rr.x))<0.09;
				float pata=senta?min(length(p-vec2(0.78,0.24)),length(p-vec2(0.78,-0.24)))
				               :min(min(length(p-vec2(0.55+0.18*passo,0.42)),length(p-vec2(0.55-0.18*passo,-0.42))),
				                    min(length(p-vec2(-0.6-0.18*passo,0.42)),length(p-vec2(-0.6+0.18*passo,-0.42))));
				float tom=fract(hue*5.3);
				vec3 pelo=tom<0.35?vec3(0.78,0.52,0.25):(tom<0.6?vec3(0.15,0.12,0.1):(tom<0.8?vec3(0.95,0.93,0.88):vec3(0.55,0.55,0.57)));
				vec2 sl=ls;
				if(foc<1.0)
					cb=distance(p,vec2(1.67,0.0))<0.09?vec3(0.03):pelo*0.85;
				else if(cabD<0.42)
					{
					cb=pelo*mix(1.12,0.85,cabD/0.42);
					if(distance(p,vec2(1.2,0.17*sign(p.y)))<0.07)
						cb=vec3(0.02);
					}
				else if(orelha<1.0)
					cb=pelo*0.65;
				else if(corpoD<1.0)
					{
					cb=pelo*mix(1.12,0.75,corpoD);
					if(tom>=0.6&&tom<0.8&&exPontos(p*2.2+vec2(fase),0.3,0.45)>0.5)
						cb=vec3(0.15);                                                                   // manchas
					}
				else if(rabo)
					cb=pelo*0.85;
				else if(pata<0.15)
					cb=pelo*0.7;
				else if(length(sl/vec2(1.15,0.55))<1.0)
					cb*=0.72;                                                                            // sombra
				else
					desenhou=false;
				}
			if(desenhou)
				{
				extrasMarca=1.0;
				baseColor=vec4(cb,1.0);
				}
			}
		else if(a.w>=4.0)
			{
			/* Outros animais marinhos (espécie = parte inteira): tubarão, tartaruga, água-viva, polvo e arraia */
			float fase=float(i)*1.7;
			vec3 cb=baseColor.rgb;
			bool desenhou=true;
			if(a.w<5.0)
				{
				/* tubarão: corpo comprido cinza-azulado que serpenteia, cauda em meia-lua, nadadeiras e barbatana */
				float onda=0.3*sin(et*5.0+l.x*0.9+fase)*smoothstep(1.5,-3.5,l.x);
				vec2 f=vec2(l.x,l.y+onda);
				float corpoT=(f.x*f.x)/11.5+(f.y*f.y)/(1.05*1.05*(1.0-0.55*clamp(-f.x/3.4,0.0,1.0)));
				bool caudaT=f.x<-3.0&&f.x>-4.4&&abs(f.y)<(-f.x-3.0)*1.1&&!(f.x<-3.9&&abs(f.y)<(-f.x-3.9)*1.0);
				bool peitoral=f.x>-0.2&&f.x<1.0&&abs(f.y)>0.8&&abs(f.y)<0.8+(1.0-f.x)*1.1&&abs(f.y)<2.1;
				if(corpoT<1.0)
					{
					cb=mix(vec3(0.42,0.5,0.58),vec3(0.2,0.26,0.33),smoothstep(0.0,0.9,abs(f.y)));
					cb=mix(cb,vec3(0.12,0.15,0.2),smoothstep(0.18,0.05,abs(f.y))*step(-1.0,f.x)*step(f.x,0.4));     // barbatana de cima
					cb=mix(cb,vec3(0.86,0.9,0.93),smoothstep(0.62,0.95,abs(f.y)/(1.05*(1.0-0.55*clamp(-f.x/3.4,0.0,1.0)))));   // barriga branca aparecendo dos lados
					if(f.x>1.2&&f.x<2.0&&abs(f.y)>0.35&&fract(f.x*3.6)<0.3)
						cb*=0.6;                                                                            // guelras
					cb+=vec3(0.22)*smoothstep(0.5,0.0,distance(f,vec2(1.0,0.2)));                           // brilho molhado
					if(distance(f,vec2(2.5,0.5*sign(f.y)))<0.13)
						cb=vec3(0.02);                                                                      // olhos
					cb*=mix(1.0,0.6,smoothstep(0.82,1.0,corpoT));
					}
				else if(caudaT||peitoral)
					cb=vec3(0.26,0.32,0.4);
				else
					desenhou=false;
				}
			else if(a.w<6.0)
				{
				/* tartaruga: casco abaulado com gomos e borda clara, cabeça com olhos, rabinho e quatro nadadeiras
				   pintadas que remam */
				float rema=0.35*sin(et*3.0+fase);
				float casco=length(l/vec2(1.9,1.5));
				float cabecaT=length((l-vec2(2.35,0.0))/vec2(0.66,0.5));
				float nadF=length((vec2(l.x-1.25-rema*0.45,abs(l.y)-1.8))/vec2(0.95,0.44));
				float nadT=length((vec2(l.x+1.45+rema*0.3,abs(l.y)-1.35))/vec2(0.62,0.36));
				bool rabo=l.x<-1.8&&l.x>-2.5&&abs(l.y)<(l.x+2.5)*0.3;
				vec3 pele=mix(vec3(0.42,0.6,0.3),vec3(0.6,0.62,0.36),fract(a.w));
				if(casco<1.0)
					{
					vec2 gomo=fract(l*0.95+vec2(0.5*step(0.5,fract(l.y*0.475)),0.0))-0.5;
					vec3 fundoC=mix(vec3(0.18,0.4,0.2),vec3(0.5,0.4,0.17),fract(a.w)*0.8);
					cb=mix(fundoC*1.35,fundoC*0.8,smoothstep(0.0,0.42,length(gomo)));                       // cada gomo é mais claro no meio
					cb*=mix(0.5,1.0,smoothstep(0.0,0.1,0.5-max(abs(gomo.x),abs(gomo.y))));                    // sulcos
					cb=mix(cb,vec3(0.72,0.66,0.38),smoothstep(0.84,0.93,casco));                             // borda clara
					cb*=mix(1.2,0.75,smoothstep(0.2,1.0,casco))+0.25*smoothstep(0.6,0.0,distance(l,vec2(0.5,0.4)));
					cb*=mix(1.0,0.6,smoothstep(0.95,1.0,casco));
					}
				else if(cabecaT<1.0)
					cb=distance(l,vec2(2.55,0.27*sign(l.y)))<0.11?vec3(0.02):pele*mix(1.15,0.8,cabecaT);
				else if(nadF<1.0||nadT<1.0)
					cb=pele*0.9*(0.8+0.3*step(0.7,exRuido(l*4.0)));                                          // nadadeiras com pintas
				else if(rabo)
					cb=pele*0.85;
				else
					desenhou=false;
				}
			else if(a.w<7.0)
				{
				/* água-viva: sombrinha que pulsa, quase transparente, e fios compridos atrás */
				float pulso=1.0+0.14*sin(et*2.6+fase);
				float dAv=length(l/vec2(1.35*pulso,1.35/pulso));
				vec3 corAv=mix(vec3(1.0,0.6,0.85),vec3(0.65,0.6,1.0),fract(a.w));
				float fio=0.0;
				for(int k=0;k<4;++k)
					{
					float yk=(float(k)-1.5)*0.55;
					fio=max(fio,smoothstep(0.14,0.03,abs(l.y-yk-0.3*sin(l.x*1.6+et*2.5+float(k)+fase)))*step(l.x,-0.6)*smoothstep(-5.0,-1.0,l.x));
					}
				if(dAv<1.0)
					{
					cb=mix(cb,corAv+vec3(0.3)*smoothstep(1.0,0.2,dAv),0.55+0.3*smoothstep(0.75,1.0,dAv));
					cb+=vec3(0.35)*smoothstep(0.22,0.0,abs(dAv-0.55))*(0.5+0.5*sin(atan(l.y,l.x)*8.0));     // desenho em trevo dentro da sombrinha
					}
				else if(dAv<1.5)
					{
					cb=mix(cb,corAv,0.25*smoothstep(1.5,1.0,dAv)*(0.6+0.4*sin(et*2.6+fase)));               // brilho em volta
					desenhou=fio>0.05||dAv<1.5;
					}
				else if(fio>0.05)
					cb=mix(cb,corAv,fio*0.7);
				else
					desenhou=false;
				}
			else if(a.w<8.0)
				{
				/* polvo: cabeça redonda com dois olhos e oito braços que ondulam */
				float dP=length(l/vec2(1.25,1.05));
				vec3 corP=mix(vec3(0.85,0.35,0.3),vec3(0.6,0.3,0.7),fract(a.w));
				float braco=0.0;
				float angP=atan(l.y,l.x);
				float rP=length(l);
				for(int k=0;k<8;++k)
					{
					float ak=float(k)*0.7854+0.39+0.25*sin(et*2.0+float(k)*1.3+fase);
					float dAng=abs(mod(angP-ak+3.1416,6.2832)-3.1416);
					braco=max(braco,smoothstep(0.22,0.05,dAng*rP+0.0*rP)*step(1.0,rP)*smoothstep(3.6,2.2,rP+0.3*sin(rP*3.0-et*3.0+float(k))));
					}
				if(dP<1.0)
					{
					cb=corP*mix(1.2,0.7,smoothstep(0.2,1.0,dP));
					if(distance(l,vec2(0.55,0.42*sign(l.y)))<0.2)
						cb=distance(l,vec2(0.62,0.42*sign(l.y)))<0.1?vec3(0.02):vec3(0.97);
					}
				else if(braco>0.3)
					cb=corP*(0.7+0.3*braco)+vec3(0.35,0.3,0.3)*step(0.75,fract(rP*2.6))*step(0.7,braco);    // braços com ventosas
				else
					desenhou=false;
				}
			else
				{
				/* arraia: corpo em losango de pontas arredondadas, asas que batem, olhos, pintas claras e rabo fino */
				float bate=0.25*sin(et*3.2+fase);
				vec2 f=vec2(l.x,l.y*(1.0+bate*smoothstep(0.4,2.4,abs(l.y))));
				float losango=pow(pow(abs(f.x)/2.05,1.5)+pow(abs(f.y)/2.5,1.5),1.0/1.5);
				bool rabo=f.x<-1.6&&f.x>-5.2&&abs(f.y-0.28*sin(f.x*1.4+et*3.0+fase))<0.11*smoothstep(-5.2,-1.6,f.x)+0.03;
				if(losango<1.0)
					{
					vec3 lombo=mix(vec3(0.5,0.45,0.36),vec3(0.24,0.3,0.42),fract(a.w));
					cb=mix(lombo*1.25,lombo*0.7,smoothstep(0.1,1.0,losango));
					cb=mix(cb,vec3(0.85,0.87,0.85),smoothstep(0.88,0.98,losango)*0.7);                       // beirada clara
					cb+=vec3(0.3)*exPontos(f*1.5+vec2(fase),0.22,0.55)*0.5;                                  // pintas
					cb*=0.9+0.2*smoothstep(0.0,2.0,abs(f.y))*bate*2.0;                                       // a asa que sobe pega luz
					if(abs(f.y)<0.14&&f.x<1.2&&f.x>-1.5)
						cb*=0.8;                                                                            // espinha
					if(distance(f,vec2(1.15,0.48*sign(f.y)))<0.13)
						cb=vec3(0.02);
					}
				else if(rabo)
					cb=vec3(0.24,0.26,0.32);
				else
					desenhou=false;
				}
			if(desenhou)
				{
				extrasMarca=1.0;
				baseColor=vec4(cb,1.0);
				}
			}
		else if(a.w>=3.0)
			{
			/* Pessoa vista de cima: sapatos que alternam, ombros com a camisa, braços balançando e cabeça.
			   Cada uma sai diferente (pelo número e pela cor): criança ou adulto, cabelo curto ou comprido,
			   boné ou chapéu, mochila, camisa lisa ou listrada. */
			float jeito=exHash(vec2(float(i)*1.7,hue*31.0));
			float porte=(jeito<0.2?1.25:(jeito>0.85?1.85:1.6));                 // criança, adulto alto ou médio
			vec2 pl=l/porte;
			vec2 pls=ls/porte;
			float passo=sin(et*7.0+float(i)*1.3);
			vec3 pele=mix(vec3(0.97,0.82,0.68),vec3(0.42,0.27,0.17),fract(hue*7.3));
			float tomCabelo=fract(hue*3.7);
			vec3 cabelo=tomCabelo<0.3?vec3(0.08,0.06,0.05):(tomCabelo<0.6?vec3(0.36,0.22,0.1):(tomCabelo<0.8?vec3(0.78,0.6,0.25):(tomCabelo<0.92?vec3(0.6,0.2,0.1):vec3(0.75))));
			float enfeite=fract(jeito*7.0);                                     // <0.25 boné, <0.4 chapéu, resto sem nada
			float peE=length((pl-vec2(0.55*passo,0.38))/vec2(0.5,0.24));
			float peD=length((pl-vec2(-0.55*passo,-0.38))/vec2(0.5,0.24));
			float ombros=length(pl/vec2(0.55,1.0));
			float bracoE=length(pl-vec2(-0.45*passo,0.98));
			float bracoD=length(pl-vec2(0.45*passo,-0.98));
			float cabeca=length(pl-vec2(0.08,0.0));
			float sombra=length(pls/vec2(0.75,1.1));
			bool mochila=fract(jeito*13.0)<0.3&&abs(pl.x+0.55)<0.28&&abs(pl.y)<0.55;
			bool comprido=fract(jeito*17.0)<0.35&&length((pl-vec2(-0.35,0.0))/vec2(0.5,0.62))<1.0;
			if(enfeite<0.4&&cabeca<(enfeite<0.25?0.56:0.86))
				{
				vec3 corChapeu=enfeite<0.25?vec3(0.55)+0.45*cos(6.2832*(vec3(jeito*5.0)+vec3(0.0,0.33,0.67))):vec3(0.86,0.76,0.5);
				baseColor=vec4(corChapeu*(cabeca<0.4?1.0:0.82),1.0);          // boné (com aba) ou chapéu de palha
				}
			else if(enfeite<0.25&&abs(pl.y)<0.38&&pl.x>0.4&&pl.x<1.0)
				baseColor=vec4((vec3(0.55)+0.45*cos(6.2832*(vec3(jeito*5.0)+vec3(0.0,0.33,0.67))))*0.8,1.0);  // aba do boné
			else if(cabeca<0.52)
				{
				vec3 topo=cabelo*(0.8+0.35*smoothstep(0.5,0.0,cabeca))+vec3(0.25)*smoothstep(0.22,0.0,distance(pl,vec2(-0.05,0.18)));  // brilho no cabelo
				baseColor=vec4(pl.x>0.3?pele*(1.05-0.25*smoothstep(0.3,0.52,cabeca)):topo,1.0);            // rosto para a frente
				baseColor.rgb*=mix(1.0,0.7,smoothstep(0.46,0.52,cabeca));
				}
			else if(comprido)
				baseColor=vec4(cabelo*0.9,1.0);
			else if(mochila)
				baseColor=vec4(mix(vec3(0.2,0.25,0.45),vec3(0.7,0.2,0.2),step(0.5,fract(jeito*29.0))),1.0);
			else if(ombros<1.0)
				{
				vec3 camisa=cor*mix(1.15,0.7,smoothstep(0.3,1.0,ombros));
				if(fract(jeito*11.0)<0.3)
					camisa*=0.75+0.35*step(0.5,fract(pl.y*3.2));              // listrada
				baseColor=vec4(camisa,1.0);
				}
			else if(bracoE<0.27||bracoD<0.27)
				baseColor=vec4(min(bracoE,bracoD)<0.15?pele:cor*0.82,1.0);                                 // manga da camisa e a mão
			else if(peE<1.0||peD<1.0)
				baseColor=vec4(fract(jeito*23.0)<0.5?vec3(0.12,0.1,0.1):mix(vec3(0.95),vec3(0.8,0.15,0.15),step(0.75,fract(jeito*23.0))),1.0);  // sapato preto ou tênis
			else if(sombra<1.0)
				baseColor.rgb*=0.72;
			}
		else
			{
			/* Carrinhos de quatro modelos (pelo número do carrinho): sedã, picape, van e esportivo;
			   o sedã amarelado vira táxi. Carroceria arredondada, vidros, rodas, faróis e lanternas. */
			float modelo=mod(float(i),4.0);
			vec2 tam=modelo<0.5?vec2(2.15,1.05):(modelo<1.5?vec2(2.35,1.08):(modelo<2.5?vec2(2.65,1.15):vec2(2.25,0.98)));
			float carro=exCaixa(l,tam,modelo>2.5?0.62:0.5);
			float sombra=exCaixa(ls,tam,0.5);
			vec2 roda=vec2(abs(l.x)-tam.x*0.62,abs(l.y)-tam.y);
			bool rodas=abs(roda.x)<0.42&&abs(roda.y)<0.28;
			float facho=l.x>tam.x-0.15&&l.x<tam.x+4.3?smoothstep(1.2+0.35*(l.x-tam.x),0.0,abs(l.y))*smoothstep(tam.x+4.3,tam.x,l.x):0.0;
			if(carro<0.0)
				{
				vec3 cc=mix(cor*1.15,cor*0.75,smoothstep(0.0,1.0,abs(l.y)));
				if(modelo<0.5)
					{
					/* sedã */
					if(exCaixa(l-vec2(-0.25,0.0),vec2(0.95,0.78),0.35)<0.0)
						{
						cc=cor*0.85;
						if(l.x>0.3)
							cc=mix(vec3(0.55,0.75,0.95),vec3(0.9,0.97,1.0),smoothstep(0.3,0.75,l.x));   // para-brisa
						else if(l.x<-0.9)
							cc=vec3(0.45,0.6,0.75);                                                    // vidro de trás
						if(hue>0.1&&hue<0.2&&abs(l.x+0.25)<0.28&&abs(l.y)<0.4)
							cc=vec3(0.98,0.98,0.9);                                                    // luminoso do táxi
						}
					}
				else if(modelo<1.5)
					{
					/* picape: cabine na frente e caçamba aberta atrás, com carga */
					if(exCaixa(l-vec2(0.55,0.0),vec2(0.7,0.8),0.3)<0.0)
						cc=l.x>0.75?mix(vec3(0.55,0.75,0.95),vec3(0.9,0.97,1.0),smoothstep(0.75,1.15,l.x)):cor*0.85;
					else if(l.x<-0.3&&l.x>-tam.x+0.3&&abs(l.y)<tam.y-0.22)
						cc=distance(l,vec2(-1.2,0.1))<0.45?vec3(0.6,0.42,0.2):cor*0.45;                // caçamba e uma caixa
					}
				else if(modelo<2.5)
					{
					/* van: teto comprido com as janelas dos lados */
					if(exCaixa(l-vec2(-0.2,0.0),vec2(1.9,0.9),0.3)<0.0)
						{
						cc=mix(vec3(0.97),cor,0.25);
						if(abs(l.y)>0.62&&fract(l.x*0.9+0.3)<0.7)
							cc=vec3(0.45,0.62,0.8);                                                    // janelas
						if(l.x>1.3)
							cc=mix(vec3(0.55,0.75,0.95),vec3(0.9,0.97,1.0),smoothstep(1.3,1.7,l.x));   // para-brisa
						}
					}
				else
					{
					/* esportivo: baixo, com faixa no meio e aerofólio */
					if(abs(l.y)<0.16)
						cc=vec3(0.97);
					if(exCaixa(l-vec2(-0.35,0.0),vec2(0.7,0.66),0.3)<0.0)
						cc=l.x>-0.05?vec3(0.7,0.86,0.98):vec3(0.12);
					if(l.x<-tam.x+0.35)
						cc=vec3(0.1);
					}
				if(l.x>tam.x-0.4&&abs(l.y)>tam.y*0.42&&abs(l.y)<tam.y*0.9)
					cc=vec3(1.0,0.98,0.75);                                                          // faróis
				if(l.x<-tam.x+0.3&&abs(l.y)>tam.y*0.42&&abs(l.y)<tam.y*0.9)
					cc=vec3(0.95,0.1,0.1);                                                           // lanternas
				if(abs(l.x-0.05)<0.03&&abs(l.y)>tam.y*0.55)
					cc*=0.65;                                                                    // vão das portas
				cc+=vec3(0.3)*smoothstep(0.22,0.0,abs(l.y-0.35-l.x*0.12))*smoothstep(tam.x,0.0,abs(l.x))*0.6;   // reflexo da lataria
				cc*=mix(1.0,0.6,smoothstep(-0.12,0.0,carro));                                      // contorno
				baseColor=vec4(cc,1.0);
				}
			else if(rodas)
				baseColor=vec4(length(roda)<0.14?vec3(0.6):vec3(0.08),1.0);                         // pneu com calota
			else
				{
				if(sombra<0.0)
					baseColor.rgb*=0.7;
				baseColor.rgb+=vec3(1.0,0.95,0.6)*facho*0.35;
				/* poeira levantada atrás: três nuvenzinhas que crescem e somem */
				float poeira=0.0;
				for(int k=0;k<3;++k)
					{
					float ph=fract(et*1.4+float(i)*0.37+float(k)/3.0);
					vec2 pp=vec2(-tam.x-0.5-ph*2.8,(float(k)-1.0)*0.45*(1.0+ph));
					float rp=0.35+0.75*ph;
					poeira=max(poeira,smoothstep(rp,rp*0.3,distance(l,pp))*(1.0-ph));
					}
				baseColor.rgb=mix(baseColor.rgb,vec3(0.9,0.85,0.74),poeira*0.5);
				}
			}
		}
	/* Chuva pelo painel: uma nuvem branca com gotas caindo em cada ponto de chuva */
	for(int k=0;k<4;++k)
		{
		vec4 ch=extrasChuva[k];
		if(ch.z<0.5)
			continue;
		vec2 qn=ep.xy-ch.xy;
		if(dot(qn,qn)>36.0)
			continue;
		float gota=smoothstep(0.12,0.02,abs(fract(qn.x*0.9)-0.5))*step(0.55,fract(qn.y*0.45+et*2.2+floor(qn.x*0.9)*0.37))
		           *smoothstep(3.6,2.0,length(qn));
		baseColor.rgb=mix(baseColor.rgb,vec3(0.75,0.9,1.0),gota*0.8);
		float nuvem=min(min(length((qn-vec2(-1.3,0.3))/vec2(1.5,1.1)),length((qn-vec2(1.2,0.2))/vec2(1.6,1.2))),
		                length((qn-vec2(0.0,0.9))/vec2(1.7,1.3)));
		if(nuvem<1.0)
			{
			baseColor.rgb=mix(baseColor.rgb,mix(vec3(1.0),vec3(0.78,0.82,0.9),smoothstep(0.3,1.0,nuvem)),0.9);
			extrasMarca=1.0; // a água que cai não cobre a nuvem
			}
		}
	/* Membros do laboratório: plaquinha pequena com o nome acima do boneco (ele fica à vista);
	   bolinha verde = está no laboratório agora. O rosto só aparece, num balão acima do nome,
	   quando alguém toca no membro no painel. */
	for(int i=0;i<8;++i)
		{
		if(float(i)>=extrasNomesCfg.x-0.5)
			break;
		vec4 a=extrasAg[i];
		float cod=extrasNomes[i*9];
		float mostra=floor(cod/32.0);
		cod-=32.0*mostra;
		float pres=floor(cod/16.0);
		float len=cod-16.0*pres;
		if(a.w<0.5||len<0.5)
			continue;
		float gr=extrasNomesCfg.y;
		vec2 cima=gr<0.5?vec2(0.0,1.0):(gr<1.5?vec2(-1.0,0.0):(gr<2.5?vec2(0.0,-1.0):vec2(1.0,0.0)));
		vec2 dir=vec2(cima.y,-cima.x);
		if(mostra>0.5&&float(i)<extrasNomesCfg.z-0.5)
			{
			float R=2.3; // raio do rosto no balão, em cm
			vec2 rr=ep.xy-(a.xy+cima*6.4);
			vec2 tr=vec2(dot(rr,dir),dot(rr,cima));
			float dr=length(tr);
			float hueM=fract(a.w);
			vec3 corM=vec3(0.55)+0.45*cos(6.2832*(vec3(hueM)+vec3(0.0,0.33,0.67)));
			if(tr.y<-R*0.8&&tr.y>-R-1.2&&abs(tr.x)<(tr.y+R+1.2)*0.45)
				baseColor.rgb=mix(corM,vec3(1.0),0.3);                  // bico do balão
			if(dr<R+0.35)
				{
				baseColor.rgb=mix(baseColor.rgb,mix(corM,vec3(1.0),0.3),smoothstep(R+0.35,R+0.2,dr));
				if(dr<R)
					{
					vec4 rosto=texture2D(extrasRostos,vec2((float(i)+0.5+tr.x/(2.0*R))/8.0,0.5-tr.y/(2.0*R)));
					baseColor.rgb=mix(vec3(0.95),rosto.rgb,rosto.a);
					}
				}
			}
		vec2 rel=ep.xy-(a.xy+cima*2.7);
		vec2 t=vec2(dot(rel,dir),dot(rel,cima));
		float px=0.2;
		float cw=4.0*px;
		float larg=len*cw-px;
		float margem=pres>0.5?0.95:0.35; // espaço para a bolinha verde
		if(t.x<-larg*0.5-margem||t.x>larg*0.5+0.35||abs(t.y)>2.5*px+0.3)
			continue;
		baseColor.rgb=mix(baseColor.rgb,vec3(0.04,0.06,0.1),0.78);
		if(pres>0.5&&distance(t,vec2(-larg*0.5-0.48,0.0))<0.3)
			baseColor.rgb=vec3(0.2,0.95,0.4);
		float cx=(t.x+larg*0.5)/cw;
		float k=floor(cx);
		float colf=floor(fract(cx)*4.0);
		float rowf=floor((2.5*px-t.y)/px);
		if(k<0.0||k>len-0.5||colf>2.5||rowf<0.0||rowf>4.5)
			continue;
		float g=extrasNomes[i*9+1+int(k)];
		if(floor(mod(g/exp2(rowf*3.0+colf),2.0))>0.5)
			baseColor.rgb=vec3(1.0);
		}
	/* Labirinto de água: nascente (ondas azuis) e alvo (bandeira que gira; comemora ao chegar) */
	if(extrasLab2.y>0.5)
		{
		float lt=extrasLab2.z;
		float dn=distance(ep.xy,extrasLab.xy);
		if(dn<4.5)
			{
			float ondas=0.5+0.5*sin(dn*3.2-et*6.0);
			vec3 agua=mix(vec3(0.15,0.45,0.95),vec3(0.75,0.95,1.0),ondas*smoothstep(4.5,0.5,dn));
			baseColor=vec4(mix(baseColor.rgb,agua,smoothstep(4.5,1.0,dn)*0.85),1.0);
			}
		float da=distance(ep.xy,extrasLab.zw);
		float ra=extrasLab2.x;
		if(extrasLab2.y<1.5)
			{
			vec2 q=ep.xy-extrasLab.zw;
			float ang=atan(q.y,q.x);
			float anel=smoothstep(0.35,0.0,abs(da-ra))*step(0.0,sin(ang*8.0+et*2.5));
			float centro=smoothstep(1.0,0.6,da);
			float bandeira=q.x>0.0&&q.x<2.2&&q.y>0.3&&q.y<1.5-q.x*0.4?1.0:0.0;
			float mastro=abs(q.x)<0.12&&q.y>-0.6&&q.y<1.6?1.0:0.0;
			baseColor.rgb=mix(baseColor.rgb,vec3(0.2,0.95,0.35),max(anel,centro)*0.9);
			baseColor.rgb=mix(baseColor.rgb,vec3(0.95,0.15,0.15),bandeira*(0.85+0.15*sin(et*6.0+q.x*3.0)));
			baseColor.rgb=mix(baseColor.rgb,vec3(0.95),mastro);
			}
		else
			{
			/* Chegou! Anéis dourados que se abrem e faíscas por alguns segundos */
			float onda=smoothstep(0.6,0.0,abs(da-mod(lt*7.0,14.0)))*smoothstep(6.0,1.0,lt);
			vec2 q=ep.xy-extrasLab.zw;
			float faisca=pow(max(0.0,sin(atan(q.y,q.x)*12.0+lt*3.0)),40.0)*smoothstep(ra*3.0,0.0,da)*smoothstep(8.0,0.0,lt);
			float ouro=smoothstep(ra+0.5,ra-0.5,da)*(0.6+0.4*sin(et*5.0));
			baseColor.rgb=mix(baseColor.rgb,vec3(1.0,0.82,0.2),clamp(ouro+onda,0.0,1.0));
			baseColor.rgb+=vec3(1.0,0.9,0.5)*faisca;
			}
		}
	/* troca de brincadeira: o desenho novo aparece aos poucos sobre o relevo */
	baseColor=mix(exAntes,baseColor,extrasMundo2.w);
	}
)GLSL";

void Extras::uploadUniforms(const GLint locations[NUM_UNIFORMS]) const
	{
	glUniformMatrix4fvARB(locations[0],1,GL_FALSE,caixa);
	GLfloat ag[MAX_AGENTES*4];
	for(int i=0;i<MAX_AGENTES;++i)
		{
		bool visivel=i<quantos&&tipo[i]>0.5f&&vel[i]!=0.0f&&!(modo==MUNDO&&(escondido[i]||!mundoPronto));
		ag[i*4+0]=ax[i]-(modo==MUNDO?camX:0.0f); // na cidade, a imagem desliza
		ag[i*4+1]=ay[i];
		ag[i*4+2]=ang[i];
		ag[i*4+3]=visivel?tipo[i]:0.0f;
		}
	glUniform4fvARB(locations[1],MAX_AGENTES,ag);
	glUniform4fARB(locations[2],tx,ty,traio,tz);
	/* .y: 0 = não achado; achado = 1 + segundos desde então (a animação do baú abrindo usa o tempo) */
	glUniform4fARB(locations[3],GLfloat(tempo),achado?1.0f+GLfloat(std::min(600.0,tempo-quandoAchou)):0.0f,
	               tesouro?GLfloat(tipoTesouro):0.0f,nivelAgua);
	glUniform4fARB(locations[4],GLfloat(efeito),referencia,GLfloat(tipoTesouro),
	               jogo==VULCAO&&jogoFase>0?1.0f:0.0f); // .w: jogo do vulcão (a água é lava; sem lava de enfeite)
	glUniform4fARB(locations[5],lsx,lsy,ltx,lty);
	glUniform4fARB(locations[6],lraio,labirinto?GLfloat(labEstado):0.0f,
	               labEstado==2?GLfloat(tempo-labChegou):GLfloat(tempo-labInicio),0.0f);
	/* nomes dos membros: cada letra é um desenho de 3x5 pontos (bit = linha*3 + coluna, de cima para baixo) */
	static const unsigned short LETRAS[26]={
		0x5BEA,0x3AEB,0x624E,0x3B6B,0x72CF,0x12CF,0x6B4E,0x5BED,0x7497,0x2B24,0x5AED,0x7249,0x5BFD,
		0x5B6F,0x2B6A,0x12EB,0x4D6A,0x5AEB,0x388E,0x2497,0x7B6D,0x2B6D,0x5FED,0x5AAD,0x24AD,0x72A7};
	GLfloat nm[MAX_NOMES*9];
	for(int i=0;i<MAX_NOMES;++i)
		{
		int len=0;
		for(int c=0;c<MAX_LETRAS;++c)
			{
			char ch=((modo==MEMBROS||modo==MUNDO)&&i<nNomes&&len==c)?nomes[i][c]:0;
			if(ch>='A'&&ch<='Z')
				{
				nm[i*9+1+c]=GLfloat(LETRAS[ch-'A']);
				len=c+1;
				}
			else
				nm[i*9+1+c]=0.0f;
			}
		bool daqui=(modo==MEMBROS||modo==MUNDO)&&i<nNomes;
		nm[i*9]=GLfloat(len+((daqui&&presente[i])?16:0)+((daqui&&tempo<rostoAte[i])?32:0)); // +16 presente, +32 mostra o rosto
		}
	glUniform1fvARB(locations[9],MAX_NOMES*9,nm);
	bool comNomes=modo==MEMBROS||modo==MUNDO;
	glUniform4fARB(locations[10],comNomes?GLfloat(nNomes):0.0f,GLfloat(giroNomes),
	               comNomes?GLfloat(nFotos):0.0f,0.0f);
	GLfloat ch[MAX_CHUVAS*4];
	for(int k=0;k<MAX_CHUVAS;++k)
		{
		ch[k*4]=chuvas[k].x; ch[k*4+1]=chuvas[k].y; ch[k*4+2]=chuvas[k].ate>tempo?1.0f:0.0f; ch[k*4+3]=0.0f;
		}
	glUniform4fvARB(locations[14],MAX_CHUVAS,ch);
	bool cidade=modo==MUNDO&&mundoPronto; // (o nome ficou da cidade antiga: hoje é a sala do laboratório)
	glUniform4fARB(locations[12],camX,bmax[0]-bmin[0],bmax[1]-bmin[1],cidade?1.0f:0.0f);
	/* hora local (0-24): a cidade escurece à noite e as janelas e os postes acendem */
	float hora=horaFixa;
	if(hora<0.0f)
		{
		std::time_t agora=std::time(0);
		std::tm* lt=std::localtime(&agora);
		hora=lt!=0?float(lt->tm_hour)+float(lt->tm_min)/60.0f:12.0f;
		}
	glUniform4fARB(locations[13],bmin[0],bmin[1],hora,
	               GLfloat(std::max(0.0,std::min(1.0,(tempo-trocaTempo)/0.6)))); // .w: transição (0 começando .. 1 pronta)
	GLfloat cg[COPIA_MAX*4];
	for(int k=0;k<COPIA_MAX;++k)
		for(int j=0;j<4;++j)
			cg[k*4+j]=copia?copiaG[k][j]:0.0f;
	glUniform4fvARB(locations[15],COPIA_MAX,cg);
	glUniform4fARB(locations[16],copia?1.0f:0.0f,copiaTol,copiaRef,copiaNota+(copiaConseguiu>0.0?1000.0f:0.0f));
	GLfloat a2[MAX_AGENTES*4];
	for(int i=0;i<MAX_AGENTES;++i)
		{
		a2[i*4+0]=GLfloat(acao[i]);
		a2[i*4+1]=voo[i];
		a2[i*4+2]=escala[i];
		a2[i*4+3]=acao[i]==2?std::min(1.0f,acaoT[i]/1.8f):0.0f;
		}
	glUniform4fvARB(locations[17],MAX_AGENTES,a2);
	GLfloat mh[MAX_MAOS*4];
	for(int k=0;k<MAX_MAOS;++k)
		{
		mh[k*4+0]=maos[k].x;
		mh[k*4+1]=maos[k].y;
		mh[k*4+2]=maos[k].r;
		mh[k*4+3]=(querMaos()&&tempo-maos[k].visto<0.6)?1.0f:0.0f;
		}
	glUniform4fvARB(locations[18],MAX_MAOS,mh);
	/* painel de rostos na unidade de textura 6 (a 5 é a foto do modo Foto). O número da unidade vai
	   sempre: um sampler2D deixado na unidade 0 (das texturas retangulares) invalidaria o desenho. */
	glUniform1iARB(locations[11],6);
	if(comNomes&&nFotos>0&&!atlas.empty())
		{
		glActiveTextureARB(GL_TEXTURE6_ARB);
		if(atlasTextura==0)
			{
			glGenTextures(1,&atlasTextura);
			atlasNovo=true;
			}
		glBindTexture(GL_TEXTURE_2D,atlasTextura);
		if(atlasNovo)
			{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
			glPixelStorei(GL_UNPACK_ALIGNMENT,1);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,ATLAS_LADO*MAX_NOMES,ATLAS_LADO,0,GL_RGBA,GL_UNSIGNED_BYTE,&atlas[0]);
			atlasNovo=false;
			}
		glActiveTextureARB(GL_TEXTURE0_ARB);
		}
	GLfloat jg[MAX_PECAS*8];
	for(int i=0;i<MAX_PECAS;++i)
		{
		const Peca& p=pecas[i];
		bool vale=(jogo!=SEM_JOGO||labirinto||modo==MUNDO)&&i<nPecas;
		GLfloat* v=jg+i*8;
		v[0]=p.x-(modo==MUNDO?camX:0.0f); v[1]=p.y; v[2]=p.raio; v[3]=vale?GLfloat(p.tipo):0.0f;
		v[4]=GLfloat(p.estado); v[5]=p.nivel; v[6]=GLfloat(p.ordem);
		v[7]=p.tipo==P_BARCO?p.ang:GLfloat(p.time);
		}
	glUniform4fvARB(locations[7],MAX_PECAS*2,jg);
	glUniform4fARB(locations[8],GLfloat(jogo!=SEM_JOGO&&jogoFase>0?jogo:(labirinto&&nPecas>0?99:(modo==MUNDO&&nPecas>0?98:0))),GLfloat(jogoFase),
	               GLfloat(jogoFase==2?tempo-jogoFim:tempo-jogoInicio),
	               jogo==ROTA?GLfloat(proximoPortao):GLfloat(jogoVencedor));
	}
