/***********************************************************************
Jogos de água da caixa de areia (Projetor-LAOB), na linha do labirinto:
a água é a da simulação do SARndbox (fontes = discos que jogam água) e a
GPU mede, 2x por segundo, quanta água há em cada peça. Aqui ficam o
sorteio das peças e as regras de cada jogo; o desenho está no shader
(Extras.cpp) e a medição no Sandbox (patch_sandbox.py).

  CORRIDA  duas nascentes e duas bandeiras (azul e vermelha): ganha o time
           cuja bandeira a água alcança primeiro
  VILA     chuva forte de um lado; diques e valas para as casas não alagarem
  FAZENDA  plantações secam com o tempo; água nelas as recupera
  ROTA     a água passa pelos portões na ordem (1, 2, 3...) até a bandeira
  LAGO     encher o lago marcado até a faixa certa (água limitada)
  VULCAO   lava desce do pico mais alto; muralhas protegem as casas
  BARCO    o barquinho segue a correnteza até o porto
  RESGATE  peixes presos em poças que secam; canais levam água até eles
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
const float SEM_MEDIDA=-1000.0f;
const float PREPARO=10.0f; // vila e vulcão: segundos para levantar os diques antes da água
const float MOLHADO=0.3f; // cm de água que contam como "a água chegou"
const char* NOMES[Extras::NUM_JOGOS]={"nada","corrida","vila","fazenda","rota","lago","vulcao","barco","resgate"};
std::string fmt(const char* f,int a=0,int b=0)
	{
	char s[200];
	std::snprintf(s,sizeof(s),f,a,b);
	return s;
	}
}

int Extras::jogoDoNome(const char* nome)
	{
	/* Em 14/11/2026 o Victor tirou fazenda, lago, barquinho e resgate (repetitivos); o código
	   deles continua aqui, mas só estes quatro podem ser pedidos: */
	static const int ATIVOS[]={CORRIDA,VILA,ROTA,VULCAO};
	for(int k=0;k<4;++k)
		if(std::string(nome)==NOMES[ATIVOS[k]])
			return ATIVOS[k];
	return -1;
	}

const char* Extras::nomeDoJogo(int j)
	{
	return j>=0&&j<NUM_JOGOS?NOMES[j]:"nada";
	}

void Extras::pedeJogo(int qual,int nivel)
	{
	labirinto=false; // um jogo por vez (o labirinto é o "jogo" mais antigo)
	labEstado=0;
	jogo=qual;
	jogoNivel=std::max(1,std::min(3,nivel));
	jogoRodadas=0;
	jogoPedido=true;
	jogoFase=0;
	nPecas=nFontes=0;
	if(mapaOk)
		novoJogo();
	}

void Extras::fimDoJogo(void)
	{
	if(jogo!=SEM_JOGO)
		labZerarAgua=true; // a água do jogo vai embora junto
	jogo=SEM_JOGO;
	jogoFase=0;
	jogoPedido=false;
	nPecas=nFontes=0;
	}

void Extras::addPeca(int tipo,float x,float y,float raio,int time,int ordem)
	{
	if(nPecas>=MAX_PECAS)
		return;
	Peca& p=pecas[nPecas++];
	p.tipo=tipo;
	p.x=x;
	p.y=y;
	p.raio=raio;
	p.time=time;
	p.ordem=ordem;
	p.estado=0;
	p.nivel=1.0f;
	p.ang=0.0f;
	}

void Extras::addFonte(float x,float y,float vazao)
	{
	if(nFontes>=MAX_FONTES)
		return;
	fontes[nFontes].x=x;
	fontes[nFontes].y=y;
	fontes[nFontes].vazao=vazao;
	fontes[nFontes].ativa=true;
	++nFontes;
	addPeca(P_NASCENTE,x,y,3.0f);
	}

