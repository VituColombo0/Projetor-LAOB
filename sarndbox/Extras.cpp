/***********************************************************************
Extras - caça ao tesouro, peixes e carrinhos sobre o relevo (Projetor-LAOB).
***********************************************************************/

#include "Extras.h"
#include "MundoPlanta.h"

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
		chuvas[k].ate=chuvas[k].desde=-1.0;
		}
	mundoPedido=mundoPronto=false;
	horaFixa=-1.0f;
	fatorVazao=1.32f; // o dobro da calibração de 16/11 (0,66), pedido em 17/11; as bocas maiores não mudam o total (ver patch_sandbox)
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
		wpX[i]=wpY[i]=0.0f;
		alvoX[i]=alvoY[i]=mAte[i]=mTroca[i]=0.0f;
		temAlvo[i]=escondido[i]=false;
		mEstado[i]=0;
		nRota[i]=kRota[i]=0;
		mDest[i]=-1;
		mVaga[i]=0;
		mAlvoAng[i]=0.0f;
		mChegou[i]=-1.0;
		mSocial[i]=-1;
		}
	mundoArcoAte=mundoMusicaAte=mundoTVAte=-100.0;
	mundoBits=0.0f;
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	atlasNovo=false;
	atlasTextura=0;
	for(int i=0;i<MAX_NOMES;++i)
		{
		nomes[i][0]=0;
		presente[i]=false;
		visual[i]=0;
		}
	comida.x=comida.y=comida.r=0.0f;
	comida.ate=-100.0;
	comida.dura=0.0;
	for(int k=0;k<MAX_RACAO;++k)
		{
		Racao& r=racao[k];
		r.x=r.y=r.vx=r.vy=r.seco=0.0f;
		r.desde=r.ate=-100.0;
		r.mordida=-100.0;
		r.mordidas=0;
		}
	nRacao=0;
	racaoUltima=-100.0;
	racaoUltX=racaoUltY=0.0f;
	vidaRacaoAnuncio=-100.0;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		acao[i]=0;
		presa[i]=-1;
		acaoT[i]=0.0f;
		voo[i]=escala[i]=1.0f;
		acaoAte[i]=0.0;
		belisca[i]=0.0;
		comeRacao[i]=-1;
		apertado[i]=0.0f;
		}
	jogo=SEM_JOGO;
	jogoFase=0;
	jogoNivel=2;
	jogoInicio=jogoFim=0.0;
	jogoDuracao=0.0f;
	jogoPontos=0;
	jogoVencedor=-1;
	proximoPortao=1;
	jogoPedido=false;
	nPecas=nFontes=0;
	for(int i=0;i<MAX_PECAS;++i)
		{
		pecas[i].tipo=P_NADA;
		pecas[i].x=pecas[i].y=pecas[i].raio=0.0f;
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
	for(int i=0;i<gw*gh;++i)
		if(conta[i]>0)
			{
			alt[i]=soma[i]/float(conta[i]); // os bichos sempre usam a areia de agora (mexeram nela, eles veem na hora)
			todas.push_back(alt[i]);
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

/* ------------------------------- comida (jogada pelo painel ou embaixo do dedo apontado; a mão aberta só faz chover) */

bool Extras::comidaPerto(float x,float y,float& borda,float folga) const
	{
	/* a comida vale até acabar e mais "folga" segundos: quem está pousado ou sentado (folga 2 s) não sai
	   no mesmo instante em que ela some */
	borda=1e9f;
	if(tempo>comida.ate+folga)
		return false;
	borda=std::sqrt((comida.x-x)*(comida.x-x)+(comida.y-y)*(comida.y-y))-comida.r;
	return true;
	}

bool Extras::jogarComidaEm(float x,float y,float segundos,int onde,int quantas)
	{
	/* comida num ponto (dedo apontado ou painel): ração na água, pão na terra */
	x=std::max(bmin[0],std::min(bmax[0],x));
	y=std::max(bmin[1],std::min(bmax[1],y));
	float s=std::max(0.0f,std::min(120.0f,segundos));
	bool naAgua=onde<0?aguaNoPonto(x,y):onde==1;
	if(naAgua&&!aguaNoPonto(x,y))
		{
		/* pedida na água, mas ali secou (ou a mão parada já entrou no relevo e tirou a água de baixo dela):
		   cai na água mais perto, até 6 cm; sem água por perto, vira comida de terra */
		naAgua=false;
		for(int r=1;r<=4&&!naAgua;++r)
			for(int k=0;k<12&&!naAgua;++k)
				{
				float a=float(k)*PI/6.0f+0.26f*float(r),d=1.5f*float(r);
				float px=x+std::cos(a)*d,py=y+std::sin(a)*d;
				if(aguaNoPonto(px,py))
					{
					x=px;
					y=py;
					naAgua=true;
					}
				}
		}
	if(naAgua)
		{
		/* ração: a primeira vez num lugar cai um punhado; o dedo parado no mesmo lugar vai soltando mais */
		bool mesmo=tempo-racaoUltima<1.5&&(racaoUltX-x)*(racaoUltX-x)+(racaoUltY-y)*(racaoUltY-y)<4.0f*4.0f;
		int n=quantas>0?quantas:(mesmo?2:5);
		jogarRacao(x,y,n,mesmo?1.5f:2.0f,s);
		racaoUltima=tempo;
		racaoUltX=x;
		racaoUltY=y;
		return true;
		}
	/* terra: a comida do painel; perto do mesmo lugar (2,5 cm) só renova, o pão fica onde está */
	bool mesma=tempo<comida.ate&&(comida.x-x)*(comida.x-x)+(comida.y-y)*(comida.y-y)<2.5f*2.5f;
	if(!mesma)
		{
		comida.x=x;
		comida.y=y;
		}
	comida.r=4.5f;
	comida.dura=s;
	comida.ate=tempo+s;
	return false;
	}

int Extras::jogarRacao(float x,float y,int n,float espalha,float segundos)
	{
	/* n bolinhas espalhadas num círculo de raio "espalha"; cada uma dura um pouco diferente (somem uma a
	   uma). Sem vaga, a mais velha dá lugar (o dedo continua soltando onde está apontando). */
	int caiu=0;
	for(int q=0;q<n;++q)
		{
		int vaga=-1;
		double velha=1e30;
		for(int k=0;k<MAX_RACAO;++k)
			{
			if(racao[k].ate<tempo)
				{
				vaga=k;
				break;
				}
			if(racao[k].desde<velha)
				{
				velha=racao[k].desde;
				vaga=k;
				}
			}
		float a=aleatorio()*2.0f*PI,d=espalha*std::sqrt(aleatorio());
		float px=x+std::cos(a)*d,py=y+std::sin(a)*d;
		if(!aguaNoPonto(px,py)) // espalhou para a margem: cai no meio
			{
			px=x;
			py=y;
			}
		Racao& r=racao[vaga];
		r.x=px;
		r.y=py;
		r.vx=r.vy=r.seco=0.0f;
		r.desde=tempo+0.05*q; // (o respingo de cada uma sai um pouquinho depois do da outra)
		r.ate=tempo+std::max(5.0f,segundos*(0.65f+0.35f*aleatorio()));
		r.mordida=-100.0;
		r.mordidas=2;
		++caiu;
		}
	return caiu;
	}

void Extras::passoRacao(float dt)
	{
	/* cada bolinha boia: vai com a correnteza (como o pato), se afasta devagar das vizinhas e encosta na
	   margem sem subir na areia; se a água some embaixo dela, procura a água mais perto e, sem achar, some */
	nRacao=0;
	for(int k=0;k<MAX_RACAO;++k)
		{
		Racao& r=racao[k];
		if(r.ate<tempo||r.mordidas<=0)
			continue;
		if(aguaSim.empty()||!dentro(r.x,r.y,0.5f))
			{
			r.ate=tempo-1.0; // sem água medida (os bichos da água saíram) ou fora da área: some
			continue;
			}
		float cvx,cvy;
		correnteza(r.x,r.y,cvx,cvy);
		float tx=cvx*0.9f+0.15f*std::sin(float(tempo)*0.7f+float(k)*2.1f),ty=cvy*0.9f+0.15f*std::cos(float(tempo)*0.6f+float(k)*1.3f);
		for(int j=0;j<MAX_RACAO;++j)
			{
			const Racao& o=racao[j];
			if(j==k||o.ate<tempo||o.mordidas<=0)
				continue;
			float dx=r.x-o.x,dy=r.y-o.y,d2=dx*dx+dy*dy;
			if(d2<1.1f*1.1f&&d2>1e-6f)
				{
				float d=std::sqrt(d2);
				tx+=dx/d*0.5f*(1.1f-d);
				ty+=dy/d*0.5f*(1.1f-d);
				}
			}
		float p0=profAgua(r.x,r.y);
		if(p0<0.12f)
			{
			/* secou embaixo: vai para onde está mais fundo; some se ficar 1,5 s sem achar água */
			r.seco+=dt;
			float melhor=p0;
			int k0=-1;
			for(int q=0;q<8;++q)
				{
				float pq=profAgua(r.x+std::cos(q*PI/4.0f)*2.0f,r.y+std::sin(q*PI/4.0f)*2.0f);
				if(pq>melhor+0.02f)
					{
					melhor=pq;
					k0=q;
					}
				}
			if(r.seco>1.5f||(k0<0&&p0<0.03f&&r.seco>0.5f))
				{
				r.ate=tempo-1.0;
				continue;
				}
			if(k0>=0)
				{
				tx=std::cos(k0*PI/4.0f)*3.0f;
				ty=std::sin(k0*PI/4.0f)*3.0f;
				}
			}
		else
			r.seco=0.0f;
		r.vx+=(tx-r.vx)*std::min(1.0f,2.0f*dt);
		r.vy+=(ty-r.vy)*std::min(1.0f,2.0f*dt);
		float nx=r.x+r.vx*dt,ny=r.y+r.vy*dt;
		if(p0<0.12f||profAgua(nx,ny)>=0.12f)
			{
			r.x=nx;
			r.y=ny;
			}
		else
			r.vx=r.vy=0.0f; // encostou na margem: fica boiando ali
		++nRacao;
		}
	}

int Extras::racaoAlcance(int i,float vista,float boca) const
	{
	/* a bolinha mais perto (até "vista" cm) que o bicho alcança nadando em linha reta pela água: um bicho
	   de outro lago, ou do outro lado de uma ilha, não vem */
	float bx=ax[i]+std::cos(ang[i])*boca*0.5f,by=ay[i]+std::sin(ang[i])*boca*0.5f;
	float jaVisto=0.0f;
	for(int tenta=0;tenta<3;++tenta)
		{
		int melhor=-1;
		float dm=vista*vista;
		for(int k=0;k<MAX_RACAO;++k)
			{
			const Racao& r=racao[k];
			if(r.ate<tempo||r.mordidas<=0)
				continue;
			float d2=(r.x-bx)*(r.x-bx)+(r.y-by)*(r.y-by);
			if(d2<dm&&d2>jaVisto)
				{
				dm=d2;
				melhor=k;
				}
			}
		if(melhor<0)
			return -1;
		float d=std::sqrt(dm),dx=(racao[melhor].x-bx)/std::max(d,1e-3f),dy=(racao[melhor].y-by)/std::max(d,1e-3f);
		bool alcanca=true;
		for(float t=2.0f;t<d-1.2f&&alcanca;t+=2.0f)
			alcanca=agua(bx+dx*t,by+dy*t);
		if(alcanca)
			return melhor;
		jaVisto=dm+1e-4f; // essa não dá: tenta a próxima mais perto
		}
	return -1;
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
	if(chuvas[livre].ate<=tempo)
		chuvas[livre].desde=tempo; // nuvem nova (tocar de novo no mesmo lugar só estica a chuva)
	chuvas[livre].ate=tempo+std::max(1.0f,std::min(60.0f,segundos));
	temChuva=true;
	}

float Extras::chuvaForca(int k) const
	{
	const Chuva& c=chuvas[k];
	if(c.desde<0.0)
		return 0.0f;
	float chega=float(std::min(1.0,std::max(0.0,(tempo-c.desde)/0.7)));
	float vai=float(std::min(1.0,std::max(0.0,(c.ate-tempo)/0.9+1.0)));   // some em 0,9 s depois do fim
	return chega*vai;
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
	const float cx=(bmin[0]+bmax[0])*0.5f,cy=(bmin[1]+bmax[1])*0.5f; // sempre no meio: o painel mostra a miniatura
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
	/* o alvo para a miniatura do painel: x, y (0..1 na área iluminada), altura (cm), largura (fração da largura) */
	std::string alvo="Copia: alvo";
	for(int k=0;k<COPIA_MAX;++k)
		if(copiaG[k][3]>0.0f)
			{
			char g[80];
			std::snprintf(g,sizeof(g)," %.3f,%.3f,%.2f,%.3f",(copiaG[k][0]-bmin[0])/w,(copiaG[k][1]-bmin[1])/h,copiaG[k][2],copiaG[k][3]/w);
			alvo+=g;
			}
	anuncia(alvo);
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
	for(int k=0;k<MAX_CHUVAS;++k)
		chuvas[k].ate=-1.0;
	temChuva=false;
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
	comeRacao[i]=-1;
	apertado[i]=0.0f;
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

void Extras::copiaNomes(const std::vector<std::string>& lista,int giro,int fotos)
	{
	/* Para o mundo (Mundo.cpp): um boneco por membro, com o nome em letras (só A-Z, até MAX_LETRAS),
	   quem está presente ("NOME+") e o rosto (os primeiros "fotos" nomes têm quadro no painel de rostos).
	   A cor da camisa sai do nome: é sempre a mesma. O lugar e o andar de cada um são do mundo. */
	int n=std::min(int(lista.size()),int(MAX_NOMES));
	nFotos=atlas.empty()?0:std::max(0,std::min(fotos,n));
	giroNomes=((giro%4)+4)%4;
	nNomes=quantos=n;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		tipo[i]=0.0f;
		vel[i]=0.0f;
		parado[i]=0.0f;
		acao[i]=0;
		presa[i]=-1;
		acaoT[i]=0.0f;
		voo[i]=escala[i]=1.0f;
		if(i>=n)
			continue;
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
				nomes[i][k++]=ch;
				soma=soma*31u+(unsigned int)ch;
				}
			}
		nomes[i][k]=0;
		tipo[i]=3.05f+0.9f*float(soma%97u)/97.0f;
		/* visual próprio de quem já é do laboratório (19/11/2026, pedido do Victor: chapéu no Gazziro e todos
		   fáceis de distinguir de longe); membro novo fica com o boneco sorteado pelo nome, como antes */
		static const char* const CONHECIDOS[4]={"GAZZIRO","LUCAS","MARIO","VICTOR"};
		visual[i]=0;
		for(int v=0;v<4;++v)
			if(std::string(nomes[i])==CONHECIDOS[v])
				visual[i]=v+1;
		}
	for(int i=n;i<MAX_NOMES;++i)
		visual[i]=0;
	}

static float difAng(float a) // ângulo em -pi..pi
	{
	while(a>PI) a-=2.0f*PI;
	while(a<-PI) a+=2.0f*PI;
	return a;
	}

namespace {
/* Os bichos são desenhados 12% maiores desde 19/11/2026 (TAM no shader, mais abaixo): o que depende do
   tamanho do desenho (fila dos patinhos, boca de quem come) acompanha */
const float TAM_BICHOS=1.12f;
/* o escorrega não deixa entrar no miolo do corpo do outro (70% da soma dos raios); entre o miolo e a
   beirada os dois se encostam e o empurrão suave separa. Medido no banco (19/11): com o corpo inteiro
   (100%) os bichos se espremiam e tremiam em lugar apertado; com 70% eles se ajeitam e passam */
const float NUCLEO=0.7f;
/* boca (cm à frente do meio) de quem come a ração */
inline float bocaDe(int esp,float esc)
	{
	return TAM_BICHOS*(esp==4?2.8f:esp==5?2.0f:esp==9?1.7f*esc:esp==1?1.5f:esp==6?0.6f:1.3f);
	}
}

float Extras::corpo(int i) const
	{
	/* raio do corpo (cm) para encostar: um pouco menor que o desenho (uma cauda, um bico ou um braço do
	   polvo podem passar por cima do outro, como de verdade) */
	switch(int(tipo[i]))
		{
		case 1: return 1.9f;            // peixe
		case 2: return 2.2f;            // carrinho
		case 3: return 1.6f;            // pessoa
		case 4: return 3.0f;            // tubarão
		case 5: return 2.5f;            // tartaruga
		case 6: return 1.6f;            // água-viva
		case 7: return 2.1f;            // polvo
		case 8: return 2.4f;            // arraia
		case 9: return 1.6f*escala[i];  // pato (o patinho é menor)
		case 10: return 2.0f;           // barquinho
		case 12: return 1.5f;           // cachorro
		default: return 1.0f;
		}
	}

void Extras::escorrega(int i,bool naAgua,int ignora,float& mx,float& my) const
	{
	/* Encostado em alguém: a parte do passo que entraria no corpo dele é tirada e sobra o movimento de lado
	   (escorrega em volta). Quase de frente, o lado preferido é a direita: dois que se encontram de frente
	   vão cada um pela sua direita e passam. A troca entre "o lado que sobrou" e "a direita" é contínua
	   (sem pulo de um lado para o outro quando o rumo balança). Antes o passo inteiro entrava e o
	   empurrão desfazia: os dois ficavam se empurrando, tremendo, até um dar meia-volta. */
	float m0=std::sqrt(mx*mx+my*my);
	if(m0<1e-6f)
		return;
	const float ri=corpo(i),meio=0.5f*m0;
	for(int j=0;j<quantos;++j)
		{
		if(j==i||j==ignora||vel[j]==0.0f||tipo[j]<0.5f||(naAgua?!aquatico(tipo[j]):!terrestre(tipo[j])))
			continue;
		float dx=ax[j]-ax[i],dy=ay[j]-ay[i],d2=dx*dx+dy*dy,junto=(ri+corpo(j))*NUCLEO;
		if(d2>=junto*junto||d2<1e-6f)
			continue;
		float d=std::sqrt(d2),ux=dx/d,uy=dy/d,entra=mx*ux+my*uy;
		if(entra<=0.0f)
			continue; // já está saindo de perto
		float dirX=uy,dirY=-ux;            // tangente para a direita de quem vai na direção dele
		float tau=mx*dirX+my*dirY;         // o que sobra de lado (+ direita, - esquerda)
		float vies=meio*std::max(0.0f,1.0f-std::abs(tau)/meio);
		tau+=vies;                         // quase de frente: escorrega pela direita
		mx=dirX*tau;
		my=dirY*tau;
		}
	}

bool Extras::vagaNaComida(int i,float alcance) const
	{
	/* O pão chama só quem está perto (até "alcance" cm da beira dele) e no máximo COMEDORES pássaros e
	   cachorros (ou OLHANDO pessoas) de cada vez: quem já está lá fica; entre os de fora, os mais perto
	   passam na frente. Os outros nem percebem e seguem passeando. */
	float borda;
	if(!comidaPerto(ax[i],ay[i],borda,0.0f)||borda>alcance)
		return false;
	const bool olhar=int(tipo[i])==3; // pessoa: só para e olha
	int na=0;
	for(int j=0;j<quantos;++j)
		{
		int t=int(tipo[j]);
		if(j==i||vel[j]==0.0f||(olhar?t!=3:(t!=11&&t!=12)))
			continue;
		if(olhar?acao[j]==1:(acao[j]==1||acao[j]==3))
			++na; // já está na comida (ou indo)
		else
			{
			float bj;
			if(comidaPerto(ax[j],ay[j],bj,0.0f)&&bj<borda&&bj<=(t==11?10.0f:t==12?8.0f:3.0f))
				++na; // está mais perto e também pode vir
			}
		}
	return na<(olhar?int(OLHANDO):int(COMEDORES));
	}

