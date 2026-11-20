/***********************************************************************
Mundo LAOB (Projetor-LAOB): o laboratório por dentro, desenhado sobre a
areia, com os membros lá.

Desde 19/11/2026 (pedido do Victor, com 15 fotos): três salas e o
corredor. A planta (paredes, portas, móveis e assentos) está em
MundoPlanta.h, a MESMA que o shader desenha (Extras.cpp):
- sala principal: bancada H (osciloscópio, torso de RCP, câmera do
  Willbert), scanner corporal, arco branco, cluster, caixa de areia,
  quadro verde, ilhas de mesas;
- forense (atrás da parede F, embaixo): urna, papéis, tabuleiro de
  calibração, TV, caixa de som, PC aberto, pôster;
- miniauditório (em cima): tela, mesa de vidro, sofá, plateia.

Cada membro tem assento e rotina próprios (pelo visual: 1 Gazziro,
2 Lucas, 3 Mario, 4 Victor; membro novo usa os pontos genéricos):
- Gazziro: posto na mesa de vidro do miniauditório; apresenta lá, dá aula
  no quadro e trabalha na forense (o pôster de lá é dele). Quando ele
  apresenta ou dá aula, os outros às vezes vão assistir;
- Victor: mesa dele na ilha 1; caixa de areia, câmeras, scanner;
- Lucas: lado B do cluster; switch, carteado, TV e música da forense;
- Mario (suposição): bancada H do osciloscópio; scanner, tabuleiro, RCP.
Quem o Willbert está vendo no laboratório de verdade fica dentro o tempo
todo; quem não está entra pelo corredor de tempos em tempos e vai embora.
Cada passeio segue uma rota (mp::rota): sai pela porta da sala, passa
pelas faixas livres e entra pela porta da outra sala, sem atravessar
móveis. O estado que o shader usa (salas ocupadas, portas abertas, tela,
scanner, música, TV...) vai em mundoBits.

As linhas do log ("Mundo: NOME foi ...", "está ...", "entrou ...",
"saiu ...") são lidas pelo painel (areia.py, mundo_situacao).
(Até 16/11/2026 havia uma cidade inteira em volta; o Victor pediu só o laboratório.)
***********************************************************************/

#include "Extras.h"
#include "MundoPlanta.h"

#include <cmath>
#include <algorithm>

using namespace mp;