bool Extras::pontoLivre(float& x,float& y,float margem,float distMin,float hMin,float hMax,int tentativas)
	{
	for(int t=0;t<tentativas;++t)
		{
		x=bmin[0]+margem+aleatorio()*(bmax[0]-bmin[0]-2.0f*margem);
		y=bmin[1]+margem+aleatorio()*(bmax[1]-bmin[1]-2.0f*margem);
		float h=altura(x,y);
		if(h<SEM_MEDIDA+1.0f)
			continue;
		/* nas primeiras tentativas respeita a faixa de altura pedida; depois aceita qualquer altura */
		if(t<tentativas/2&&(h<hMin||h>hMax))
			continue;
		bool longe=true;
		for(int i=0;i<nPecas&&longe;++i)
			longe=(pecas[i].x-x)*(pecas[i].x-x)+(pecas[i].y-y)*(pecas[i].y-y)>=distMin*distMin;
		if(longe)
			return true;
		}
	return false;
	}

void Extras::novoJogo(void)
	{
	jogoPedido=false;
	nPecas=nFontes=0;
	jogoFase=1;
	jogoInicio=tempo;
	jogoPontos=0;
	jogoVencedor=-1;
	proximoPortao=1;
	vbx=vby=0.0f;
	labZerarAgua=true; // rodada nova começa sem água
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],m=5.0f;
	const int n=jogoNivel;
	const float r=referencia;
	float x,y;
	/* lado sorteado para a(s) nascente(s): 0 esquerda, 1 direita, 2 baixo, 3 cima */
	int lado=int(aleatorio()*4.0f)%4;
	auto naBorda=[&](int l,float f,float& px,float& py)
		{
		if(l<2)
			{
			px=l==0?bmin[0]+m:bmax[0]-m;
			py=bmin[1]+m+f*(h-2.0f*m);
			}
		else
			{
			py=l==2?bmin[1]+m:bmax[1]-m;
			px=bmin[0]+m+f*(w-2.0f*m);
			}
		};
	const int oposto[4]={1,0,3,2};
	switch(jogo)
		{
		case CORRIDA:
			{
			/* azul em cima, vermelho embaixo (ou esquerda/direita); bandeiras do outro lado */
			int l=lado,o=oposto[lado];
			float lev=n==1?0.55f:1.0f; // fácil: bandeira no meio do caminho
			for(int t=0;t<2;++t)
				{
				float f=t==0?0.22f:0.78f;
				float sx,sy,bx,by;
				naBorda(l,f,sx,sy);
				naBorda(o,f,bx,by);
				/* pouca água: com 0,9 ela atravessava a caixa sozinha e a rodada acabava em segundos */
				addFonte(sx,sy,n==1?0.4f:n==2?0.32f:0.25f);
				addPeca(P_ALVO,sx+(bx-sx)*lev,sy+(by-sy)*lev,n==1?5.0f:4.0f,t);
				}
			jogoDuracao=240.0f; // sem vencedor em 4 min: empate (a água não enche a caixa para sempre)
			anuncia("Jogo: corrida de rios (azul x vermelho)");
			break;
			}
		case VILA:
		case VULCAO:
			{
			int casas=n+2; // 3, 4 ou 5
			if(jogo==VULCAO)
				{
				/* a lava nasce no ponto mais alto da areia */
				float melhor=-1e9f,px=0,py=0;
				for(int t=0;t<400;++t)
					{
					float qx=bmin[0]+m+aleatorio()*(w-2.0f*m),qy=bmin[1]+m+aleatorio()*(h-2.0f*m);
					float a=altura(qx,qy);
					if(a>melhor)
						{
						melhor=a;
						px=qx;
						py=qy;
						}
					}
				addFonte(px,py,n==1?0.9f:n==2?1.3f:1.7f);
				jogoDuracao=n==1?40.0f:n==2?55.0f:70.0f;
				for(int i=0;i<casas;++i)
					if(pontoLivre(x,y,7.0f,12.0f,r-4.0f,r+0.5f))
						addPeca(P_CASA,x,y,2.6f);
				anuncia(fmt("Jogo: vulcão em erupção (%d casas); a lava sai em %d s",casas,int(PREPARO)));
				}
			else
				{
				/* a enchente vem de três lados diferentes (o quarto, sorteado, fica seco): com as três
				   nascentes na mesma borda bastava uma barreira só para vencer */
				for(int k=0;k<3;++k)
					{
					naBorda((lado+1+k)%4,0.25f+0.5f*aleatorio(),x,y);
					addFonte(x,y,n==1?0.3f:n==2?0.4f:0.55f); // de três lados, cada uma mais fraca
					}
				jogoDuracao=n==1?40.0f:n==2?55.0f:70.0f;
				for(int i=0;i<casas;++i)
					if(pontoLivre(x,y,7.0f,12.0f,r-1.0f,r+2.0f) /* altura média: no fundo do vale alagavam em segundos */)
						addPeca(P_CASA,x,y,2.6f);
				anuncia(fmt("Jogo: salve a vila (%d casas); a chuva começa em %d s",casas,int(PREPARO)));
				}
			break;
			}
		case FAZENDA:
			{
			naBorda(lado,0.5f,x,y);
			addFonte(x,y,0.8f);
			int plantas=n+2;
			for(int i=0;i<plantas;++i)
				if(pontoLivre(x,y,5.0f,9.0f,r-2.0f,r+2.0f))
					addPeca(P_PLANTA,x,y,2.8f);
			jogoDuracao=90.0f;
			anuncia(fmt("Jogo: irrigue a fazenda (%d plantações)",plantas));
			break;
			}
		case ROTA:
			{
			float sx,sy,bx,by;
			naBorda(lado,0.2f+0.6f*aleatorio(),sx,sy);
			addFonte(sx,sy,0.9f);
			int portoes=n==1?2:n==2?3:4;
			/* portões sorteados pela caixa inteira, longe uns dos outros e da nascente, em ordem
			   também sorteada: o caminho vira um zigue-zague (antes ficavam em fila entre a nascente
			   e a bandeira e bastava um canal reto) */
			for(int k=1;k<=portoes;++k)
				{
				if(!pontoLivre(x,y,7.0f,16.0f,-1e9f,1e9f)&&!pontoLivre(x,y,6.0f,10.0f,-1e9f,1e9f))
					{
					x=bmin[0]+m+aleatorio()*(w-2.0f*m);
					y=bmin[1]+m+aleatorio()*(h-2.0f*m);
					}
				addPeca(P_PORTAO,x,y,3.2f,0,k);
				}
			if(!pontoLivre(bx,by,6.0f,16.0f,-1e9f,1e9f))
				naBorda(oposto[lado],0.2f+0.6f*aleatorio(),bx,by);
			addPeca(P_ALVO,bx,by,4.0f,2);
			jogoDuracao=n==1?180.0f:n==2?240.0f:300.0f;
			anuncia(fmt("Jogo: rota com %d paradas",portoes));
			break;
			}
		case LAGO:
			{
			/* num lugar baixo ou médio (em cima de um morro a água não chegaria) e longe da nascente */
			float sx,sy;
			naBorda(lado,0.5f,sx,sy);
			addFonte(sx,sy,1.0f);
			if(!pontoLivre(x,y,10.0f,18.0f,r-6.0f,r+0.5f))
				{
				x=(bmin[0]+bmax[0])/2;
				y=(bmin[1]+bmax[1])/2;
				}
			addPeca(P_LAGO,x,y,n==1?7.0f:n==2?6.0f:5.0f);
			pecas[nPecas-1].nivel=0.0f;
			jogoDuracao=n==1?25.0f:n==2?30.0f:35.0f; // a nascente joga água só durante esse tempo
			anuncia("Jogo: encha o lago na medida");
			break;
			}
		case BARCO:
			{
			float sx,sy,bx,by;
			naBorda(lado,0.2f+0.6f*aleatorio(),sx,sy);
			naBorda(oposto[lado],0.2f+0.6f*aleatorio(),bx,by);
			addFonte(sx,sy,1.1f);
			/* o barco começa logo abaixo da nascente */
			float dx=bx-sx,dy=by-sy,dl=std::sqrt(dx*dx+dy*dy)+1e-6f;
			addPeca(P_BARCO,sx+dx/dl*6.0f,sy+dy/dl*6.0f,2.6f);
			pecas[nPecas-1].ang=std::atan2(dy,dx);
			float lev=n==1?0.6f:1.0f;
			addPeca(P_PORTO,sx+dx*lev,sy+dy*lev,n==1?5.0f:4.0f);
			jogoDuracao=180.0f;
			anuncia("Jogo: leve o barquinho até o porto");
			break;
			}
		case RESGATE:
			{
			naBorda(lado,0.5f,x,y);
			addFonte(x,y,1.0f);
			int peixes=n+2;
			for(int i=0;i<peixes;++i)
				if(pontoLivre(x,y,5.0f,10.0f,r-3.0f,r+0.5f))
					addPeca(P_PEIXE,x,y,2.4f);
			jogoDuracao=0.0f;
			anuncia(fmt("Jogo: resgate dos peixes (%d peixes)",peixes));
			break;
			}
		default:
			jogo=SEM_JOGO;
			jogoFase=0;
			return;
		}
	if(jogo==VILA||jogo==VULCAO)
		{
		/* placar: uma casinha pequena para cada casa, em fila num canto; fica vermelha quando a
		   casa é perdida e verde quando termina a salvo */
		int casas=nPecas,k=0;
		/* no canto mais longe das nascentes (a água ou a lava cobririam o placar) */
		float melhorD=-1.0f,px0=bmin[0]+4.0f,py0=bmin[1]+3.5f,dir=1.0f;
		for(int c=0;c<4;++c)
			{
			float cx=(c&1)?bmax[0]-4.0f:bmin[0]+4.0f,cy=(c&2)?bmax[1]-3.5f:bmin[1]+3.5f;
			float dmin=1e9f;
			for(int f=0;f<nFontes;++f)
				dmin=std::min(dmin,(fontes[f].x-cx)*(fontes[f].x-cx)+(fontes[f].y-cy)*(fontes[f].y-cy));
			if(dmin>melhorD)
				{
				melhorD=dmin;
				px0=cx;
				py0=cy;
				dir=(c&1)?-1.0f:1.0f;
				}
			}
		for(int i=0;i<casas;++i)
			if(pecas[i].tipo==P_CASA)
				{
				addPeca(P_PLACAR,px0+dir*4.2f*float(k),py0,1.6f,0,i);
				++k;
				}
		}
	jogoTotal=0;
	for(int i=0;i<nPecas;++i)
		if(pecas[i].tipo!=P_NASCENTE&&pecas[i].tipo!=P_PLACAR)
			++jogoTotal;
	}

