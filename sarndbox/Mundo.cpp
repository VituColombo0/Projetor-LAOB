/***********************************************************************
Mundo LAOB (Projetor-LAOB): o laboratório por dentro, desenhado sobre a
areia, com os membros lá.

A sala é a planta do laboratório de verdade (tirada de um vídeo): janelas
numa parede, porta azul e quadro na outra, bancada de parede com
computadores, bancada comprida no meio, mesas perto das janelas, arco
branco, manequim e a caixa de areia.

Quem o Willbert está vendo no laboratório de verdade fica dentro da sala o
tempo todo: quase sempre na própria bancada e, de vez em quando, na caixa de
areia, no quadro, na bancada do meio ou perto do manequim. Quem não está
presente entra pela porta de tempos em tempos, fica um pouco e vai embora
(a sala não fica vazia quando o Willbert está desligado).

As posições (frações da área iluminada: 0..1 em x e em y) são as MESMAS do
desenho no shader (Extras.cpp): se mudar uma, mude a outra.
(Até 16/11/2026 havia uma cidade inteira em volta; o Victor pediu só o laboratório.)
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <algorithm>

namespace {
const float PI=3.14159265f;
const float PORTA[2]={0.435f,0.12f};
const float ASSENTOS[8][2]={{0.56f,0.215f},{0.66f,0.215f},{0.76f,0.215f},{0.86f,0.215f},
                            {0.14f,0.65f},{0.20f,0.65f},{0.26f,0.65f},{0.17f,0.42f}};
const float PONTOS[4][2]={{0.64f,0.63f},{0.25f,0.17f},{0.60f,0.40f},{0.84f,0.62f}};
const char* NOME_PONTO[4]={"foi até a caixa de areia","foi até o quadro","foi até a bancada do meio","foi ver o manequim"};
}

void Extras::mundo(const std::vector<std::string>& lista,int giro,int fotos,int)
	{
	/* Mesmos nomes de antes e a sala já montada: só atualiza quem está presente (cada um continua
	   de onde estava). */
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
	jogo=SEM_JOGO; // um de cada vez
	jogoFase=0;
	labirinto=false;
	labEstado=0;
	nPecas=nFontes=0;
	membros(lista,giro,fotos); // nomes, cores e rostos
	modo=MUNDO;
	quantos=nNomes;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		if(i>=quantos)
			tipo[i]=0.0f;
		vel[i]=0.0f;
		mEstado[i]=1;
		mAte[i]=mTroca[i]=0.0f;
		escondido[i]=true;
		noLab[i]=false;
		}
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	camX=camAlvo=0.0f;
	mundoPronto=false;
	mundoPedido=true;
	if(mapaOk)
		mundoMonta();
	}

void Extras::mundoMonta(void)
	{
	mundoPedido=false;
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1];
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		tipo[i]=3.0f+std::fmod(tipo[i],1.0f);
		vb[i]=3.0f+aleatorio()*1.0f;
		vel[i]=vb[i];
		parado[i]=0.0f;
		if(presente[i])
			{
			/* quem está no laboratório de verdade já começa na própria bancada */
			mEstado[i]=0;
			escondido[i]=false;
			ax[i]=bmin[0]+ASSENTOS[i%8][0]*w;
			ay[i]=bmin[1]+ASSENTOS[i%8][1]*h;
			ang[i]=-PI*0.5f;
			wpX[i][0]=ax[i];
			wpY[i][0]=ay[i];
			mTroca[i]=float(tempo)+6.0f+10.0f*aleatorio();
			mAte[i]=0.0f;
			}
		else
			{
			/* os outros chegam aos poucos */
			mEstado[i]=1;
			escondido[i]=true;
			mAte[i]=float(tempo)+3.0f+8.0f*float(i)+6.0f*aleatorio();
			}
		}
	mundoPronto=true;
	anuncia("Mundo: laboratório montado");
	for(int i=0;i<nNomes;++i)
		if(presente[i])
			anuncia(std::string("Mundo: ")+nomes[i]+" está na bancada");
		else
			anuncia(std::string("Mundo: ")+nomes[i]+" saiu do laboratório");
	}

int Extras::mundoEscolhe(int)
	{
	return 0; // (era o sorteio de lugares da cidade; a sala só tem os pontos fixos)
	}

