/***********************************************************************
Mundo LAOB (Projetor-LAOB): os membros do laboratório vivendo numa cidade
desenhada sobre a areia.

A cidade tem TRÊS TELAS de largura (a caixa mostra uma por vez) e um traçado
fixo: uma avenida no meio, ruas transversais e quarteirões dos dois lados.
  bairro 0 (residencial)  as casas dos membros
  bairro 1 (centro)       laboratório, café, mercado e a praça
  bairro 2 (lazer)        parque, campinho e o lago de pesca
A imagem desliza para o bairro onde os membros estão (camX) e visita, de
tempos em tempos, os outros bairros que têm gente.

Cada boneco decide aonde ir, sai pela calçada, atravessa a avenida e entra no
lugar; se é longe, vai de carro pela avenida. Em casa, no laboratório, no café
e no mercado ele entra (some; as janelas acendem); na praça, no parque, no
campinho e no lago fica à vista. Quem o Willbert está vendo no laboratório de
verdade prefere o laboratório da cidade. Moradores sem nome completam a cidade.
O chão (grama, ruas, calçadas) e os lugares são desenhados no shader (Extras.cpp)
com as MESMAS medidas daqui: se mudar uma, mude a outra.
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <algorithm>

namespace {
const float PI=3.14159265f;
/* medidas do traçado, em cm (iguais às do shader) */
const float RA=2.4f;  // meia largura da avenida
const float CAL=1.1f; // largura da calçada
bool aberto(int tipo) // lugar onde o boneco fica à vista
	{
	return tipo==Extras::P_PRACA||tipo==Extras::P_PARQUE||tipo==Extras::P_CAMPO||tipo==Extras::P_PESCA;
	}
const char* ondeEsta(int tipo)
	{
	switch(tipo)
		{
		case Extras::P_CASA: return "em casa";
		case Extras::P_LAB: return "no laboratório";
		case Extras::P_PRACA: return "na praça";
		case Extras::P_CAFE: return "no café";
		case Extras::P_MERCADO: return "no mercado";
		case Extras::P_PARQUE: return "no parque";
		case Extras::P_CAMPO: return "jogando bola no campinho";
		case Extras::P_PESCA: return "pescando no lago";
		}
	return "passeando";
	}
const char* paraOnde(int tipo)
	{
	switch(tipo)
		{
		case Extras::P_CASA: return "para casa";
		case Extras::P_LAB: return "para o laboratório";
		case Extras::P_PRACA: return "para a praça";
		case Extras::P_CAFE: return "para o café";
		case Extras::P_MERCADO: return "para o mercado";
		case Extras::P_PARQUE: return "para o parque";
		case Extras::P_CAMPO: return "para o campinho";
		case Extras::P_PESCA: return "para o lago, pescar";
		}
	return "passear";
	}
}

void Extras::mundo(const std::vector<std::string>& lista,int giro,int fotos,int moradores)
	{
	/* Mesmos nomes de antes e a cidade já montada: só atualiza quem está presente (a rotina de
	   cada um continua de onde estava). */
	if(modo==MUNDO&&int(lista.size())==nNomes)
		{
		bool iguais=true;
		for(int i=0;i<nNomes&&iguais;++i)
			{
			std::string limpo;
			for(size_t c=0;c<lista[i].size()&&limpo.size()<size_t(MAX_LETRAS);++c)
				{
				char ch=lista[i][c];
				if(ch>='a'&&ch<='z')
					ch=char(ch-'a'+'A');
				if(ch>='A'&&ch<='Z')
					limpo.push_back(ch);
				}
			iguais=limpo==nomes[i];
			}
		if(iguais)
			{
			for(int i=0;i<nNomes;++i)
				presente[i]=!lista[i].empty()&&lista[i][lista[i].size()-1]=='+';
			giroNomes=((giro%4)+4)%4;
			if(!atlas.empty())
				nFotos=std::max(0,std::min(fotos,nNomes));
			return;
			}
		}
	jogo=SEM_JOGO; // a cidade usa as peças dos jogos: um de cada vez
	jogoFase=0;
	labirinto=false;
	labEstado=0;
	nPecas=nFontes=0;
	membros(lista,giro,fotos); // nomes, cores e rostos
	modo=MUNDO;
	int n=nNomes;
	quantos=std::min(int(MAX_AGENTES),n+std::max(0,moradores));
	for(int i=0;i<MAX_AGENTES;++i)
		{
		if(i>=n)
			tipo[i]=i<quantos?3.05f+0.9f*aleatorio():0.0f;
		vel[i]=0.0f;
		mEstado[i]=0;
		mLugar[i]=mCasa[i]=-1;
		mAte[i]=mTroca[i]=0.0f;
		wpN[i]=wpI[i]=wpCarro[i]=0;
		escondido[i]=true;
		}
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	camX=camAlvo=0.0f;
	camTroca=0.0;
	mundoPedido=true;
	if(mapaOk)
		mundoMonta();
	}