void Extras::terminaRodada(const std::string& resultado,int vencedor)
	{
	if(jogoFase!=1)
		return;
	jogoFase=2;
	jogoFim=tempo;
	jogoVencedor=vencedor;
	++jogoRodadas;
	for(int k=0;k<nFontes;++k)
		fontes[k].ativa=false;
	anuncia("Jogo: fim da rodada; "+resultado);
	}

void Extras::jogoLeitura(const float* maxima,const float* media,float vx,float vy)
	{
	if(jogo==SEM_JOGO)
		{
		/* Labirinto: a água não pode encostar nas armadilhas (senão recomeça do zero) */
		if(labirinto&&labEstado==1&&tempo-labInicio>1.5)
			for(int i=0;i<nPecas;++i)
				if(pecas[i].tipo==P_PERIGO&&maxima[i]>=MOLHADO)
					{
					pecas[i].estado=1; // pisca forte por 2 s
					labZerarAgua=true;
					labInicio=tempo;
					anuncia("Labirinto: a água tocou numa armadilha! Começa de novo");
					break;
					}
		return;
		}
	if(jogoFase!=1)
		return;
	vbx=vx;
	vby=vy;
	for(int i=0;i<nPecas;++i)
		{
		Peca& p=pecas[i];
		bool molhou=maxima[i]>=MOLHADO;
		switch(p.tipo)
			{
			case P_ALVO:
				if(molhou&&jogo==CORRIDA)
					terminaRodada(p.time==0?"venceu o time azul":"venceu o time vermelho",p.time);
				else if(molhou&&jogo==ROTA&&proximoPortao>0&&
				        std::none_of(pecas,pecas+nPecas,[](const Peca& q){return q.tipo==P_PORTAO&&q.estado!=1;}))
					{
					p.estado=1;
					terminaRodada(fmt("a água passou pelos portões e chegou em %d s",int(tempo-jogoInicio)),0);
					}
				break;
			case P_CASA:
				if(molhou&&p.estado==0)
					{
					p.estado=2;
					anuncia(jogo==VULCAO?"Jogo: a lava pegou uma casa!":"Jogo: uma casa alagou!");
					}
				break;
			case P_PLANTA:
				if(molhou&&p.estado==0)
					p.nivel=1.0f; // regada
				break;
			case P_PORTAO:
				if(molhou&&p.estado==0&&p.ordem==proximoPortao)
					{
					p.estado=1;
					anuncia(fmt("Jogo: a água passou pelo portão %d",p.ordem));
					++proximoPortao;
					}
				break;
			case P_LAGO:
				{
				p.nivel=media[i];
				float lo=jogoNivel==1?0.6f:jogoNivel==2?0.9f:1.2f,hi=jogoNivel==1?2.6f:jogoNivel==2?2.0f:1.7f;
				p.ordem=p.nivel<lo?0:(p.nivel<=hi?1:2); // cor no projetor: falta, na medida, demais
				break;
				}
			case P_PEIXE:
				if(molhou&&p.estado==0)
					{
					p.estado=1;
					++jogoPontos;
					anuncia(fmt("Jogo: um peixe foi salvo! (%d)",jogoPontos));
					}
				break;
			case P_BARCO:
				p.nivel=maxima[i]; // profundidade embaixo do barco (encalhado se rasa)
				break;
			}
		}
	}

