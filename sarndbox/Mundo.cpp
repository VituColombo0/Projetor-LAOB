/***********************************************************************
Mundo LAOB (Projetor-LAOB): os membros do laboratório vivendo numa cidadezinha
desenhada na areia. Lugares = peças (as mesmas dos jogos de água): uma casa por
membro, o laboratório, a praça, o café, o parque e o campinho. Cada boneco
decide aonde ir, vai a pé (ou de carro, se for longe), fica um tempo e sai de
novo. Em casa, no laboratório e no café ele entra (some; a construção mostra
que tem gente); na praça, no parque e no campinho fica à vista.
Quem o Willbert está vendo no laboratório de verdade prefere o laboratório da
areia. Moradores sem nome completam a cidade.
O movimento é o dos carrinhos e das pessoas (Extras::passo), puxado para o
destino; aqui ficam os lugares e a rotina. O desenho está no shader (Extras.cpp).
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <algorithm>

namespace {
const float PI=3.14159265f;
bool aberto(int tipo) // lugar onde o boneco fica à vista
	{
	return tipo==Extras::P_PRACA||tipo==Extras::P_PARQUE||tipo==Extras::P_CAMPO;
	}
const char* ondeEsta(int tipo)
	{
	switch(tipo)
		{
		case Extras::P_CASA: return "em casa";
		case Extras::P_LAB: return "no laboratório";
		case Extras::P_PRACA: return "na praça";
		case Extras::P_CAFE: return "no café";
		case Extras::P_PARQUE: return "no parque";
		case Extras::P_CAMPO: return "jogando bola no campinho";
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
		case Extras::P_PARQUE: return "para o parque";
		case Extras::P_CAMPO: return "para o campinho";
		}
	return "passear";
	}
}

void Extras::mundo(const std::vector<std::string>& lista,int giro,int fotos,int moradores)
	{
	/* Mesmos nomes de antes e o mundo já montado: só atualiza quem está presente (a rotina de
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
	jogo=SEM_JOGO; // o mundo usa as peças dos jogos: um de cada vez
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
		temAlvo[i]=false;
		escondido[i]=true;
		}
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	mundoPedido=true;
	if(mapaOk)
		mundoMonta();
	}

void Extras::mundoMonta(void)
	{
	mundoPedido=false;
	nPecas=0;
	float x,y;
	/* lugares longe uns dos outros, em chão firme e longe da beirada */
	const int comuns[5]={P_LAB,P_PRACA,P_CAFE,P_PARQUE,P_CAMPO};
	const float raios[5]={4.2f,4.0f,3.2f,4.2f,4.6f};
	int total=5+nNomes;
	float dist=total>10?10.0f:12.5f;
	for(int k=0;k<total&&nPecas<MAX_PECAS;++k)
		{
		bool casa=k>=5;
		bool achou=false;
		for(int t=0;t<40&&!achou;++t)
			{
			achou=pontoLivre(x,y,7.0f,t<25?dist:dist*0.7f,-1e9f,1e9f,60)&&chaoLivre(x,y,3.0f);
			if(achou&&t<30)
				{
				/* de preferência em chão plano (uma casa na ladeira fica difícil de alcançar) */
				float h0=altura(x,y);
				for(int k=0;k<4&&achou;++k)
					achou=std::abs(altura(x+3.5f*std::cos(k*PI/2.0f),y+3.5f*std::sin(k*PI/2.0f))-h0)<1.2f;
				}
			}
		if(!achou)
			continue; // sem lugar para este: a cidade fica sem ele
		if(casa)
			{
			addPeca(P_CASA,x,y,2.9f,1+int(std::fmod(tipo[k-5],1.0f)*99.0f),0); // telhado da cor do dono
			mCasa[k-5]=nPecas-1;
			}
		else
			addPeca(comuns[k],x,y,raios[k]);
		pecas[nPecas-1].estado=0;
		pecas[nPecas-1].nivel=0.0f;
		}
	/* todo mundo começa em algum lugar (cada um em casa, se tiver), saindo aos poucos */
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f||nPecas==0)
			continue;
		int l=mCasa[i]>=0?mCasa[i]:int(aleatorio()*float(nPecas))%nPecas;
		if(pecas[l].tipo==P_CASA&&mCasa[i]!=l) // morador sem nome não entra na casa dos outros
			l=int(aleatorio()*5.0f)%std::min(5,nPecas);
		mLugar[i]=l;
		mEstado[i]=0;
		mAte[i]=float(tempo)+1.0f+10.0f*aleatorio();
		ax[i]=pecas[l].x;
		ay[i]=pecas[l].y;
		ang[i]=aleatorio()*2.0f*PI;
		tipo[i]=3.0f+std::fmod(tipo[i],1.0f);
		vb[i]=2.6f+aleatorio()*1.2f;
		vel[i]=vb[i];
		parado[i]=0.0f;
		}
	anuncia("Mundo: cidade montada");
	}