void Extras::mundoMonta(void)
	{
	mundoPedido=false;
	nPecas=0;
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],cw=w/3.0f,meio=bmin[1]+h*0.5f;
	const float alt=h*0.5f-(RA+CAL)-0.8f; // altura de um quarteirão
	/* centro do quarteirão da coluna k (0..8), de cima (1) ou de baixo (0) */
	auto lote=[&](int k,int cima,float& x,float& y)
		{
		x=bmin[0]+(float(k)+0.5f)*cw;
		y=meio+(cima?1.0f:-1.0f)*(RA+CAL+alt*0.5f);
		};
	float x,y;
	/* bairro residencial: uma casa por membro (até seis), telhado da cor do dono */
	for(int i=0;i<nNomes&&i<6;++i)
		{
		lote(i/2,1-(i&1),x,y);
		addPeca(P_CASA,x,y,4.4f,1+int(std::fmod(tipo[i],1.0f)*99.0f),0);
		mCasa[i]=nPecas-1;
		}
	/* os lotes residenciais que sobram ganham casas dos moradores sem nome */
	for(int i=std::min(nNomes,6);i<6;++i)
		{
		lote(i/2,1-(i&1),x,y);
		addPeca(P_CASA,x,y,4.4f,1+int(aleatorio()*99.0f),0);
		int dono=nNomes+(i-std::min(nNomes,6));
		if(dono<quantos)
			mCasa[dono]=nPecas-1;
		}
	/* centro */
	lote(3,1,x,y); addPeca(P_CAFE,x,y,4.8f);
	lote(4,1,x,y); addPeca(P_LAB,x,y,6.6f);
	lote(5,1,x,y); addPeca(P_MERCADO,x,y,5.0f);
	lote(4,0,x,y); addPeca(P_PRACA,x,y,6.4f);
	/* lazer */
	lote(6,1,x,y); addPeca(P_PARQUE,x,y,7.0f);
	lote(8,1,x,y); addPeca(P_CAMPO,x,y,7.0f);
	lote(7,0,x,y); addPeca(P_PESCA,x,y,6.6f);
	for(int p=0;p<nPecas;++p)
		{
		pecas[p].estado=0;
		pecas[p].nivel=0.0f;
		}
	/* cada um começa em casa (ou num lugar sorteado), saindo aos poucos */
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		int l=mCasa[i];
		while(l<0||(pecas[l].tipo==P_CASA&&mCasa[i]!=l))
			l=int(aleatorio()*float(nPecas))%nPecas;
		mLugar[i]=l;
		mEstado[i]=0;
		mAte[i]=float(tempo)+1.0f+12.0f*aleatorio();
		mTroca[i]=0.0f;
		ax[i]=pecas[l].x;
		ay[i]=pecas[l].y;
		ang[i]=aleatorio()*2.0f*PI;
		tipo[i]=3.0f+std::fmod(tipo[i],1.0f);
		vb[i]=3.0f+aleatorio()*1.0f;
		vel[i]=vb[i];
		wpN[i]=wpI[i]=wpCarro[i]=0;
		escondido[i]=!aberto(pecas[l].tipo);
		}
	camX=camAlvo=0.0f;
	camTroca=tempo+6.0;
	anuncia("Mundo: cidade montada");
	}