namespace {
const float PI=3.14159265f;

#define MP_CPP_ASS(n,a,b,g) {float(a),float(b),float(g)},
const float ASSENTO[8][3]={MP_ASSENTOS(MP_CPP_ASS)};
#undef MP_CPP_ASS
const char* const TXT_ASSENTO[8]={"está no miniauditório","está no cluster","está na bancada de eletrônica","está na mesa dele",
                                   "está na bancada","está na bancada","está na bancada","está na bancada"};

/* Rotina de cada visual (0 = membro novo, 1 Gazziro, 2 Lucas, 3 Mario, 4 Victor): peso do assento e de cada
   ponto, em %. Victor: sofá e plateia só com a tela do miniauditório ligada (alguém lá). */
const float PESO_ASSENTO[5]={55.0f,35.0f,45.0f,42.0f,42.0f};
const float PESO[5][N_PONTOS]={
	/*         areia quad clus swit cam  scan arco gel  cart banc fund urna tab  pap  post tv   mus  pc   apr  sofa plat vidr rcp  wb   aula */
	/* novo */ {7.5f,0,   7.5f,0,   0,   0,   0,   7.5f,7.5f,7.5f,7.5f,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
	/* Gaz. */ {4,   12,  0,   0,   0,   0,   0,   3,   6,   2,   0,   7,   0,   10,  6,   0,   0,   0,   15,  0,   0,   0,   0,   0,   0},
	/* Lucas*/ {0,   0,   12,  10,  5,   0,   0,   5,   7,   0,   0,   0,   0,   0,   0,   6,   6,   0,   0,   0,   4,   0,   0,   0,   0},
	/* Mario*/ {4,   0,   0,   0,   0,   13,  7,   5,   0,   0,   7,   0,   10,  0,   0,   0,   0,   0,   0,   0,   4,   0,   8,   0,   0},
	/* Vict.*/ {14,  0,   6,   0,   7,   6,   3,   3,   0,   0,   0,   0,   4,   0,   0,   0,   0,   5,   0,   1.5f,1.5f,0,  0,   7,   0}};

/* Portas (centro do vão, F) e a chegada pelo corredor */
const float ENTRADA_X=0.5f*float(MP_PORTA_ENT_0+MP_PORTA_ENT_1);
const float CHEGA[2]={1.00f,0.050f},SAI[2]={1.02f,0.050f};

/* assento pelo visual: A0 Gazziro, A1 Lucas, A2 Mario, A3 Victor; membro novo, um de A4 a A7 */
int assentoDe(int visual,int i)
	{
	return visual>0?visual-1:4+(i%4);
	}
}

void Extras::mundo(const std::vector<std::string>& lista,int giro,int fotos)
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
	if(lencol())
		labZerarAgua=true; // o lago dos bichos vai embora (a sala não tem água)
	nivelAgua=-1000.0f; // sem lago (o SEM de Extras.cpp)
	copiaNomes(lista,giro,fotos); // nomes, cores e rostos
	modo=MUNDO;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		if(i>=quantos)
			tipo[i]=0.0f;
		vel[i]=0.0f;
		mEstado[i]=1;
		mAte[i]=mTroca[i]=0.0f;
		escondido[i]=true;
		nRota[i]=kRota[i]=0;
		mDest[i]=-1;
		mSocial[i]=-1;
		mChegou[i]=-1.0;
		}
	for(int i=0;i<MAX_NOMES;++i)
		rostoAte[i]=0.0;
	mundoArcoAte=mundoMusicaAte=mundoTVAte=-100.0;
	mundoBits=0.0f;
	mundoPronto=false;
	mundoPedido=true;
	if(mapaOk)
		mundoMonta();
	}

void Extras::mundoRota(int i,float fx,float fy)
	{
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1];
	float sx=(ax[i]-bmin[0])/w,sy=(ay[i]-bmin[1])/h;
	nRota[i]=mp::rota(sx,sy,fx,fy,h/w,mRota[i],MAX_ROTA);
	if(nRota[i]==0)
		{
		mRota[i][0][0]=fx; // (não acontece: o teste_mundo confere todas as rotas)
		mRota[i][0][1]=fy;
		nRota[i]=1;
		}
	kRota[i]=0;
	wpX[i]=bmin[0]+fx*w;
	wpY[i]=bmin[1]+fy*h;
	mChegou[i]=-1.0;
	}

