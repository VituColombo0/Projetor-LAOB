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
- Gazziro: posto na mesa de vidro do miniauditório e quase sempre lá
  (apresenta, mesa de vidro); sai raramente (aula no quadro, forense,
  café). Quando ele apresenta ou dá aula, os outros às vezes vão assistir;
- Victor: ilha 1, lado do quadro, 3 monitores; caixa de areia, câmera do
  Willbert, câmeras do trilho, scanner;
- Mario: ilha 1, de frente para o Victor (monitores de costas uns para os
  outros); osciloscópio, scanner, tabuleiro, RCP;
- Lucas: mesinha colada na parede F, atrás da mesa de carteado; cluster,
  roteador, switch, carteado, TV e música da forense.
(Lugares marcados pelo Victor numa foto do projetor, 20/11/2026.)
Parado, cada um mexe as mãos conforme o que faz (mAtiv: digitando,
apontando o slide, bebendo, mexendo no aparelho) e às vezes dois que
estão perto conversam (balãozinho "..." no shader).
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
const char* const TXT_ASSENTO[8]={"está no miniauditório","está na mesa dele","está na mesa dele","está na mesa dele",
                                   "está na bancada","está na bancada","está na bancada","está na bancada"};

/* Rotina de cada visual (0 = membro novo, 1 Gazziro, 2 Lucas, 3 Mario, 4 Victor): peso do assento e de cada
   ponto, em %. Victor: sofá e plateia só com a tela do miniauditório ligada (alguém lá).
   20/11/2026: o Gazziro fica no miniauditório (assento, apresentar, mesa de vidro: ~88% das escolhas) */
const float PESO_ASSENTO[5]={55.0f,50.0f,46.0f,44.0f,44.0f};
const float PESO[5][N_PONTOS]={
	/*         areia quad clus swit cam  scan arco gel  cart banc fund urna tab  pap  post tv   mus  pc   apr  sofa plat vidr rcp  wb   aula osc  rot  est  bebe */
	/* novo */ {7.5f,0,   7.5f,0,   0,   0,   0,   7.5f,7.5f,7.5f,7.5f,0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   3},
	/* Gaz. */ {1,   2.5f,0,   0,   0,   0,   0,   1.5f,0,   0,   0,   1,   0,   2,   1,   0,   0,   0,   26,  0,   0,   12,  0,   0,   0,   0,   0,   0,   1},
	/* Lucas*/ {0,   0,   8,   6,   3,   0,   0,   5,   6,   0,   0,   0,   0,   0,   0,   4,   5,   0,   0,   0,   3,   0,   0,   0,   0,   0,   7,   8,   2},
	/* Mario*/ {3,   0,   0,   0,   0,   10,  4,   4,   0,   4,   5,   0,   7,   0,   0,   0,   0,   0,   0,   0,   3,   0,   5,   0,   0,   10,  0,   0,   2},
	/* Vict.*/ {12,  0,   4,   0,   6,   5,   2,   3,   0,   0,   0,   0,   3,   0,   0,   0,   0,   4,   0,   1.5f,1.5f,0,  0,   8,   0,   0,   0,   0,   2}};

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
		mAtiv[i]=0.0f;
		mConversaAte[i]=-100.0;
		mConversaCom[i]=-1;
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
	mConversaAte[i]=-100.0; // (20/11/2026, revisão) saiu andando (chamado para a aula, por ex.): a conversa acabou
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
			if(vis==1)
				mAte[i]+=80.0f; // (20/11/2026) o Gazziro fica mais: o miniauditório é o lugar dele
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
				mAte[i]=float(tempo)+(vis==1?20.0f:35.0f)+70.0f*aleatorio();
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
					mundoConversa(i); // (20/11/2026) às vezes puxa conversa com quem está perto
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
	   21 algum membro presente de verdade (o cone da câmera do Willbert fica mais forte); 22 alguém nas câmeras.
	   20/11/2026, mundoBits2 (extrasMundo.w = 1 + 2*mundoBits2): 0 alguém no osciloscópio; 1 alguém mexendo na
	   caixa de areia (o relevo muda); 2 alguém no cluster, no roteador, no switch ou na estação; 3 geladeira aberta.
	   E o que cada um faz com as mãos (mAtiv). */
	const float w=bmax[0]-bmin[0],h=bmax[1]-bmin[1],asp=h/w;
	unsigned int b=0u,b2=0u;
	static const int BIT_ASSENTO[8]={16,14,15,13,17,18,19,20};
	for(int i=0;i<quantos;++i)
		{
		mAtiv[i]=tipo[i]<0.5f?0.0f:mundoAtividade(i);
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
			if(p==P_OSCILO) b2|=1u;
			if(p==P_AREIA) b2|=2u;
			if(p==P_CLUSTER||p==P_ROTEADOR||p==P_SWITCH||p==P_ESTACAO) b2|=4u;
			if(p==P_GELADEIRA) b2|=8u;
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
	mundoBits2=float(b2);
	}

