/***********************************************************************
MundoPlanta.h - a planta do Mundo LAOB (19/11/2026): as três salas do
laboratório (principal, forense e miniauditório) e o corredor, tiradas
das 15 fotos que o Victor mandou.

Uma fonte só: os mesmos números servem ao shader (Extras.cpp, que recebe
MP_GLSL como constantes GLSL) e ao Mundo.cpp (assentos, portas e móveis
que as rotas contornam). Mudou um móvel aqui, muda nos dois.

Posições em F = fração da área iluminada: Fx 0..1 da esquerda para a
direita, Fy 0..1 de baixo para cima (Fy=1 = janelas). Raios em "cm da
planta": a largura da área iluminada vale 100 (na areia de verdade,
~59 cm: 1 cm da planta ~ 0,6 cm de areia ~ 10 pixels do projetor).
Ângulos: 0 aponta para +Fx e pi/2 para +Fy.

Também aqui (só C++): o desvio das rotas (mp::rota), pelos vãos das
portas e pelas faixas de passagem, sem atravessar móveis. O teste
testes/teste_mundo.cpp confere todas as rotas entre todos os pontos.
***********************************************************************/

#ifndef MUNDO_PLANTA_INCLUDED
#define MUNDO_PLANTA_INCLUDED

/* Paredes e salas (F) */
#define MP_CORREDOR_Y   0.120  /* corredor: Fy 0..0,120; parede H até 0,147 */
#define MP_PAREDE_H_Y   0.147
#define MP_PAREDE_B_Y   0.973
#define MP_EXTERNA_X    0.020
#define MP_PAREDE_C_X   0.980
#define MP_PAREDE_F_X0  0.255  /* parede entre a sala principal e os anexos */
#define MP_PAREDE_F_X1  0.275
#define MP_DIVISORIA_Y0 0.529  /* divisória forense x miniauditório */
#define MP_DIVISORIA_Y1 0.556

/* Móveis que ninguém atravessa: nome, Fx0, Fx1, Fy0, Fy1 (no shader: const vec4 MP_nome=vec4(Fx0,Fx1,Fy0,Fy1)) */
#define MP_MOVEIS(R) \
	R(BANC_H,      0.508,0.900,0.147,0.201) /* bancada da parede H (osciloscópio, torso de RCP, câmera do Willbert) */ \
	R(ARM_ALTO,    0.465,0.494,0.147,0.180) /* armário alto branco */ \
	R(GELADEIRA,   0.277,0.303,0.180,0.221) \
	R(ARM_ACO,     0.277,0.300,0.230,0.271) /* armários de aço */ \
	R(COLUNA,      0.275,0.310,0.271,0.312) \
	R(MESAS_ROXO,  0.275,0.300,0.296,0.428) /* mesas do banner roxo */ \
	R(NICHO,       0.282,0.320,0.296,0.312) /* mesa do nicho (f07) */ \
	R(TORRE_SCAN,  0.310,0.334,0.364,0.386) /* torre do scanner corporal */ \
	R(BANC_F,      0.277,0.307,0.659,0.890) /* bancada de eletrônica da parede F */ \
	R(ARCO_PE1,    0.349,0.371,0.698,0.720) /* pés do arco branco */ \
	R(ARCO_PE2,    0.349,0.371,0.830,0.852) \
	R(CLUSTER,     0.437,0.881,0.312,0.420) /* bancada do cluster */ \
	R(AREIA,       0.374,0.437,0.370,0.453) /* bancada da caixa de areia */ \
	R(EXTINTORES,  0.445,0.457,0.468,0.486) /* coluna com 2 extintores */ \
	R(MESA_C,      0.888,0.917,0.329,0.395) /* mesa da ponta C do cluster */ \
	R(CAIXAS,      0.945,0.966,0.312,0.742) /* 9 caixas pretas no chão, junto do quadro */ \
	R(CAIXA_ALTA,  0.951,0.967,0.276,0.298) \
	R(ILHA1,       0.779,0.857,0.560,0.857) /* ilha do Victor */ \
	R(ILHA2,       0.557,0.663,0.560,0.849) \
	R(PERSIANAS,   0.430,0.557,0.890,0.948) /* mesas junto das persianas */ \
	R(MESA_L1,     0.881,0.931,0.880,0.915) /* mesa em L do canto B/C */ \
	R(MESA_L2,     0.900,0.931,0.835,0.880) \
	R(ESCUDO,      0.938,0.973,0.857,0.932) /* armário do escudo */ \
	R(F_ARM,       0.053,0.227,0.147,0.178) /* forense: 5 armários altos */ \
	R(F_COFRE,     0.029,0.051,0.147,0.172) \
	R(F_MESA1,     0.020,0.051,0.231,0.330) \
	R(F_GAV,       0.020,0.044,0.334,0.361) /* gaveteiro com a caixa de som */ \
	R(F_MESA2,     0.020,0.051,0.365,0.483) /* com o PC aberto */ \
	R(F_RACK,      0.114,0.203,0.491,0.520) /* rack da TV */ \
	R(F_ARMB,      0.234,0.255,0.155,0.338) /* armários baixos com a urna */ \
	R(F_ARM3,      0.234,0.255,0.346,0.391) \
	R(M_GAV,       0.025,0.048,0.564,0.598) /* miniauditório: gaveteiro do PC RGB */ \
	R(M_SOFA,      0.217,0.250,0.656,0.815) \
	R(M_LONG,      0.045,0.160,0.564,0.592) /* longarinas embrulhadas da parede L */ \
	R(M_FTRAS,     0.042,0.193,0.930,0.958) /* fileira de trás */ \
	R(M_FFRENTE,   0.066,0.174,0.884,0.912) /* fileira da frente */ \
	R(M_COLS,      0.229,0.250,0.848,0.930) /* coluna de cadeiras da parede S */ \
	R(M_VIDRO,     0.115,0.137,0.694,0.768) /* mesa de vidro (posto do Gazziro) */