void Extras::mundoEscolhe(int i,int ponto,int vaga,const char* texto)
	{
	float fx,fy;
	const int k=assentoDe(i<nNomes?visual[i]:0,i);
	if(ponto<0)
		{
		fx=ASSENTO[k][0];
		fy=ASSENTO[k][1];
		mAlvoAng[i]=ASSENTO[k][2];
		mTroca[i]=float(tempo)+9.0f+10.0f*aleatorio();
		}
	else
		{
		if(ponto==P_SOFA||ponto==P_PLATEIA||ponto==P_AULA)
			{
			/* 19/11/2026 (revisão): a primeira vaga livre a partir da sugerida (antes, o 1º e o 4º membro
			   caíam no mesmo lugar do sofá, um em cima do outro) */
			for(int t=0;t<3;++t)
				{
				int v=(vaga+t)%3;
				bool ocupada=false;
				for(int j=0;j<quantos;++j)
					if(j!=i&&mEstado[j]!=1&&mDest[j]==ponto&&mVaga[j]%3==v)
						ocupada=true;
				if(!ocupada)
					{
					vaga=v;
					break;
					}
				}
			}
		lugar(ponto,vaga,fx,fy);
		mAlvoAng[i]=PONTO[ponto].ang;
		mTroca[i]=float(tempo)+6.0f+6.0f*aleatorio();
		}
	mDest[i]=ponto;
	mVaga[i]=vaga;
	if(ponto==P_ARCO)
		{
		/* atravessa o arco: primeiro até o lado de lá, depois passa por baixo dele */
		mundoRota(i,0.395f,0.775f);
		if(nRota[i]<MAX_ROTA)
			{
			mRota[i][nRota[i]][0]=fx;
			mRota[i][nRota[i]][1]=fy;
			++nRota[i];
			}
		const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1];
		wpX[i]=bmin[0]+fx*w;
		wpY[i]=bmin[1]+fy*h;
		}
	else
		mundoRota(i,fx,fy);
	if(i<nNomes)
		anuncia(std::string("Mundo: ")+nomes[i]+" "+(texto!=0?texto:(ponto<0?TXT_ASSENTO[k]:PONTO[ponto].txt)));
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
		vb[i]=(4.0f+aleatorio())*w/100.0f; // 4 a 5 cm da planta por segundo (~2,4 a 3 cm de areia)
		vel[i]=vb[i];
		parado[i]=0.0f;
		mSocial[i]=-1;
		if(presente[i])
			{
			/* quem está no laboratório de verdade já começa no próprio assento */
			const int k=assentoDe(i<nNomes?visual[i]:0,i);
			mEstado[i]=0;
			escondido[i]=false;
			ax[i]=bmin[0]+ASSENTO[k][0]*w;
			ay[i]=bmin[1]+ASSENTO[k][1]*h;
			ang[i]=ASSENTO[k][2];
			wpX[i]=ax[i];
			wpY[i]=ay[i];
			nRota[i]=kRota[i]=0;
			mDest[i]=-1;
			mAlvoAng[i]=ang[i];
			mChegou[i]=tempo;
			mTroca[i]=float(tempo)+6.0f+10.0f*aleatorio();
			mAte[i]=0.0f;
			}
		else
			{
			/* os outros chegam aos poucos, pelo corredor */
			mEstado[i]=1;
			escondido[i]=true;
			mAte[i]=float(tempo)+3.0f+8.0f*float(i)+6.0f*aleatorio();
			}
		}
	mundoPronto=true;
	anuncia("Mundo: laboratório montado");
	for(int i=0;i<nNomes;++i)
		if(presente[i])
			anuncia(std::string("Mundo: ")+nomes[i]+" "+TXT_ASSENTO[assentoDe(visual[i],i)]);
		else
			anuncia(std::string("Mundo: ")+nomes[i]+" saiu do laboratório");
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
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		const int vis=i<nNomes?visual[i]:0;
		if(mEstado[i]==1)
			{
			/* fora: quem está presente de verdade entra logo; os outros, quando der a hora */
			escondido[i]=true;
			if(presente[i]&&mAte[i]>float(tempo)+4.0f)
				mAte[i]=float(tempo)+1.0f+3.0f*aleatorio();
			if(float(tempo)<mAte[i])
				continue;
			mEstado[i]=0;
			ax[i]=bmin[0]+CHEGA[0]*w;
			ay[i]=bmin[1]+CHEGA[1]*h;
			ang[i]=PI;
			vel[i]=vb[i];
			mSocial[i]=-1;
			mAte[i]=float(tempo)+40.0f+40.0f*aleatorio(); // a visita de quem não está presente dura isso
			if(i<nNomes)
				{
				rostoAte[i]=tempo+4.0; // o rosto aparece um instante: dá para ver quem entrou
				anuncia(std::string("Mundo: ")+nomes[i]+" entrou no laboratório");
				}
			mundoEscolhe(i,-1,0,0); // primeiro, o próprio assento (o Gazziro vai direto ao miniauditório)
			mTroca[i]+=4.0f;
			}
		else if(mEstado[i]==0)
			{
			/* dentro: o próprio assento ou um ponto da rotina dele */
			if(presente[i]||float(tempo)<mAte[i])
				{
				if(mSocial[i]>=0&&float(tempo)>=mTroca[i])
					{
					/* foi chamado para assistir o Gazziro */
					int p=mSocial[i];
					mSocial[i]=-1;
					mundoEscolhe(i,p,i,p==P_AULA?"foi assistir a aula no quadro":"foi assistir o Gazziro no miniauditório");
					mTroca[i]=float(tempo)+14.0f+8.0f*aleatorio();
					}
				else if(float(tempo)>=mTroca[i]&&mChegou[i]>=0.0)
					{
					const int v=std::max(0,std::min(4,vis));
					bool telaLigada=std::fmod(std::floor(mundoBits/8.0f),2.0f)>0.5f;
					float soma=PESO_ASSENTO[v];
					for(int p=0;p<N_PONTOS;++p)
						soma+=(v==4&&(p==P_SOFA||p==P_PLATEIA)&&!telaLigada)?0.0f:PESO[v][p];
					float r=aleatorio()*soma;
					int escolha=-1;
					if(r>=PESO_ASSENTO[v])
						{
						r-=PESO_ASSENTO[v];
						for(int p=0;p<N_PONTOS;++p)
							{
							float pp=(v==4&&(p==P_SOFA||p==P_PLATEIA)&&!telaLigada)?0.0f:PESO[v][p];
							if(r<pp)
								{
								escolha=p;
								break;
								}
							r-=pp;
							}
						}
					/* um ponto de vaga única já ocupado por outro: fica no assento */
					if(escolha>=0&&escolha!=P_SOFA&&escolha!=P_PLATEIA&&escolha!=P_AULA)
						for(int j=0;j<quantos;++j)
							if(j!=i&&mEstado[j]!=1&&mDest[j]==escolha)
								escolha=-1;
					if(escolha==mDest[i]&&escolha>=0)
						escolha=-1; // (não repete o mesmo ponto)
					mundoEscolhe(i,escolha,i,0);
					}
				}
			else if(mChegou[i]>=0.0||nRota[i]==0)
				{
				/* acabou a visita: vai embora pelo corredor (espera chegar ao ponto antes) */
				mEstado[i]=2;
				mDest[i]=-2;
				mSocial[i]=-1;
				mundoRota(i,SAI[0],SAI[1]);
				}
			}
		else
			{
			/* saindo pela entrada até a ponta do corredor */
			if(presente[i])
				{
				mEstado[i]=0; // o Willbert viu a pessoa de novo: ela fica e volta ao assento
				mundoEscolhe(i,-1,0,0);
				}
			else if(kRota[i]>=nRota[i])
				{
				mEstado[i]=1;
				escondido[i]=true;
				mAte[i]=float(tempo)+35.0f+70.0f*aleatorio();
				if(i<nNomes)
					anuncia(std::string("Mundo: ")+nomes[i]+" saiu do laboratório");
				continue;
				}
			}
		/* anda pela rota, virando aos poucos; ao chegar, vira para o lado certo (no máximo 4 rad/s) */
		escondido[i]=(ax[i]-bmin[0])/w>0.99f;
		if(kRota[i]<nRota[i])
			{
			float tx=bmin[0]+mRota[i][kRota[i]][0]*w,ty=bmin[1]+mRota[i][kRota[i]][1]*h;
			float dx=tx-ax[i],dy=ty-ay[i],dl=std::sqrt(dx*dx+dy*dy);
			bool ultimo=kRota[i]+1>=nRota[i];
			if(dl<(ultimo?0.15f:0.35f)*w/59.0f)
				{
				++kRota[i];
				if(kRota[i]>=nRota[i])
					{
					mChegou[i]=tempo;
					/* chegou: o arco "acende", a música e a TV ficam ligadas um tempo; o Gazziro chama a plateia */
					int p=mDest[i];
					if(p==P_ARCO)
						mundoArcoAte=tempo+2.0;
					else if(p==P_MUSICA)
						mundoMusicaAte=tempo+60.0;
					else if(p==P_TV)
						mundoTVAte=tempo+90.0;
					if(vis==1&&(p==P_APRESENTA||p==P_QUADRO))
						for(int j=0;j<quantos;++j)
							if(j!=i&&tipo[j]>0.5f&&mEstado[j]==0&&aleatorio()<(p==P_APRESENTA?0.5f:0.3f))
								{
								mSocial[j]=p==P_APRESENTA?(aleatorio()<0.5f?P_SOFA:P_PLATEIA):P_AULA;
								mTroca[j]=float(tempo)+3.0f+7.0f*aleatorio();
								}
					}
				}
			else
				{
				float da=std::atan2(dy,dx)-ang[i];
				while(da>PI) da-=2.0f*PI;
				while(da<-PI) da+=2.0f*PI;
				ang[i]+=std::max(-7.0f*dt,std::min(7.0f*dt,da));
				/* anda na direção do ponto (o corpo vira junto, aos poucos): não fica rodando em volta dele */
				float passo=std::min(dl,vb[i]*dt*(std::abs(da)>1.2f?0.35f:1.0f));
				ax[i]+=dx/dl*passo;
				ay[i]+=dy/dl*passo;
				}
			vel[i]=vb[i];
			}
		else
			{
			/* parado no lugar: vira para o alvo (no quadro, alterna entre o quadro e a turma a cada 4 s) */
			float alvo=mAlvoAng[i];
			if(mDest[i]==P_QUADRO&&std::fmod(tempo-std::max(0.0,mChegou[i]),8.0)>=4.0)
				alvo=PI;
			float da=alvo-ang[i];
			while(da>PI) da-=2.0f*PI;
			while(da<-PI) da+=2.0f*PI;
			ang[i]+=std::max(-4.0f*dt,std::min(4.0f*dt,da));
			vel[i]=vb[i];
			}
		}
	mundoEstado();
	}