void Extras::escolheComedores(const int patoDaFrente[MAX_AGENTES])
	{
	/* Quem vem comer a ração: cada bicho da água que vê uma bolinha perto (e chega nela pela água) é
	   candidato; quem já estava comendo fica com a vez e os outros entram pelo mais perto, até COMEDORES
	   por grupo de bolinhas (as que estão a menos de 6 cm umas das outras). Os demais seguem a vida. */
	int cand[MAX_AGENTES];
	float chave[MAX_AGENTES];
	int nc=0;
	for(int i=0;i<quantos;++i)
		{
		int k=-1,esp=int(tipo[i]);
		if(nRacao>0&&vel[i]!=0.0f&&aquatico(tipo[i])&&esp!=10) // o barquinho não come
			{
			bool cacando=false; // tubarão com fome e peixe por perto: vai caçar, não comer
			if(esp==4&&acaoAte[i]<tempo)
				for(int j=0;j<quantos&&!cacando;++j)
					cacando=int(tipo[j])==1&&vel[j]!=0.0f&&(ax[j]-ax[i])*(ax[j]-ax[i])+(ay[j]-ay[i])*(ay[j]-ay[i])<25.0f*25.0f;
			/* até onde cada um enxerga a ração (cm; antes 12 a 25 cm, o lago inteiro vinha) */
			float vista=esp==1?12.0f:esp==5||esp==9?11.0f:esp==4?10.0f:esp==6?7.0f:9.0f;
			if(esp==9&&patoDaFrente[i]>=0)
				vista=6.0f; // patinho na fila: só se a ração passa do lado
			bool jaComia=comeRacao[i]>=0;
			if(jaComia)
				vista*=1.4f; // quem já está comendo não larga porque a bolinha boiou um pouco
			if(!cacando)
				k=racaoAlcance(i,vista,bocaDe(esp,escala[i]));
			if(k>=0)
				{
				float dx=racao[k].x-ax[i],dy=racao[k].y-ay[i];
				chave[nc]=std::sqrt(dx*dx+dy*dy)-(jaComia?1000.0f:0.0f);
				cand[nc++]=i;
				}
			}
		comeRacao[i]=k;
		}
	/* pela ordem (quem já comia, depois o mais perto) */
	for(int a=1;a<nc;++a)
		for(int b=a;b>0&&chave[b]<chave[b-1];--b)
			{
			std::swap(chave[b],chave[b-1]);
			std::swap(cand[b],cand[b-1]);
			}
	int aceitos[MAX_AGENTES];
	int na=0;
	for(int c=0;c<nc;++c)
		{
		int i=cand[c];
		const Racao& r=racao[comeRacao[i]];
		int grupo=0;
		for(int a=0;a<na;++a)
			{
			const Racao& o=racao[comeRacao[aceitos[a]]];
			if((o.x-r.x)*(o.x-r.x)+(o.y-r.y)*(o.y-r.y)<6.0f*6.0f)
				++grupo;
			}
		if(grupo<COMEDORES)
			aceitos[na++]=i;
		else
			comeRacao[i]=-1;
		}
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
	if(temChuva)
		{
		temChuva=false;
		for(int k=0;k<MAX_CHUVAS;++k)
			temChuva=temChuva||chuvas[k].ate+1.0>tempo; // fica ligada mais 1 s: a nuvem se desfaz aos poucos
		}
	if(!mapaOk||modo==NADA)
		return;
	dt=std::min(dt,0.1f);
	if(modo==MUNDO)
		{
		mundoPasso(dt); // o laboratório tem rotina própria (Mundo.cpp): não usa o movimento pelo relevo
		return;
		}
	passoRacao(dt);
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
	escolheComedores(patoDaFrente); // quem vai comer a ração (só os mais perto de cada grupo de bolinhas)
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
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
			vx+=0.85f*sepX;
			vy+=0.85f*sepY;
			/* Vida mais viva: patinhos em fila, o tubarão caça, fogem do perigo (da mão não) */
			const int esp=int(tipo[i]);
			float susto=0.0f,fila=-1.0f;
			bool cacando=false;
			int presaT=-1; // o peixe que o tubarão está caçando
			if(esp==9&&patoDaFrente[i]>=0)
				{
				int f=patoDaFrente[i];
				float atras=(1.75f*escala[f]+1.1f*escala[i]+0.3f)*TAM_BICHOS; // logo atrás do rabo do pato da frente
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
					presaT=alvo;
					vx+=(ax[alvo]-ax[i])/std::max(0.5f,melhor)*3.0f;
					vy+=(ay[alvo]-ay[i])/std::max(0.5f,melhor)*3.0f;
					float bx=ax[i]+c0*2.8f*TAM_BICHOS-ax[alvo],by=ay[i]+s0*2.8f*TAM_BICHOS-ay[alvo];
					if(bx*bx+by*by<1.8f*1.8f)
						{
						nasce(alvo); // pegou: outro peixe aparece em outro lugar
						acaoAte[i]=tempo+12.0+6.0*aleatorio(); // satisfeito por um tempo
						anuncia("Vida: o tubarao pegou um peixe");
						}
					}
				}
			/* Ração na água (dedo apontado ou painel): quem está perto vai até a bolinha mais perto que alcança
			   pela água e belisca; no máximo COMEDORES por grupo de bolinhas (escolheComedores), os outros seguem
			   a vida. O tubarão também come quando não está caçando (e os peixes continuam fugindo dele, logo
			   abaixo). O barquinho não come. */
			bool comendo=false;
			float pertoRacao=1e9f;
			if(nRacao>0&&esp!=10&&!cacando)
				{
				/* boca (cm à frente do meio), força da virada e pressa de cada um */
				float boca=bocaDe(esp,escala[i]);
				float puxa=esp==1?2.6f:esp==9?2.4f:esp==5?2.2f:esp==4?1.6f:esp==6?0.9f:1.3f;
				int k=comeRacao[i];
				if(k>=0&&(racao[k].ate<tempo||racao[k].mordidas<=0))
					k=-1; // outro comeu a última beliscada neste mesmo passo
				if(k>=0)
					{
					comendo=true;
					const Racao& r=racao[k];
					float dx=r.x-ax[i],dy=r.y-ay[i],d=std::max(0.4f,std::sqrt(dx*dx+dy*dy));
					pertoRacao=d;
					float w=puxa*(d<3.0f?0.7f:1.0f);
					if(fila>=0.0f) // o patinho sai da fila para pegar: a vontade dele manda
						{
						vx=0.5f*vx+dx/d*2.0f;
						vy=0.5f*vy+dy/d*2.0f;
						}
					else
						{
						vx+=dx/d*w;
						vy+=dy/d*w;
						}
					float mx=ax[i]+c0*boca-r.x,my=ay[i]+s0*boca-r.y;
					float alcance=(esp==4||esp==5?1.5f:1.2f)*TAM_BICHOS;
					if(mx*mx+my*my<alcance*alcance&&belisca[i]<tempo)
						{
						/* beliscou: a bolinha diminui (peixe, patinho e água-viva levam duas beliscadas; os
						   grandes, uma) e o bicho se afasta um pouco antes da próxima */
						Racao& rr=racao[k];
						rr.mordidas-=(esp==1||esp==6||(esp==9&&escala[i]<0.9f))?1:2;
						rr.mordida=tempo;
						if(rr.mordidas<=0)
							rr.ate=tempo+0.6; // comida: só o respingo da beliscada ainda aparece
						belisca[i]=tempo+0.6+0.6*aleatorio();
						ang[i]=difAng(ang[i]+(aleatorio()<0.5f?0.7f:-0.7f));
						if(tempo-vidaRacaoAnuncio>10.0)
							{
							vidaRacaoAnuncio=tempo;
							anuncia(std::string("Vida: ")+(esp==1?"os peixes":esp==4?"o tubarao":esp==5?"a tartaruga":esp==6?"a agua-viva":
							        esp==7?"o polvo":esp==8?"a arraia":"o pato")+" comendo a racao");
							}
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
			/* Espaço de cada um, entre todas as espécies da água (o cardume acima só cuida dos da mesma
			   espécie): afasta de quem está quase encostando e, de quem está no caminho, desvia antes de bater,
			   pelo lado livre (bem de frente, pela direita: os dois passam um pelo outro). O tubarão caçando
			   não desvia da presa e o patinho não desvia do pato da frente da fila (mas não entra nele: o
			   escorrega e o empurrão, mais abaixo, valem para ele também). */
			const float ri=corpo(i);
			const int ignora=cacando?presaT:-1;
			const int ignoraDesvio=cacando?presaT:(fila>=0.0f?patoDaFrente[i]:-1);
			for(int j=0;j<quantos;++j)
				{
				if(j==i||j==ignoraDesvio||!aquatico(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i],d2=dx*dx+dy*dy,junto=ri+corpo(j);
				if(d2>(junto+4.0f)*(junto+4.0f)||d2<1e-6f)
					continue;
				float d=std::sqrt(d2);
				if(int(tipo[j])!=esp&&d<junto+1.0f)
					{
					float f=(junto+1.0f-d)/(junto+1.0f);
					vx-=dx/d*f*1.6f;
					vy-=dy/d*f*1.6f;
					}
				float adiante=dx*c0+dy*s0,lado=-dx*s0+dy*c0;
				if(adiante>0.0f&&adiante<junto+3.5f&&std::abs(lado)<junto)
					{
					/* vai para o lado contrário ao dele, com preferência pela direita (só vai pela esquerda
					   se ele já está bem à direita); a força cresce aos poucos com o lado (sem pulo) */
					float w=1.2f*(1.0f-adiante/(junto+3.5f))*(1.0f-std::abs(lado)/junto);
					float lad=-std::max(-1.0f,std::min(1.0f,(lado+0.5f*junto)/(0.3f*junto))); // +1 esquerda, -1 direita
					vx-=lad*s0*w;
					vy+=lad*c0*w;
					}
				}
			if(!folgado&&fila<0.0f)
				{
				/* rente à margem: puxa para onde tem água em volta (para o meio do lago), em vez de seguir
				   raspando nela até o "apertado" mandar renascer em outro lugar */
				float wx=0.0f,wy=0.0f;
				for(int k=0;k<8;++k)
					{
					float a=float(k)*PI/4.0f;
					if(agua(ax[i]+std::cos(a)*(raioA+0.8f),ay[i]+std::sin(a)*(raioA+0.8f)))
						{
						wx+=std::cos(a);
						wy+=std::sin(a);
						}
					}
				vx+=0.5f*wx;
				vy+=0.5f*wy;
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
					if(passada==0?(aguaLarga(x3,y3,raioA)&&aguaLarga(x1,y1,raioA)):(agua(x3,y3)&&agua(x1,y1))) // (o tubarão confere o corpo dele, não o de um peixe)
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
			else if(comendo) // vem depressa; chegando, devagar (rodeia a ração e belisca)
				alvoVel=pertoRacao<3.0f?vb[i]*0.55f:std::max(alvoVel,vb[i]*(esp==6||esp==7||esp==8?1.0f:1.3f));
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
			/* encostado em outro: escorrega pelo lado dele em vez de entrar */
			float mx=nx-ax[i],my=ny-ay[i];
			escorrega(i,true,ignora,mx,my);
			nx=ax[i]+mx;
			ny=ay[i]+my;
			/* só avança se o corpo inteiro continuar na água (encostado na margem, qualquer água serve
			   para ele conseguir sair dali); a margem na frente: tenta seguir rente a ela antes de desistir */
			bool cabe=aguaLarga(nx,ny,raioA)||(!folgado&&agua(nx,ny));
			const float rots[]={0.5f,-0.5f,1.0f,-1.0f};
			for(int q=0;q<4&&!cabe;++q)
				{
				float cr=std::cos(rots[q]),sr=std::sin(rots[q]);
				float qx=ax[i]+(mx*cr-my*sr)*0.8f,qy=ay[i]+(mx*sr+my*cr)*0.8f;
				if(aguaLarga(qx,qy,raioA)||(!folgado&&agua(qx,qy)))
					{
					nx=qx;
					ny=qy;
					cabe=true;
					ang[i]=difAng(ang[i]+rots[q]*std::min(1.0f,2.0f*dt));
					}
				}
			if(cabe)
				{
				ax[i]=nx;
				ay[i]=ny;
				parado[i]=0.0f;
				}
			else
				parado[i]+=dt;
			/* sem folga (poça mais estreita que ele) por muito tempo: vai para um lago de verdade. Antes
			   isso somava ao "parado" mesmo nadando e quem passava uns segundos rente à margem (empurrado
			   por outro, por exemplo) sumia e aparecia em outro lugar */
			apertado[i]=folgado?0.0f:apertado[i]+dt;
			/* Separação suave entre seres aquáticos ao encostarem (sem tremedeira): só corrige o que
			   ainda sobrou de um dentro do outro (o escorrega acima já não deixa entrar) */
			for(int j=0;j<quantos;++j)
				{
				if(j==i||j==ignora||!aquatico(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[i]-ax[j],dy=ay[i]-ay[j],d2=dx*dx+dy*dy;
				float junto=ri+corpo(j);
				if(d2<junto*junto&&d2>1e-4f)
					{
					float d=std::sqrt(d2);
					float emp=(junto-d)*std::min(1.0f,3.0f*dt);
					float px=ax[i]+dx/d*emp,py=ay[i]+dy/d*emp;
					if(agua(px,py))
						{
						ax[i]=px;
						ay[i]=py;
						}
					}
				}
			if(parado[i]>4.0f||apertado[i]>15.0f) // preso numa poça ou espremido na margem: aparece num lago de verdade
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
				continue; // sentado do lado da comida, abanando o rabo
				}
			if(int(tipo[i])==3&&modo!=MUNDO) // (o cachorro tem a dele, logo acima)
				{
				/* quem passeia e passa do lado do pão para um pouco para olhar (no máximo a cada 20 s, até OLHANDO
				   pessoas de cada vez) e segue */
				float bp;
				if(acao[i]==0&&acaoAte[i]<tempo&&comidaPerto(ax[i],ay[i],bp,0.0f)&&bp>0.6f&&bp<3.0f&&vagaNaComida(i,3.0f))
					{
					acao[i]=1;
					acaoAte[i]=tempo+1.5+1.5*aleatorio();
					}
				if(acao[i]==1)
					{
					if(tempo<acaoAte[i]&&comidaPerto(ax[i],ay[i],bp,0.0f))
						{
						float olha=std::atan2(comida.y-ay[i],comida.x-ax[i]);
						ang[i]=difAng(ang[i]+std::max(-2.5f*dt,std::min(2.5f*dt,difAng(olha-ang[i]))));
						vel[i]=std::max(0.05f,vel[i]*(1.0f-std::min(1.0f,5.0f*dt)));
						parado[i]=0.0f;
						continue;
						}
					acao[i]=0;
					acaoAte[i]=tempo+20.0;
					}
				}
			const float desvios[]={0.0f,0.35f,-0.35f,0.7f,-0.7f,1.1f,-1.1f};
			/* Pessoa (tipo 3): mesmo jeito de andar do carrinho, com corpo menor, passo curto e um
			   vai-e-vem de quem passeia */
			const bool pessoa=ehPessoa(tipo[i]);
			const float rc=pessoa?1.7f:2.2f,olha=pessoa?3.5f:4.0f,perto=pessoa?5.5f:7.0f;
			const float ri=corpo(i);
			/* alguém no caminho de agora? as direções para a direita ganham um pouco: dois que vêm de frente
			   desviam cada um pela sua direita e passam um pelo outro (antes os dois escolhiam o mesmo lado) */
			bool temFrente=false;
			for(int j=0;j<quantos&&!temFrente;++j)
				if(j!=i&&terrestre(tipo[j])&&vel[j]!=0.0f)
					{
					float dx=ax[j]-ax[i],dy=ay[j]-ay[i];
					float adiante=dx*c0+dy*s0,lado=-dx*s0+dy*c0;
					temFrente=adiante>0.0f&&adiante<perto&&std::abs(lado)<ri+corpo(j);
					}
			float melhor=1e9f,escolha=PI,hEscolha=h0;
			for(int k=0;k<7;++k)
				{
				float a=ang[i]+desvios[k];
				float ca=std::cos(a),sa=std::sin(a);
				float px=ax[i]+ca*olha,py=ay[i]+sa*olha;
				float h=altura(px,py);
				/* à frente tem que caber o carrinho inteiro em chão firme, sem paredão nem barranco */
				if(!chaoLivre(px,py,rc)||!chaoLivre(ax[i]+ca*olha*0.5f,ay[i]+sa*olha*0.5f,rc)
				   ||h-h0>2.0f||h0-h>3.0f)
					continue;
				/* vale: baixo, mas sem despencar (segue a curva de nível); longe dos outros carros */
				float nota=0.35f*h+0.6f*std::abs(h-h0)+0.8f*std::abs(desvios[k]);
				if(pessoa) // passeia: de tempos em tempos prefere virar para um lado
					nota=0.5f*std::abs(h-h0)+0.6f*std::abs(desvios[k]-0.7f*std::sin(float(tempo)*0.35f+float(i)*2.3f));
				if(temAlvo[i]) // (o cachorro indo até a comida ou atrás de um pássaro)
					{
					/* tem destino: prefere a direção que aponta para ele (o relevo ainda pesa) */
					float rumo=difAng(std::atan2(alvoY[i]-ay[i],alvoX[i]-ax[i])-ang[i]);
					rumo=std::max(-1.1f,std::min(1.1f,rumo));
					nota=0.4f*std::abs(h-h0)+0.9f*std::abs(desvios[k]-rumo);
					}
				/* os outros no caminho (andando ou parados): pior quanto mais no meio dessa direção e mais
				   perto. Antes só contava quem estava no ponto de chegada, e quem estava no meio do caminho
				   ficava na frente */
				for(int j=0;j<quantos;++j)
					{
					if(j==i||!terrestre(tipo[j])||vel[j]==0.0f)
						continue;
					float dx=ax[j]-ax[i],dy=ay[j]-ay[i],folga=ri+corpo(j)+0.5f;
					float adiante=dx*ca+dy*sa;
					if(adiante<-0.5f||adiante>perto+folga)
						continue;
					float lado=std::abs(-dx*sa+dy*ca);
					if(lado<folga)
						nota+=3.0f*(1.0f-lado/folga)*(1.0f-0.5f*std::max(0.0f,adiante)/(perto+folga));
					}
				if(temFrente&&desvios[k]>0.0f)
					nota+=0.25f*desvios[k];
				if(nota<melhor)
					{
					melhor=nota;
					escolha=desvios[k];
					hEscolha=h;
					}
				}
			if(escolha==PI) // sem saída: vira no lugar até achar caminho
				{
				ang[i]=difAng(ang[i]+2.5f*dt);
				vel[i]+=(0.3f*vb[i]-vel[i])*std::min(1.0f,3.0f*dt);
				continue;
				}
			ang[i]=difAng(ang[i]+std::max(-1.8f*dt,std::min(1.8f*dt,escolha)));
			float c1=std::cos(ang[i]),s1=std::sin(ang[i]);
			/* Desce mais rápido, sobe devagar; curva freia */
			float rampa=h0>SEM+1.0f?(hEscolha-h0):0.0f;
			float alvoVel=vb[i]*std::max(0.45f,std::min(1.35f,1.0f-0.25f*rampa))*(1.0f-0.3f*std::min(1.0f,std::abs(escolha)));
			if(int(tipo[i])==12&&(acao[i]==3||acao[i]==4))
				alvoVel*=acao[i]==4?1.8f:1.5f; // corre até a comida / atrás do pássaro
			/* alguém logo à frente: chega devagar e, se ele vai para o mesmo lado, acompanha no passo dele;
			   nunca para de vez (encostado, escorrega pelo lado, logo abaixo). Antes o de trás freava até
			   parar e ficava esperando até dar meia-volta. */
			float folgaFrente=1e9f,velFrente=0.0f;
			for(int j=0;j<quantos;++j)
				{
				if(j==i||!terrestre(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i],junto=ri+corpo(j);
				float adiante=dx*c1+dy*s1,lado=std::abs(-dx*s1+dy*c1);
				if(adiante<=0.0f||lado>=junto*0.9f)
					continue;
				float falta=adiante-std::sqrt(junto*junto-lado*lado); // quanto ainda anda até encostar
				if(falta<folgaFrente)
					{
					folgaFrente=falta;
					velFrente=vel[j]*std::cos(ang[j]-ang[i]);
					}
				}
			if(folgaFrente>0.0f&&folgaFrente<2.0f) // (já encostado não freia mais: escorrega pelo lado)
				alvoVel=std::min(alvoVel,std::max(vb[i]*(0.6f+0.2f*folgaFrente),velFrente));
			vel[i]+=(alvoVel-vel[i])*std::min(1.0f,2.5f*dt);
			vel[i]=std::max(vel[i],0.05f); // 0 é "escondido"
			float mx=c1*vel[i]*dt,my=s1*vel[i]*dt;
			escorrega(i,false,-1,mx,my);
			float nx=ax[i]+mx,ny=ay[i]+my;
			const bool preso=!chaoLivre(ax[i],ay[i],rc); // (já está apertado: qualquer chão serve para sair)
			bool andou=chaoLivre(nx,ny,rc)||(preso&&dentro(nx,ny,1.5f)&&!agua(nx,ny)&&altura(nx,ny)>SEM+1.0f);
			const float rots[]={0.5f,-0.5f,1.0f,-1.0f};
			for(int q=0;q<4&&!andou;++q)
				{
				/* o passo bate num barranco ou na água: tenta seguir rente antes de desistir */
				float cr=std::cos(rots[q]),sr=std::sin(rots[q]);
				float qx=ax[i]+(mx*cr-my*sr)*0.8f,qy=ay[i]+(mx*sr+my*cr)*0.8f;
				if(chaoLivre(qx,qy,rc))
					{
					nx=qx;
					ny=qy;
					andou=true;
					ang[i]=difAng(ang[i]+rots[q]*std::min(1.0f,2.0f*dt));
					}
				}
			float avanco=0.0f;
			if(andou)
				{
				avanco=std::sqrt((nx-ax[i])*(nx-ax[i])+(ny-ay[i])*(ny-ay[i]));
				ax[i]=nx;
				ay[i]=ny;
				}
			/* Carrinhos e seres terrestres encostando: afastamento elástico e suave, proporcional a dt (sem
			   tremedeira); só corrige o que sobrou de um dentro do outro */
			for(int j=0;j<quantos;++j)
				{
				if(j==i||!terrestre(tipo[j])||vel[j]==0.0f)
					continue;
				float dx=ax[i]-ax[j],dy=ay[i]-ay[j],d2=dx*dx+dy*dy;
				float junto=ri+corpo(j);
				if(d2<junto*junto&&d2>1e-4f)
					{
					float d=std::sqrt(d2),ux=dx/d,uy=dy/d;
					float empurra=(junto-d)*std::min(1.0f,4.0f*dt);
					/* O empurrão só vale se o corpo inteiro continua em chão firme (17/11: bicho de terra não
					   pisa na água; aceitar 70% do corpo deixava carros e pessoas por cima da água desenhada).
					   Rente à água: tenta o empurrão inteiro e depois a metade; se nem assim couber, os dois
					   ficam encostados (o escorrega já não deixa ninguém entrar no miolo do outro, e o outro
					   empurra para o lado dele). Medido no banco (19/11): empurrar de viés ou empurrar o outro
					   fazia tremer mais na beira da água; inteiro+metade ficou com menos tremidas que antes
					   e com o corpo sempre fora da água */
					for(int q=0;q<2;++q)
						{
						float e=q==0?empurra:0.5f*empurra;
						float px=ax[i]+ux*e,py=ay[i]+uy*e;
						if(chaoLivre(px,py,rc))
							{
							ax[i]=px;
							ay[i]=py;
							break;
							}
						}
					}
				}
			/* Travado (quase sem sair do lugar) por 3 s: dá meia-volta; andando de novo, esquece aos poucos */
			parado[i]=avanco<0.2f*vb[i]*dt?parado[i]+dt:std::max(0.0f,parado[i]-2.0f*dt);
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

bool Extras::lugarDePouso(int i)
	{
	/* um lugar no chão perto da comida, do lado de onde o pássaro vem e longe dos outros que vão pousar */
	const Comida& h=comida;
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
	/* Pássaro: voa em curvas largas por cima de tudo. Vê comida: desce e pousa perto dela (cada um do seu
	   lado) e fica bicando o chão; sai voando quando a comida acaba ou chega um cachorro.
	   Vê um peixe: mergulha para pescar (às vezes consegue). voo = altura (1 alto, 0 no chão). */
	acaoT[i]+=dt;
	float borda;
	bool tem=comidaPerto(ax[i],ay[i],borda,(acao[i]==1||acao[i]==3)?2.0f:0.6f);
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
		bool sai=!tem||dCao<7.0f||(acao[i]==1&&(acaoT[i]>25.0f||agua(ax[i],ay[i])));
		if(!sai)
			{
			/* jogaram a comida em outro lugar: escolhe outro lugar perto dela */
			const Comida& h=comida;
			float dl=std::sqrt((alvoX[i]-h.x)*(alvoX[i]-h.x)+(alvoY[i]-h.y)*(alvoY[i]-h.y))-h.r;
			if(dl<2.0f||dl>9.0f)
				{
				if(lugarDePouso(i))
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
			else if(tem)
				{
				fx=ax[i]-comida.x;
				fy=ay[i]-comida.y;
				}
			ang[i]=std::atan2(fy,fx);
			acao[i]=0;
			acaoT[i]=0.0f;
			temAlvo[i]=false;
			acaoAte[i]=tempo+6.0;
			}
		else if(acao[i]==1)
			{
			/* pousado: olha para a comida de vez em quando e bica o chão (o desenho faz a bicada) */
			voo[i]=std::max(0.0f,voo[i]-2.0f*dt);
			float olha=std::atan2(comida.y-ay[i],comida.x-ax[i])+0.9f*std::sin(float(tempo)*0.5f+float(i)*1.3f);
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
		if(tem&&acaoAte[i]<tempo&&vagaNaComida(i,10.0f)&&lugarDePouso(i)) // só quem passa perto (até 10 cm do pão)
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
	/* Cachorro: vê comida, corre até ela e senta do lado, olhando para ela e abanando o rabo; sem comida, de
	   vez em quando corre atrás de um pássaro (o pousado sai voando). true = está sentado. */
	acaoT[i]+=dt;
	float borda;
	/* só quem já está lá ou está perto (até 8 cm do pão) e tem vaga: os outros nem percebem */
	if(comidaPerto(ax[i],ay[i],borda,(acao[i]==1||acao[i]==3)?2.0f:0.6f)&&(acao[i]==1||acao[i]==3||vagaNaComida(i,8.0f)))
		{
		const Comida& h=comida;
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
	"extrasLab","extrasLab2","extrasJogo","extrasJogoInfo","extrasNomes","extrasNomesCfg","extrasRostos","extrasMundo","extrasMundo2","extrasChuva","extrasCopiaInfo",
	"extrasAg2","extrasComida","extrasRacao"};

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
uniform vec4 extrasEfeito;   // efeito do tema (0 nenhum, 1 lava, 2 neve, 3 poeira, 4 areia, 5 ondas, 6 brilho), altura média da areia (cm), -, jogo do vulcão (1)
uniform vec4 extrasLab;      // labirinto: nascente (x, y) e alvo (x, y), cm
uniform vec4 extrasLab2;     // labirinto: raio do alvo, estado (0 desligado, 1 correndo, 2 chegou), segundos no estado
uniform vec4 extrasJogo[32]; // jogos de água: por peça (x, y, raio, tipo) e (estado, -, ordem, time)
uniform vec4 extrasJogoInfo; // jogo (0 nenhum), fase, segundos na fase, vencedor ou próximo portão
uniform float extrasNomes[72]; // membros: por pessoa, nº de letras e 8 letras (desenho 3x5 em bits)
uniform vec4 extrasNomesCfg;  // quantos nomes, giro do texto (quartos de volta), quantos têm rosto no painel
uniform vec4 extrasCopiaInfo;   // ligado, folga (cm), altura de referência (cm), nota (+1000 = conseguiu)
uniform vec4 extrasChuva[4]; // nuvens de chuva do painel: x, y (cm), força 0..1 (chega e vai aos poucos), segundos desde o começo
uniform vec4 extrasComida;  // comida do painel: x, y, raio (cm), fração do tempo que falta (0 = sem comida)
uniform vec4 extrasRacao[25]; // [0] caixa que envolve a ração (min x, min y, max x, max y; vazia = nenhuma);
                              // [1..24] bolinhas: x, y (cm), segundos desde que caiu (<0 = vaga),
                              // tamanho*20 (parte inteira) + segundos desde a última beliscada (fração)
uniform vec4 extrasAg2[32];  // por bicho: estado (0 normal, 1 pousado/sentado, 2 mergulho, 3 indo, 4 caçando), altura do voo (0-1), tamanho, andamento do mergulho (0-1; no laboratório: visual do membro, 0 = sorteado)
float extrasMarca;          // 1 onde há um animal marinho (o shader da água o deixa à vista)
float extrasLimpa;          // 0..1: tira as curvas de nível (corpo dos bichos e beirada em volta; patch_surface.py)
float extrasAguaSombra;     // -1..1: sombra macia (> 0) ou clarão (< 0) na água em volta dos bichos (patch_agua.py)
float exDifCor(vec3 a,vec3 b)
	{
	/* diferença de cor aproximada (0 = igual; ~0,3 já se nota bem): a luminância pesa mais que o matiz,
	   como no olho (bicho pequeno projetado: o amarelo-claro e o branco quase se confundem) */
	vec3 d=a-b;
	return length(vec3(dot(d,vec3(0.3,0.59,0.11))*2.2,(d.r-d.g)*0.9,((d.r+d.g)*0.5-d.b)*0.5));
	}
uniform vec4 extrasMundo;   // laboratório (Mundo.cpp): estado (bits; mundoEstado), largura e altura da área iluminada, ligado (1)
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
float exDigito(float d,vec2 p)
	{
	/* algarismo 3x5 (10 = %): p = coluna (0..3) e linha (0..5, 0 = em cima) */
	if(p.x<0.0||p.y<0.0||p.x>=3.0||p.y>=5.0)
		return 0.0;
	float c=0.0;
	if(d<0.5) c=31599.0;
	else if(d<1.5) c=11415.0;
	else if(d<2.5) c=29671.0;
	else if(d<3.5) c=29647.0;
	else if(d<4.5) c=23497.0;
	else if(d<5.5) c=31183.0;
	else if(d<6.5) c=31215.0;
	else if(d<7.5) c=29257.0;
	else if(d<8.5) c=31727.0;
	else if(d<9.5) c=31695.0;
	else if(d<10.5) c=21157.0;
	float idx=floor(p.y)*3.0+floor(p.x);
	return mod(floor(c/pow(2.0,14.0-idx)),2.0);
	}
float exPontos(vec2 p,float tamanho,float chance)
	{
	vec2 c=floor(p);
	float h=exHash(c);
	if(h>chance)
		return 0.0;
	vec2 centro=vec2(exHash(c+vec2(3.1,7.7)),exHash(c+vec2(9.2,1.3)))*0.7+0.15;
	return smoothstep(tamanho,tamanho*0.3,length(fract(p)-centro));
	}
)GLSL"
MP_GLSL R"GLSL(
/* ------------------------------------------------------------------ Mundo LAOB
   O laboratório visto de cima (19/11/2026, pedido do Victor: "deixar o mapa com mais coisas: o scanner,
   as câmeras, o cluster..." e, com as 15 fotos, "a sala principal e outras duas, uma de forense e outra
   um auditório, com o máximo de detalhes em cada lugar"). Três salas e o corredor:
   - PRINCIPAL (Fx 0,275-0,980): quadro verde e armário do escudo na parede C; bancada H com osciloscópio,
     torso de RCP e a câmera do Willbert; cena do scanner (plataforma girando com o manequim, torre, laser)
     no xadrez; arco branco no outro xadrez; cluster de PCs deitados com cabos azuis, LEDs, roteador,
     switch e as câmeras do trilho; caixa de areia; ilhas de mesas (a do Victor com teclado RGB).
   - FORENSE (atrás da parede F, embaixo): armários, urna eletrônica com as 3 teclas, mesas redondas com
     papéis e com o tabuleiro de calibração, TV com webcam, PC aberto, pôster.
   - MINIAUDITÓRIO (atrás da parede F, em cima): espuma acústica com rodapé branco, painel com o slide
     trocando, mesa de vidro, sofás pretos, longarinas e cadeiras embrulhadas, ventilador.
   As posições vêm de MundoPlanta.h (as mesmas do Mundo.cpp). Tamanhos em "cm da planta" (100 = largura
   da área iluminada). Cor final = mG * luz da sala + mE (luz própria: LEDs, telas, a areia; não escurece
   à noite). MUNDO_NIVEL 3 = tudo; 2 tira os detalhes de prioridade 3; 1 deixa só o que identifica. */
#define MUNDO_NIVEL 3
vec3 mG;        // cor (recebe a luz da sala)
vec3 mE;        // luz própria
vec2 mF;        // fração da área iluminada
vec2 mC;        // cm da planta
vec2 mK;        // cm da planta por unidade de F
float mT;       // tempo (s)
float mBits;    // estado vindo do Mundo.cpp (bits; extrasMundo.x)
float mDia;     // 1 de dia, 0 de noite
vec2 exGira(vec2 p,float a)
	{
	float c=cos(a),s=sin(a);
	return vec2(c*p.x+s*p.y,-s*p.x+c*p.y);
	}
float mBit(float k)
	{
	return mod(floor(mBits/exp2(k)),2.0);
	}
vec2 mL(vec2 cF)
	{
	return mC-cF*mK;                                                    // posição local (cm) em volta de um ponto F
	}
float mRet(vec4 r)
	{
	/* distância (cm) até um retângulo dado em F (Fx0, Fx1, Fy0, Fy1); < 0 dentro */
	vec2 c=vec2(r.x+r.y,r.z+r.w)*0.5*mK;
	vec2 h=vec2(r.y-r.x,r.w-r.z)*0.5*mK;
	vec2 q=abs(mC-c)-h;
	return length(max(q,0.0))+min(max(q.x,q.y),0.0);
	}
bool mEm(vec4 r,float folga)
	{
	return mF.x>r.x-folga&&mF.x<r.y+folga&&mF.y>r.z-folga&&mF.y<r.w+folga;
	}
void mPoe(vec3 c)
	{
	mG=c;
	mE=vec3(0.0);                                                       // o que está por cima tapa a luz de baixo
	}
void mPinta(float d,vec3 c)
	{
	if(d<0.0)
		mPoe(c);
	}
void mClaro(float d,vec3 c)
	{
	/* objeto claro: contorno escuro de 0,25 cm por fora (sem ele, branco some no piso claro da areia) */
	if(d<0.0)
		mPoe(c);
	else if(d<0.25)
		mG*=0.55;
	}
float mSeg(vec2 p,vec2 a,vec2 b)
	{
	vec2 pa=p-a,ba=b-a;
	return length(pa-ba*clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0));
	}
void mLed(vec2 cF,vec3 cor,float liga)
	{
	/* LED de r 0,18 cm com halo de 0,5 cm; apagado, só o pontinho escuro */
	float d=length(mL(cF));
	if(d>0.5)
		return;
	if(liga>0.5)
		{
		if(d<0.18)
			mPoe(cor*0.5);
		mE+=cor*(d<0.18?0.6:0.3*smoothstep(0.5,0.18,d));
		}
	else if(d<0.18)
		mPoe(cor*0.22);
	}
void mCone(vec2 cF,float a,float abre,float alc,vec3 cor)
	{
	/* luz de câmera: cone a partir de cF, para o ângulo a, meia-abertura "abre" (tangente), alcance em cm */
	vec2 q=exGira(mL(cF),a);
	if(q.x>0.3&&q.x<alc&&abs(q.y)<q.x*abre)
		mE+=cor*(1.0-q.x/alc)*smoothstep(q.x*abre,q.x*abre*0.6,abs(q.y));
	}
void mCadeira(vec2 cF,float a,vec3 cor)
	{
	/* cadeira de escritório vista de cima (sem atan): assento, encosto atrás e o anel dos pés;
	   a = para onde olha quem senta */
	vec2 q=mL(cF);
	float r2=dot(q,q);
	if(r2>1.9)
		return;
	q=exGira(q,a);
	if(exCaixa(q-vec2(-0.95,0.0),vec2(0.22,0.9),0.18)<0.0)
		mPoe(cor*0.65);
	else if(exCaixa(q,vec2(0.95),0.35)<0.0)
		mPoe(cor*(1.0+0.25*smoothstep(0.9,0.0,length(q-vec2(0.3,0.3)))));
	else
		{
		float r=sqrt(r2);
		if(r>1.0&&r<1.35)
			mG=mix(mG,vec3(0.1,0.1,0.11),0.55);
		}
	}
void mPlastico(vec2 cF,vec3 cor)
	{
	/* cadeira de plástico (laranja, amarela): só o assento, com os 4 pés nos cantos */
	vec2 q=mL(cF);
	if(dot(q,q)>2.0)
		return;
	if(length(abs(q)-vec2(0.75))<0.18)
		mPoe(vec3(0.06));
	else
		mClaro(exCaixa(q,vec2(0.9),0.2),cor*(1.0+0.15*smoothstep(0.8,0.0,length(q-vec2(0.3,0.3)))));
	}
void mEmbrulhada(vec2 cF,float a,vec3 cor)
	{
	/* cadeira embrulhada em plástico (miniauditório, carteado): assento escuro, encosto, reflexo do
	   plástico e borda clara */
	vec2 q=mL(cF);
	if(dot(q,q)>2.7)
		return;
	q=exGira(q,a);
	float ds=exCaixa(q,vec2(0.95),0.3);
	float de=exCaixa(q-vec2(-0.95,0.0),vec2(0.25,1.0),0.2);
	float d=min(ds,de);
	if(d<0.0)
		{
		vec3 c=de<0.0?cor*0.7:cor;
		if(fract((q.x+q.y)*0.8)<0.15)
			c=mix(c,vec3(1.0),0.3);
		if(d>-0.15)
			c=mix(c,vec3(0.85),0.6);
		mPoe(c);
		}
	}
void mMonitor(vec2 cF,float a,float liga)
	{
	/* monitor visto de cima: barra preta de 2,2 x 0,35 cm, de frente para quem olha no ângulo a;
	   ligado = filete de tela azul do lado de quem senta; desligado = LED laranja de espera */
	vec2 q=exGira(mL(cF),a);
	if(abs(q.x)>0.3||abs(q.y)>1.25)
		return;
	if(exCaixa(q,vec2(0.175,1.1),0.08)<0.0)
		{
		mPoe(vec3(0.06,0.065,0.08));
		if(liga>0.5&&q.x<-0.03&&abs(q.y)<0.98)
			mE=vec3(0.2,0.45,0.85)*0.8;
		}
	else if(liga<0.5)
		mLed(cF+exGira(vec2(-0.25,0.9),-a)/mK,vec3(1.0,0.55,0.1),1.0);
	}
void mTeclado(vec2 cF,float a,vec3 cor)
	{
	vec2 q=exGira(mL(cF),a);
	if(abs(q.x)<0.35&&abs(q.y)<1.1)
		mPoe(cor*(0.9+0.2*step(0.5,fract(q.y*2.5))*step(0.1,abs(q.x))));
	}
float mDentroDaFolha(vec2 dob,vec2 dir,float comp)
	{
	/* folha de porta aberta (0,35 cm de espessura) a partir da dobradiça dob (F), na direção dir */
	return mSeg(mC,dob*mK,dob*mK+dir*comp)-0.175;
	}
void mFolha(vec2 dob,vec2 dirAberta,vec2 dirFechada,float comp)
	{
	/* porta aberta: folha azul-marinho perpendicular à parede e o arco de abertura (0,2 cm) */
	vec2 q=mL(dob);
	float r=length(q);
	if(r>comp+0.4)
		return;
	if(dot(q,dirAberta)>0.0&&dot(q,dirFechada)>0.0&&abs(r-comp)<0.1)
		mG*=0.93;
	float d=mDentroDaFolha(dob,dirAberta,comp);
	if(d<0.0)
		mPoe(abs(dot(q,dirAberta)-comp*0.5)<0.15*comp?vec3(0.55,0.65,0.75)*0.8:vec3(0.10,0.18,0.42));
	else if(d<0.2)
		mG*=0.6;
	}

/* ---------------------------------------------------------- corredor (Fy 0 a 0,120) */
void mCorredor()
	{
	vec3 c=mF.y<0.008?vec3(0.78,0.72,0.60):vec3(0.86,0.80,0.68);   // piso bege; faixa da parede oposta
	vec2 j=abs(fract(mC/3.2+0.5)-0.5)*3.2;
	if(min(j.x,j.y)<0.08)
		c*=0.93;                                                       // juntas
	c+=vec3(0.05)*smoothstep(7.0,0.0,length(vec2(mod(mC.x,20.0)-10.0,mC.y-0.06*mK.y))); // luzes do corredor
	mPoe(c);
	vec2 q=mL(vec2(0.955,0.112));
	if(abs(q.x)<0.5&&abs(q.y)<0.2)
		mPoe(vec3(0.15,0.15,0.17));                                   // plaquinha do número da sala
#if MUNDO_NIVEL>=3
	/* parede oposta do corredor: portas de outras salas (sem plaquinha), bebedouro e extintor */
	if(mF.y<0.02)
		{
		float xp=fract(mF.x/0.25+0.4);
		if(abs(xp-0.5)*25.0<1.8&&mF.y<0.010)
			mPoe(abs(xp-0.5)*25.0>1.55?vec3(0.6):vec3(0.10,0.18,0.42));
		vec2 b=mL(vec2(0.580,0.014));
		if(abs(b.x)<0.6&&abs(b.y)<0.45)
			mClaro(-0.01,length(b-vec2(0.0,0.1))<0.25?vec3(0.45,0.5,0.55):vec3(0.78,0.8,0.82));  // bebedouro
		if(length(mL(vec2(0.330,0.012)))<0.6)
			mPoe(vec3(0.85,0.1,0.1));                                 // extintor
		}
#endif
	if(mBit(5.0)>0.5)
		mFolha(vec2(MP_PORTA_ENT_0,MP_CORREDOR_Y),vec2(0.0,-1.0),vec2(1.0,0.0),(MP_PORTA_ENT_1-MP_PORTA_ENT_0)*mK.x);
	}

/* ---------------------------------------------------------- paredes, vãos e o que está pendurado nelas */
void mParede()
	{
	float x=mF.x,y=mF.y;
	float df=1e3;                                                       // distância até a face mais perto (cm)
	bool espuma=false;
	if(y<MP_PAREDE_H_Y)
		{
		/* parede H; o vão da entrada */
		if(x>MP_PORTA_ENT_0&&x<MP_PORTA_ENT_1)
			{
			mPoe(vec3(0.70,0.71,0.70));
			if(min(x-MP_PORTA_ENT_0,MP_PORTA_ENT_1-x)*mK.x<0.3)
				mPoe(vec3(0.6));                                       // batente
			if(mBit(5.0)<0.5&&abs(y-0.1335)*mK.y<0.175)
				mPoe(abs(x-0.927)*mK.x<0.15?vec3(0.55,0.65,0.75)*0.8:vec3(0.10,0.18,0.42)); // porta azul fechada, com o vidro
			return;
			}
		df=min(y-MP_CORREDOR_Y,MP_PAREDE_H_Y-y)*mK.y;
		}
	else if(y>MP_PAREDE_B_Y)
		df=(y-MP_PAREDE_B_Y)*mK.y;
	else if(x<MP_EXTERNA_X)
		{
		df=(MP_EXTERNA_X-x)*mK.x;
		espuma=y>MP_DIVISORIA_Y1;
		}
	else if(x>MP_PAREDE_C_X)
		df=(x-MP_PAREDE_C_X)*mK.x;
	else if(x<MP_PAREDE_F_X0)
		{
		df=min(y-MP_DIVISORIA_Y0,MP_DIVISORIA_Y1-y)*mK.y;
		espuma=y>0.5*(MP_DIVISORIA_Y0+MP_DIVISORIA_Y1);               // lado do miniauditório
		}
	else
		{
		/* parede F: os vãos da forense e do miniauditório */
		bool vaoF=y>MP_PORTA_FOR_0&&y<MP_PORTA_FOR_1;
		bool vaoM=y>MP_PORTA_MINI_0&&y<MP_PORTA_MINI_1;
		if(vaoF||vaoM)
			{
			float y0=vaoF?MP_PORTA_FOR_0:MP_PORTA_MINI_0,y1=vaoF?MP_PORTA_FOR_1:MP_PORTA_MINI_1;
			mPoe(vec3(0.70,0.71,0.70));
			if(min(y-y0,y1-y)*mK.y<0.3)
				mPoe(vec3(0.6));
			if(mBit(vaoF?6.0:7.0)<0.5&&abs(x-0.265)*mK.x<0.175)
				mPoe(vec3(0.10,0.18,0.42));                            // porta fechada
			return;
			}
		df=min(x-MP_PAREDE_F_X0,MP_PAREDE_F_X1-x)*mK.x;
		espuma=x<0.265&&y>MP_DIVISORIA_Y1;
		}
	vec3 c=vec3(0.16,0.17,0.19);                                        // miolo
	if(df<0.4)
		c=espuma?vec3(0.2,0.2,0.22):vec3(0.90,0.90,0.88);               // face (no miniauditório, espuma)
	mPoe(c);
	/* o que está pendurado na face da parede H, do lado de dentro */
	if(y>0.1375&&y<MP_PAREDE_H_Y)
		{
		if(x>0.649&&x<0.684)
			{
			mPoe(x<0.6665?vec3(0.80,0.15,0.15):vec3(0.6,0.8,0.95));   // cartaz de RCP
			vec2 h=mL(vec2(0.6665,0.1425));
			if(length(vec2(abs(h.x)-0.18,h.y-0.08))<0.22||(h.y<0.05&&abs(h.x)<0.35+h.y*1.2&&h.y>-0.3))
				mPoe(vec3(0.97));                                      // coração
			}
		else if(x>0.588&&x<0.610)
			mPoe(min(x-0.588,0.610-x)*mK.x<0.25||(y-0.1375)*mK.y<0.2?vec3(0.08):vec3(0.85,0.78,0.6)); // papiro com moldura
		else if(x>0.529&&x<0.564)
			mPoe(fract((x-0.529)*mK.x/1.2)<0.35?vec3(0.2,0.6,0.3):vec3(0.95)); // banner de iniciação científica
		else if(x>0.053&&x<0.086)
			mPoe(vec3(0.12,0.14,0.22));                                // forense: roll-up escuro
		else if(x>0.086&&x<0.185)
			mPoe(exHash(floor(mC*1.6))>0.8?vec3(0.35,0.55,0.5):vec3(0.95)); // banner do congresso de linguística forense
#if MUNDO_NIVEL>=3
		vec2 q=mL(vec2(0.955,0.143));
		if(abs(q.x)<0.25&&abs(q.y)<0.3)
			{mPoe(vec3(0.85,0.1,0.1)); mE=vec3(0.15,0.0,0.0);}          // alarme de incêndio
		q=mL(vec2(0.900,0.143));
		if(abs(q.x)<0.4&&abs(q.y)<0.25)
			mPoe(vec3(0.05));                                          // porta-chaves
#endif
		}
	/* parede F, face da forense: banner da urna e crucifixo */
	if(x>MP_PAREDE_F_X0&&x<MP_PAREDE_F_X0+0.007&&y<MP_DIVISORIA_Y0)
		{
		if(y>0.178&&y<0.269)
			mPoe(vec3(0.7,0.85,0.95));
#if MUNDO_NIVEL>=3
		vec2 q=mL(vec2(0.2585,0.275));
		if((abs(q.x)<0.15&&abs(q.y)<0.45)||(abs(q.y-0.12)<0.1&&abs(q.x)<0.3))
			mPoe(vec3(0.05));                                          // crucifixo
#endif
		}
	/* parede externa, face da forense: mural preto com folhas e alfinetes; cartaz-calendário */
	if(x>MP_EXTERNA_X-0.007&&x<MP_EXTERNA_X&&y<MP_DIVISORIA_Y0)
		{
		if(y>0.242&&y<0.357)
			{
			mPoe(vec3(0.06));
			vec2 r=vec2(mC.y-0.242*mK.y,0.0);
			float k=floor(r.x/2.1);
			if(k<4.0&&fract(r.x/2.1)>0.4)
				mPoe(vec3(0.95));                                      // folhas
			if(exHash(floor(mC*3.0))>0.93)
				mPoe(vec3(0.95,0.25,0.2));                             // alfinetes
			}
#if MUNDO_NIVEL>=3
		if(y>0.360&&y<0.380)
			{
			mPoe(vec3(0.95));
			if(fract(mC.y/0.47)<0.6&&abs(x-0.0165)*mK.x<0.15)
				mPoe(0.5+0.5*cos(6.283*(exHash(vec2(floor(mC.y/0.47),3.0))+vec3(0.0,0.33,0.67))));
			}
#endif
		}
	}

/* ---------------------------------------------------------- FORENSE (Fx 0,020-0,255, Fy 0,147-0,529) */
void mForense()
	{
	mPoe(vec3(0.70,0.71,0.70));
#if MUNDO_NIVEL>=3
	if(mBit(1.0)>0.5)
		mG+=vec3(0.04)*(smoothstep(6.0,0.0,length(mL(vec2(0.161,0.300))))+smoothstep(6.0,0.0,length(mL(vec2(0.102,0.319)))));
#endif
	/* TV ligada: brilho no chão na frente dela */
	if(mBit(11.0)>0.5&&mF.y>0.46&&mF.y<0.49&&mF.x>0.12&&mF.x<0.21)
		mE+=vec3(0.15,0.25,0.4)*(0.8+0.2*sin(mT*1.3))*smoothstep(0.46,0.49,mF.y);
	if(mF.y<0.215)
		{
		/* parede H: 5 armários altos, tripés, cofre com o porquinho, pôster e tubo */
		if(mEm(MP_F_ARM,0.004))
			{
			float d=mRet(MP_F_ARM);
			mClaro(d,vec3(0.84,0.82,0.74));
			if(d<0.0)
				{
				float u=fract((mF.x-MP_F_ARM.x)/0.0348);
				if(min(u,1.0-u)*3.48<0.1)
					mPoe(vec3(0.6));                                   // divisórias
				else if(abs(u-0.5)*3.48<0.075)
					mG*=0.8;                                           // vão das portas
				}
			}
		if(mF.x>0.120&&mF.x<0.200&&mF.y<0.18)
			{
			for(int k=0;k<2;++k)
				{
				vec2 a=vec2(0.126,k==0?0.158:0.168)*mK,b=vec2(0.184,k==0?0.158:0.168)*mK;
				if(mSeg(mC,a,b)<0.125)
					mPoe(vec3(0.15));                                  // tripé deitado
				for(int p=0;p<3;++p)
					{
					float an=-0.6+0.6*float(p);
					if(mSeg(mC,b,b+vec2(cos(an),sin(an))*0.8)<0.1)
						mPoe(vec3(0.15));
					}
				}
			}
		if(mEm(MP_F_COFRE,0.004))
			{
			mClaro(mRet(MP_F_COFRE),vec3(0.95));
			vec2 q=mL(vec2(0.034,0.163));
			if(abs(q.x)<0.25&&abs(q.y)<0.25)
				mPoe(vec3(0.05));                                      // teclado do cofre
			float dp=length(mL(vec2(0.044,0.157))/vec2(1.0,0.8))-0.5;
			if(dp<0.0)
				mPoe(vec3(0.85,0.18,0.2)*(1.15-0.4*smoothstep(-0.5,0.0,dp)));   // porquinho
			}
		vec2 q=mL(vec2(0.026,0.183));
		if(abs(q.x)<0.4&&abs(q.y)<1.8)
			mPoe(abs(q.x)>0.25||abs(q.y)>1.6?vec3(0.95):vec3(0.12,0.14,0.24)); // pôster Minority Report (roll-up)
		if(length(mL(vec2(0.230,0.160)))<0.35)
			mPoe(vec3(0.95));                                          // tubo de pôster
		}
	if(mF.x<0.06)
		{
		/* parede externa: coluna, mesa 1 (2 monitores, CPU, teclados), gaveteiro com a caixa de som, mesa 2 com o PC aberto */
		if(length(mL(vec2(0.026,0.220)))<0.7)
			mPoe(vec3(0.55));
		if(mEm(MP_F_MESA1,0.004))
			{
			mClaro(mRet(MP_F_MESA1),vec3(0.92,0.91,0.88));
			mMonitor(vec2(0.027,0.262),3.1416,mBit(1.0));
			mMonitor(vec2(0.027,0.296),3.1416,mBit(1.0));
			vec2 q=mL(vec2(0.030,0.322));
			if(abs(q.x)<0.5&&abs(q.y)<0.8)
				mPoe(vec3(0.08));                                      // CPU
			mTeclado(vec2(0.041,0.262),3.1416,vec3(0.22));
			mTeclado(vec2(0.041,0.296),3.1416,vec3(0.22));
			}
		if(mEm(MP_F_GAV,0.012))
			{
			mClaro(mRet(MP_F_GAV),vec3(0.92,0.91,0.88));
			float r=length(mL(vec2(0.032,0.348)));
			float pulso=mBit(10.0)>0.5?1.0+0.1*sin(mT*12.0):1.0;
			if(r<1.0*pulso)
				mPoe(r>0.7*pulso?vec3(0.8,0.1,0.1):vec3(0.06));       // caixa de som com o anel vermelho
			if(mBit(10.0)>0.5&&r>1.1&&mF.x>0.032)
				for(int k=0;k<2;++k)
					{
					float rr=1.2+2.0*fract(mT+0.5*float(k));
					if(abs(r-rr)<0.1)
						mE+=vec3(0.5,0.6,1.0)*0.5*(1.0-fract(mT+0.5*float(k)));   // música: ondas
					}
			}
		if(mEm(MP_F_MESA2,0.004))
			{
			mClaro(mRet(MP_F_MESA2),vec3(0.92,0.91,0.88));
			mMonitor(vec2(0.027,0.385),3.1416,mBit(1.0));
			mMonitor(vec2(0.027,0.415),3.1416,mBit(1.0));
			mMonitor(vec2(0.029,0.440),3.1416-0.44,mBit(1.0));
			vec2 q=mL(vec2(0.034,0.468));
			if(abs(q.x)<0.8&&abs(q.y)<0.6)
				{
				mPoe(vec3(0.22,0.23,0.25));                           // PC aberto
				if(abs(q.x+0.15)<0.5&&abs(q.y)<0.4)
					mPoe(vec3(0.2,0.45,0.25));                         // placa-mãe
				if(abs(fract(q.x/0.35+0.5)-0.5)*0.35<0.15&&abs(q.y-0.12)<0.15&&q.x<0.3)
					mPoe(vec3(0.85,0.7,0.25));                         // chips
				if(abs(length(q-vec2(0.5,-0.2))-0.25)<0.08)
					mPoe(vec3(0.6));                                   // ventoinha
				}
			}
		}
	/* cadeiras pretas das mesas da parede */
	mCadeira(vec2(0.067,0.262),3.1416,vec3(0.13));
	mCadeira(vec2(0.069,0.315),3.1416,vec3(0.13));
	mCadeira(vec2(0.067,0.414),3.1416,vec3(0.13));
	/* mesa dos papéis e mesa do tabuleiro de calibração */
	vec2 qp=mL(MP_F_PAPEIS.xy);
	if(dot(qp,qp)<16.0)
		{
		mCadeira(MP_F_PAPEIS.xy+vec2(-0.039,0.0),0.0,vec3(0.13));
		mCadeira(MP_F_PAPEIS.xy+vec2(0.039,0.0),3.1416,vec3(0.13));
		mCadeira(MP_F_PAPEIS.xy+vec2(0.0,-0.052),1.5708,vec3(0.13));
		mCadeira(MP_F_PAPEIS.xy+vec2(0.0,0.052),-1.5708,vec3(0.13));
		mClaro(length(qp)-MP_F_PAPEIS.z,vec3(0.90,0.88,0.82));
		for(int k=0;k<3;++k)
			{
			float fk=float(k);
			vec2 c=vec2(cos(fk*2.1+0.4),sin(fk*2.1+0.4))*1.2;
			vec2 q=exGira(qp-c,exHash(vec2(fk,7.0))*3.0);
			if(abs(q.x)<0.7&&abs(q.y)<0.5)
				{
				mPoe(vec3(0.97));                                     // folha
				if(fract(q.y*5.0)<0.3&&abs(q.x)<0.55)
					mPoe(vec3(0.5,0.6,0.7));                           // linhas
				if(q.x>0.35&&q.y>0.25)
					mPoe(k==1?vec3(0.2,0.75,0.3):vec3(0.95,0.55,0.15)); // bloquinhos
				}
			}
		vec2 q=exGira(qp-vec2(-0.6,-1.4),0.5);
		if(abs(q.x)<0.4&&abs(q.y)<0.15)
			mPoe(q.x>0.0?vec3(0.95,0.8,0.1):vec3(0.06));               // ferramenta amarela e preta
		}
	vec2 qt=mL(MP_F_TABULEIRO.xy);
	if(dot(qt,qt)<16.0)
		{
		mCadeira(MP_F_TABULEIRO.xy+vec2(0.039,0.0),3.1416,vec3(0.13));
		mCadeira(MP_F_TABULEIRO.xy+vec2(0.0,0.052),-1.5708,vec3(0.13));
		mCadeira(MP_F_TABULEIRO.xy+vec2(-0.039,0.0),0.0,vec3(0.13));
		mClaro(length(qt)-MP_F_TABULEIRO.z,vec3(0.90,0.88,0.82));
		vec2 q=exGira(qt,0.26);
		if(abs(q.x)<1.8&&abs(q.y)<1.2)
			mPoe(mod(floor(q.x/0.6)+floor(q.y/0.6),2.0)<0.5?vec3(0.06):vec3(0.95)); // tabuleiro 6 x 4
		}
	/* parede da porta: armários baixos com a urna eletrônica, terminal do mesário, cabos, terceiro armário */
	if(mF.x>0.20)
		{
		if(mEm(MP_F_ARMB,0.004))
			{
			float d=mRet(MP_F_ARMB);
			mClaro(d,vec3(0.93,0.92,0.89));
			if(d<0.0&&abs(mF.y-0.247)*mK.y<0.08)
				mPoe(vec3(0.6));                                      // emenda
			vec2 q=mL(vec2(0.244,0.262));
			if(abs(q.x)<1.0&&abs(q.y)<1.4)
				{
				mPoe(vec3(0.06));                                     // suporte
				if(abs(q.x)<0.8&&abs(q.y)<1.2)
					{
					mPoe(vec3(0.90,0.88,0.82));                        // urna
					if(q.y>0.1&&q.y<1.0&&abs(q.x)<0.6)
						mPoe(vec3(0.35,0.38,0.40));                    // telinha
					if(q.y<-0.1&&q.y>-1.0&&abs(q.x)<0.4)
						{
						mPoe(vec3(0.3));                               // teclado
						float t=q.y+0.75;
						if(length(vec2(q.x+0.2,t))<0.18) mPoe(vec3(0.95));            // branca
						if(length(vec2(q.x,t))<0.18) mPoe(vec3(0.95,0.55,0.15));      // laranja (corrige)
						if(length(vec2(q.x-0.2,t))<0.18) mPoe(vec3(0.2,0.75,0.3));    // verde (confirma)
						if(t>0.25&&fract(q.y*3.3)<0.4&&fract(q.x*3.3)<0.5) mPoe(vec3(0.75)); // números
						}
					}
				}
			if(length(mL(vec2(0.243,0.300))/vec2(0.6,0.4))<1.0)
				mPoe(vec3(0.3,0.2,0.15));                              // terminal do mesário
			if(mF.y<0.21&&abs(mC.x-0.243*mK.x-0.3*sin(mC.y*3.0))<0.15)
				mPoe(vec3(0.95));                                      // cabos brancos
			}
		if(mEm(MP_F_ARM3,0.004))
			{
			mClaro(mRet(MP_F_ARM3),vec3(0.93,0.92,0.89));
			if(length(mL(vec2(0.244,0.369)))<0.45)
				mPoe(vec3(0.97));                                      // caixinha redonda
			vec2 q=mL(vec2(0.244,0.355));
			if(abs(q.x)<0.5&&abs(q.y)<0.3)
				mPoe(vec3(0.15,0.17,0.25));                            // pasta
			}
		}
	/* TV na divisória, com a webcam; rack com teclado, controle, mini-PC e tubo */
	if(mF.y>0.48)
		{
		if(mEm(MP_F_RACK,0.004))
			{
			mClaro(mRet(MP_F_RACK),vec3(0.93,0.92,0.89));
			if(mF.x<0.133&&abs(mF.x-0.1235)*mK.x<0.06)
				mG*=0.8;
			vec2 q=mL(vec2(0.175,0.505));
			if(abs(q.x)<1.0&&abs(q.y)<0.35)
				mPoe(vec3(0.15));                                     // teclado
			q=mL(vec2(0.152,0.503));
			if(abs(q.x)<0.4&&abs(q.y)<0.15)
				mPoe(vec3(0.08));                                     // controle
			q=mL(vec2(0.141,0.512));
			if(abs(q.x)<0.5&&abs(q.y)<0.5)
				mPoe(vec3(0.12));                                     // mini-PC
			mLed(vec2(0.141,0.512),vec3(0.2,0.5,1.0),step(0.5,fract(mT*0.3)));
			if(mSeg(mC,vec2(0.195,0.495)*mK,vec2(0.160,0.516)*mK)<0.15)
				mPoe(vec3(0.95));                                      // tubo
			}
		if(mF.y>0.522&&mF.x>0.133&&mF.x<0.196)
			{
			mPoe(vec3(0.03));                                         // TV
			if(mBit(11.0)>0.5&&mF.y<0.5245)
				mE=vec3(0.3,0.45,0.7)*(0.8+0.2*sin(mT*1.3));
			}
		if(length(mL(vec2(0.166,0.520)))<0.35)
			mPoe(vec3(0.05));                                         // webcam
		mCone(vec2(0.166,0.520),-1.5708,0.5,6.0,vec3(0.03,0.04,0.05));
#if MUNDO_NIVEL>=3
		if(mF.y>0.522&&mF.x>0.095&&mF.x<0.126)
			mPoe(length(mL(vec2(0.110,0.5255)))<0.3?vec3(0.9,0.75,0.2):vec3(0.1,0.12,0.2)); // banner da universidade
#endif
		}
	/* porta da forense aberta (dobradiça no lado do miniauditório, abre para dentro) */
	if(mBit(6.0)>0.5)
		mFolha(vec2(MP_PAREDE_F_X0,MP_PORTA_FOR_1),vec2(-1.0,0.0),vec2(0.0,-1.0),(MP_PORTA_FOR_1-MP_PORTA_FOR_0)*mK.y*0.8);
	}

/* ---------------------------------------------------------- MINIAUDITÓRIO (Fx 0,020-0,255, Fy 0,556-0,973) */
void mMini()
	{
	vec3 piso=vec3(0.70,0.71,0.70);
	float dl=(mF.y-MP_DIVISORIA_Y1)*mK.y;                              // até a parede L (divisória)
	float dsx=(MP_PAREDE_F_X0-mF.x)*mK.x;                              // até a parede S (porta)
	float ddx=(mF.x-MP_EXTERNA_X)*mK.x;                                // até a parede D (tela)
	float db=(MP_PAREDE_B_Y-mF.y)*mK.y;                                // até a parede B (cortinas)
	if(min(min(dl,dsx),min(ddx,db))<3.0)
		piso*=0.92;
	mPoe(piso);
	/* tela ligada: luz no chão na frente dela */
	if(mBit(3.0)>0.5&&mF.x<0.080&&mF.y>0.689&&mF.y<0.848)
		mE+=vec3(0.85,0.9,1.0)*0.18*smoothstep(0.080,0.036,mF.x);
#if MUNDO_NIVEL>=3
	if(mBit(2.0)>0.5)
		{
		vec2 g=vec2(fract((mF.x-0.020)/0.1175),fract((mF.y-0.556)/0.139));
		mG+=vec3(0.035)*smoothstep(0.35,0.0,length(g-0.5));          // luminárias
		}
#endif
	/* espuma acústica (com rodapé branco) nas paredes L, S e D; faixa de espuma embaixo das cortinas */
	bool vaoM=mF.y>MP_PORTA_MINI_0&&mF.y<MP_PORTA_MINI_1;
	float dEsp=min(dl,min(vaoM?1e3:dsx,ddx));
	if(dEsp<0.8)
		{
		vec3 e=vec3(0.17,0.17,0.19)*(0.78+0.4*sin(mC.x*8.0)*sin(mC.y*8.0));   // espuma "caixa de ovo"
		if(ddx<0.8&&exHash(floor(mC/2.5))>0.8)
			e*=1.15;
		mPoe(e);
		}
	else if(dEsp<1.15)
		mPoe(vec3(0.88));                                             // rodapé
	if(db<0.4+0.78)
		{
		if(db<0.78)
			{
			/* cortinas (de dia claras, à noite cinza-azuladas) com as emendas */
			vec3 c=mix(vec3(0.35,0.37,0.42),vec3(0.93,0.93,0.90),mDia);
			if(mF.x<0.032||mF.x>0.236)
				c=vec3(0.90,0.90,0.88);
			else if(abs(mF.x-0.100)*mK.x<0.15||abs(mF.x-0.168)*mK.x<0.15)
				c*=0.8;
			mPoe(c*(0.92+0.08*sin(mC.x*5.0)));
			}
		else
			mPoe(vec3(0.17,0.17,0.19));
		}
	if(mF.x<0.06)
		{
		/* parede D: tela (painel), câmera PTZ, ventilador, cano, gaveteiro com o PC RGB */
		if(mF.y>0.689&&mF.y<0.848)
			{
			float t=(mF.y-0.689)/0.159;
			if(mF.x<0.026)
				mPoe(vec3(0.05));
			else if(mF.x<0.036)
				{
				mPoe(vec3(0.06));
				if(mBit(3.0)>0.5)
					{
					float per=mBit(4.0)>0.5?5.0:8.0;
					float s=floor(mT/per);
					vec3 c=vec3(0.92,0.94,0.97);
					if(t<0.12)
						c=vec3(0.80,0.82,0.86);                        // barra lateral do slide
					else if(abs(t-(0.35+0.3*exHash(vec2(s,1.0))))<0.12+0.08*exHash(vec2(s,2.0)))
						c=vec3(0.18,0.2,0.24);                         // o quadro escuro do slide
					else if(fract(t*14.0)<0.3&&mF.x>0.030)
						c=vec3(0.6,0.62,0.68);                         // linhas de texto
					c+=0.3*smoothstep(0.06,0.0,fract(mT/per));          // a troca de slide pisca
					mE=c*0.95;
					mG=vec3(0.0);
					}
				}
			}
		float r=length(mL(vec2(0.022,0.905)));
		if(r<0.45)
			mPoe(vec3(0.05));                                         // câmera PTZ
		mLed(vec2(0.022,0.898),vec3(0.2,1.0,0.3),step(0.5,fract(mT*0.5)));
		mCone(vec2(0.022,0.905),0.0,0.55,8.0,vec3(0.04));
		vec2 q=mL(MP_M_VENT.xy);
		r=length(q);
		if(r<1.0)
			{
			mPoe(vec3(0.25));                                         // ventilador: pás girando
			if(sin(3.0*atan(q.y,q.x)+mT*6.0)>0.0&&r>0.25)
				mPoe(vec3(0.4));
			if(r<0.25)
				mPoe(vec3(0.15));
			if(r>0.8)
				mPoe(vec3(0.12));                                     // grade
			}
		if(length(mL(vec2(0.030,0.955)))<0.35)
			mPoe(vec3(0.95));                                         // cano
		if(mEm(MP_M_GAV,0.004))
			{
			mClaro(mRet(MP_M_GAV),vec3(0.93,0.92,0.89));
			q=mL(vec2(0.036,0.575));
			if(abs(q.x)<0.5&&abs(q.y)<0.5)
				{
				mPoe(vec3(0.08));                                     // PC com borda RGB
				if(max(abs(q.x),abs(q.y))>0.38)
					{
					vec3 c=0.5+0.5*cos(6.283*(fract(mT*0.2+atan(q.y,q.x)/6.283)+vec3(0.0,0.33,0.67)));
					mPoe(c*0.3);
					mE=c*0.7;
					}
				}
			if(length(mL(vec2(0.036,0.590)))<0.4)
				mPoe(vec3(0.97));                                      // caixinha inteligente
			}
		}
	/* longarinas embrulhadas na parede L, ligadas por uma barra */
	if(mF.y<0.60&&mF.x>0.04&&mF.x<0.165)
		{
		if(abs(mF.y-0.571)*mK.y<0.15&&mF.x>0.055&&mF.x<0.149)
			mPoe(vec3(0.6));
		float k=clamp(floor((mF.x-0.055)/0.0235+0.5),0.0,4.0);
		mEmbrulhada(vec2(0.055+k*0.0235,0.578),1.5708,vec3(0.1));
		}
	/* fileiras de cadeiras embrulhadas (de frente para a tela? não: para a mesa de vidro, como na foto) */
	if(mF.y>0.88)
		{
		float k=clamp(floor((mF.x-0.055)/0.025+0.5),0.0,5.0);
		mEmbrulhada(vec2(0.055+k*0.025,0.944),-1.5708,vec3(0.1));
		k=clamp(floor((mF.x-0.079)/0.0273+0.5),0.0,3.0);
		mEmbrulhada(vec2(0.079+k*0.0273,0.898),-1.5708,vec3(0.1));
		}
	if(mF.x>0.215)
		{
		float k=clamp(floor((mF.y-0.862)/0.0265+0.5),0.0,2.0);
		mEmbrulhada(vec2(0.239,0.862+k*0.0265),3.1416,vec3(0.1));
#if MUNDO_NIVEL>=3
		vec2 q=mL(vec2(0.236,0.835));
		for(int j=0;j<2;++j)
			{
			float o=0.25*float(j+1);
			if(abs(exCaixa(q+vec2(o,-o),vec2(1.0),0.3))<0.07)
				mPoe(vec3(0.75));                                      // pilha de cadeiras
			}
		mEmbrulhada(vec2(0.236,0.835),3.1416,vec3(0.1));
#endif
		}
	/* caixa de som com o anel vermelho; apresentando: ondas */
	vec2 qs=mL(MP_M_SOM.xy);
	if(dot(qs,qs)<12.0)
		{
		if(abs(qs.x)<0.7&&abs(qs.y)<0.7)
			mPoe(length(qs)>0.35&&length(qs)<0.5?vec3(0.8,0.1,0.1):vec3(0.05));
		else if(mBit(4.0)>0.5)
			{
			float r=length(qs),f=fract(mT*0.8);
			if(abs(r-(0.9+2.2*f))<0.1)
				mE+=vec3(0.5,0.6,1.0)*0.4*(1.0-f);
			}
		}
	/* mesa de vidro (posto do Gazziro) com teclado, controle, tablet e celular; fita azul no chão */
	if(mF.x>0.10&&mF.x<0.19&&mF.y>0.66&&mF.y<0.78)
		{
		vec2 q=mL(vec2(0.1615,0.695));
		vec2 h=vec2(0.0165,0.027)*mK;
		if(abs(exCaixa(q,h,0.0))<0.125)
			mPoe(vec3(0.15,0.35,0.85));                                // fita azul
		mCadeira(vec2(0.160,0.695),2.6,vec3(0.13));
		mCadeira(MP_A0.xy,MP_A0.z,vec3(0.80,0.72,0.58));              // cadeira bege do Gazziro
		q=mL(vec2(0.126,0.731));
		if(abs(q.x)<1.1&&abs(q.y)<2.6)
			{
			mPoe(mix(piso,vec3(0.75,0.85,0.88),0.5));                  // vidro
			if(abs(abs(q.x*2.6)-abs(q.y*1.1))<0.35||max(abs(q.x)-0.9,abs(q.y)-2.4)>0.0)
				mPoe(vec3(0.82,0.84,0.86));                            // cromado
			if(abs(q.x+0.0)<0.3&&abs(q.y)<1.1)
				mPoe(vec3(0.15));                                      // teclado
			vec2 c=mL(vec2(0.124,0.755));
			if(length(c-vec2(0.0,0.2))<0.25||length(c+vec2(0.0,0.2))<0.25)
				mPoe(vec3(0.95));                                      // controle
			c=mL(vec2(0.130,0.708));
			if(abs(c.x)<0.45&&abs(c.y)<0.6)
				mPoe(vec3(0.05));                                      // tablet
			c=mL(vec2(0.119,0.700));
			if(abs(c.x)<0.15&&abs(c.y)<0.3)
				mPoe(vec3(0.1,0.1,0.14));                              // celular
			}
		else if(abs(exCaixa(q,vec2(1.1,2.6),0.0))<0.2)
			mG*=0.6;
		}
#if MUNDO_NIVEL>=3
	vec2 qa=mL(vec2(0.138,0.765));
	if(abs(qa.x)<0.8&&abs(qa.y)<0.6)
		mPoe(length(qa-vec2(0.4,0.0))<0.2?vec3(0.1):vec3(0.7));        // aparelho no teto
#endif
	/* sofá preto na parede S, com a mochila */
	if(mEm(MP_M_SOFA,0.003))
		{
		float d=mRet(MP_M_SOFA);
		if(d<0.0)
			{
			vec3 c=vec3(0.15);
			if(mF.x>0.240||mF.y<0.664||mF.y>0.807)
				c=vec3(0.08);                                          // encosto e braços
			else if(abs(mF.y-0.709)*mK.y<0.08||abs(mF.y-0.762)*mK.y<0.08)
				c=vec3(0.05);                                          // emendas das almofadas
			c+=vec3(0.06)*step(0.8,fract(mC.y*0.35+mC.x*0.1));         // brilho do couro
			mPoe(c);
			}
		else if(d<0.2)
			mG*=0.6;
		if(length(mL(vec2(0.230,0.673))/vec2(0.6,0.45))<1.0)
			mPoe(vec3(0.04,0.04,0.05));                                // mochila
		}
	/* porta do miniauditório aberta (dobradiça no lado da forense, abre para dentro) */
	if(mBit(7.0)>0.5)
		mFolha(vec2(MP_PAREDE_F_X0,MP_PORTA_MINI_0),vec2(-1.0,0.0),vec2(0.0,1.0),(MP_PORTA_MINI_1-MP_PORTA_MINI_0)*mK.y*0.8);
	}

/* ---------------------------------------------------------- SALA PRINCIPAL: cada grupo de móveis */
void mXadrez(vec4 r)
	{
	if(mEm(r,0.0))
		mPoe(mod(floor(mC.x/1.6)+floor(mC.y/1.6),2.0)<0.5?vec3(0.95):vec3(0.08));
	}
void mBancadaH()
	{
	/* armário alto e lixeira; bancada da parede H com torres, câmera do Willbert, aparelhos, osciloscópio,
	   torso de RCP, monitor; as cadeiras laranja, amarela e pretas */
	if(mEm(MP_ARM_ALTO,0.004))
		mClaro(mRet(MP_ARM_ALTO),vec3(0.93,0.92,0.89));
	if(length(mL(vec2(0.500,0.157)))<0.55)
		mPoe(vec3(0.07));
	if(mEm(MP_BANC_H,0.004))
		{
		mPinta(mRet(MP_BANC_H),vec3(0.07));
		if(mF.y>0.1975)
			mPoe(vec3(0.92));                                         // borda branca
		else if(mF.y<0.160)
			mPoe(vec3(0.12));                                         // prateleira
		if(mF.x>0.858)
			{
			/* 2 torres pretas de grade e a câmera branca de pan/tilt em cima (a do Willbert) */
			vec2 q=mL(vec2(0.871,0.174));
			vec2 q2=mL(vec2(0.889,0.174));
			if((abs(q.x)<0.8&&abs(q.y)<0.8)||(abs(q2.x)<0.8&&abs(q2.y)<0.8))
				mPoe(fract(mC.y/0.3)<0.4?vec3(0.18):vec3(0.04));
			if(mSeg(mC,vec2(0.880,0.170)*mK,vec2(0.880,0.150)*mK)<0.1)
				mPoe(vec3(0.5));                                       // cabo
			float r=length(mL(vec2(0.880,0.170)));
			if(r<0.6)
				mPoe(r<0.42?vec3(0.97):vec3(0.85));
			if(length(mL(vec2(0.880,0.170))-vec2(0.0,0.3))<0.2)
				mPoe(vec3(0.03));                                      // lente
			mLed(vec2(0.880,0.170)+vec2(0.0,-0.25)/mK,vec3(0.2,0.5,1.0),1.0);
			}
#if MUNDO_NIVEL>=3
		vec2 qn=mL(vec2(0.845,0.160));
		if(abs(qn.x)<0.9&&abs(qn.y)<0.6)
			mPoe(fract(mC.x/0.3)<0.4?vec3(0.18):vec3(0.08));          // nobreak/amplificador
#endif
		if(mF.x>0.690&&mF.x<0.775)
			{
			/* 3 aparelhos de bancada; o do meio é o osciloscópio */
			float k=clamp(floor((mF.x-0.708)/0.025+0.5),0.0,2.0);
			vec2 q=mL(vec2(0.708+0.025*k,0.172));
			if(abs(q.x)<1.0&&abs(q.y)<0.8)
				{
				mPoe(vec3(0.55,0.56,0.58));
				if(abs(q.x)<0.5&&abs(q.y-0.1)<0.3)
					{
					mPoe(vec3(0.05,0.08,0.06));
					if(abs(k-1.0)<0.5)
						{
						bool liga=mBit(15.0)>0.5||mBit(0.0)>0.5||mDia>0.5;
						vec2 s=q-vec2(0.0,0.1);
						if(liga&&abs(s.y-0.12*sin(s.x*9.0+mT*6.0))<0.07)
							{mPoe(vec3(0.1,0.3,0.12)); mE=vec3(0.3,1.0,0.4)*0.8;}
						else if(!liga&&length(s)<0.08)
							mE=vec3(0.3,1.0,0.4)*0.6;
						}
					else if(mDia>0.5||mBit(0.0)>0.5)
						mE=vec3(0.15,0.3,0.45)*0.6;
					}
				}
			}
		if(mF.x>0.655&&mF.x<0.700)
			{
			if(length(mL(vec2(0.684,0.172))/vec2(0.8,0.5))<1.0)
				mPoe(vec3(0.85,0.72,0.60)*(1.1-0.2*length(mL(vec2(0.684,0.172))/vec2(0.8,0.5)))); // torso de RCP
			if(mSeg(mC,vec2(0.665,0.152)*mK,vec2(0.690,0.152)*mK)<0.15||mSeg(mC,vec2(0.665,0.157)*mK,vec2(0.688,0.157)*mK)<0.15)
				mPoe(vec3(0.95));                                      // tubos de pôster
			}
		mMonitor(vec2(0.592,0.168),-1.5708,mBit(0.0));
		mMonitor(vec2(0.535,0.168),-1.5708,0.0);
		}
	/* cadeiras da bancada H */
	if(mF.y<0.26)
		{
		mPlastico(vec2(0.642,0.221),vec3(0.95,0.50,0.12));
		mPlastico(vec2(0.606,0.221),vec3(0.95,0.80,0.15));
		mCadeira(MP_A2.xy,MP_A2.z,vec3(0.13));
		mCadeira(vec2(0.804,0.230),-1.5708,vec3(0.13));
		}
	/* câmera do Willbert: cone de visão para dentro da sala (mais forte com algum membro presente) */
	mCone(vec2(0.880,0.170),1.5708,0.5,8.0,vec3(0.06,0.08,0.1)*(0.6+0.4*sin(mT*2.0))*(mBit(21.0)>0.5?1.8:1.0));
	}
void mCantoFH()
	{
	/* canto F/H: geladeira com ímãs, armários de aço, mesinha, coluna, mesa de carteado */
	if(mEm(MP_GELADEIRA,0.004))
		{
		float d=mRet(MP_GELADEIRA);
		mClaro(d,vec3(0.94));
		if(d<0.0&&abs(mF.y-0.2005)*mK.y<0.07)
			mG*=0.75;                                                  // as 2 portas
		for(int k=0;k<4;++k)
			if(length(mL(vec2(0.283+0.005*float(k),0.190+0.006*mod(float(k),2.0))))<0.3)
				mPoe(0.5+0.5*cos(6.283*(float(k)*0.27+vec3(0.0,0.33,0.67))));   // ímãs
		}
	if(mEm(MP_ARM_ACO,0.004))
		{
		float d=mRet(MP_ARM_ACO);
		mPinta(d,vec3(0.45,0.50,0.45));
		if(d<0.0&&(abs(fract((mF.y-MP_ARM_ACO.z)/0.01025)-0.5)>0.45||abs(mF.x-0.2885)*mK.x<0.06))
			mG*=0.75;                                                  // portas: 2 colunas x 4
		}
#if MUNDO_NIVEL>=3
	vec2 qm=mL(vec2(0.318,0.262));
	if(abs(qm.x)<0.95&&abs(qm.y)<0.95)
		mClaro(exCaixa(qm,vec2(0.7),0.1),vec3(0.93));                  // mesinha branca
#endif
	if(mEm(MP_COLUNA,0.006))
		{
		vec2 q=mL(vec2(0.2925,0.2915));
		vec2 h=vec2(0.0175*mK.x,0.0205*mK.y);
		if(exCaixa(q-vec2(0.4,-0.4),h,0.0)<0.0)
			mG*=0.8;                                                   // sombra
		mClaro(exCaixa(q,h,0.0),vec3(0.92,0.92,0.9));
		}
	vec2 q=mL(MP_CARTEADO.xy);
	if(dot(q,q)<25.0)
		{
		for(int k=0;k<5;++k)
			{
			float a=1.5708+1.2566*float(k);
			mEmbrulhada(MP_CARTEADO.xy+vec2(cos(a),sin(a))*3.6/mK,a+3.1416,vec3(0.12));
			}
		float r=length(q);
		if(r<MP_CARTEADO.z)
			{
			mPoe(r>MP_CARTEADO.z-0.35?vec3(0.75,0.15,0.12):vec3(0.10,0.45,0.25)*(1.05-0.1*r/2.4)); // barrado e feltro
			for(int k=0;k<4;++k)
				{
				float a=0.7854+1.5708*float(k);
				vec2 n=q-vec2(cos(a),sin(a))*1.2;
				bool vermelho=mod(float(k),2.0)>0.5;
				float d=vermelho?abs(n.x)+abs(n.y)-0.35:length(n-vec2(0.0,0.08))-0.22;
				if(!vermelho&&abs(n.x)<0.06&&n.y<0.0&&n.y>-0.32)
					d=-1.0;                                            // naipe preto com a hastezinha
				if(d<0.0)
					mPoe(vermelho?vec3(0.85,0.1,0.1):vec3(0.03));
				}
			}
		else if(r<MP_CARTEADO.z+0.25)
			mG*=0.6;
		}
	}
void mScanner()
	{
	/* xadrez 1 com o scanner corporal: plataforma girando com o manequim, torre, laser; câmera F no alto
	   da parede do nicho; banner roxo com as mesas; nicho com o PC aberto; banner do provador virtual */
	mXadrez(vec4(MP_PAREDE_F_X1,0.395,0.287,0.444));
	float th=mT*0.35;
	vec2 cP=MP_PLATAFORMA.xy;
	vec2 q=mL(cP);
	float r=length(q);
	vec2 qt=mL(vec2(0.322,0.375));
	if(abs(qt.x)<0.6&&mF.y>0.375&&mF.y<cP.y)
		mPoe(vec3(0.9));                                               // base da torre até a plataforma
	if(r<MP_PLATAFORMA.z+0.25)
		{
		if(r>MP_PLATAFORMA.z)
			mG*=0.55;
		else if(r>1.55)
			{
			mPoe(vec3(0.95));                                         // aro com as marcas girando
			if(abs(fract((atan(q.y,q.x)-th)*0.955)-0.5)>0.42)
				mPoe(vec3(0.6));
			}
		else
			{
			mPoe(vec3(0.18,0.19,0.21));
			vec2 p=exGira(q,th);
			if(abs(abs(p.y)-0.55)<0.15&&abs(p.x)<1.3)
				mPoe(vec3(0.7));                                      // barras de apoio dos pés
			/* manequim girando junto */
			float o=length(p/vec2(0.55,1.05));
			if(o<1.0)
				mPoe(vec3(0.96)*(1.05-0.15*o));
			if(length(p)<0.42)
				mPoe(vec3(0.99));
			if(min(length(vec2(p.x,abs(p.y)-0.75)),length(vec2(p.x-0.35,abs(p.y)-0.3)))<0.18)
				mPoe(vec3(0.9,0.12,0.1));                              // marcadores
			}
		}
	if(abs(qt.x)<1.6&&abs(qt.y)<1.2)
		{
		mClaro(exCaixa(qt,vec2(1.2,0.8),0.6),vec3(0.95));              // torre
		if(length(qt-vec2(0.95,0.0))<0.3)
			mPoe(vec3(0.85,0.1,0.1));                                 // botão
		}
	if(mBit(8.0)>0.5)
		{
		vec2 a=vec2(0.322,0.375)*mK+vec2(0.0,0.8),b=cP*mK;
		if(mSeg(mC,a,b)<0.1&&step(0.5,fract(mT*3.0))>0.5)
			{mPoe(vec3(1.0,0.2,0.2)); mE=vec3(1.0,0.2,0.2)*0.8;}       // laser
		}
	/* banner do provador virtual (girado 30 graus) */
	vec2 qb=exGira(mL(vec2(0.346,0.304)),0.5236);
	if(abs(qb.x)<1.2&&abs(qb.y)<0.25)
		mClaro(-0.01,abs(qb.y)<0.08?vec3(0.6):vec3(0.95));
	else if(abs(qb.x)<1.45&&abs(qb.y)<0.5)
		mG*=0.7;
	/* banner roxo na parede F e as mesas brancas com 3 monitores e o gaveteiro */
	if(mF.x<0.322&&mF.y<0.44)
		{
		if(mEm(MP_MESAS_ROXO,0.004))
			{
			mClaro(mRet(vec4(0.282,0.300,0.296,0.428)),vec3(0.92,0.91,0.88));
			mMonitor(vec2(0.2855,0.320),3.1416,mBit(0.0));
			mMonitor(vec2(0.2855,0.355),3.1416,0.0);
			mMonitor(vec2(0.2855,0.390),3.1416,0.0);
			vec2 g=mL(vec2(0.291,0.418));
			if(abs(g.x)<0.8&&abs(g.y)<0.6)
				mPoe(abs(g.y)<0.05?vec3(0.6):vec3(0.93));              // gaveteiro
			if(mF.x<0.282&&mF.y>0.296&&mF.y<0.428)
				mPoe(exHash(floor(mC*2.0))>0.85?(exHash(floor(mC*2.0)+3.0)>0.5?vec3(1.0,0.4,0.8):vec3(0.3,0.9,1.0)):vec3(0.35,0.25,0.55)); // banner anime roxo
			}
		if(mEm(MP_NICHO,0.004))
			{
			mClaro(mRet(MP_NICHO),vec3(0.92,0.91,0.88));
#if MUNDO_NIVEL>=3
			vec2 n=mL(vec2(0.294,0.304));
			if(abs(n.x)<0.5&&abs(n.y)<0.5)
				mPoe(vec3(0.06));                                     // PC
			n=mL(vec2(0.310,0.304));
			if(abs(n.x)<0.6&&abs(n.y)<0.55)
				mPoe(abs(n.x)<0.4&&abs(n.y)<0.35?vec3(0.2,0.45,0.25):vec3(0.4)); // PC aberto
#endif
			}
		/* câmera F no alto da parede do nicho, olhando a plataforma */
		if(length(mL(vec2(0.279,0.305)))<0.4)
			mPoe(vec3(0.05));
		mLed(vec2(0.279,0.305)+vec2(0.25,0.0)/mK,vec3(1.0,0.15,0.1),step(0.5,fract(mT*(mBit(8.0)>0.5?2.0:1.0))));
		}
	mCone(vec2(0.279,0.305),1.05,0.45,7.0,vec3(0.05,0.04,0.04)*(mBit(8.0)>0.5?2.0:1.0));
	}
void mCluster()
	{
	/* bancada do cluster: cabos azuis, PCs SFF deitados com LEDs, roteador, switch, mesh, câmeras do trilho;
	   estações dos dois lados; mesa da ponta C; caixa de areia com o projetor */
	if(mEm(MP_CLUSTER,0.004))
		{
		mPinta(mRet(MP_CLUSTER),vec3(0.07));
		if(abs(mF.y-0.3135)*mK.y<0.15||abs(mF.y-0.4185)*mK.y<0.15)
			mPoe(vec3(0.92));                                         // bordas brancas
		/* cabos (antes dos PCs) */
		float cy=mC.y-0.366*mK.y;
		if(abs(cy)<2.6&&mF.x>0.45&&mF.x<0.875)
			{
			float v=(cy+0.5*sin(mC.x*0.9)+0.3*sin(mC.x*2.3+1.0))/0.7;
			float id=floor(v);
			if(abs(fract(v)-0.5)<0.12&&exHash(vec2(id,floor(mC.x/6.0)))>0.35)
				{
				float h=exHash(vec2(id,5.0));
				mPoe(h<0.75?vec3(0.2,0.5,0.95):(h<0.88?vec3(0.95,0.85,0.2):vec3(0.9,0.2,0.15)));
				}
			}
		if(abs(mF.y-0.366)*mK.y<0.225&&!(mF.x>0.56&&mF.x<0.82&&mF.y>0.366))
			{
			mPoe(vec3(0.92));                                         // estrutura de cima
#if MUNDO_NIVEL>=3
			if(abs(fract(mC.x/5.0)-0.5)*5.0<0.2)
				mPoe(vec3(0.75));                                      // tomadas
#endif
			}
		/* 6 grupos de 2 PCs SFF deitados, frente prateada para o lado B */
		if(mF.x>0.560&&mF.x<0.820&&mF.y>0.375)
			{
			float g=clamp(floor((mF.x-0.570)/0.040),0.0,5.0);
			vec2 q=mL(vec2(0.590+0.040*g,0.390));
			float k=step(0.0,q.x);
			vec2 p=q-vec2(k*1.4-0.7,0.0);
			if(abs(p.x)<0.65&&abs(p.y)<0.75)
				{
				mPoe(vec3(0.25,0.26,0.28));
				if(p.y>0.4)
					mPoe(vec3(0.72,0.74,0.77));                        // frente
				if(abs(p.x)<0.075)
					mPoe(vec3(0.12));                                  // pilha de 2
				float n=g*2.0+k;
				vec2 l1=vec2(0.590+0.040*g,0.390)+vec2(k*1.4-0.7-0.3,0.55)/mK;
				vec2 l2=vec2(0.590+0.040*g,0.390)+vec2(k*1.4-0.7+0.25,0.55)/mK;
				mLed(l1,vec3(0.2,1.0,0.3),step(0.3,fract(mT*(1.3+0.4*k)+exHash(vec2(k,g)))));
				mLed(l2,vec3(1.0,0.55,0.1),step(0.7,exHash(vec2(n+4.0*g,floor(mT*7.0)))));
				}
			}
		/* roteador (antenas e LEDs azuis) e switch na estrutura */
		vec2 qr=mL(vec2(0.684,0.366));
		if(abs(qr.x)<1.6&&abs(qr.y)<1.3)
			{
			for(int k=0;k<4;++k)
				{
				float a=(float(k)<1.5?0.5236+0.5236*float(k):3.1416-0.5236-0.5236*(float(k)-2.0));
				if(mSeg(qr,vec2(0.0),vec2(cos(a),sin(a))*1.2)<0.1)
					mPoe(vec3(0.05));
				}
			if(abs(qr.x)<0.8&&abs(qr.y)<0.5)
				{
				mPoe(vec3(0.06));
				for(int k=0;k<3;++k)
					mLed(vec2(0.684,0.366)+vec2(-0.4+0.4*float(k),-0.2)/mK,vec3(0.2,0.5,1.0),step(0.2,fract(mT*0.7+float(k)*0.3)));
				}
			}
		vec2 qs=mL(vec2(0.557,0.366));
		if(abs(qs.x)<1.5&&abs(qs.y)<0.4)
			{
			mPoe(vec3(0.06));
			float k=floor((qs.x+1.5)/0.375);
			mLed(vec2(0.557,0.366)+vec2(-1.5+0.375*(k+0.5),0.0)/mK,exHash(vec2(k,2.0))>0.6?vec3(1.0,0.55,0.1):vec3(0.2,1.0,0.3),step(0.3,exHash(vec2(k,floor(mT*5.0)))));
			}
		vec2 qm=mL(vec2(0.850,0.395));
		if(abs(qm.x)<0.85&&abs(qm.y)<0.85)
			mClaro(exCaixa(qm,vec2(0.6),0.3),vec3(0.96));              // roteador mesh branco
		/* trilho das câmeras Tapo (lado H, ponta C) */
		if(mF.x>0.733&&mF.x<0.874&&abs(mF.y-0.341)*mK.y<0.15)
			mPoe(vec3(0.5));
		if(mF.x>0.73&&mF.x<0.88&&mF.y<0.36)
			{
			float k=clamp(floor((mF.x-0.745)/0.030+0.5),0.0,4.0);
			vec2 c=vec2(0.745+0.030*k,0.341);
			float rc=length(mL(c));
			if(rc<0.45)
				mClaro(-0.01,vec3(0.97));
			else if(rc<0.65)
				mG*=0.55;
			if(length(mL(c)-vec2(0.0,-0.2))<0.2)
				mPoe(vec3(0.03));                                      // lente
			mLed(c+vec2(0.2,0.15)/mK,vec3(0.2,0.5,1.0),step(0.5,fract(mT*0.5+0.2*k)));
			}
		/* estações do lado H: monitores, teclados, joysticks e o carregador */
		if(mF.y<0.36&&mF.x<0.735)
			{
			float k=clamp(floor((mF.x-0.48)/0.04+0.5),0.0,6.0);
			mMonitor(vec2(0.48+0.04*k,0.345),1.5708,(k==3.0&&mBit(0.0)>0.5)?1.0:0.0);
			vec2 t=mL(vec2(0.48+0.04*k,0.322));
			if(abs(t.x)<1.0&&abs(t.y)<0.3)
				mPoe(vec3(0.25)*(0.9+0.2*step(0.5,fract(t.x*2.5))));
#if MUNDO_NIVEL>=3
			for(int j=0;j<2;++j)
				{
				float rj=length(mL(vec2(0.455+0.015*float(j),0.330)));
				if(rj<0.45)
					mPoe(rj<0.3?vec3(0.9,0.1,0.1)*(1.2-rj):vec3(0.05)); // joysticks
				}
			vec2 u=mL(vec2(0.500,0.325));
			if(abs(u.x)<0.7&&abs(u.y)<0.3)
				mPoe(vec3(0.05));                                      // carregador USB
#endif
			}
		/* estação do Lucas no lado B */
		mMonitor(vec2(0.820,0.404),-1.5708,mBit(14.0));
		vec2 t=mL(vec2(0.820,0.414));
		if(abs(t.x)<1.1&&abs(t.y)<0.35)
			mPoe(vec3(0.22)*(0.9+0.2*step(0.5,fract(t.x*2.5))));
		}
	/* câmeras do trilho: cone para o corredor (mais forte com alguém ajustando ou com o Victor sentado) */
	if(mF.y<0.345&&mF.x>0.70)
		{
		float k=clamp(floor((mF.x-0.745)/0.030+0.5),0.0,4.0);
		mCone(vec2(0.745+0.030*k,0.341),-1.5708,0.45,5.0,vec3(0.04)*(mBit(22.0)>0.5||mBit(13.0)>0.5?2.0:1.0));
		}
	/* cadeiras do lado B */
	if(mF.y>0.42&&mF.y<0.48)
		{
		mCadeira(MP_A1.xy,MP_A1.z,vec3(0.13));
		mCadeira(vec2(0.580,0.447),-1.5708,vec3(0.13));
		mCadeira(vec2(0.640,0.452),-1.5708,vec3(0.13));
		}
	/* mesa da ponta C, com monitor e PC */
	if(mEm(MP_MESA_C,0.025))
		{
		mCadeira(vec2(0.902,0.312),1.5708,vec3(0.13));
		float d=mRet(MP_MESA_C);
		mClaro(d,vec3(0.92,0.91,0.88));
		if(d<0.0&&d>-0.3&&length(abs(mL(vec2(0.9025,0.362)))-vec2(1.2,2.1))<0.3)
			mPoe(vec3(0.05));                                         // pés
		mMonitor(vec2(0.900,0.370),1.5708,0.0);
		vec2 q=mL(vec2(0.911,0.383));
		if(abs(q.x)<0.5&&abs(q.y)<0.8)
			mPoe(vec3(0.06));
		}
	/* caixa de areia: bancada, moldura, o relevo colorido (luz própria), projetor e Kinect, monitor */
	if(mEm(MP_AREIA,0.006))
		{
		mPinta(mRet(MP_AREIA),vec3(0.06));
		float d=mRet(vec4(0.381,0.430,0.382,0.440));
		if(d<0.0)
			{
			if(d>-0.4)
				mPoe(vec3(0.93));
			else if(d>-0.6)
				mPoe(vec3(0.04));
			else
				{
				vec2 p=mC*0.55;
				float h=exRuido(p+vec2(mT*0.05,0.0))*0.8+0.2*sin(p.x*1.3+p.y*0.7);
				vec3 c=h<0.35?vec3(0.2,0.5,0.85):(h<0.5?vec3(0.3,0.7,0.35):(h<0.68?vec3(0.92,0.84,0.5):vec3(0.75,0.4,0.2)));
				mPoe(vec3(0.0));
				mE=c*0.85;
				}
			}
		vec2 q=mL(vec2(0.405,0.411));
		if(abs(q.x)<0.6&&abs(q.y)<0.4)
			mPoe(vec3(0.05));                                         // projetor e Kinect
		if(abs(q.x)<0.15&&q.y<0.0&&mF.y>0.382)
			mPoe(vec3(0.95));                                         // poste
		if(abs(mF.y-0.447)*mK.y<0.18&&mF.x>0.413&&mF.x<0.435)
			{mPoe(vec3(0.05)); if(mF.y<0.447) mE=vec3(0.2,0.45,0.85)*0.8;} // monitor com o painel
		}
	/* banner "Sensória" e a coluna com 2 extintores */
	vec2 qb=mL(vec2(0.432,0.468));
	if(abs(qb.x)<1.25&&abs(qb.y)<0.45)
		{
		mClaro(max(abs(qb.x)-1.0,abs(qb.y)-0.2),vec3(0.95));
		if(abs(qb.y)<0.2&&abs(fract(qb.x/0.5+0.5)-0.5)*0.5<0.12&&abs(qb.x)<0.9)
			mPoe(0.5+0.5*cos(6.283*(floor(qb.x/0.5)*0.23+vec3(0.0,0.33,0.67))));
		}
	if(mEm(MP_EXTINTORES,0.012))
		{
		vec2 q=mL(vec2(0.451,0.477));
		mClaro(exCaixa(q,vec2(0.6),0.0),vec3(0.92));
		if(length(vec2(abs(q.x)-0.42,q.y-0.95))<0.4)
			mPoe(vec3(0.85,0.1,0.1));
		}
	}
void mParedeC()
	{
	/* parede C: painel elétrico, caixa alta, 9 caixas pretas, quadro verde com giz, armário do escudo,
	   pôster e expositor */
	if(mF.y<0.26&&mF.y>0.19)
		{
		if(mF.x>0.966&&mF.y>0.197&&mF.y<0.254)
			mPoe(vec3(0.60,0.61,0.62));
		float r=length(mL(vec2(0.973,0.240)));
		if(r<0.3)
			{mPoe(vec3(0.6,0.05,0.05)); mE=vec3(1.0,0.15,0.1)*0.7;}
		}
	if(mEm(MP_CAIXA_ALTA,0.004))
		mPinta(mRet(MP_CAIXA_ALTA),vec3(0.05));
	if(mEm(MP_CAIXAS,0.004))
		{
		float k=clamp(floor((mF.y-0.312)/0.0478),0.0,8.0);
		vec2 q=mL(vec2(0.9555,0.312+0.0478*(k+0.5)));
		float d=exCaixa(q,vec2(1.0,1.3),0.5);
		if(d<0.0)
			mPoe(vec3(0.08)+vec3(0.1)*smoothstep(0.6,0.0,length(q-vec2(-0.4,0.6))));
		else if(d<0.15)
			mG*=0.6;
		}
	if(mF.x>0.962&&mF.y>0.395&&mF.y<0.766)
		{
		if(mF.x<0.966)
			{
			mPoe(vec3(0.6));                                          // calha
			if(abs(mF.y-0.48)*mK.y<0.4||abs(mF.y-0.70)*mK.y<0.4)
				mPoe(vec3(0.85,0.75,0.55));                            // apagadores
			}
		else
			{
			mPoe(vec3(0.16,0.36,0.30));
			bool giz=exHash(floor(mC*vec2(1.5,3.0)))>0.9;
			if(mBit(12.0)>0.5)
				giz=giz&&mF.y<0.395+0.371*fract(mT/20.0);
			else
				giz=giz&&(abs(mF.y-0.46)<0.012||abs(mF.y-0.55)<0.01||abs(mF.y-0.63)<0.008);
			if(giz)
				mPoe(vec3(0.9));
			}
		}
	if(mEm(MP_ESCUDO,0.006))
		{
		float d=mRet(MP_ESCUDO);
		mClaro(d,vec3(0.93));
		if(d<0.0&&d>-0.25)
			mPoe(vec3(0.80,0.12,0.12));                               // friso vermelho
		vec2 q=mL(vec2(0.955,0.895));
		float r=length(q);
		if(r<1.4)
			{
			float t=r/1.4;
			mPoe(t>0.78?vec3(0.8,0.1,0.12):(t>0.57?vec3(0.95):(t>0.38?vec3(0.8,0.1,0.12):vec3(0.10,0.25,0.65))));
			float a=atan(q.y,q.x);
			if(t<0.38&&t<0.17+0.17*(0.5+0.5*cos(5.0*(a-1.5708))))
				mPoe(vec3(0.97));                                     // estrela
			}
#if MUNDO_NIVEL>=3
		if(mF.x<0.938&&abs(fract((mF.y-0.857)/0.01875)-0.5)*1.33<0.3)
			mPoe(vec3(0.05));                                         // losangos pretos na lateral
#endif
		}
	if(mF.x>0.972&&mF.y>0.791&&mF.y<0.857)
		{
		mPoe(vec3(0.95));                                             // pôster
		if(exHash(floor(mC*3.3))>0.86)
			mPoe(0.5+0.5*cos(6.283*(exHash(floor(mC*3.3)+1.0)+vec3(0.0,0.33,0.67))));
		}
	float re=length(mL(vec2(0.945,0.808)));
	if(re<0.4)
		mPoe(re<0.2?vec3(0.1):vec3(0.85));                             // expositor
	}
void mFundoF()
	{
	/* lado F perto da janela: xadrez 2, placa do miniauditório, banner cyberpunk, bancada de eletrônica,
	   arco branco, bola de pilates e as 2 cadeiras */
	mXadrez(vec4(0.296,0.416,0.659,0.940));
	if(mF.x<0.281&&mF.y>0.575&&mF.y<0.625)
		mPoe(abs(mF.y-0.600)*mK.y<0.15?vec3(0.95):vec3(0.05));        // placa do miniauditório
	if(mEm(MP_BANC_F,0.004))
		{
		mPinta(mRet(MP_BANC_F),vec3(0.07));
		vec2 q=mL(vec2(0.292,0.700));
		if(abs(q.x)<0.8&&abs(q.y)<0.8)
			mPoe(vec3(0.6,0.45,0.28)*(abs(q.x)<0.05?0.8:1.0));      // caixa de papelão
		mMonitor(vec2(0.300,0.800),3.1416,mBit(0.0));
		vec2 c=floor(mC/0.8);
		if(mF.y>0.72&&mF.y<0.78&&exHash(c)>0.86&&length(fract(mC/0.8)-0.5)<0.2)
			mPoe(0.5+0.5*cos(6.283*(exHash(c+2.0)+vec3(0.0,0.33,0.67))));  // pecinhas
		}
	if(mF.x<0.282&&mF.y>0.709&&mF.y<0.841)
		{
		vec2 c=floor(mC*1.5);
		float h=exHash(c);
		mPoe(h>0.9?vec3(1.0,0.85,0.3):(h>0.8?vec3(0.3,0.9,1.0):vec3(0.12,0.15,0.25))); // banner cyberpunk
		}
	/* arco branco: sombra, barra e os pés (com faixa vermelha e preta) */
	if(mF.x>0.34&&mF.x<0.385&&mF.y>0.69&&mF.y<0.86)
		{
		vec2 a=vec2(0.360,0.709)*mK,b=vec2(0.360,0.841)*mK;
		if(mSeg(mC-vec2(0.25,-0.25),a,b)<0.4)
			mG*=0.75;
		if(mSeg(mC,a,b)<0.4)
			{
			if(mBit(9.0)>0.5)
				{mPoe(vec3(0.5,0.95,1.0)*0.7); mE=vec3(0.5,0.95,1.0)*0.4;}
			else
				mClaro(-0.01,vec3(0.97));
			}
		else if(mSeg(mC,a,b)<0.6)
			mG*=0.6;
		for(int k=0;k<2;++k)
			{
			vec2 q=mL(vec2(0.360,k==0?0.709:0.841));
			if(abs(q.x)<1.1&&abs(q.y)<0.8)
				mPoe(abs(q.x-0.5)<0.15?vec3(0.85,0.1,0.1):(abs(q.x-0.15)<0.15?vec3(0.05):vec3(0.97)));
			else if(abs(q.x)<1.35&&abs(q.y)<1.05)
				mG*=0.6;
			}
		}
	vec2 q=mL(MP_BOLA.xy);
	float r=length(q);
	if(r<MP_BOLA.z)
		mPoe(vec3(0.55,0.56,0.58)+vec3(0.25)*smoothstep(0.8,0.0,length(q-vec2(-0.5,0.5))));
	else if(r<MP_BOLA.z+0.25)
		mG*=0.6;
	if(mF.x<0.34&&mF.y>0.75&&mF.y<0.83)
		{
		mCadeira(vec2(0.318,0.775),3.1416,vec3(0.13));
		mCadeira(vec2(0.318,0.807),3.1416,vec3(0.13));
		}
	}
void mIlha(vec4 r,float div,float xe,float xo,vec3 ligaE,vec3 ligaO)
	{
	/* ilha de mesas: 3 lugares de cada lado (Fy 0,643 / 0,725 / 0,808), com 2 monitores, teclado e cadeira;
	   leste vira para pi, oeste para 0. ligaE/ligaO: lugar com membro sentado (monitores acesos), por lugar;
	   -1 = o lugar tem desenho próprio (o do Victor) */
	if(!mEm(r,0.03))
		return;
	float k=clamp(floor((mF.y-0.643)/0.0825+0.5),0.0,2.0);
	float yl=0.643+0.0825*k;
	float d=mRet(r);
	mClaro(d,vec3(0.92,0.91,0.88));
	if(d<0.0)
		{
		if(abs(mF.x-div)*mK.x<0.15)
			mPoe(vec3(0.6));                                          // divisória
		if(length(abs(mL(vec2(r.x+r.y,r.z+r.w)*0.5))-vec2(r.y-r.x,r.w-r.z)*0.5*mK+0.4)<0.3)
			mPoe(vec3(0.05));                                         // pés
		}
	for(int j=0;j<2;++j)
		{
		bool leste=j==0;
		float xs=leste?xe:xo;
		float a=leste?3.1416:0.0;
		if(abs(mF.x-xs)>0.10)
			continue;
		mCadeira(vec2(xs,yl),a,vec3(0.13));
		vec3 lg=leste?ligaE:ligaO;
		float liga=k<0.5?lg.x:(k<1.5?lg.y:lg.z);
		if(liga<-0.5)
			continue;
		float xm=leste?div+0.006:div-0.006;
		mMonitor(vec2(xm,yl-0.0156),a,liga);
		mMonitor(vec2(xm,yl+0.0156),a,liga);
		mTeclado(vec2(leste?xs-0.030:xs+0.030,yl),a,vec3(0.22));
		}
	}
void mVictor()
	{
	/* o lugar do Victor (ilha 1, leste, perto da janela): 3 monitores, teclado RGB no mousepad, caixinhas
	   de som, garrafa, copo térmico, PC torre e a plaquinha */
	vec2 q=mL(vec2(0.845,0.808));
	if(abs(q.x)<0.9&&abs(q.y)<1.9)
		mPoe(vec3(0.12));                                             // mousepad
	if(abs(q.x)<0.4&&abs(q.y)<1.3)
		{
		vec3 rgb=0.5+0.5*cos(6.283*(fract(mC.y*0.3-mT*0.25)+vec3(0.0,0.33,0.67)));
		mPoe(rgb*0.3);
		mE=rgb*(mBit(13.0)>0.5?0.8:0.3)*(0.85+0.15*step(0.5,fract(q.y*2.5))*step(0.1,abs(q.x)));
		}
	for(int k=0;k<3;++k)
		{
		float fk=float(k)-1.0;
		vec2 c=vec2(0.824,0.808+0.026*fk);
		vec2 m=exGira(mL(c),3.1416-0.26*fk);
		if(abs(m.x)<0.2&&abs(m.y)<1.0)
			{
			mPoe(vec3(0.06,0.065,0.08));
			if(m.x<-0.03&&abs(m.y)<0.88)
				{
				if(mBit(13.0)>0.5)
					mE=fract(m.y*2.2+floor(mT*2.0)*0.37)<0.55?vec3(0.25,0.9,0.35)*0.8:vec3(0.05,0.15,0.07);  // terminal rolando
				else if(mBit(0.0)>0.5)
					mE=vec3(0.2,0.45,0.85)*0.6;
				}
			}
		}
	if(mBit(13.0)<0.5&&mBit(0.0)<0.5)
		mLed(vec2(0.826,0.795),vec3(1.0,0.55,0.1),1.0);
	for(int k=0;k<2;++k)
		{
		vec2 c=mL(vec2(0.834,k==0?0.794:0.822));
		if(abs(c.x)<0.25&&abs(c.y)<0.25)
			mPoe(vec3(0.05));                                         // caixinhas de som
		}
	float g=length(mL(vec2(0.847,0.770)));
	if(g<0.3)
		mPoe(vec3(0.3,0.55,0.85)*(1.2-g));                            // garrafa
	g=length(mL(vec2(0.850,0.846)));
	if(g<0.35)
		mPoe(g<0.2?vec3(0.2):vec3(0.05));                              // copo térmico
	vec2 p=mL(vec2(0.832,0.851));
	if(abs(p.x)<0.9&&abs(p.y)<0.45)
		mPoe(vec3(0.06)+vec3(0.1)*step(0.7,fract(p.x*2.0)));           // PC torre
	vec2 pl=mL(vec2(0.8555,0.795));
	if(abs(pl.x)<0.15&&abs(pl.y)<0.6)
		mPoe(abs(pl.x)<0.05&&abs(pl.y)<0.45?vec3(0.05):vec3(0.97));   // plaquinha
	}
void mFundoB()
	{
	/* mesas das persianas, mesa em L do canto, extintor */
	if(mEm(MP_PERSIANAS,0.03))
		{
		for(int k=0;k<3;++k)
			mCadeira(vec2(0.455+0.0375*float(k),0.870),1.5708,vec3(0.13));
		float d=mRet(MP_PERSIANAS);
		mClaro(d,vec3(0.92,0.91,0.88));
		if(d<0.0)
			{
			float k=clamp(floor((mF.x-0.455)/0.0375+0.5),0.0,2.0);
			mMonitor(vec2(0.455+0.0375*k,0.935),1.5708,0.0);
			vec2 t=mL(vec2(0.455+0.0375*k,0.905));
			if(abs(t.x)<1.0&&abs(t.y)<0.3)
				mPoe(vec3(0.22));
			}
		}
	if(mF.x>0.85&&mF.y>0.80)
		{
		mCadeira(vec2(0.870,0.895),0.0,vec3(0.13));
		mCadeira(vec2(0.915,0.825),1.5708,vec3(0.13));
		float d=min(mRet(MP_MESA_L1),mRet(MP_MESA_L2));
		mClaro(d,vec3(0.92,0.91,0.88));
		if(d<0.0)
			{
			mMonitor(vec2(0.886,0.897),0.0,0.0);
			mMonitor(vec2(0.915,0.862),1.5708,0.0);
			vec2 q=mL(vec2(0.924,0.905));
			if(abs(q.x)<0.5&&abs(q.y)<0.8)
				mPoe(vec3(0.06));
			}
		vec2 q=mL(vec2(0.881,0.955));
		float r=length(q);
		if(r<1.0)
			mPoe(r<0.7?vec3(0.85,0.1,0.1)*(1.15-0.3*r):(r<1.0&&r>0.7?vec3(0.05):mG)); // extintor com a mangueira
		}
	}
void mPrincipal()
	{
	mPoe(vec3(0.70,0.71,0.70));
#if MUNDO_NIVEL>=3
	if(mBit(0.0)>0.5||mDia<0.5)
		{
		vec2 g=vec2(fract((mF.x-0.275)/0.1175),fract((mF.y-0.147)/0.2753));
		mG+=vec3(0.035)*smoothstep(0.5,0.0,length((g-0.5)*vec2(1.0,0.6)))*mBit(0.0); // luminárias
		}
	if(mF.y>0.201&&mF.y<0.312&&abs(mC.x-(64.0+0.6*sin(mC.y*1.3)))<0.15)
		mPoe(vec3(0.05));                                              // cabo preto no chão
#endif
	/* janelas: vidro e persianas (dia/noite) */
	if(mF.y>0.962)
		{
		float x=mF.x;
		if((x>0.296&&x<0.360)||(x>0.881&&x<0.952))
			{
			vec3 c=mix(vec3(0.06,0.08,0.16),vec3(0.62,0.82,0.95),mDia);
			mPoe(c);
			if(mDia<0.5&&exHash(floor(mC*0.8))>0.97)
				mE=vec3(1.0,0.85,0.4)*0.6;                             // luzes da cidade à noite
			}
		else if(x>0.360&&x<0.881)
			mPoe(abs(fract((x-0.360)/0.0744+0.5)-0.5)*7.44<0.15?vec3(0.6):mix(vec3(0.28,0.30,0.36),vec3(0.92,0.92,0.90),mDia));
		else
			mPoe(vec3(0.85));
		}
	if(mF.y<0.31)
		{
		if(mF.x<0.41)
			mCantoFH();
		else
			mBancadaH();
		}
	if(mF.x<0.41&&mF.y>0.28&&mF.y<0.47)
		mScanner();
	if(mF.x>0.36&&mF.y>0.26&&mF.y<0.50)
		mCluster();
	if(mF.x>0.93)
		mParedeC();
	if(mF.y>0.5)
		{
		if(mF.x<0.43)
			mFundoF();
		else if(mF.x<0.72)
			mIlha(MP_ILHA2,0.610,MP_A5.x,MP_A6.x,vec3(0.0,mBit(18.0),0.0),vec3(mBit(19.0),0.0,0.0)); // A5 (leste, meio) e A6 (oeste, embaixo)
		else
			{
			mIlha(MP_ILHA1,0.818,MP_A3.x,MP_A4.x,vec3(mBit(20.0),0.0,-1.0),vec3(0.0,0.0,mBit(17.0)));  // A7 e o Victor (leste), A4 (oeste, em cima)
			if(mF.x>0.81&&mF.x<0.89&&mF.y>0.75&&mF.y<0.87)
				mVictor();
			}
		if(mF.x>0.42)
			mFundoB();
		}
	}

/* ---------------------------------------------------------- a planta toda */
vec3 mundoCor(vec2 u)
	{
	/* u = cm da areia desde o canto da área iluminada */
	mK=vec2(100.0,100.0*extrasMundo.z/extrasMundo.y);
	mF=u/extrasMundo.yz;
	mC=mF*mK;
	mT=extrasInfo.x;
	mBits=extrasMundo.x;
	mDia=smoothstep(5.3,6.8,extrasMundo2.z)*(1.0-smoothstep(17.5,19.2,extrasMundo2.z));
	mG=vec3(0.70,0.71,0.70);
	mE=vec3(0.0);
	float luz=1.0;
	bool parede=mF.y<MP_PAREDE_H_Y||mF.y>MP_PAREDE_B_Y||mF.x<MP_EXTERNA_X||mF.x>MP_PAREDE_C_X
	            ||(mF.x>MP_PAREDE_F_X0&&mF.x<MP_PAREDE_F_X1)||(mF.x<MP_PAREDE_F_X0&&mF.y>MP_DIVISORIA_Y0&&mF.y<MP_DIVISORIA_Y1);
	if(mF.y<MP_CORREDOR_Y)
		{
		mCorredor();
		luz=mix(0.85,0.95,mDia);
		}
	else if(parede)
		{
		mParede();
		luz=mix(0.6,1.0,max(mDia,mBit(0.0)));
		}
	else if(mF.x<MP_PAREDE_F_X0)
		{
		if(mF.y<MP_DIVISORIA_Y0)
			{
			mForense();
			luz=mBit(1.0)>0.5?1.0:mix(0.35,0.55,mDia);                 // sem janela: vazia, fica escura
			}
		else
			{
			mMini();
			luz=mBit(2.0)>0.5?(mBit(4.0)>0.5?0.75:0.92):mix(0.32,0.50,mDia); // apresentando, a luz cai
			}
		}
	else
		{
		mPrincipal();
		luz=mix(0.40,1.0,max(0.92*mDia,mBit(0.0)))+0.07*mDia*smoothstep(0.80,0.97,mF.y);
		}
	return mG*luz+mE;
	}
)GLSL";

const char* Extras::glslFragmentMain=R"GLSL(
	{
	/* Projetor-LAOB: efeito do tema, lago, tesouro e animais sobre o relevo */
	vec3 ep=extrasPos;
	float et=extrasInfo.x;
	extrasMarca=0.0;
	extrasLimpa=0.0;
	extrasAguaSombra=0.0;
	vec4 exAntes=baseColor;     // para a transição suave quando a brincadeira muda
	float ef=extrasEfeito.x;
	float ref=extrasEfeito.y;
	if(extrasEfeito.w>0.5)
		{
		/* Jogo do vulcão: nada de lava de enfeite (não dava para ver onde estava a lava de
		   verdade); o relevo fica escuro, como rocha à noite, e só a lava do jogo brilha */
		float cinza=dot(baseColor.rgb,vec3(0.3,0.5,0.2));
		baseColor=vec4(mix(vec3(cinza),baseColor.rgb,0.45)*0.5,1.0);
		}
	else if(extrasJogoInfo.x>0.5)
		{
		/* Jogos de água (corrida, vila, rota, labirinto): tira a água do mapa para não
		   confundir os jogadores. As valas e cotas baixas viram leito seco de terra/areia,
		   deixando apenas a água que sai das nascentes visível como água */
		if(baseColor.b>baseColor.r+0.08&&baseColor.b>baseColor.g*0.85)
			{
			vec3 leito=mix(vec3(0.22,0.48,0.25),vec3(0.52,0.48,0.34),clamp(baseColor.b*1.2-0.2,0.0,1.0));
			baseColor=vec4(leito,1.0);
			}
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
	/* (o lago dos bichos não é mais desenhado aqui: é a água de verdade da simulação, pintada pelo
	   SurfaceAddWaterColor com o mesmo visual, junto com a chuva) */
	if(extrasCopiaInfo.x>0.5)
		{
		/* Copie o relevo (17/11/2026, pedido do Victor: estava confuso e feio): a areia fica com o relevo
		   normal do tema, sem nada por cima; no canto só a barra com a porcentagem de acerto. O alvo fica
		   no meio da areia e o painel mostra a miniatura dele. */
		float nota=mod(extrasCopiaInfo.w,1000.0);
		bool feito=extrasCopiaInfo.w>=1000.0;
		vec2 pb=ep.xy-(extrasMundo2.xy+vec2(3.0,2.5));
		if(pb.x>-0.7&&pb.x<31.0&&pb.y>-0.8&&pb.y<2.8)
			{
			vec3 corB=vec3(0.05,0.06,0.09);                              // fundo do quadro
			if(pb.x>0.0&&pb.x<22.0&&pb.y>0.0&&pb.y<2.0)
				{
				corB=vec3(0.16,0.17,0.22);                               // trilho vazio
				if(pb.x<nota*0.22)
					corB=feito?mix(vec3(1.0,0.78,0.2),vec3(1.0,0.97,0.7),0.5+0.5*sin(et*6.0)):
					     mix(mix(vec3(0.92,0.25,0.18),vec3(0.98,0.78,0.15),smoothstep(25.0,55.0,nota)),vec3(0.25,0.86,0.38),smoothstep(55.0,85.0,nota));
				if(abs(pb.x-18.7)<0.1)
					corB=vec3(0.9);                                      // marca dos 85%
				}
			/* número: até 3 algarismos e o % */
			float n=floor(nota+0.5);
			float c0=floor(n/100.0),c1=floor(mod(n,100.0)/10.0),c2=mod(n,10.0);
			float cel=0.5;
			vec2 t=vec2(pb.x-23.0,2.25-pb.y)/cel;                        // coluna, linha (0 = em cima)
			float tinta=0.0;
			if(c0>0.5) tinta=max(tinta,exDigito(c0,t));
			if(c0>0.5||c1>0.5) tinta=max(tinta,exDigito(c1,t-vec2(4.0,0.0)));
			tinta=max(tinta,exDigito(c2,t-vec2(8.0,0.0)));
			tinta=max(tinta,exDigito(10.0,t-vec2(12.0,0.0)));
			if(tinta>0.5)
				corB=feito?vec3(1.0,0.9,0.4):vec3(1.0);
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
	/* Mundo LAOB: o chão deixa de ser o relevo colorido e vira o laboratório por dentro, visto de cima, com as
	   três salas (mundoCor, nas funções do começo do shader; planta em MundoPlanta.h, a mesma do Mundo.cpp) */
	if(extrasMundo.w>0.5)
		baseColor=vec4(mundoCor(vec2(ep.x-extrasMundo2.x,ep.y-extrasMundo2.y)),1.0);
	/* Jogos de água (Jogos.cpp) e labirinto: peças desenhadas antes dos bichos (eles passam por cima) */
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
					vec3 cc=corpo?vec3(0.96,0.9,0.74):vec3(0.78,0.2,0.14);
					if(porta)
						cc=vec3(0.45,0.28,0.15);
					if(est>1.5)
						cc=mix(cc,vec3(0.2,0.45,0.9),0.55+0.15*sin(et*4.0+p.x*3.0));
					c=cc*mix(1.0,0.7,smoothstep(0.85,1.1,max(abs(p.x)/1.1,0.0)));
					}
				else if(est>0.5&&est<1.5&&d<r*1.5)
					c=mix(c,vec3(0.3,0.95,0.4),smoothstep(r*1.5,r*1.1,d)*0.6);
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
			baseColor.rgb=c;
			}
		}
	/* Comida jogada pelo painel (18/11/2026): um pão no meio e migalhas em volta, vistos de cima, com sombra
	   e contorno escuro (lê em qualquer tema). As migalhas somem uma a uma conforme o tempo acaba e, no fim,
	   o pão vai diminuindo. Longe dela não faz conta nenhuma. (A mão não desenha nada: o anel em volta
	   dela foi tirado a pedido do Victor em 17/11/2026.) */
	if(extrasComida.w>0.0)
		{
		vec2 qc=ep.xy-extrasComida.xy;
		float rc=extrasComida.z;
		if(dot(qc,qc)<(rc+0.8)*(rc+0.8))
			{
			float resta=extrasComida.w;                                  // 1 = acabou de cair .. 0 = acabou
			const vec2 sm=vec2(0.25,-0.25);                               // sombra: luz do mesmo canto dos bichos
			/* pão: bisnaga (cápsula) girada ao acaso, com três cortes na casca */
			float giroP=exHash(extrasComida.xy)*3.1416;
			vec2 eixo=vec2(cos(giroP),sin(giroP)),lado=vec2(-eixo.y,eixo.x);
			float tamP=0.45+0.55*smoothstep(0.0,0.1,resta);             // no fim o pão vai sendo comido
			vec2 pp=vec2(dot(qc,eixo),dot(qc,lado))/tamP;
			vec2 ps=vec2(dot(qc-sm,eixo),dot(qc-sm,lado))/tamP;
			float dP=exCaixa(pp,vec2(1.9,1.0),0.95)*tamP;                // < 0 dentro do pão (cm)
			float sombra=smoothstep(0.35,-0.15,exCaixa(ps,vec2(1.9,1.0),0.95)*tamP);
			/* migalhas: 10 em volta do pão, cada uma com lugar, tamanho, jeito e tom sorteados pela posição
			   da comida; a de número maior some primeiro */
			float dM=1e3,tomM=0.0;
			for(int k=0;k<10;++k)
				{
				float fk=float(k);
				float lo=(fk+1.0)/11.0;
				float vive=smoothstep(lo-0.03,lo+0.02,resta);
				if(vive<=0.0)
					continue;
				vec2 sem=extrasComida.xy+vec2(fk*3.7,fk*1.3);
				float h1=exHash(sem),h2=exHash(sem+vec2(5.1,2.3)),h3=exHash(sem+vec2(1.7,8.9));
				float ak=fk*2.39996+h1*0.9;                                   // ângulo de ouro: espalhadas sem amontoar
				vec2 cm=vec2(cos(ak),sin(ak))*rc*(0.5+0.42*h2);
				float tk=(0.22+0.22*h3)*vive;
				float ca=cos(h1*6.2832),sa=sin(h1*6.2832);
				vec2 dq=qc-cm;
				dq=vec2(ca*dq.x+sa*dq.y,-sa*dq.x+ca*dq.y)/vec2(1.0,0.7);  // um pouco achatadas, cada uma num rumo
				float dk=length(dq)-tk;
				if(dk<dM)
					{
					dM=dk;
					tomM=h2;
					}
				sombra=max(sombra,smoothstep(tk*0.4,-tk*0.6,length(qc-sm-cm)-tk)*0.8);
				}
			vec3 cc=mix(baseColor.rgb,baseColor.rgb*0.4,sombra*0.65);
			vec3 contorno=vec3(0.2,0.1,0.04);
			if(dM<0.0)
				{
				vec3 miolo=mix(vec3(0.84,0.56,0.25),vec3(0.98,0.86,0.58),tomM); // casca ou miolo
				cc=dM>-0.09?contorno:miolo*(0.85+0.15*smoothstep(-0.25,0.0,-dM));
				}
			if(dP<0.0)
				{
				/* casca dourada, mais tostada na beira; cortes claros em diagonal; brilho de um lado */
				vec3 casca=mix(vec3(0.62,0.33,0.11),vec3(0.93,0.66,0.3),smoothstep(0.0,0.75,-dP));
				float corte=0.0;
				for(int k=0;k<3;++k)
					{
					float xk=(float(k)-1.0)*1.05;
					corte=max(corte,smoothstep(0.15,0.06,abs(pp.x-xk-0.45*pp.y))*step(abs(pp.y),0.62));
					}
				casca=mix(casca,vec3(0.99,0.9,0.66),corte*0.9);
				casca+=vec3(0.14,0.1,0.05)*smoothstep(1.0,0.0,length(pp-vec2(-0.7,0.45)));
				cc=dP>-0.1?contorno:casca;
				}
			if(dM<0.0||dP<0.0)
				extrasMarca=1.0; // caiu na água: fica à vista, boiando
			baseColor.rgb=cc;
			}
		}
	/* Peixes e carrinhos (vistos de cima, com sombra): */
	float tamBichos=extrasMundo.w>0.5?1.0:1.12; // 19/11/2026: bichos 12% maiores (os bonecos do laboratório não); TAM_BICHOS no passo()
	for(int i=0;i<32;++i)
		{
		vec4 a=extrasAg[i];
		if(a.w<0.5)
			continue;
		vec2 r=(ep.xy-a.xy)/tamBichos;                                 // (o desenho é feito em "cm do bicho")
		if(dot(r,r)>49.0)
			continue;
		float marcaAntes=extrasMarca;                                  // para saber o que ESTE bicho pintou (realce no fim)
		vec3 exFundo=baseColor.rgb;                                    // o chão que ESTE bicho cobre (destaque no fim)
		extrasMarca=0.0;
		bool esteira=false;                                            // rastro e respingo: não levam o realce do corpo
		float c=cos(a.z);
		float s=sin(a.z);
		vec2 l=vec2(c*r.x+s*r.y,-s*r.x+c*r.y);
		float hue=fract(a.w);
		vec3 cor=vec3(0.5)+0.5*cos(6.2832*(vec3(hue)+vec3(0.0,0.33,0.67)));   // (antes 0,55 + 0,45: cores mais vivas)
		/* Realce adaptativo de contraste dos seres vivos contra os temas */
		float temaClaro=(ef>1.5&&ef<2.5)?1.0:((ef>3.5&&ef<4.5)?0.6:0.0);
		float temaEscuro=(extrasEfeito.w>0.5||(ef>0.5&&ef<1.5))?1.0:0.0;
		if(ef>2.5&&ef<3.5)
			cor=mix(cor,vec3(cor.b,cor.g*1.15,cor.r*0.65+0.35),0.45); // Marte: destaca da poeira vermelha
		if(ef>1.5&&ef<2.5)
			cor=mix(cor,cor*0.82+vec3(0.06,0.12,0.22),0.35);          // Inverno: saturação e contraste na neve
		vec2 rs=r-vec2(0.4,-0.4);                                     // sombra: luz vinda do canto, igual para todos
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
					pc*=mix(1.0,temaClaro>0.5?0.28:0.42,smoothstep(0.72,1.0,corpo));              // contorno (mais firme desde 19/11)
					pc+=vec3(0.22)*temaEscuro*smoothstep(0.75,0.98,corpo);
					vec2 olho=vec2(1.45,0.42*sign(f.y));
					float dOlho=distance(f,olho);
					if(dOlho<0.24)
						pc=dOlho<0.12?vec3(0.03):vec3(0.97);
					}
				else
					pc=cor*0.7+vec3(0.05);                                        // cauda e nadadeiras
				baseColor=vec4(pc,1.0);
				}
			else
				{
				if(sombra<1.0)
					baseColor.rgb*=0.6;
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
					cb=corH*mix(1.15,0.8,cabP)*mix(1.0,0.55,smoothstep(0.8,1.0,cabP));                  // (com contorno)
					if(distance(p,vec2(1.3,0.24*sign(p.y)))<0.08)
						cb=vec3(0.02);
					}
				else if(corpoP<1.0)
					{
					cb=corC*mix(1.15,0.72,corpoP)*mix(1.0,0.55,smoothstep(0.85,1.0,corpoP));            // (com contorno)
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
					esteira=true;
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
					if(abs(p.y)>meia-0.16)
						cb=vec3(0.24,0.14,0.07);                                                           // borda
					if(fract(a.w)>0.5&&abs(p.y)>meia-0.3&&abs(p.y)<=meia-0.14)
						cb=vec3(0.75,0.15,0.15);                                                         // faixa colorida
					}
				else
					{
					float v=smoothstep(0.13,0.0,abs(abs(p.y)-(-p.x-1.6)*0.45))*step(p.x,-1.6)*smoothstep(-6.0,-1.7,p.x)
					        *(0.5+0.5*sin(p.x*2.6+et*5.0));
					esteira=true;
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
						cb*=0.6;
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
					cb*=0.64;
				else if(mergulho&&e2.w>0.45)
					{
					/* tocou a água: respingo e ondinhas */
					float t2=(e2.w-0.45)/0.55;
					float onda=smoothstep(0.18,0.0,abs(length(r)-(0.4+t2*2.6)))*(1.0-t2)
					          +smoothstep(0.15,0.0,abs(length(r)-(0.2+t2*1.5)))*(1.0-t2)*0.7;
					esteira=true;
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
				float abana=senta?0.55*sin(et*24.0+fase):0.35*sin(et*12.0+fase);                         // sentado do lado da comida: abana muito
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
					cb=pelo*mix(1.12,0.85,cabD/0.42)*mix(1.0,0.6,smoothstep(0.34,0.42,cabD));              // (com contorno)
					if(distance(p,vec2(1.2,0.17*sign(p.y)))<0.07)
						cb=vec3(0.02);
					}
				else if(orelha<1.0)
					cb=pelo*0.65;
				else if(corpoD<1.0)
					{
					cb=pelo*mix(1.12,0.75,corpoD)*mix(1.0,0.6,smoothstep(0.84,1.0,corpoD));                // (com contorno)
					if(tom>=0.6&&tom<0.8&&exPontos(p*2.2+vec2(fase),0.3,0.45)>0.5)
						cb=vec3(0.15);                                                                   // manchas
					}
				else if(rabo)
					cb=pelo*0.85;
				else if(pata<0.15)
					cb=pelo*0.7;
				else if(length(sl/vec2(1.15,0.55))<1.0)
					cb*=0.6;                                                                            // sombra
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
					cb*=mix(1.0,temaClaro>0.5?0.32:0.46,smoothstep(0.74,1.0,corpoT));
					cb+=vec3(0.2)*temaEscuro*smoothstep(0.75,0.98,corpoT);
					}
				else if(caudaT||peitoral)
					cb=vec3(0.2,0.26,0.34);
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
					cb*=mix(1.0,temaClaro>0.5?0.3:0.45,smoothstep(0.92,1.0,casco));
					cb+=vec3(0.2)*temaEscuro*smoothstep(0.9,0.98,casco);
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
					cb=mix(cb,corAv+vec3(0.3)*smoothstep(1.0,0.2,dAv),0.68+0.3*smoothstep(0.75,1.0,dAv));
					cb=mix(cb,corAv*0.55,smoothstep(0.86,1.0,dAv));                                       // beirada da sombrinha
					cb+=vec3(0.35)*smoothstep(0.22,0.0,abs(dAv-0.55))*(0.5+0.5*sin(atan(l.y,l.x)*8.0));     // desenho em trevo dentro da sombrinha
					}
				else if(dAv<1.5)
					{
					cb=mix(cb,corAv,0.25*smoothstep(1.5,1.0,dAv)*(0.6+0.4*sin(et*2.6+fase)));
					esteira=true;                                                                        // (o brilho em volta é o fundo: sem realce)
					desenhou=fio>0.05||dAv<1.5;
					}
				else if(fio>0.05)
					cb=mix(cb,corAv*0.85,fio*0.85);
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
					cb=corP*mix(1.2,0.7,smoothstep(0.2,1.0,dP))*mix(1.0,0.6,smoothstep(0.86,1.0,dP));     // (com contorno)
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
					cb=mix(cb,vec3(0.85,0.87,0.85),smoothstep(0.86,0.94,losango)*0.7);                       // beirada clara
					cb=mix(cb,vec3(0.12,0.12,0.16),smoothstep(0.95,1.0,losango));                            // e o contorno escuro por fora
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
			float vis=extrasMundo.w>0.5?floor(extrasAg2[i].w+0.5):0.0;          // membro com visual próprio (copiaNomes; 0 = sorteado)
			if(vis>0.5)
				porte=1.6;
			if(extrasMundo.w>0.5)
				porte*=0.8;                                                    // 19/11/2026: no laboratório de 3 salas, 20% menores (antes 15% maiores):
				                                                               // grandes, tapavam a mesa, o scanner e o cluster (medido nas prévias)
			vec2 pl=l/porte;
			vec2 pls=ls/porte;
			float passo=extrasAg2[i].x>0.5&&extrasAg2[i].x<1.5?0.0:sin(et*7.0+float(i)*1.3); // parada olhando o pão: pés juntos
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
			if(vis>0.5)
				{
				/* Membros do laboratório com visual próprio (19/11/2026, pedido do Victor: "colocar chapéu no do
				   Gazziro e deixar eles mais diferenciáveis"): silhueta e cores fixas, tiradas das fotos, para
				   reconhecer cada um de cima e de longe, sem depender da plaquinha. Contorno escuro em volta
				   (o piso da sala é cinza).
				   1 Gazziro: fedora grafite escuro com fita preta (como nas fotos f14/f15 do miniauditório); camisa verde-água de botão.
				   2 Lucas: boné grafite com a aba para a frente; camiseta cinza mesclada.
				   3 Mario: cabelo castanho-claro aloirado, óculos (aro na frente e hastes) e jaleco branco comprido.
				   4 Victor: cabelo castanho-escuro bagunçado; camisa preta de gola. */
				bool g1=vis<1.5,g2=vis>1.5&&vis<2.5,g3=vis>2.5&&vis<3.5,g4=vis>3.5;
				vec3 peleM=g1?vec3(0.86,0.66,0.52):(g2?vec3(0.93,0.76,0.63):(g3?vec3(0.96,0.79,0.69):vec3(0.92,0.75,0.62)));
				vec3 cabeloM=g3?vec3(0.6,0.4,0.21):vec3(0.17,0.11,0.07);
				vec3 roupa=g1?vec3(0.58,0.86,0.78):(g2?vec3(0.47,0.49,0.52):(g3?vec3(0.97):vec3(0.1,0.1,0.11)));
				vec2 hp=pl-vec2(0.08,0.0);                                       // centro da cabeça
				float angC=atan(hp.y,hp.x);
				float rCab=g4?0.53+0.08*abs(sin(angC*5.0+0.4)):0.52;            // Victor: cabelo arrepiado
				float dCab=cabeca-rCab;
				if(g1)
					dCab=cabeca-0.92;                                            // aba do chapéu
				if(g2)
					dCab=min(cabeca-0.55,max((length((hp-vec2(0.5,0.0))/vec2(0.48,0.42))-1.0)*0.42,0.34-hp.x)); // boné com aba
				if(g3)
					dCab=min(dCab,max(abs(hp.x-0.55)-0.08,abs(hp.y)-0.47));      // aro dos óculos, na frente do rosto
				float casaco=length((pl-vec2(-0.24,0.0))/vec2(0.84,1.04));       // jaleco: desce atrás dos ombros
				float dTronco=(ombros-1.0)*0.55;
				if(g3)
					dTronco=min(dTronco,(casaco-1.0)*0.84);
				float dBraco=min(bracoE,bracoD)-0.27;
				float dPe=(min(peE,peD)-1.0)*0.24;
				float dM=min(min(dCab,dTronco),min(dBraco,dPe));
				if(g1)
					sombra=min(sombra,length(pls-vec2(0.08,0.0))/0.92);         // a aba também faz sombra
				if(g3)
					sombra=min(sombra,length((pls-vec2(-0.24,0.0))/vec2(0.9,1.1)));
				vec3 cM=vec3(0.0);
				if(dCab<0.0)
					{
					if(g1)
						{
						float copa=length(hp/vec2(0.5,0.42));
						vec3 feltro=vec3(0.27,0.265,0.255);                       // grafite (19/11/2026: era marrom; as fotos mostram escuro)
						if(copa<1.0)
							{
							cM=feltro*(1.2-0.35*copa)+vec3(0.13,0.13,0.12)*smoothstep(0.4,0.0,distance(hp,vec2(-0.12,0.14)));
							if(abs(hp.y)<0.05&&abs(hp.x)<0.3)
								cM*=0.62;                                         // vinco no alto da copa
							}
						else if(copa<1.22)
							cM=vec3(0.05,0.045,0.045);                            // fita
						else
							cM=feltro*mix(0.9,0.68,smoothstep(0.6,0.84,cabeca))+vec3(0.1)*smoothstep(0.82,0.9,cabeca); // aba, com a beirada virada (mais clara)
						}
					else if(g2)
						{
						if(cabeca<0.55)
							{
							cM=vec3(0.25,0.26,0.29)*(1.18-0.38*cabeca/0.55);
							if(abs(fract(angC*6.0/6.2832+0.5)-0.5)<0.06)
								cM*=0.72;                                         // costuras dos gomos
							if(cabeca<0.08)
								cM=vec3(0.42,0.43,0.46);                          // botão no alto
							}
						else
							cM=mix(vec3(0.15,0.16,0.18),vec3(0.3,0.31,0.34),step(0.0,hp.x-0.86+0.6*hp.y*hp.y)); // aba (pesponto na beirada)
						}
					else if(g3&&hp.x>0.47)
						cM=abs(hp.y)<0.08||abs(hp.y)>0.38||hp.x>0.6?vec3(0.08,0.07,0.07):vec3(0.72,0.86,0.94); // óculos: aro e lentes
					else
						{
						float franja=g4?0.3+0.07*sin(hp.y*24.0):0.32;
						vec3 topo=cabeloM*(0.8+0.4*smoothstep(0.5,0.0,cabeca))+vec3(0.22)*smoothstep(0.2,0.0,distance(hp,vec2(-0.12,0.16)));
						if(g4)
							topo*=0.85+0.3*step(0.5,fract(angC*2.4+cabeca*7.0));           // mechas
						if(g3)
							topo*=0.88+0.18*smoothstep(0.2,0.8,abs(fract(hp.y*7.0+0.3*hp.x)*2.0-1.0)); // penteado para trás
						cM=hp.x>franja&&cabeca<0.52?peleM*(1.05-0.25*smoothstep(0.3,0.52,cabeca)):topo;
						cM*=mix(1.0,0.75,smoothstep(0.46,0.52,cabeca));
						if(g3&&abs(abs(hp.y)-0.45)<0.045&&hp.x>-0.12)
							cM=vec3(0.08,0.07,0.07);                              // hastes dos óculos
						}
					}
				else if(dTronco<0.0)
					{
					float ro=g3?min(ombros,casaco):ombros;
					cM=roupa*mix(1.12,0.78,smoothstep(0.3,1.0,ro));
					if(g2)
						cM*=0.9+0.2*exHash(floor(pl*16.0));                   // mesclada
					if(g4)
						cM+=vec3(0.1)*smoothstep(0.75,1.0,ombros);            // brilho nos ombros (preto não some)
					if(g1&&pl.x>0.35&&abs(pl.y)<0.035)
						cM*=0.75;                                             // carreira de botões
					if(g3&&pl.x<-0.45&&abs(pl.y)<0.03)
						cM=vec3(0.7,0.72,0.75);                               // costura nas costas do jaleco
					if(cabeca<0.66&&hp.x>-0.15)
						cM=g3?(hp.x>0.3?vec3(0.62,0.76,0.93):roupa*0.84):(g4?vec3(0.24):roupa*0.82); // gola (no jaleco, a camisa azul aparece na frente)
					}
				else if(dBraco<0.0)
					cM=min(bracoE,bracoD)<0.15?peleM:roupa*(g4?1.6:0.85);          // manga e mão
				else if(dPe<0.0)
					cM=g1||g3?vec3(0.33,0.2,0.11):vec3(0.1,0.1,0.11);             // sapatos
				if(dM<0.0)
					baseColor=vec4(cM,1.0);
				else if(dM<0.075)
					baseColor=vec4(0.06,0.06,0.08,1.0);                           // contorno
				else if(sombra<1.0)
					baseColor.rgb*=0.6;
				}
			else if(enfeite<0.4&&cabeca<(enfeite<0.25?0.56:0.86))
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
				baseColor.rgb*=0.6;
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
				cc*=mix(1.0,temaClaro>0.5?0.32:0.48,smoothstep(-0.16,0.0,carro));                    // contorno
				cc+=vec3(0.22)*temaEscuro*smoothstep(-0.08,0.0,carro);
				baseColor=vec4(cc,1.0);
				}
			else if(rodas)
				baseColor=vec4(length(roda)<0.14?vec3(0.6):vec3(0.08),1.0);                         // pneu com calota
			else
				{
				if(sombra<0.0)
					baseColor.rgb*=0.58;
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
		if(extrasMundo.w<0.5)
			{
			/* Destaque (19/11/2026, pedido do Victor: "deixar os seres vivos mais diferentes do ambiente, sem
			   deixar muito discrepante"). Sem mexer no desenho de cada espécie:
			   - uma silhueta aproximada (caixa arredondada, em "cm do bicho") diz onde fica a beirada;
			   - sob o corpo e logo em volta somem as curvas de nível (extrasLimpa, no patch_surface.py);
			   - se o corpo tem a luminância do chão (ou da água) embaixo, ele escurece por igual no chão claro
			     ou médio e clareia no chão escuro (o matiz e o desenho continuam);
			   - em volta, sombra de contato macia (no chão escuro, um clarão leve); na água a sombra vai para
			     o SurfaceAddWaterColor (extrasAguaSombra), porque a água cobre o chão. Rastros, respingos e as
			     sombras de cada bicho ficam como estão. */
			vec2 sc=vec2(0.0);                                             // centro, meia-caixa (cm do bicho)
			vec2 sm=vec2(1.7,0.55);
			vec3 cRep=cor;                                                 // cor típica do corpo
			bool voando=false;
			if(a.w<2.0)       { sc=vec2(-0.6,0.0); sm=vec2(2.9,1.05); }                                    // peixe
			else if(a.w<3.0)  { sm=vec2(2.45,1.3); }                                                       // carrinho
			else if(a.w<4.0)  { sm=vec2(1.0,1.75); cRep=mix(cor,vec3(0.3,0.22,0.15),0.4); }                 // pessoa (cabelo no meio)
			else if(a.w<5.0)  { sc=vec2(-0.9,0.0); sm=vec2(3.5,1.15); cRep=vec3(0.33,0.4,0.47); }           // tubarão
			else if(a.w<6.0)  { sc=vec2(0.3,0.0); sm=vec2(2.7,2.0); cRep=vec3(0.3,0.38,0.2); }              // tartaruga
			else if(a.w<7.0)  { sc=vec2(-0.8,0.0); sm=vec2(2.2,1.35); cRep=vec3(0.85,0.6,0.9); }            // água-viva
			else if(a.w<8.0)  { sm=vec2(1.7); cRep=vec3(0.75,0.33,0.45); }                                  // polvo (cabeça e começo dos braços)
			else if(a.w<9.0)  { sm=vec2(2.0,2.3); cRep=vec3(0.4,0.38,0.38); }                               // arraia
			else if(a.w<10.0)
				{
				float escP=max(0.3,extrasAg2[i].z);
				sc=vec2(0.3,0.0)*escP; sm=vec2(1.7,0.85)*escP;
				cRep=escP<0.9?vec3(1.0,0.86,0.3):(hue<0.45?vec3(0.95,0.95,0.9):vec3(0.5,0.45,0.35));       // patinho, pato branco, marreco
				}
			else if(a.w<11.0) { sc=vec2(0.2,0.0); sm=vec2(1.85,0.8); cRep=vec3(0.7,0.55,0.4); }             // barquinho
			else if(a.w<12.0)
				{
				voando=!(extrasAg2[i].x>0.5&&extrasAg2[i].x<1.5);                                         // voando: lá no alto, sem sombra de contato
				sm=voando?vec2(1.0,1.6):vec2(1.6,0.55);
				cRep=hue<0.55?vec3(0.92):vec3(0.12,0.15,0.28);                                               // gaivota ou andorinha
				}
			else
				{
				float tomD=fract(hue*5.3);
				sm=vec2(1.75,0.62);
				cRep=tomD<0.35?vec3(0.78,0.52,0.25):(tomD<0.6?vec3(0.15,0.12,0.1):(tomD<0.8?vec3(0.95,0.93,0.88):vec3(0.55,0.55,0.57)));  // cachorro
				}
			float lRep=dot(cRep,vec3(0.3,0.59,0.11));
			float dS=exCaixa(l-sc,sm,min(sm.x,sm.y));                      // < 0 dentro da silhueta
			float dH=exCaixa(mix(l,ls,0.5)-sc,sm,min(sm.x,sm.y));          // a mesma, meio caminho da sombra (luz do canto)
			vec3 qd=baseColor.rgb/max(exFundo,vec3(0.01));
			bool sombraB=max(max(qd.r,qd.g),qd.b)-min(min(qd.r,qd.g),qd.b)<0.03&&qd.g<0.75;     // só escureceu o chão por igual
			bool mudou=dot(abs(baseColor.rgb-exFundo),vec3(1.0))>0.004;
			bool corpoB=(a.w<2.0||a.w>=4.0)?(extrasMarca>0.5&&!esteira&&!sombraB):(mudou&&!sombraB&&dS<0.05);
			/* o que se vê embaixo: o chão ou, dentro do lago, a água (mesmas cores do patch_agua.py com o lago ligado) */
			bool molhado=extrasInfo.w>-999.0&&ep.z<extrasInfo.w-0.05;
			vec3 chao=exFundo;
			if(molhado)
				{
				vec3 raso=vec3(0.14,0.56,0.93);
				vec3 prof=vec3(0.02,0.11,0.42);
				if(ef>1.5&&ef<2.5)      { raso=vec3(0.20,0.65,0.96); prof=vec3(0.05,0.28,0.65); }
				else if(ef>2.5&&ef<3.5) { raso=vec3(0.18,0.72,0.82); prof=vec3(0.05,0.35,0.48); }
				else if(ef>3.5&&ef<4.5) { raso=vec3(0.16,0.78,0.90); prof=vec3(0.02,0.40,0.52); }
				else if(ef>5.5&&ef<6.5) { raso=vec3(0.6); prof=vec3(0.33); }
				chao=mix(raso,prof,clamp((extrasInfo.w-ep.z)/2.0,0.0,1.0))+vec3(0.03);
				}
			float lf=dot(chao,vec3(0.3,0.59,0.11));
			float lado=smoothstep(0.16,0.3,lf)*2.0-1.0;                    // +1 chão claro ou médio (escurece), -1 chão escuro (clareia)
			float falta=1.0-smoothstep(0.15,0.42,exDifCor(cRep,chao));     // 1 = corpo da cor do chão
			float v=falta*0.5;                                             // quanto o corpo escurece (ou clareia)
			float lB=lRep*(1.0-v*max(lado,0.0))+(1.0-lRep)*v*max(-lado,0.0);
			if(corpoB)
				{
				/* metade pela cor típica (o bicho todo muda junto) e metade pela cor deste pixel (o chapéu da
				   cor da areia escurece mais que a camisa colorida) */
				vec3 cb=baseColor.rgb;
				float vP=0.5*mix(falta,1.0-smoothstep(0.15,0.42,exDifCor(cb,chao)),0.5);
				baseColor.rgb=cb*(1.0-vP*max(lado,0.0))+(1.0-cb)*vP*max(-lado,0.0);
				extrasLimpa=1.0;
				}
			else if(!mudou&&!voando)
				{
				float hS=1.0-smoothstep(-0.5,1.1,dH);
				hS*=hS;                                                    // macia: some a ~1 cm da beirada
				/* no chão claro a sombra só ajuda o corpo mais claro que o chão (o contorno escuro do bicho já se
				   vê, e o corpo que acabou de escurecer não pode afundar junto na sombra); no chão escuro o
				   contorno some, então o clarão ajuda todos, mais ainda o corpo escuro */
				float ajuda=mix(0.5+0.5*smoothstep(-0.1,0.1,lf-lB),smoothstep(-0.1,0.1,lB-lf),smoothstep(-0.3,0.3,lado));
				float escuro=(0.08+0.32*ajuda)*hS;
				float agua=escuro*lado*falta;                               // na água, só quando o bicho tem a cor da água
				if(molhado&&abs(agua)>abs(extrasAguaSombra))
					extrasAguaSombra=agua;                                 // > 0 escurece a água, < 0 clareia
				baseColor.rgb=baseColor.rgb*(1.0-escuro*max(lado,0.0))+(1.0-baseColor.rgb)*0.6*escuro*max(-lado,0.0);
				extrasLimpa=max(extrasLimpa,0.8*(1.0-smoothstep(0.0,0.7,dS)));
				}
			}
		if(extrasMarca>0.5&&!esteira)
			{
			/* Realce (19/11/2026): o corpo do bicho um pouco mais saturado; e o da água já vem "contra" o azul
			   que a água ainda passa por cima dele (25% da água, no SurfaceAddWaterColor: a cor estimada da água
			   funda de cada tema é descontada antes), para não ficar lavado */
			vec3 cr=baseColor.rgb;
			cr=clamp(mix(vec3(dot(cr,vec3(0.3,0.59,0.11))),cr,1.2),0.0,1.0);
			if(a.w<2.0||(a.w>=4.0&&a.w<11.0))
				{
				vec3 aguaEst=extrasEfeito.w>0.5?vec3(0.82,0.24,0.06):(ef>1.5&&ef<2.5)?vec3(0.11,0.38,0.74):(ef>2.5&&ef<3.5)?vec3(0.11,0.45,0.58):
				             (ef>3.5&&ef<4.5)?vec3(0.08,0.51,0.63):(ef>5.5&&ef<6.5)?vec3(0.38):vec3(0.07,0.23,0.55);
				cr=clamp((cr-0.21*aguaEst)/0.79,0.0,1.0);
				}
			baseColor.rgb=cr;
			}
		extrasMarca=max(extrasMarca,marcaAntes);
		}
	/* Ração na água (dedo apontado ou painel, 18/11/2026): bolinhas marrons boiando, com contorno escuro e
	   um halo claro fino (lê em todos os temas, por cima da água e dos bichos que comem embaixo); ao cair, um
	   respingo; a cada beliscada, uma ondinha e a bolinha encolhe. Fora da caixa que envolve todas elas
	   não faz conta nenhuma. */
	vec4 rcx=extrasRacao[0];
	if(ep.x>rcx.x&&ep.y>rcx.y&&ep.x<rcx.z&&ep.y<rcx.w)
		{
		float dB=1e3,anel=0.0;
		vec2 qB=vec2(0.0);
		for(int k=1;k<25;++k)
			{
			vec4 r=extrasRacao[k];
			if(r.z<0.0)
				continue;
			vec2 q=ep.xy-r.xy;
			if(dot(q,q)>4.0) // 2 cm: o maior respingo
				continue;
			float d=length(q);
			float tam=floor(r.w)/20.0;                                   // 1 inteira .. 0 acabou (comida ou desmanchou)
			float mord=fract(r.w);                                       // segundos desde a beliscada (0,99 = nenhuma)
			if(r.z<0.7)
				anel=max(anel,smoothstep(0.13,0.0,abs(d-0.3-1.5*r.z/0.7))*(1.0-r.z/0.7));        // respingo ao cair
			if(mord<0.5)
				anel=max(anel,0.85*smoothstep(0.1,0.0,abs(d-0.3-0.9*mord/0.5))*(1.0-mord/0.5)); // ondinha da beliscada
			if(tam<=0.0)
				continue;
			float fk=float(k);
			vec2 bob=0.06*vec2(sin(et*2.1+fk*1.7),cos(et*1.7+fk*2.3));     // balança de leve na água
			float rb=0.4*tam*(mord<0.2?0.8+mord:1.0)*min(1.0,0.6+r.z*2.0); // encolhe na beliscada; cresce ao cair
			float db=length(q-bob)-rb;
			if(db<dB)
				{
				dB=db;
				qB=(q-bob)/rb;
				}
			}
		if(anel>0.3) // anel nítido de espuma (a água por cima deixaria um anel fraco da cor da areia)
			{
			baseColor.rgb=mix(vec3(0.7,0.86,1.0),vec3(1.0),smoothstep(0.3,1.0,anel));
			extrasMarca=1.0;
			}
		if(dB<0.08)
			{
			extrasMarca=1.0;
			if(dB>0.0)
				baseColor.rgb=vec3(0.96,0.93,0.84);                                          // halo (temas escuros)
			else if(dB>-0.08)
				baseColor.rgb=vec3(0.2,0.1,0.03);                                            // contorno
			else
				{
				/* ração: marrom, mais clara no alto (luz do mesmo canto dos bichos), com um brilho */
				vec3 rc=mix(vec3(0.84,0.58,0.3),vec3(0.46,0.26,0.1),smoothstep(0.0,1.3,length(qB-vec2(-0.35,0.35))));
				rc+=vec3(0.3,0.26,0.2)*smoothstep(0.3,0.0,length(qB-vec2(-0.35,0.38)));
				baseColor.rgb=rc;
				}
			}
		}
	/* Chuva pelo painel: a nuvem chega crescendo, deriva de leve, as gotas caem em riscos e respingam no
	   chão; no fim ela se desfaz aos poucos (antes aparecia e sumia de uma vez, com gotas em faixas) */
	for(int k=0;k<4;++k)
		{
		vec4 ch=extrasChuva[k];
		if(ch.z<0.01)
			continue;
		float f=ch.z;
		vec2 centro=ch.xy+vec2(0.35*sin(ch.w*0.9+float(k)),0.25*sin(ch.w*0.7+float(k)*2.0));
		vec2 qn=ep.xy-centro;
		if(dot(qn,qn)>42.0)
			continue;
		/* gotas: cada coluna de 0,7 cm tem a sua velocidade e fase; riscos curtos caindo para "baixo" da imagem */
		float col=floor(qn.x/0.7);
		float vel=3.0+1.5*exHash(vec2(col,float(k)));
		float fase=fract((qn.y+ch.w*vel*2.2)/2.3+exHash(vec2(col,7.0+float(k))));
		float risco=smoothstep(0.16,0.0,abs(fract(qn.x/0.7)-0.5)*0.7)*smoothstep(0.0,0.12,fase)*smoothstep(0.42,0.18,fase);
		float dentroR=smoothstep(3.8,2.4,length(qn*vec2(1.0,0.9)))*step(qn.y,0.6);
		baseColor.rgb=mix(baseColor.rgb,vec3(0.72,0.88,1.0),risco*dentroR*0.75*f);
		/* respingos: anéis pequenos que abrem onde as gotas chegam */
		vec2 cr=floor((qn+vec2(4.0))/1.6);
		float tr=fract(ch.w*1.3+exHash(cr+float(k)*3.1));
		vec2 pr=(cr+vec2(0.3+0.4*exHash(cr+vec2(1.0,9.0)),0.3+0.4*exHash(cr+vec2(5.0,2.0))))*1.6-vec2(4.0);
		float anelR=smoothstep(0.08,0.0,abs(distance(qn,pr)-tr*0.55))*(1.0-tr)*smoothstep(3.6,2.2,length(qn));
		baseColor.rgb=mix(baseColor.rgb,vec3(0.85,0.95,1.0),anelR*0.6*f);
		/* nuvem: três bolas fofas com sombra embaixo; cresce ao chegar e se desfaz no fim */
		float esc=0.75+0.25*f;
		vec2 qc=(qn-vec2(0.0,1.2))/esc;
		float nuvem=min(min(length((qc-vec2(-1.3,0.0))/vec2(1.5,1.1)),length((qc-vec2(1.2,-0.1))/vec2(1.6,1.2))),
		                length((qc-vec2(0.0,0.6))/vec2(1.7,1.3)));
		float sombraN=smoothstep(1.25,0.9,length((qc-vec2(0.15,-0.55))/vec2(2.6,1.2)));
		baseColor.rgb*=1.0-0.18*sombraN*f;
		float cobreN=smoothstep(1.0,0.86,nuvem)*f;
		if(cobreN>0.01)
			{
			vec3 corN=mix(vec3(1.0),vec3(0.74,0.79,0.88),smoothstep(0.2,1.0,nuvem)+0.25*smoothstep(0.3,-0.6,qc.y));
			baseColor.rgb=mix(baseColor.rgb,corN,cobreN*0.95);
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
			vec2 rr=ep.xy-(a.xy+cima*6.3);
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
		vec2 rel=ep.xy-(a.xy+cima*2.6); // (19/11/2026: 2,7 -> 3,1 com os bonecos 15% maiores; 2,6 com eles 20% menores, nas 3 salas)
		vec2 t=vec2(dot(rel,dir),dot(rel,cima));
		float px=0.2;
		float cw=4.0*px;
		float larg=len*cw-px;
		float margem=pres>0.5?0.95:0.35; // espaço para a bolinha verde
		if(extrasMundo.w>0.5)
			{
			/* 19/11/2026 (revisão): no laboratório, perto da borda (pôster da forense, tela do miniauditório)
			   a plaquinha corre para dentro da área iluminada em vez de perder o começo do nome */
			float c0=dot(extrasMundo2.xy,dir),c1=dot(extrasMundo2.xy+extrasMundo.yz,dir),cd=dot(a.xy,dir);
			t.x+=cd-clamp(cd,min(c0,c1)+larg*0.5+margem+0.3,max(c0,c1)-larg*0.5-0.65);
			}
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
		ag[i*4+0]=ax[i];
		ag[i*4+1]=ay[i];
		ag[i*4+2]=ang[i];
		ag[i*4+3]=visivel?tipo[i]:0.0f;
		}
	glUniform4fvARB(locations[1],MAX_AGENTES,ag);
	glUniform4fARB(locations[2],tx,ty,traio,tz);
	/* .y: 0 = não achado; achado = 1 + segundos desde então (a animação do baú abrindo usa o tempo) */
	glUniform4fARB(locations[3],GLfloat(tempo),achado?1.0f+GLfloat(std::min(600.0,tempo-quandoAchou)):0.0f,
	               tesouro?GLfloat(tipoTesouro):0.0f,(jogo!=SEM_JOGO||labirinto)?SEM:nivelAgua);
	glUniform4fARB(locations[4],GLfloat(efeito),referencia,0.0f,
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
			char ch=(modo==MUNDO&&i<nNomes&&len==c)?nomes[i][c]:0;
			if(ch>='A'&&ch<='Z')
				{
				nm[i*9+1+c]=GLfloat(LETRAS[ch-'A']);
				len=c+1;
				}
			else
				nm[i*9+1+c]=0.0f;
			}
		bool daqui=modo==MUNDO&&i<nNomes;
		nm[i*9]=GLfloat(len+((daqui&&presente[i])?16:0)+((daqui&&tempo<rostoAte[i])?32:0)); // +16 presente, +32 mostra o rosto
		}
	glUniform1fvARB(locations[9],MAX_NOMES*9,nm);
	bool comNomes=modo==MUNDO;
	glUniform4fARB(locations[10],comNomes?GLfloat(nNomes):0.0f,GLfloat(giroNomes),
	               comNomes?GLfloat(nFotos):0.0f,0.0f);
	GLfloat ch[MAX_CHUVAS*4];
	for(int k=0;k<MAX_CHUVAS;++k)
		{
		ch[k*4]=chuvas[k].x; ch[k*4+1]=chuvas[k].y; ch[k*4+2]=chuvaForca(k); ch[k*4+3]=chuvas[k].desde>=0.0?GLfloat(tempo-chuvas[k].desde):0.0f;
		}
	glUniform4fvARB(locations[14],MAX_CHUVAS,ch);
	bool sala=modo==MUNDO&&mundoPronto;
	glUniform4fARB(locations[12],sala?mundoBits:0.0f,bmax[0]-bmin[0],bmax[1]-bmin[1],sala?1.0f:0.0f); // .x: estado (bits; Mundo.cpp)
	/* hora local (0-24): o laboratório escurece à noite e as luzes de dentro acendem */
	float hora=horaFixa;
	if(hora<0.0f)
		{
		std::time_t agora=std::time(0);
		std::tm* lt=std::localtime(&agora);
		hora=lt!=0?float(lt->tm_hour)+float(lt->tm_min)/60.0f:12.0f;
		}
	glUniform4fARB(locations[13],bmin[0],bmin[1],hora,
	               GLfloat(std::max(0.0,std::min(1.0,(tempo-trocaTempo)/0.6)))); // .w: transição (0 começando .. 1 pronta)
	glUniform4fARB(locations[15],copia?1.0f:0.0f,copiaTol,copiaRef,copiaNota+(copiaConseguiu>0.0?1000.0f:0.0f)); // o shader só usa .x e .w
	GLfloat a2[MAX_AGENTES*4];
	for(int i=0;i<MAX_AGENTES;++i)
		{
		a2[i*4+0]=GLfloat(acao[i]);
		a2[i*4+1]=voo[i];
		a2[i*4+2]=escala[i];
		a2[i*4+3]=acao[i]==2?std::min(1.0f,acaoT[i]/1.8f):0.0f;
		if(modo==MUNDO&&i<nNomes)
			a2[i*4+3]=GLfloat(visual[i]); // no laboratório ninguém mergulha: o .w leva o visual do membro
		}
	glUniform4fvARB(locations[16],MAX_AGENTES,a2);
	/* comida: só enquanto não acabou (o desenho some sozinho com a fração que falta) */
	float resta=(querComida()&&comida.dura>0.0&&tempo<comida.ate)?float((comida.ate-tempo)/comida.dura):0.0f;
	glUniform4fARB(locations[17],comida.x,comida.y,comida.r,resta);
	/* ração: só as bolinhas que já caíram e ainda aparecem (comidas mostram a ondinha por mais 0,6 s) */
	GLfloat rr[(MAX_RACAO+1)*4];
	float rx0=1e9f,ry0=1e9f,rx1=-1e9f,ry1=-1e9f;
	for(int k=0;k<MAX_RACAO;++k)
		{
		const Racao& r=racao[k];
		GLfloat* v=rr+(k+1)*4;
		v[0]=r.x;
		v[1]=r.y;
		v[2]=-1.0f;
		v[3]=0.0f;
		if(!querComida()||tempo<r.desde||tempo>=r.ate)
			continue;
		float tam=r.mordidas<=0?0.0f:std::min(1.0f,float(r.ate-tempo)/2.0f)*(r.mordidas==1?0.75f:1.0f); // desmancha nos últimos 2 s
		v[2]=GLfloat(std::min(9.0,tempo-r.desde));
		v[3]=GLfloat(std::floor(tam*20.0f+0.5f)+std::min(0.99,std::max(0.0,tempo-r.mordida)));
		rx0=std::min(rx0,r.x-2.1f);
		ry0=std::min(ry0,r.y-2.1f);
		rx1=std::max(rx1,r.x+2.1f);
		ry1=std::max(ry1,r.y+2.1f);
		}
	rr[0]=rx0; rr[1]=ry0; rr[2]=rx1; rr[3]=ry1; // sem nenhuma: caixa vazia (min > max), o shader nem olha
	glUniform4fvARB(locations[18],MAX_RACAO+1,rr);
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
		bool vale=(jogo!=SEM_JOGO||labirinto)&&i<nPecas;
		GLfloat* v=jg+i*8;
		v[0]=p.x; v[1]=p.y; v[2]=p.raio; v[3]=vale?GLfloat(p.tipo):0.0f;
		v[4]=GLfloat(p.estado); v[5]=0.0f; v[6]=GLfloat(p.ordem);
		v[7]=GLfloat(p.time);
		}
	glUniform4fvARB(locations[7],MAX_PECAS*2,jg);
	glUniform4fARB(locations[8],GLfloat(jogo!=SEM_JOGO&&jogoFase>0?jogo:(labirinto&&nPecas>0?99:0)),GLfloat(jogoFase),
	               GLfloat(jogoFase==2?tempo-jogoFim:tempo-jogoInicio),
	               jogo==ROTA?GLfloat(proximoPortao):GLfloat(jogoVencedor));
	}