/* Móveis redondos: nome, Fx, Fy, raio (cm da planta) */
#define MP_REDONDOS(R) \
	R(CARTEADO,    0.367,0.246,2.4) /* mesa de carteado (feltro verde) */ \
	R(PLATAFORMA,  0.328,0.420,2.2) /* plataforma giratória do scanner */ \
	R(BOLA,        0.324,0.874,1.5) /* bola de pilates */ \
	R(F_PAPEIS,    0.133,0.285,2.7) /* forense: mesa dos papéis */ \
	R(F_TABULEIRO, 0.177,0.391,2.7) /* forense: mesa do tabuleiro de calibração */ \
	R(M_VENT,      0.039,0.940,1.0) /* miniauditório: ventilador */ \
	R(M_SOM,       0.205,0.925,1.0) /* miniauditório: caixa de som */

/* Assentos dos membros (onde a cadeira é desenhada): nome, Fx, Fy, ângulo de quem senta */
#define MP_ASSENTOS(R) \
	R(A0, 0.150,0.740, 3.1416)  /* Gazziro: cadeira bege da mesa de vidro, no miniauditório */ \
	R(A1, 0.820,0.447,-1.5708)  /* Lucas: lado B do cluster, ponta C */ \
	R(A2, 0.754,0.230,-1.5708)  /* Mario: bancada H, junto do osciloscópio (suposição) */ \
	R(A3, 0.875,0.808, 3.1416)  /* Victor: ilha 1, lado leste, o lugar mais perto da janela */ \
	R(A4, 0.762,0.808, 0.0)     /* os outros (membro novo) */ \
	R(A5, 0.677,0.725, 3.1416) \
	R(A6, 0.543,0.643, 0.0) \
	R(A7, 0.875,0.643, 3.1416)

/* Vãos das portas: entrada (na parede H, Fx), forense e miniauditório (na parede F, Fy) */
#define MP_PORTA_ENT_0  0.909  /* a dobradiça fica aqui (lado F); abre para o corredor */
#define MP_PORTA_ENT_1  0.945
#define MP_PORTA_FOR_0  0.445
#define MP_PORTA_FOR_1  0.505  /* dobradiça; abre para dentro da forense */
#define MP_PORTA_MINI_0 0.567  /* dobradiça; abre para dentro do miniauditório */
#define MP_PORTA_MINI_1 0.633