float Extras::mundoAtividade(int i) const
	{
	/* 20/11/2026 (pedido do Victor: "fazer eles mexerem em outras coisas"): o que o boneco parado faz com as
	   mãos, para o shader (extrasAg2.x): 0 andando, 1 parado, 2 trabalhando (digita), 3 apontando (slide,
	   quadro), 4 bebendo (café, água), 5 falando (balão "..."), 6 mexendo num aparelho */
	if(mEstado[i]!=0||kRota[i]<nRota[i]||mChegou[i]<0.0)
		return 0.0f;
	const int com=mConversaCom[i];
	if(tempo<mConversaAte[i]&&com>=0&&com<quantos&&mConversaCom[com]==i&&tempo<mConversaAte[com]) // (revisão: os dois ainda na conversa)
		{
		/* conversa em dupla: um fala de cada vez, trocando a cada 2,5 s */
		int vez=int(std::floor(tempo/2.5))+(i<mConversaCom[i]?0:1);
		if(vez%2==0)
			return 5.0f;
		}
	switch(mDest[i])
		{
		case -1: // o próprio assento: trabalhando no computador (o Gazziro, na mesa de vidro)
		case P_BANCADA: case P_FUNDO: case P_PCABERTO: case P_PAPEIS: case P_ESTACAO: case P_URNA: case P_SWITCH:
			return 2.0f;
		case P_AREIA: case P_OSCILO: case P_SCANNER: case P_CAMERAS: case P_WILLBERT: case P_ROTEADOR: case P_CLUSTER:
		case P_TABULEIRO: case P_RCP: case P_MUSICA: case P_TV: case P_VIDRO:
			return 6.0f;
		case P_APRESENTA: case P_QUADRO:
			return 3.0f;
		case P_GELADEIRA: case P_BEBEDOURO:
			return 4.0f;
		default:
			return 1.0f;
		}
	}

