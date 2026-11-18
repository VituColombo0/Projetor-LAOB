/***********************************************************************
Jogos de água da caixa de areia (Projetor-LAOB), na linha do labirinto:
a água é a da simulação do SARndbox (fontes = discos que jogam água) e a
GPU mede, 2x por segundo, quanta água há em cada peça. Aqui ficam o
sorteio das peças e as regras de cada jogo; o desenho está no shader
(Extras.cpp) e a medição no Sandbox (patch_sandbox.py).

  CORRIDA  duas nascentes e duas bandeiras (azul e vermelha): ganha o time
           cuja bandeira a água alcança primeiro
  VILA     a enchente vem de três lados; diques e valas para as casas não alagarem
  ROTA     a água passa pelos portões na ordem (1, 2, 3...) até a bandeira
  VULCAO   lava desce do pico mais alto; muralhas protegem as casas
(Fazenda, lago, barquinho e resgate foram tirados em 14/11/2026 e o código
deles apagado em 18/11/2026; os números do enum Jogo ficaram reservados.)
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
const float SEM_MEDIDA=-1000.0f;
const float MOLHADO=0.3f; // cm de água que contam como "a água chegou"
/* indexado pelo enum Jogo; "" = número de um jogo tirado (não renumerar: o número vai para o log e o shader) */
const char* NOMES[Extras::NUM_JOGOS]={"nada","corrida","vila","","rota","","vulcao","",""};
std::string fmt(const char* f,int a=0,int b=0)
	{
	char s[200];
	std::snprintf(s,sizeof(s),f,a,b);
	return s;
	}
}

int Extras::jogoDoNome(const char* nome)
	{
	/* Em 14/11/2026 o Victor tirou fazenda, lago, barquinho e resgate (repetitivos); só estes
	   quatro existem: */
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
	labVazao=1.0f; // a vazão do labirinto difícil (0,6) não vale para os jogos (antes ficava valendo)
	jogo=qual;
	jogoNivel=std::max(1,std::min(3,nivel));
	jogoPedido=true;
	jogoFase=0;
	nPecas=nFontes=0;
	labZerarAgua=true; // rodada nova começa sem água anterior
	for(int k=0;k<MAX_CHUVAS;++k)
		chuvas[k].ate=-1.0;
	temChuva=false;
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
	labZerarAgua=true; // rodada nova começa sem água
	for(int k=0;k<MAX_CHUVAS;++k)
		chuvas[k].ate=-1.0;
	temChuva=false;
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
				anuncia(fmt("Jogo: vulcão em erupção (%d casas)! Proteja as casas da lava",casas));
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
				anuncia(fmt("Jogo: salve a vila (%d casas)! A água começou a descer",casas));
				}
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
	}

void Extras::terminaRodada(const std::string& resultado,int vencedor)
	{
	if(jogoFase!=1)
		return;
	jogoFase=2;
	jogoFim=tempo;
	jogoVencedor=vencedor;
	for(int k=0;k<nFontes;++k)
		fontes[k].ativa=false;
	anuncia("Jogo: fim da rodada; "+resultado);
	}

void Extras::jogoLeitura(const float* maxima)
	{
	if(jogo==SEM_JOGO)
		{
		/* Labirinto: a água não pode encostar nas armadilhas (senão recomeça do zero).
		   Tempo de carência de 6 s evita loops infinitos se ainda houver água escorrendo. */
		if(labirinto&&labEstado==1&&tempo-labInicio>6.0)
			for(int i=0;i<nPecas;++i)
				if(pecas[i].tipo==P_PERIGO&&maxima[i]>=0.4f)
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
			case P_PORTAO:
				if(molhou&&p.estado==0&&p.ordem==proximoPortao)
					{
					p.estado=1;
					anuncia(fmt("Jogo: a água passou pelo portão %d",p.ordem));
					++proximoPortao;
					}
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
		if(altura(x,y)<referencia-0.5f)
			continue; // não põe em valas ou depressões onde a água empossa naturalmente
		addPeca(P_PERIGO,x,y,3.2f);
		}
	}

void Extras::jogoPasso(float) // (o dt era da fazenda e do resgate; as regras de hoje usam o relógio)
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
	/* Corrida e rota: sem chegada até o tempo limite, a rodada acaba (antes a nascente jogava
	   água para sempre e a caixa inteira alagava); vila e vulcão contam as casas no fim, abaixo */
	if(jogoDuracao>0.0f&&t>=jogoDuracao)
		{
		if(jogo==CORRIDA)
			terminaRodada("empate: o tempo acabou",2);
		else if(jogo==ROTA)
			terminaRodada("o tempo acabou antes de a água chegar",1);
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
					fontes[k].ativa=true; // água desce imediatamente desde o começo da rodada
			else
				{
				/* Vulcão: é de reagir. A cratera cospe lava em pulsos (9 s sim, 5 s não) e, durante a
				   erupção, abrem bocas novas em lugares sorteados: 6 s antes aparece o aviso piscando. */
				float te=float(t);
				fontes[0].ativa=std::fmod(te,14.0f)<9.0f;
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
			if(t>=jogoDuracao)
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
		default:
			break;
		}
	}