int Extras::mundoEscolhe(int i)
	{
	/* Próximo lugar do boneco i, por sorteio com pesos: a própria casa, o laboratório (bem mais
	   para quem está no laboratório de verdade) e os outros lugares. Nunca a casa dos outros. */
	float pesos[MAX_PECAS],soma=0.0f;
	bool membro=i<nNomes;
	for(int p=0;p<nPecas;++p)
		{
		float w=0.0f;
		switch(pecas[p].tipo)
			{
			case P_CASA: w=mCasa[i]==p?2.4f:0.0f; break;
			case P_LAB: w=membro?(presente[i]?9.0f:1.6f):0.8f; break;
			case P_PRACA: w=2.0f; break;
			case P_CAFE: w=1.5f; break;
			case P_MERCADO: w=1.3f; break;
			case P_PARQUE: w=1.6f; break;
			case P_CAMPO: w=1.8f; break;
			case P_PESCA: w=1.5f; break;
			}
		if(p==mLugar[i])
			w*=membro&&presente[i]&&pecas[p].tipo==P_LAB?0.6f:0.0f; // quem está no lab de verdade pode ficar
		pesos[p]=w;
		soma+=w;
		}
	if(soma<=0.0f)
		return mLugar[i];
	float r=aleatorio()*soma;
	for(int p=0;p<nPecas;++p)
		if((r-=pesos[p])<=0.0f)
			return p;
	return mLugar[i];
	}