void Extras::mundoConversa(int i)
	{
	/* 20/11/2026: chegou e há alguém parado perto (até 12 cm da planta; Victor e Mario, de frente um para o
	   outro na ilha 1, ficam a 11 cm): às vezes os dois conversam um pouco. Quem dá aula não para. */
	if(i>=nNomes||mEstado[i]!=0||tempo<mConversaAte[i]||mDest[i]==P_APRESENTA||mDest[i]==P_QUADRO||aleatorio()>0.4f)
		return;
	const float w=bmax[0]-bmin[0];
	int com=-1;
	float dm=144.0f;
	for(int j=0;j<nNomes&&j<quantos;++j)
		{
		if(j==i||tipo[j]<0.5f||mEstado[j]!=0||escondido[j]||kRota[j]<nRota[j]||mChegou[j]<0.0||tempo<mConversaAte[j]
		   ||mDest[j]==P_APRESENTA||mDest[j]==P_QUADRO)
			continue;
		float dx=(ax[j]-ax[i])/w*100.0f,dy=(ay[j]-ay[i])/w*100.0f; // cm da planta nos dois eixos
		if(dx*dx+dy*dy<dm)
			{
			dm=dx*dx+dy*dy;
			com=j;
			}
		}
	if(com<0)
		return;
	const double dura=7.0+5.0*aleatorio();
	mConversaAte[i]=mConversaAte[com]=tempo+dura;
	mConversaCom[i]=com;
	mConversaCom[com]=i;
	mTroca[i]=std::max(mTroca[i],float(tempo+dura)+1.0f); // ninguém sai no meio da conversa
	mTroca[com]=std::max(mTroca[com],float(tempo+dura)+1.0f);
	std::string a(nomes[i]),b(nomes[com]);
	for(size_t c=1;c<a.size();++c) a[c]=char(a[c]-'A'+'a');
	for(size_t c=1;c<b.size();++c) b[c]=char(b[c]-'A'+'a');
	anuncia(std::string("Mundo: ")+nomes[i]+" está conversando com "+b);
	anuncia(std::string("Mundo: ")+nomes[com]+" está conversando com "+a);
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

/* ------------------------------------------------------------------ vista
   20/11/2026, pedido do Victor: "pelo site trocar qual área eu quero ver, seguir os personagens".
   Uma câmera sobre a planta: centro em F e zoom (1 = o laboratório inteiro). Cada sala aparece
   inteira na área iluminada, sem deformar (o zoom é o mesmo nos dois eixos); seguindo um membro, o
   centro vai no boneco com zoom 2. A câmera nunca mostra o que fica fora da planta, e a troca
   desliza em 0,6 s. O shader (Extras.cpp, extrasVista) passa tudo do laboratório por ela. */
namespace {
const float VISTA_SALA[4][4]={                                            // Fx0, Fx1, Fy0, Fy1 de cada vista
	{0.0f,1.0f,0.0f,1.0f},                                                // tudo
	{float(MP_PAREDE_F_X0),1.0f,float(MP_CORREDOR_Y),1.0f},               // sala principal, com as paredes
	{0.0f,float(MP_PAREDE_F_X1),float(MP_CORREDOR_Y),float(MP_DIVISORIA_Y1)}, // forense
	{0.0f,float(MP_PAREDE_F_X1),float(MP_DIVISORIA_Y0),1.0f}};            // miniauditório
const char* const NOME_VISTA[4]={"tudo","principal","forense","mini"};
const float VISTA_ZOOM_SEGUE=2.0f,VISTA_TROCA=0.6f;
}

bool Extras::mundoVista(const char* qual)
	{
	/* mundoVista tudo|principal|forense|mini|NOME: a câmera do laboratório (painel, aba LAOB) */
	if(modo!=MUNDO)
		{
		anuncia("Mundo: sem vista (o laboratório não está aberto)");
		return false;
		}
	std::string q,nome;
	for(const char* c=qual;*c!=0&&q.size()<32;++c)
		{
		char ch=*c;
		q.push_back(ch>='A'&&ch<='Z'?char(ch-'A'+'a'):ch);
		if(ch>='a'&&ch<='z')
			ch=char(ch-'a'+'A');
		if(ch>='A'&&ch<='Z'&&nome.size()<size_t(MAX_LETRAS))
			nome.push_back(ch);
		}
	int novo=-1;
	if(q=="tudo"||q=="todo"||q=="laboratorio")
		novo=VISTA_TUDO;
	else if(q=="principal"||q=="sala")
		novo=VISTA_PRINCIPAL;
	else if(q=="forense")
		novo=VISTA_FORENSE;
	else if(q=="mini"||q=="miniauditorio"||q=="auditorio")
		novo=VISTA_MINI;
	int quem=-1;
	bool desconhecido=false;
	if(novo<0)
		{
		for(int i=0;i<nNomes;++i)
			if(nome==nomes[i])
				quem=i;
		if(quem<0)
			{
			/* não é sala nem membro: volta ao laboratório inteiro */
			nome=q;
			novo=VISTA_TUDO;
			desconhecido=true;
			}
		else
			novo=VISTA_SEGUE;
		}
	/* a troca parte de onde a câmera está agora */
	vistaDe[0]=vistaC[0];
	vistaDe[1]=vistaC[1];
	vistaDe[2]=vistaZ;
	vistaDesde=tempo;
	vistaModo=novo;
	vistaSegOk=false;
	if(novo==VISTA_SEGUE)
		{
		vistaNome=nomes[quem];
		vistaFora=quem>=quantos||tipo[quem]<0.5f||escondido[quem]||mEstado[quem]==1||!mundoPronto;
		if(mEstado[quem]==1)
			mAte[quem]=float(tempo); // como no rosto: quem está fora entra para aparecer
		anuncia("Mundo: vista seguindo "+vistaNome);
		return true;
		}
	vistaNome.clear();
	if(desconhecido)
		{
		anuncia("Mundo: vista tudo ("+nome+" não é sala nem membro)");
		return false;
		}
	anuncia(std::string("Mundo: vista ")+NOME_VISTA[novo]);
	return true;
	}

void Extras::mundoVistaPasso(float dt)
	{
	float alvo[3]={0.5f,0.5f,1.0f};
	if(vistaModo>=VISTA_PRINCIPAL&&vistaModo<=VISTA_MINI)
		{
		const float* r=VISTA_SALA[vistaModo];
		alvo[0]=0.5f*(r[0]+r[1]);
		alvo[1]=0.5f*(r[2]+r[3]);
		alvo[2]=std::min(1.0f/(r[1]-r[0]),1.0f/(r[3]-r[2])); // a sala inteira cabe nos dois sentidos
		}
	else if(vistaModo==VISTA_SEGUE)
		{
		int i=-1;
		for(int k=0;k<nNomes;++k)
			if(vistaNome==nomes[k])
				i=k;
		if(i<0)
			{
			/* a lista de membros mudou e ele não está mais nela: volta ao laboratório inteiro */
			anuncia("Mundo: vista tudo ("+vistaNome+" não está mais na lista)");
			vistaModo=VISTA_TUDO;
			vistaNome.clear();
			vistaDe[0]=vistaC[0];
			vistaDe[1]=vistaC[1];
			vistaDe[2]=vistaZ;
			vistaDesde=tempo;
			}
		else
			{
			bool fora=i>=quantos||tipo[i]<0.5f||escondido[i]||mEstado[i]==1||!mundoPronto;
			if(fora!=vistaFora)
				{
				/* saiu da sala: mostra o laboratório inteiro até ele voltar (e volta a segui-lo) */
				vistaFora=fora;
				vistaDe[0]=vistaC[0];
				vistaDe[1]=vistaC[1];
				vistaDe[2]=vistaZ;
				vistaDesde=tempo;
				anuncia("Mundo: vista seguindo "+vistaNome+(fora?" (fora do laboratório)":""));
				}
			if(fora)
				vistaSegOk=false;
			else
				{
				const float fx=(ax[i]-bmin[0])/(bmax[0]-bmin[0]),fy=(ay[i]-bmin[1])/(bmax[1]-bmin[1]);
				if(!vistaSegOk)
					{
					vistaSeg[0]=fx;
					vistaSeg[1]=fy;
					vistaSegOk=true;
					}
				else
					{
					/* acompanha o boneco com um pouco de atraso: o passo dele não sacode a imagem */
					const float k=1.0f-std::exp(-dt/0.35f);
					vistaSeg[0]+=(fx-vistaSeg[0])*k;
					vistaSeg[1]+=(fy-vistaSeg[1])*k;
					}
				alvo[0]=vistaSeg[0];
				alvo[1]=vistaSeg[1];
				alvo[2]=VISTA_ZOOM_SEGUE;
				}
			}
		}
	float s=float((tempo-vistaDesde)/VISTA_TROCA);
	if(s<1.0f)
		{
		s=std::max(0.0f,s);
		s=s*s*(3.0f-2.0f*s);
		vistaZ=std::exp(std::log(vistaDe[2])+(std::log(alvo[2])-std::log(vistaDe[2]))*s); // zoom por fator: a troca não acelera no fim
		vistaC[0]=vistaDe[0]+(alvo[0]-vistaDe[0])*s;
		vistaC[1]=vistaDe[1]+(alvo[1]-vistaDe[1])*s;
		}
	else
		{
		vistaZ=alvo[2];
		vistaC[0]=alvo[0];
		vistaC[1]=alvo[1];
		}
	/* nada de fora da planta na imagem */
	vistaZ=std::max(1.0f,vistaZ);
	const float meia=0.5f/vistaZ;
	for(int k=0;k<2;++k)
		vistaC[k]=std::max(meia,std::min(1.0f-meia,vistaC[k]));
	}