void Extras::armadilhasDoLabirinto(void)
	{
	/* Armadilhas no caminho reto entre a nascente e o alvo (2, 3 ou 4), com um desvio pequeno
	   para um lado e para o outro: a água tem que dar a volta. É o que diferencia o labirinto
	   da rota com paradas (lá a água TEM que passar pelos portões; aqui ela NÃO pode encostar). */
	nPecas=0;
	int quantas=labDificuldade+1;
	float dx=ltx-lsx,dy=lty-lsy,dl=std::sqrt(dx*dx+dy*dy)+1e-6f;
	float nx=-dy/dl,ny=dx/dl;
	for(int k=0;k<quantas;++k)
		{
		float f=(float(k)+1.0f)/(float(quantas)+1.0f);
		float lateral=((k&1)?-1.0f:1.0f)*(1.0f+3.0f*aleatorio());
		float x=lsx+dx*f+nx*lateral,y=lsy+dy*f+ny*lateral;
		float ds=(x-lsx)*(x-lsx)+(y-lsy)*(y-lsy),dt2=(x-ltx)*(x-ltx)+(y-lty)*(y-lty);
		if(ds<64.0f||dt2<64.0f||!dentro(x,y,3.0f))
			continue; // colada na nascente, no alvo ou fora da areia: não põe
		addPeca(P_PERIGO,x,y,3.2f);
		}
	}