void Extras::mundoEstado(void)
	{
	/* O que o shader precisa saber (extrasMundo.x, um inteiro em bits; exato em float até 2^24):
	   0, 1, 2 alguém na sala principal, na forense, no miniauditório; 3 tela do miniauditório ligada (alguém lá);
	   4 apresentando; 5, 6, 7 portas de entrada, da forense e do miniauditório abertas (alguém a menos de 4 cm
	   da planta); 8 scanner em uso; 9 arco (há menos de 2 s); 10 música (60 s); 11 TV da forense (90 s, ou forense
	   ocupada); 12 aula no quadro; 13-20 assentos ocupados (A3 Victor, A1 Lucas, A2 Mario, A0 Gazziro, A4-A7);
	   21 algum membro presente de verdade (o cone da câmera do Willbert fica mais forte); 22 alguém nas câmeras */
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],asp=h/w;
	unsigned int b=0u;
	static const int BIT_ASSENTO[8]={16,14,15,13,17,18,19,20};
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f||mEstado[i]==1||escondido[i])
			continue;
		float fx=(ax[i]-bmin[0])/w,fy=(ay[i]-bmin[1])/h;
		int s=mp::sala(fx,fy);
		if(s==mp::PRINCIPAL) b|=1u;
		if(s==mp::FORENSE) b|=2u|(1u<<11);
		if(s==mp::MINI) b|=4u|8u;
		/* portas: distância em cm da planta até o meio de cada vão */
		const float PORTAS[3][2]={{ENTRADA_X,0.1335f},{0.265f,0.5f*float(MP_PORTA_FOR_0+MP_PORTA_FOR_1)},{0.265f,0.5f*float(MP_PORTA_MINI_0+MP_PORTA_MINI_1)}};
		for(int k=0;k<3;++k)
			{
			float dx=(fx-PORTAS[k][0])*100.0f,dy=(fy-PORTAS[k][1])*100.0f*asp;
			if(dx*dx+dy*dy<16.0f)
				b|=1u<<(5+k);
			}
		bool parado=kRota[i]>=nRota[i]&&mChegou[i]>=0.0;
		if(parado)
			{
			int p=mDest[i];
			if(p==P_APRESENTA) b|=1u<<4;
			if(p==P_SCANNER) b|=1u<<8;
			if(p==P_QUADRO) b|=1u<<12;
			if(p==P_CAMERAS||p==P_WILLBERT) b|=1u<<22;
			}
		for(int k=0;k<8;++k)
			{
			float dx=(fx-ASSENTO[k][0])*100.0f,dy=(fy-ASSENTO[k][1])*100.0f*asp;
			if(dx*dx+dy*dy<1.0f)
				b|=1u<<BIT_ASSENTO[k];
			}
		}
	if(tempo<mundoArcoAte) b|=1u<<9;
	if(tempo<mundoMusicaAte) b|=1u<<10;
	if(tempo<mundoTVAte) b|=1u<<11;
	for(int i=0;i<nNomes;++i)
		if(presente[i])
			b|=1u<<21;
	mundoBits=float(b);
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