void Extras::mundoPasso(float dt)
	{
	if(!mapaOk)
		return;
	if(mundoPedido)
		mundoMonta();
	if(!mundoPronto)
		return;
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1];
	const float px=bmin[0]+PORTA[0]*w,py=bmin[1]+PORTA[1]*h;
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		float tx=ax[i],ty=ay[i];
		bool anda=false;
		if(mEstado[i]==1)
			{
			/* fora da sala: quem está presente de verdade entra logo; os outros, quando der a hora */
			escondido[i]=true;
			if(presente[i]&&mAte[i]>float(tempo)+4.0f)
				mAte[i]=float(tempo)+1.0f+3.0f*aleatorio();
			if(float(tempo)<mAte[i])
				continue;
			mEstado[i]=0;
			escondido[i]=false;
			ax[i]=px;
			ay[i]=py-1.5f;
			ang[i]=PI*0.5f;
			vel[i]=vb[i];
			wpX[i][0]=bmin[0]+ASSENTOS[i%8][0]*w;
			wpY[i][0]=bmin[1]+ASSENTOS[i%8][1]*h;
			mTroca[i]=float(tempo)+8.0f+8.0f*aleatorio();
			mAte[i]=float(tempo)+25.0f+30.0f*aleatorio(); // a visita de quem não está presente dura isso
			if(i<nNomes)
				{
				rostoAte[i]=tempo+4.0; // o rosto aparece um instante: dá para ver quem entrou
				anuncia(std::string("Mundo: ")+nomes[i]+" entrou no laboratório");
				}
			continue;
			}
		if(mEstado[i]==0)
			{
			/* dentro: quase sempre na própria bancada; de vez em quando num dos pontos da sala */
			if(float(tempo)>=mTroca[i])
				{
				float fx,fy;
				if(aleatorio()<0.6f)
					{
					fx=ASSENTOS[i%8][0];
					fy=ASSENTOS[i%8][1];
					mTroca[i]=float(tempo)+9.0f+10.0f*aleatorio();
					if(i<nNomes)
						anuncia(std::string("Mundo: ")+nomes[i]+" está na bancada");
					}
				else
					{
					int k=int(aleatorio()*4.0f)%4;
					fx=PONTOS[k][0]+0.03f*(aleatorio()-0.5f);
					fy=PONTOS[k][1]+0.03f*(aleatorio()-0.5f);
					mTroca[i]=float(tempo)+6.0f+6.0f*aleatorio();
					if(i<nNomes)
						anuncia(std::string("Mundo: ")+nomes[i]+" "+NOME_PONTO[k]);
					}
				wpX[i][0]=bmin[0]+fx*w;
				wpY[i][0]=bmin[1]+fy*h;
				}
			tx=wpX[i][0];
			ty=wpY[i][0];
			anda=true;
			if(!presente[i]&&float(tempo)>=mAte[i])
				mEstado[i]=2; // acabou a visita: vai embora
			}
		else
			{
			/* saindo: até a porta */
			tx=px;
			ty=py;
			anda=true;
			float dx=tx-ax[i],dy=ty-ay[i];
			if(presente[i])
				{
				mEstado[i]=0; // o Willbert viu a pessoa de novo: ela fica
				mTroca[i]=0.0f;
				}
			else if(dx*dx+dy*dy<1.2f*1.2f)
				{
				mEstado[i]=1;
				escondido[i]=true;
				mAte[i]=float(tempo)+35.0f+70.0f*aleatorio();
				if(i<nNomes)
					anuncia(std::string("Mundo: ")+nomes[i]+" saiu do laboratório");
				continue;
				}
			}
		if(!anda)
			continue;
		/* anda até o ponto, virando aos poucos; sentado, fica de frente para a bancada */
		float dx=tx-ax[i],dy=ty-ay[i],dl=std::sqrt(dx*dx+dy*dy);
		if(dl>0.25f)
			{
			float da=std::atan2(dy,dx)-ang[i];
			while(da>PI) da-=2.0f*PI;
			while(da<-PI) da+=2.0f*PI;
			ang[i]+=std::max(-7.0f*dt,std::min(7.0f*dt,da));
			float passo=std::min(dl,vb[i]*dt*(std::abs(da)>1.2f?0.35f:1.0f));
			ax[i]+=std::cos(ang[i])*passo;
			ay[i]+=std::sin(ang[i])*passo;
			vel[i]=vb[i];
			}
		}
	}

bool Extras::mundoRosto(const char* nome,float segundos)
	{
	/* mostra o rosto do membro num balão acima do boneco por alguns segundos (toque no painel);
	   se ele está fora, entra para aparecer */
	for(int i=0;i<nNomes;++i)
		if(std::string(nomes[i])==nome)
			{
			rostoAte[i]=tempo+segundos;
			if(modo==MUNDO&&mEstado[i]==1)
				mAte[i]=float(tempo);
			return true;
			}
	return false;
	}