void Extras::jogoPasso(float dt)
	{
	if(jogo==SEM_JOGO)
		{
		if(labirinto)
			for(int i=0;i<nPecas;++i)
				if(pecas[i].tipo==P_PERIGO&&pecas[i].estado==1&&tempo-labInicio>2.0)
					pecas[i].estado=0;
		return;
		}
	if(jogoPedido)
		{
		if(mapaOk)
			novoJogo();
		return;
		}
	for(int i=0;i<nPecas;++i)
		if(pecas[i].tipo==P_PLACAR&&pecas[i].ordem>=0&&pecas[i].ordem<nPecas)
			pecas[i].estado=pecas[pecas[i].ordem].estado;
	if(jogoFase==2)
		{
		if(tempo-jogoFim>20.0) // 20 s de comemoração e começa outra rodada
			novoJogo();
		return;
		}
	if(jogoFase!=1)
		return;
	double t=tempo-jogoInicio;
	auto contaEstado=[&](int tipo,int estado)
		{
		return int(std::count_if(pecas,pecas+nPecas,[&](const Peca& q){return q.tipo==tipo&&q.estado==estado;}));
		};
	/* Corrida, rota e barquinho: sem chegada até o tempo limite, a rodada acaba (antes a nascente
	   jogava água para sempre e a caixa inteira alagava) */
	if(jogoDuracao>0.0f&&t>=jogoDuracao)
		{
		if(jogo==CORRIDA)
			terminaRodada("empate: o tempo acabou",2);
		else if(jogo==ROTA)
			terminaRodada("o tempo acabou antes de a água chegar",1);
		else if(jogo==BARCO)
			terminaRodada("o tempo acabou: o barquinho não chegou ao porto",1);
		if(jogoFase!=1)
			return;
		}
	switch(jogo)
		{
		case VILA:
		case VULCAO:
			{
			if(jogo==VILA)
				for(int k=0;k<nFontes;++k)
					fontes[k].ativa=t>=PREPARO; // chuva constante: dá para planejar tudo antes
			else
				{
				/* Vulcão: é de reagir. A cratera cospe lava em pulsos (9 s sim, 5 s não) e, durante a
				   erupção, abrem bocas novas em lugares sorteados: 6 s antes aparece o aviso piscando. */
				float te=float(t)-PREPARO;
				fontes[0].ativa=te>=0.0f&&std::fmod(te,14.0f)<9.0f;
				for(int k=1;k<nFontes;++k)
					fontes[k].ativa=true;
				int bocas=jogoNivel; // 1, 2 ou 3 bocas extras
				int abertas=nFontes-1;
				Peca* aviso=std::find_if(pecas,pecas+nPecas,[](const Peca& q){return q.tipo==P_AVISO;});
				float quando=12.0f+16.0f*float(abertas); // segundos de erupção até a próxima boca abrir
				if(abertas<bocas&&nFontes<MAX_FONTES)
					{
					if(aviso==pecas+nPecas&&te>=quando-6.0f&&nPecas<MAX_PECAS)
						{
						float vx,vy;
						if(pontoLivre(vx,vy,8.0f,9.0f,referencia-1.0f,1e9f,300))
							{
							addPeca(P_AVISO,vx,vy,3.0f);
							anuncia("Jogo: a terra tremeu! Uma boca de lava vai abrir");
							}
						}
					else if(aviso!=pecas+nPecas&&te>=quando)
						{
						/* o aviso vira nascente (sem criar outra peça) */
						fontes[nFontes].x=aviso->x;
						fontes[nFontes].y=aviso->y;
						fontes[nFontes].vazao=0.6f;
						fontes[nFontes].ativa=true;
						++nFontes;
						aviso->tipo=P_NASCENTE;
						anuncia("Jogo: abriu uma boca de lava!");
						}
					}
				}
			int casas=contaEstado(P_CASA,0)+contaEstado(P_CASA,2);
			if(t>=PREPARO+jogoDuracao)
				{
				int secas=contaEstado(P_CASA,0);
				for(int i=0;i<nPecas;++i)
					if(pecas[i].tipo==P_CASA&&pecas[i].estado==0)
						pecas[i].estado=1;
				jogoPontos=secas;
				terminaRodada(fmt(jogo==VULCAO?"%d de %d casas a salvo da lava":"%d de %d casas secas",secas,casas),secas==casas?0:1);
				}
			break;
			}
		case FAZENDA:
			{
			float seca=jogoNivel==1?1.0f/45.0f:jogoNivel==2?1.0f/35.0f:1.0f/25.0f;
			for(int i=0;i<nPecas;++i)
				if(pecas[i].tipo==P_PLANTA&&pecas[i].estado==0)
					{
					pecas[i].nivel-=seca*dt;
					if(pecas[i].nivel<=0.0f)
						{
						pecas[i].nivel=0.0f;
						pecas[i].estado=2;
						anuncia("Jogo: uma plantação secou!");
						}
					}
			int vivas=contaEstado(P_PLANTA,0);
			if(t>=jogoDuracao||vivas==0)
				{
				int total=vivas+contaEstado(P_PLANTA,2);
				for(int i=0;i<nPecas;++i)
					if(pecas[i].tipo==P_PLANTA&&pecas[i].estado==0)
						pecas[i].estado=1;
				jogoPontos=vivas;
				terminaRodada(fmt("%d de %d plantações vivas",vivas,total),vivas>0?0:1);
				}
			break;
			}
		case LAGO:
			{
			for(int k=0;k<nFontes;++k)
				fontes[k].ativa=t<jogoDuracao;
			/* a água acabou: espera 10 s para assentar e confere a faixa */
			if(t>=jogoDuracao+10.0f)
				{
				Peca* lago=std::find_if(pecas,pecas+nPecas,[](const Peca& q){return q.tipo==P_LAGO;});
				if(lago!=pecas+nPecas)
					{
					float lo=jogoNivel==1?0.6f:jogoNivel==2?0.9f:1.2f,hi=jogoNivel==1?2.6f:jogoNivel==2?2.0f:1.7f;
					float v=lago->nivel;
					lago->estado=v>=lo&&v<=hi?1:2;
					int mm=int(v*10.0f+0.5f);
					terminaRodada(lago->estado==1?fmt("o lago encheu na medida (%d mm)",mm):
					              v<lo?fmt("faltou água no lago (%d mm)",mm):fmt("o lago transbordou (%d mm)",mm),
					              lago->estado==1?0:1);
					}
				}
			break;
			}
		case RESGATE:
			{
			float perde=jogoNivel==1?1.0f/60.0f:jogoNivel==2?1.0f/45.0f:1.0f/30.0f;
			for(int i=0;i<nPecas;++i)
				if(pecas[i].tipo==P_PEIXE&&pecas[i].estado==0)
					{
					pecas[i].nivel-=perde*dt;
					if(pecas[i].nivel<=0.0f)
						{
						pecas[i].nivel=0.0f;
						pecas[i].estado=2;
						anuncia("Jogo: a poça de um peixe secou!");
						}
					}
			int esperando=contaEstado(P_PEIXE,0);
			if(esperando==0)
				{
				int salvos=contaEstado(P_PEIXE,1),total=salvos+contaEstado(P_PEIXE,2);
				jogoPontos=salvos;
				terminaRodada(fmt("%d de %d peixes salvos",salvos,total),salvos==total?0:1);
				}
			break;
			}
		case BARCO:
			{
			Peca* barco=std::find_if(pecas,pecas+nPecas,[](const Peca& q){return q.tipo==P_BARCO;});
			Peca* porto=std::find_if(pecas,pecas+nPecas,[](const Peca& q){return q.tipo==P_PORTO;});
			if(barco==pecas+nPecas||porto==pecas+nPecas)
				break;
			/* anda com a correnteza (velocidade limitada) só se houver água embaixo */
			float v=std::sqrt(vbx*vbx+vby*vby);
			if(barco->nivel>=0.25f&&v>0.2f)
				{
				float s=std::min(v,7.0f)/v;
				float nx=barco->x+vbx*s*dt,ny=barco->y+vby*s*dt;
				if(dentro(nx,ny,2.0f))
					{
					barco->x=nx;
					barco->y=ny;
					}
				float alvo=std::atan2(vby,vbx),d=alvo-barco->ang;
				while(d>3.14159265f) d-=6.2831853f;
				while(d<-3.14159265f) d+=6.2831853f;
				barco->ang+=std::max(-2.0f*dt,std::min(2.0f*dt,d));
				}
			float dx=barco->x-porto->x,dy=barco->y-porto->y;
			if(dx*dx+dy*dy<=porto->raio*porto->raio)
				{
				barco->estado=porto->estado=1;
				terminaRodada(fmt("o barquinho chegou ao porto em %d s",int(t)),0);
				}
			break;
			}
		default:
			break;
		}
	}