void Extras::mundoPasso(float dt)
	{
	if(!mapaOk)
		return;
	if(mundoPedido)
		mundoMonta();
	if(nPecas==0)
		return;
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],meio=bmin[1]+h*0.5f;
	/* a porta de um lugar: o ponto da calçada em frente a ele */
	auto porta=[&](const Peca& L,float desvio,float& x,float& y)
		{
		x=L.x+desvio;
		y=meio+(L.y>meio?1.0f:-1.0f)*(RA+CAL*0.55f);
		};
	/* quanta gente há em cada lugar (acende as janelas, solta a bola no campinho) */
	int gente[MAX_PECAS];
	for(int p=0;p<MAX_PECAS;++p)
		gente[p]=0;
	for(int i=0;i<quantos;++i)
		if(tipo[i]>0.5f&&mEstado[i]==0&&mLugar[i]>=0&&mLugar[i]<nPecas)
			++gente[mLugar[i]];
	for(int p=0;p<nPecas;++p)
		{
		pecas[p].estado=gente[p];
		pecas[p].nivel+=((gente[p]>0?1.0f:0.0f)-pecas[p].nivel)*std::min(1.0f,2.0f*dt); // luz que acende devagar
		}
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f||mLugar[i]<0||mLugar[i]>=nPecas)
			continue;
		Peca& L=pecas[mLugar[i]];
		float cor=std::fmod(tipo[i],1.0f);
		float desvio=(std::fmod(cor*37.0f,1.0f)-0.5f)*1.2f; // cada um anda num fio da calçada
		float tx=ax[i],ty=ay[i];
		bool anda=false;
		if(mEstado[i]==0)
			{
			/* está no lugar */
			if(aberto(L.tipo))
				{
				escondido[i]=false;
				if(float(tempo)>=mTroca[i])
					{
					/* de vez em quando escolhe outro canto do lugar */
					mTroca[i]=float(tempo)+(L.tipo==P_PESCA?9.0f:2.0f)+3.0f*aleatorio();
					float a=aleatorio()*2.0f*PI;
					if(L.tipo==P_CAMPO)
						{
						wpX[i][0]=L.x+(aleatorio()*2.0f-1.0f)*L.raio*0.85f;
						wpY[i][0]=L.y+(aleatorio()*2.0f-1.0f)*L.raio*0.5f;
						}
					else
						{
						float rr=L.tipo==P_PESCA?L.raio*0.98f:L.raio*(0.45f+0.4f*aleatorio()); // pesca: na margem
						wpX[i][0]=L.x+std::cos(a)*rr;
						wpY[i][0]=L.y+std::sin(a)*rr;
						}
					}
				tx=wpX[i][0];
				ty=wpY[i][0];
				anda=true;
				}
			else
				escondido[i]=true; // dentro da casa, do laboratório, do café ou do mercado
			if(float(tempo)>=mAte[i])
				{
				int d=mundoEscolhe(i);
				if(d==mLugar[i])
					mAte[i]=float(tempo)+6.0f+10.0f*aleatorio(); // fica mais um pouco
				else
					{
					/* sai: pela calçada, atravessando a avenida se precisar; de carro se for longe */
					const Peca& D=pecas[d];
					float px,py,qx,qy;
					porta(L,desvio,px,py);
					porta(D,desvio,qx,qy);
					bool carro=std::abs(D.x-L.x)>w*0.6f;
					int n=0;
					if(carro)
						{
						float faixa=meio+(D.x>L.x?-1.0f:1.0f)*RA*0.5f; // mão direita
						wpX[i][n]=L.x; wpY[i][n]=faixa; ++n;
						wpX[i][n]=D.x; wpY[i][n]=faixa; ++n;
						wpCarro[i]=2;
						ax[i]=L.x;
						ay[i]=faixa;
						}
					else
						{
						wpX[i][n]=px; wpY[i][n]=py; ++n;
						wpX[i][n]=qx; wpY[i][n]=py; ++n; // anda pela calçada do seu lado até a frente do destino
						wpCarro[i]=0;
						if(escondido[i])
							{
							ax[i]=px;
							ay[i]=py+(L.y>meio?1.0f:-1.0f)*1.6f; // saindo pela porta
							}
						}
					wpX[i][n]=qx; wpY[i][n]=qy; ++n;   // a porta do destino (atravessa a avenida, se for do outro lado)
					wpX[i][n]=D.x+desvio; wpY[i][n]=D.y+(D.y>meio?-1.0f:1.0f)*D.raio*(aberto(D.tipo)?0.4f:0.75f); ++n;
					wpN[i]=n;
					wpI[i]=0;
					tipo[i]=(carro?2.0f:3.0f)+cor;
					vb[i]=carro?(10.0f+3.0f*aleatorio()):(3.0f+1.0f*aleatorio());
					vel[i]=vb[i];
					ang[i]=std::atan2(wpY[i][0]-ay[i],wpX[i][0]-ax[i]);
					escondido[i]=false;
					mLugar[i]=d;
					mEstado[i]=1;
					if(i<nNomes)
						anuncia(std::string("Mundo: ")+nomes[i]+(carro?" foi de carro ":" foi a pé ")+paraOnde(D.tipo));
					continue;
					}
				}
			}
		else
			{
			/* a caminho: segue os pontos de passagem */
			escondido[i]=false;
			tx=wpX[i][wpI[i]];
			ty=wpY[i][wpI[i]];
			anda=true;
			float dx=tx-ax[i],dy=ty-ay[i];
			if(dx*dx+dy*dy<0.8f*0.8f)
				{
				++wpI[i];
				if(wpI[i]==wpCarro[i]&&wpCarro[i]>0)
					{
					tipo[i]=3.0f+cor; // estacionou: desce do carro
					vb[i]=3.0f+1.0f*aleatorio();
					vel[i]=vb[i];
					}
				if(wpI[i]>=wpN[i])
					{
					/* chegou */
					mEstado[i]=0;
					mTroca[i]=0.0f;
					float fica=L.tipo==P_CASA?(7.0f+9.0f*aleatorio()):
					           L.tipo==P_LAB?((i<nNomes&&presente[i])?(20.0f+22.0f*aleatorio()):(8.0f+8.0f*aleatorio())):
					           (L.tipo==P_CAFE||L.tipo==P_MERCADO)?(6.0f+6.0f*aleatorio()):(14.0f+16.0f*aleatorio());
					mAte[i]=float(tempo)+fica;
					escondido[i]=!aberto(L.tipo);
					wpX[i][0]=ax[i];
					wpY[i][0]=ay[i];
					if(i<nNomes)
						{
						rostoAte[i]=tempo+5.0; // o rosto aparece um instante: dá para ver quem chegou
						anuncia(std::string("Mundo: ")+nomes[i]+" está "+ondeEsta(L.tipo));
						}
					continue;
					}
				tx=wpX[i][wpI[i]];
				ty=wpY[i][wpI[i]];
				}
			}
		if(!anda||escondido[i])
			continue;
		/* anda até o ponto (vira aos poucos; parado, olha para o meio do lugar) */
		float dx=tx-ax[i],dy=ty-ay[i],dl=std::sqrt(dx*dx+dy*dy);
		if(dl>0.25f)
			{
			float alvoA=std::atan2(dy,dx),da=alvoA-ang[i];
			while(da>PI) da-=2.0f*PI;
			while(da<-PI) da+=2.0f*PI;
			float giro=(tipo[i]<3.0f?5.0f:7.0f)*dt;
			ang[i]+=std::max(-giro,std::min(giro,da));
			float passo=std::min(dl,vb[i]*dt*(std::abs(da)>1.2f?0.35f:1.0f));
			ax[i]+=std::cos(ang[i])*passo;
			ay[i]+=std::sin(ang[i])*passo;
			vel[i]=vb[i];
			}
		else if(mEstado[i]==0)
			{
			float da=std::atan2(L.y-ay[i],L.x-ax[i])-ang[i];
			while(da>PI) da-=2.0f*PI;
			while(da<-PI) da+=2.0f*PI;
			ang[i]+=std::max(-3.0f*dt,std::min(3.0f*dt,da));
			}
		}
	/* Câmera: a cidade tem três telas; a imagem vai para o bairro com mais membros e, a cada 18 s,
	   visita o próximo bairro que tenha algum membro (sem membros com nome, passeia pelos três). */
	if(tempo>=camTroca)
		{
		camTroca=tempo+18.0;
		int conta[3]={0,0,0};
		for(int i=0;i<quantos;++i)
			if(tipo[i]>0.5f&&(nNomes==0||i<nNomes))
				++conta[std::max(0,std::min(2,int((ax[i]-bmin[0])/w)))];
		int atual=std::max(0,std::min(2,int(camAlvo/w+0.5f)));
		int melhor=atual;
		for(int k=1;k<=3;++k)
			{
			int b=(atual+k)%3;
			if(conta[b]>0&&(b!=atual||conta[atual]>0))
				{
				melhor=b;
				break;
				}
			}
		camAlvo=float(melhor)*w;
		}
	float falta=camAlvo-camX;
	camX+=std::max(-14.0f*dt,std::min(14.0f*dt,falta)); // desliza (uma tela em ~4 s)
	}

bool Extras::mundoRosto(const char* nome,float segundos)
	{
	/* mostra o rosto do membro num balão acima do boneco e leva a imagem até o bairro onde ele está */
	for(int i=0;i<nNomes;++i)
		if(std::string(nomes[i])==nome)
			{
			rostoAte[i]=tempo+segundos;
			if(modo==MUNDO&&mapaOk)
				{
				float w=bmax[0]-bmin[0];
				camAlvo=float(std::max(0,std::min(2,int((ax[i]-bmin[0])/w))))*w;
				camTroca=tempo+segundos+4.0;
				}
			return true;
			}
	return false;
	}