/* ---- as constantes em GLSL (o Extras.cpp junta MP_GLSL ao texto do shader) */
#define MP_TXT_(...) #__VA_ARGS__
#define MP_TXT(...) MP_TXT_(__VA_ARGS__)
#define MP_GLSL_RET(n,a,b,c,d) "const vec4 MP_" #n "=vec4(" #a "," #b "," #c "," #d ");\n"
#define MP_GLSL_RED(n,a,b,r) "const vec3 MP_" #n "=vec3(" #a "," #b "," #r ");\n"
#define MP_GLSL_ASS(n,a,b,g) "const vec3 MP_" #n "=vec3(" #a "," #b "," #g ");\n"
#define MP_GLSL_NUM(n) "const float " #n "=" MP_TXT(n) ";\n"
#define MP_GLSL \
	MP_MOVEIS(MP_GLSL_RET) MP_REDONDOS(MP_GLSL_RED) MP_ASSENTOS(MP_GLSL_ASS) \
	MP_GLSL_NUM(MP_CORREDOR_Y) MP_GLSL_NUM(MP_PAREDE_H_Y) MP_GLSL_NUM(MP_PAREDE_B_Y) MP_GLSL_NUM(MP_EXTERNA_X) \
	MP_GLSL_NUM(MP_PAREDE_C_X) MP_GLSL_NUM(MP_PAREDE_F_X0) MP_GLSL_NUM(MP_PAREDE_F_X1) \
	MP_GLSL_NUM(MP_DIVISORIA_Y0) MP_GLSL_NUM(MP_DIVISORIA_Y1) \
	MP_GLSL_NUM(MP_PORTA_ENT_0) MP_GLSL_NUM(MP_PORTA_ENT_1) MP_GLSL_NUM(MP_PORTA_FOR_0) MP_GLSL_NUM(MP_PORTA_FOR_1) \
	MP_GLSL_NUM(MP_PORTA_MINI_0) MP_GLSL_NUM(MP_PORTA_MINI_1)

/* ---- só C++: salas e rotas */
#ifdef __cplusplus
#include <cmath>