int Extras::mundoEscolhe(int i)
	{
	/* Próximo lugar do boneco i, por sorteio com pesos: a própria casa, o laboratório (bem mais
	   para quem está no laboratório de verdade) e os lugares de passeio. Nunca a casa dos outros. */
	float pesos[MAX_PECAS],soma=0.0f;
	bool membro=i<nNomes;
	for(int p=0;p<nPecas;++p)
		{
		float w=0.0f;
		switch(pecas[p].tipo)
			{
			case P_CASA: w=mCasa[i]==p?2.5f:0.0f; break;
			case P_LAB: w=membro?(presente[i]?9.0f:1.5f):0.8f; break;
			case P_PRACA: w=2.2f; break;
			case P_CAFE: w=1.6f; break;
			case P_PARQUE: w=1.6f; break;
			case P_CAMPO: w=1.8f; break;
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
	/* quanta gente há em cada lugar (acende a casa, enche o laboratório, solta a bola no campinho) */
	int gente[MAX_PECAS];
	for(int p=0;p<MAX_PECAS;++p)
		gente[p]=0;
	for(int i=0;i<quantos;++i)
		if(tipo[i]>0.5f&&mEstado[i]==0&&mLugar[i]>=0&&mLugar[i]<nPecas)
			++gente[mLugar[i]];
	for(int p=0;p<nPecas;++p)
		{
		pecas[p].estado=gente[p];
		/* "nível" = luz/fumaça que acende e apaga devagar */
		float alvoN=gente[p]>0?1.0f:0.0f;
		pecas[p].nivel+=(alvoN-pecas[p].nivel)*std::min(1.0f,2.0f*dt);
		}
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f||mLugar[i]<0||mLugar[i]>=nPecas)
			continue;
		Peca& L=pecas[mLugar[i]];
		float cor=std::fmod(tipo[i],1.0f);
		if(mEstado[i]==0)
			{
			/* está no lugar */
			if(aberto(L.tipo))
				{
				/* à vista: anda de um lado para o outro dentro do lugar */
				escondido[i]=false;
				if(float(tempo)>=mTroca[i])
					{
					mTroca[i]=float(tempo)+1.5f+3.0f*aleatorio();
					float a=aleatorio()*2.0f*PI,rr=L.raio*(0.25f+0.6f*aleatorio());
					alvoX[i]=L.x+std::cos(a)*rr;
					alvoY[i]=L.y+std::sin(a)*rr;
					}
				temAlvo[i]=true;
				vel[i]=std::max(vel[i],0.3f);
				}
			else
				{
				escondido[i]=true; // dentro da casa, do laboratório ou do café
				temAlvo[i]=false;
				}
			if(float(tempo)<mAte[i])
				continue;
			int d=mundoEscolhe(i);
			if(d==mLugar[i])
				{
				mAte[i]=float(tempo)+6.0f+10.0f*aleatorio(); // fica mais um pouco
				continue;
				}
			/* sai: de carro se for longe */
			if(escondido[i])
				{
				ax[i]=L.x;
				ay[i]=L.y-L.raio*0.6f;
				}
			float dx=pecas[d].x-ax[i],dy=pecas[d].y-ay[i];
			bool carro=dx*dx+dy*dy>22.0f*22.0f;
			ang[i]=std::atan2(dy,dx);
			tipo[i]=(carro?2.0f:3.0f)+cor;
			vb[i]=carro?(6.5f+2.5f*aleatorio()):(2.6f+1.2f*aleatorio());
			vel[i]=vb[i]*0.5f;
			parado[i]=0.0f;
			escondido[i]=false;
			temAlvo[i]=true;
			alvoX[i]=pecas[d].x;
			alvoY[i]=pecas[d].y;
			mLugar[i]=d;
			mEstado[i]=1;
			mAte[i]=float(tempo)+(carro?40.0f:75.0f); // se não chegar até lá (areia mexida), chega "por mágica"
			if(i<nNomes)
				anuncia(std::string("Mundo: ")+nomes[i]+(carro?" foi de carro ":" foi a pé ")+paraOnde(pecas[d].tipo));
			}
		else
			{
			/* a caminho */
			alvoX[i]=L.x;
			alvoY[i]=L.y;
			temAlvo[i]=true;
			escondido[i]=false;
			float dx=L.x-ax[i],dy=L.y-ay[i];
			float chega=aberto(L.tipo)?L.raio*0.8f:3.2f;
			if(dx*dx+dy*dy>chega*chega&&float(tempo)<mAte[i])
				continue;
			mEstado[i]=0;
			tipo[i]=3.0f+cor; // desce do carro
			vb[i]=2.6f+1.2f*aleatorio();
			vel[i]=vb[i];
			mTroca[i]=0.0f;
			float fica=L.tipo==P_CASA?(6.0f+9.0f*aleatorio()):L.tipo==P_LAB?((i<nNomes&&presente[i])?(18.0f+20.0f*aleatorio()):(7.0f+8.0f*aleatorio())):
			           L.tipo==P_CAFE?(6.0f+6.0f*aleatorio()):(12.0f+16.0f*aleatorio());
			mAte[i]=float(tempo)+fica;
			if(!aberto(L.tipo))
				{
				ax[i]=L.x;
				ay[i]=L.y;
				escondido[i]=true;
				}
			if(i<nNomes)
				anuncia(std::string("Mundo: ")+nomes[i]+" está "+ondeEsta(L.tipo));
			}
		}
	}

bool Extras::mundoRosto(const char* nome,float segundos)
	{
	/* mostra o rosto do membro num balão acima do boneco por alguns segundos (clique no painel) */
	for(int i=0;i<nNomes;++i)
		if(std::string(nomes[i])==nome)
			{
			rostoAte[i]=tempo+segundos;
			return true;
			}
	return false;
	}