namespace mp {

enum Sala {CORREDOR=0,PRINCIPAL=1,FORENSE=2,MINI=3};

inline int sala(float fx,float fy)
	{
	if(fy<float(MP_CORREDOR_Y))
		return CORREDOR;
	if(fx<float(MP_PAREDE_F_X0))
		return fy<float(MP_DIVISORIA_Y0)?FORENSE:MINI;
	return PRINCIPAL;
	}

/* Pontos aonde os bonecos vão (F), para onde ficam virados e o texto do log */
struct Ponto {float x,y,ang; const char* txt;};
enum {P_AREIA=0,P_QUADRO=1,P_CLUSTER=2,P_SWITCH=3,P_CAMERAS=4,P_SCANNER=5,P_ARCO=6,P_GELADEIRA=7,P_CARTEADO=8,
      P_BANCADA=9,P_FUNDO=10,P_URNA=11,P_TABULEIRO=12,P_PAPEIS=13,P_POSTER=14,P_TV=15,P_MUSICA=16,P_PCABERTO=17,
      P_APRESENTA=18,P_SOFA=19,P_PLATEIA=20,P_VIDRO=21,P_RCP=22,P_WILLBERT=23,P_AULA=24,N_PONTOS=25};
const Ponto PONTO[N_PONTOS]={
	{0.400f,0.470f,-1.5707963f,"foi até a caixa de areia"},
	{0.925f,0.700f,0.0f,"foi dar aula no quadro"},                    // (19/11/2026, revisão: 0,580 -> 0,700; a plaquinha do Victor na turma cobria a dele)
	{0.720f,0.447f,-1.5707963f,"foi ver o cluster"},
	{0.600f,0.290f,1.5707963f,"foi mexer no switch do cluster"},
	{0.805f,0.290f,1.5707963f,"foi ajustar as câmeras"},
	{0.340f,0.333f,1.5707963f,"foi escanear o manequim"},
	{0.325f,0.775f,3.1415927f,"foi passar pelo arco"},                      // (entra pelo lado de lá: 0,395; 0,775)
	{0.318f,0.200f,3.1415927f,"foi até a geladeira"},
	{0.367f,0.294f,-1.5707963f,"foi para a mesa de carteado"},
	{0.642f,0.221f,-1.5707963f,"foi para a bancada de eletrônica"},
	{0.322f,0.740f,3.1415927f,"foi para a bancada do fundo"},
	{0.222f,0.262f,0.0f,"foi ver a urna eletrônica"},
	{0.216f,0.391f,3.1415927f,"foi calibrar a câmera no tabuleiro"},
	{0.172f,0.285f,3.1415927f,"foi revisar os papéis na forense"},
	{0.045f,0.190f,3.1415927f,"foi ver o pôster Minority Report"},
	{0.166f,0.470f,1.5707963f,"foi ligar a TV da forense"},
	{0.060f,0.348f,3.1415927f,"foi pôr música na caixa de som"},
	{0.067f,0.455f,3.1415927f,"foi mexer no PC aberto"},
	{0.075f,0.765f,0.0f,"foi apresentar no miniauditório"},          // (revisão: 0,053 -> 0,075; ele tapava o slide e o nome saía da areia)
	{0.234f,0.735f,3.1415927f,"foi assistir no sofá"},                      // vagas: Fy 0,689 / 0,735 / 0,781
	{0.239f,0.862f,3.1415927f,"foi sentar na plateia"},                     // vagas: Fy 0,862 / 0,888 / 0,915
	{0.160f,0.695f,2.6f,"foi para a mesa de vidro"},
	{0.684f,0.215f,-1.5707963f,"foi treinar RCP no boneco"},
	{0.880f,0.215f,-1.5707963f,"foi mexer na câmera do Willbert"},
	{0.868f,0.440f,0.0f,"foi assistir a aula no quadro"}};          // vagas: (0,868; 0,440), (0,885; 0,530), (0,872; 0,620) (19/11/2026: mais abertas, os corpos se encostavam)

/* Lugar de verdade de um ponto com vagas (sofá, plateia e aula têm 3) */
inline void lugar(int p,int vaga,float& fx,float& fy)
	{
	fx=PONTO[p].x;
	fy=PONTO[p].y;
	vaga=((vaga%3)+3)%3;
	if(p==P_SOFA)
		fy=0.689f+0.046f*float(vaga);
	else if(p==P_PLATEIA)
		fy=0.862f+0.0265f*float(vaga);
	else if(p==P_AULA)
		{
		const float AX[3]={0.868f,0.885f,0.872f},AY[3]={0.440f,0.530f,0.620f}; // espalhados: um boneco não cobre o outro
		fx=AX[vaga];
		fy=AY[vaga];
		}
	}

/* Obstáculos em "cm da planta" (x*100, y*100*asp; asp = altura/largura da área iluminada) */
struct Obst {float x0,x1,y0,y1,cx,cy,r; bool redondo;};

#define MP_CPP_RET(n,a,b,c,d) {float(a),float(b),float(c),float(d),0.0f,0.0f,0.0f,false},
#define MP_CPP_RED(n,a,b,r) {0.0f,0.0f,0.0f,0.0f,float(a),float(b),float(r),true},
static const Obst MOVEIS[]={MP_MOVEIS(MP_CPP_RET) MP_REDONDOS(MP_CPP_RED)};
#undef MP_CPP_RET
#undef MP_CPP_RED
static const int N_MOVEIS=int(sizeof(MOVEIS)/sizeof(MOVEIS[0]));

/* Paredes como retângulos (F), com os vãos das portas abertos */
static const float PAREDES[][4]={
	{0.0f,float(MP_PORTA_ENT_0),float(MP_CORREDOR_Y),float(MP_PAREDE_H_Y)},  /* parede H, até a porta de entrada */
	{float(MP_PORTA_ENT_1),1.0f,float(MP_CORREDOR_Y),float(MP_PAREDE_H_Y)},  /* e depois dela */
	{0.0f,1.0f,float(MP_PAREDE_B_Y),1.0f},                                     /* parede B (janelas) */
	{0.0f,float(MP_EXTERNA_X),float(MP_PAREDE_H_Y),1.0f},                      /* parede externa dos anexos */
	{float(MP_PAREDE_C_X),1.0f,float(MP_PAREDE_H_Y),1.0f},                     /* parede C (quadro) */
	{float(MP_PAREDE_F_X0),float(MP_PAREDE_F_X1),float(MP_PAREDE_H_Y),float(MP_PORTA_FOR_0)},
	{float(MP_PAREDE_F_X0),float(MP_PAREDE_F_X1),float(MP_PORTA_FOR_1),float(MP_PORTA_MINI_0)},
	{float(MP_PAREDE_F_X0),float(MP_PAREDE_F_X1),float(MP_PORTA_MINI_1),float(MP_PAREDE_B_Y)},
	{0.0f,float(MP_PAREDE_F_X0),float(MP_DIVISORIA_Y0),float(MP_DIVISORIA_Y1)}};
static const int N_PAREDES=int(sizeof(PAREDES)/sizeof(PAREDES[0]));

/* Pontos de passagem (F): faixas livres entre os móveis, os dois lados de cada porta e o corredor */
static const float NOS[][2]={
	/* corredor e entrada */
	{0.927f,0.050f},{0.927f,0.200f},{0.990f,0.050f},
	/* sala principal: faixa da bancada H e o corredor F (entre o scanner e a caixa de areia) */
	{0.420f,0.200f},{0.420f,0.272f},{0.600f,0.272f},{0.805f,0.272f},{0.932f,0.272f},
	{0.400f,0.302f},{0.362f,0.335f},{0.362f,0.470f},
	/* corredor C (entre a mesa da ponta C e as caixas pretas) e o corredor central */
	{0.932f,0.450f},{0.362f,0.505f},{0.500f,0.505f},{0.720f,0.505f},{0.915f,0.505f},
	/* faixas entre as ilhas e em volta delas */
	{0.915f,0.820f},{0.720f,0.870f},{0.500f,0.870f},{0.870f,0.870f},
	/* lado F: portas e arco */
	{0.300f,0.475f},{0.300f,0.600f},{0.400f,0.600f},{0.400f,0.700f},{0.400f,0.880f},
	/* forense: porta, faixa Fy 0,475 e o miolo */
	{0.228f,0.475f},{0.180f,0.475f},{0.120f,0.475f},{0.075f,0.475f},{0.075f,0.355f},
	{0.080f,0.205f},{0.200f,0.205f},{0.215f,0.335f},
	/* miniauditório: porta, passagem e frente da tela */
	{0.228f,0.600f},{0.190f,0.640f},{0.060f,0.640f},{0.185f,0.835f},{0.085f,0.800f}};
static const int N_NOS=int(sizeof(NOS)/sizeof(NOS[0]));

/* O segmento a-b (cm da planta) passa a menos de "folga" do obstáculo? (retângulo: recorte de Liang-Barsky
   no retângulo inflado; redondo: distância ponto-segmento) */
inline bool cruzaRet(float ax,float ay,float bx,float by,float x0,float x1,float y0,float y1)
	{
	float t0=0.0f,t1=1.0f,dx=bx-ax,dy=by-ay;
	const float p[4]={-dx,dx,-dy,dy},q[4]={ax-x0,x1-ax,ay-y0,y1-ay};
	for(int k=0;k<4;++k)
		{
		if(p[k]==0.0f)
			{
			if(q[k]<0.0f)
				return false;
			continue;
			}
		float t=q[k]/p[k];
		if(p[k]<0.0f) {if(t>t1) return false; if(t>t0) t0=t;}
		else {if(t<t0) return false; if(t<t1) t1=t;}
		}
	return t0<=t1;
	}
inline float distSeg(float px,float py,float ax,float ay,float bx,float by)
	{
	float dx=bx-ax,dy=by-ay,l=dx*dx+dy*dy;
	float t=l>0.0f?((px-ax)*dx+(py-ay)*dy)/l:0.0f;
	t=t<0.0f?0.0f:(t>1.0f?1.0f:t);
	float ex=ax+t*dx-px,ey=ay+t*dy-py;
	return std::sqrt(ex*ex+ey*ey);
	}

/* Livre entre a e b (F)? folga em cm da planta. Um ponta que está dentro de um móvel (o sofá, o
   assento de uma longarina) não conta esse móvel: é para lá mesmo que o boneco vai. */
inline bool livre(float afx,float afy,float bfx,float bfy,float asp,float folga)
	{
	const float ky=100.0f*asp;
	float ax=afx*100.0f,ay=afy*ky,bx=bfx*100.0f,by=bfy*ky;
	for(int k=0;k<N_MOVEIS;++k)
		{
		const Obst& o=MOVEIS[k];
		if(o.redondo)
			{
			float cx=o.cx*100.0f,cy=o.cy*ky;
			float da=std::sqrt((ax-cx)*(ax-cx)+(ay-cy)*(ay-cy)),db=std::sqrt((bx-cx)*(bx-cx)+(by-cy)*(by-cy));
			float f=folga;
			if(da<o.r+f||db<o.r+f)
				{
				if(da<o.r||db<o.r)
					continue;
				f=0.0f;
				}
			if(distSeg(cx,cy,ax,ay,bx,by)<o.r+f)
				return false;
			}
		else
			{
			float x0=o.x0*100.0f,x1=o.x1*100.0f,y0=o.y0*ky,y1=o.y1*ky,f=folga;
			bool dentroA=ax>x0-f&&ax<x1+f&&ay>y0-f&&ay<y1+f,dentroB=bx>x0-f&&bx<x1+f&&by>y0-f&&by<y1+f;
			if(dentroA||dentroB)
				{
				if((ax>x0&&ax<x1&&ay>y0&&ay<y1)||(bx>x0&&bx<x1&&by>y0&&by<y1))
					continue;
				f=0.0f;
				}
			if(cruzaRet(ax,ay,bx,by,x0-f,x1+f,y0-f,y1+f))
				return false;
			}
		}
	/* nas paredes a folga é maior: só se atravessa um vão de porta quase de frente, pelo meio dele */
	const float fp=2.5f*folga;
	for(int k=0;k<N_PAREDES;++k)
		if(cruzaRet(ax,ay,bx,by,PAREDES[k][0]*100.0f-fp,PAREDES[k][1]*100.0f+fp,PAREDES[k][2]*ky-fp,PAREDES[k][3]*ky+fp))
			return false;
	return true;
	}

/* Rota de a até b (F) pelos pontos de passagem (menor caminho entre os que enxergam um ao outro).
   Devolve quantos pontos escreveu em saida (o último é b); 0 = não achou caminho (não deve acontecer). */
inline int rota(float ax,float ay,float bx,float by,float asp,float saida[][2],int maximo,float folga=0.6f)
	{
	if(livre(ax,ay,bx,by,asp,folga))
		{
		saida[0][0]=bx;
		saida[0][1]=by;
		return 1;
		}
	const int N=N_NOS+2; /* nós, a origem (N_NOS) e o destino (N_NOS+1) */
	float px[N_NOS+2],py[N_NOS+2],dist[N_NOS+2];
	int ant[N_NOS+2];
	bool feito[N_NOS+2];
	for(int k=0;k<N_NOS;++k)
		{
		px[k]=NOS[k][0];
		py[k]=NOS[k][1];
		}
	px[N_NOS]=ax; py[N_NOS]=ay; px[N_NOS+1]=bx; py[N_NOS+1]=by;
	for(int k=0;k<N;++k)
		{
		dist[k]=1e9f;
		ant[k]=-1;
		feito[k]=false;
		}
	dist[N_NOS]=0.0f;
	const float ky=100.0f*asp;
	for(;;)
		{
		int u=-1;
		for(int k=0;k<N;++k)
			if(!feito[k]&&dist[k]<1e8f&&(u<0||dist[k]<dist[u]))
				u=k;
		if(u<0||u==N_NOS+1)
			break;
		feito[u]=true;
		for(int v=0;v<N;++v)
			{
			if(feito[v]||v==N_NOS)
				continue;
			float dx=(px[v]-px[u])*100.0f,dy=(py[v]-py[u])*ky;
			float d=dist[u]+std::sqrt(dx*dx+dy*dy);
			if(d<dist[v]&&livre(px[u],py[u],px[v],py[v],asp,folga))
				{
				dist[v]=d;
				ant[v]=u;
				}
			}
		}
	if(ant[N_NOS+1]<0)
		return folga>0.05f?rota(ax,ay,bx,by,asp,saida,maximo,0.0f):0;
	int caminho[N_NOS+2],n=0;
	for(int k=N_NOS+1;k!=N_NOS&&k>=0;k=ant[k])
		caminho[n++]=k;
	if(n>maximo)
		n=maximo; /* (não acontece: as rotas da planta têm até ~10 pontos) */
	for(int j=0;j<n;++j)
		{
		int k=caminho[n-1-j];
		saida[j][0]=px[k];
		saida[j][1]=py[k];
		}
	return n;
	}

}
#endif

#endif
