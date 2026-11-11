/***********************************************************************
Extras - caça ao tesouro, peixes e carrinhos sobre o relevo (Projetor-LAOB).
***********************************************************************/

#include "Extras.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Geometry/ProjectiveTransformation.h>

namespace {
const float SEM=-1000.0f; // célula sem medida
const float PI=3.14159265f;
}

Extras::Extras(void)
	:celula(1.5f),gw(0),gh(0),mapaOk(false),referencia(0.0f),
	 tesouro(false),achado(false),tipoTesouro(1),tx(0),ty(0),traio(5.0f),tz(0),quandoAchou(0.0),
	 profundidade(3.0f),reaparece(20.0f),quantosAchados(0),
	 modo(NADA),quantos(0),nivelAgua(SEM),semente(12345u),tempo(0.0),efeito(SEM_EFEITO),
	 labirinto(false),labEstado(0),lsx(0),lsy(0),ltx(0),lty(0),lraio(4.0f),labVazao(1.0f),
	 labInicio(0.0),labChegou(0.0),labRodadas(0),labDificuldade(2),labZerarAgua(false)
	{
	bmin[0]=bmin[1]=-10.0f;
	bmax[0]=bmax[1]=10.0f;
	for(int i=0;i<16;++i)
		caixa[i]=(i%5==0)?1.0f:0.0f;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		ax[i]=ay[i]=ang[i]=vel[i]=vb[i]=parado[i]=0.0f;
		tipo[i]=0.0f;
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
			alt[i]=soma[i]/float(conta[i]);
			todas.push_back(alt[i]);
			}
	if(todas.size()<10)
		return;
	std::nth_element(todas.begin(),todas.begin()+todas.size()/2,todas.end());
	referencia=todas[todas.size()/2];
	mapaOk=true;
	/* O lago dos peixes: tudo 1,5 cm abaixo da areia "normal" (cavar um buraco cria um lago): */
	nivelAgua=(modo&PEIXES)?referencia-1.5f:SEM;
	}

float Extras::altura(float x,float y) const
	{
	int cx=int((x-bmin[0])/celula),cy=int((y-bmin[1])/celula);
	if(!mapaOk||cx<0||cy<0||cx>=gw||cy>=gh)
		return SEM;
	return alt[cy*gw+cx];
	}

bool Extras::dentro(float x,float y,float margem) const
	{
	return x>bmin[0]+margem&&x<bmax[0]-margem&&y>bmin[1]+margem&&y<bmax[1]-margem;
	}

bool Extras::agua(float x,float y) const
	{
	float h=altura(x,y);
	return h>SEM+1.0f&&h<nivelAgua&&dentro(x,y,1.0f);
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
		bool ok=queAgua?agua(x,y):(nivelAgua<SEM+1.0f||h>=nivelAgua+0.5f);
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
		{"classico",CHUVA},{"vulcao",LAVA},{"inverno",NEVE},{"marte",POEIRA},{"deserto",AREIA},
		{"oceano",ONDAS},{"arco-iris",BRILHO},{"mapa",SEM_EFEITO},{"nada",SEM_EFEITO}};
	for(unsigned i=0;i<sizeof(tabela)/sizeof(tabela[0]);++i)
		if(std::strcmp(nome,tabela[i].tema)==0)
			return tabela[i].efeito;
	return -1;
	}

/* --------------------------------------------------------------- labirinto */

void Extras::novoLabirinto(int dificuldade)
	{
	labDificuldade=std::max(1,std::min(3,dificuldade));
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
	bool peixe=tipo[i]<2.0f;
	float x,y;
	if(!sorteiaPonto(peixe,x,y))
		{
		vel[i]=0.0f; // fica escondido até haver lugar (ex.: cavar um lago)
		return;
		}
	ax[i]=x;
	ay[i]=y;
	ang[i]=aleatorio()*2.0f*PI;
	vb[i]=peixe?(4.0f+aleatorio()*3.0f):(6.0f+aleatorio()*4.0f);
	vel[i]=vb[i];
	parado[i]=0.0f;
	}

void Extras::povoa(int novoModo,int quantidade)
	{
	modo=novoModo;
	quantos=std::max(0,std::min(int(MAX_AGENTES),quantidade));
	if(modo&PEIXES)
		nivelAgua=referencia-1.5f;
	else
		nivelAgua=SEM;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		tipo[i]=0.0f;
		vel[i]=0.0f;
		if(i>=quantos||modo==NADA)
			continue;
		bool peixe=(modo==PEIXES)||(modo==TODOS&&i%2==0);
		tipo[i]=(peixe?1.0f:2.0f)+0.05f+0.9f*aleatorio(); // a fração é a cor
		if(mapaOk)
			nasce(i);
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
	if(!mapaOk||modo==NADA)
		return;
	dt=std::min(dt,0.1f);
	for(int i=0;i<quantos;++i)
		{
		if(tipo[i]<0.5f)
			continue;
		bool peixe=tipo[i]<2.0f;
		if(vel[i]==0.0f)
			{
			if(aleatorio()<dt) // tenta aparecer de vez em quando
				nasce(i);
			continue;
			}
		float c0=std::cos(ang[i]),s0=std::sin(ang[i]);
		if(peixe)
			{
			if(!agua(ax[i],ay[i])) // o lago secou embaixo dele: reaparece em outro lugar
				{
				nasce(i);
				continue;
				}
			/* Cardume: afasta dos muito perto, nada junto com os vizinhos e puxa para o grupo */
			float vx=c0,vy=s0,sepX=0.0f,sepY=0.0f,aliX=0.0f,aliY=0.0f,cx=0.0f,cy=0.0f;
			int n=0;
			for(int j=0;j<quantos;++j)
				{
				if(j==i||tipo[j]<0.5f||tipo[j]>=2.0f||vel[j]==0.0f)
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
			/* Passeio: cada peixe vira devagar para um lado e para o outro */
			float vaga=0.8f*std::sin(float(tempo)*0.6f+float(i)*1.7f)*dt;
			float desejo=std::atan2(vy,vx)+vaga;
			/* Desvia das margens: a primeira direção livre perto da desejada */
			const float desvios[]={0.0f,0.4f,-0.4f,0.8f,-0.8f,1.3f,-1.3f,1.9f,-1.9f,2.6f,-2.6f,PI};
			bool livre=false;
			for(int k=0;k<12;++k)
				{
				float a=desejo+desvios[k];
				if(agua(ax[i]+std::cos(a)*3.0f,ay[i]+std::sin(a)*3.0f)&&agua(ax[i]+std::cos(a)*1.5f,ay[i]+std::sin(a)*1.5f))
					{
					desejo=a;
					livre=true;
					break;
					}
				}
			float giro=livre?difAng(desejo-ang[i]):2.0f;
			ang[i]=difAng(ang[i]+std::max(-2.8f*dt,std::min(2.8f*dt,giro)));
			/* Nadadas: acelera e desacelera de leve, mais devagar quando está virando */
			float alvoVel=vb[i]*(0.85f+0.25f*std::sin(float(tempo)*2.3f+float(i)))*(1.0f-0.35f*std::min(1.0f,std::abs(giro)));
			vel[i]+=(alvoVel-vel[i])*std::min(1.0f,2.0f*dt);
			vel[i]=std::max(vel[i],0.5f);
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			if(agua(nx,ny))
				{
				ax[i]=nx;
				ay[i]=ny;
				parado[i]=0.0f;
				}
			else if((parado[i]+=dt)>4.0f) // preso numa poça: some e aparece num lago de verdade
				nasce(i);
			}
		else
			{
			/* Carrinho: entre as direções possíveis, a que segue mais baixo (o vale), sem subir
			   rampa íngreme, sem entrar no lago e sem sair da área iluminada: */
			float h0=altura(ax[i],ay[i]);
			const float desvios[]={0.0f,0.35f,-0.35f,0.7f,-0.7f,1.1f,-1.1f};
			float melhor=1e9f,escolha=PI,hEscolha=h0;
			for(int k=0;k<7;++k)
				{
				float a=ang[i]+desvios[k];
				float px=ax[i]+std::cos(a)*4.0f,py=ay[i]+std::sin(a)*4.0f;
				float h=altura(px,py);
				if(h<SEM+1.0f||!dentro(px,py,2.0f)||agua(px,py)||(h0>SEM+1.0f&&h-h0>2.0f))
					continue;
				/* vale: baixo, mas sem despencar (segue a curva de nível); longe dos outros carros */
				float nota=0.35f*h+0.6f*std::abs(h-h0)+0.8f*std::abs(desvios[k]);
				for(int j=0;j<quantos;++j)
					if(j!=i&&tipo[j]>=2.0f&&vel[j]!=0.0f)
						{
						float d=std::sqrt((ax[j]-px)*(ax[j]-px)+(ay[j]-py)*(ay[j]-py));
						if(d<7.0f)
							nota+=2.5f*(1.0f-d/7.0f);
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
				if(tipo[j]<2.0f||vel[j]==0.0f)
					continue;
				float dx=ax[j]-ax[i],dy=ay[j]-ay[i];
				float adiante=dx*c0+dy*s0,lado=-dx*s0+dy*c0;
				if(adiante>0.0f&&adiante<7.0f&&std::abs(lado)<3.0f)
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
			if(frente<7.0f)
				alvoVel*=std::max(0.0f,(frente-3.0f)/4.0f);
			vel[i]+=(alvoVel-vel[i])*std::min(1.0f,2.5f*dt);
			vel[i]=std::max(vel[i],0.05f); // 0 é "escondido"
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			bool andou=dentro(nx,ny,1.5f)&&!agua(nx,ny);
			if(andou)
				{
				ax[i]=nx;
				ay[i]=ny;
				}
			/* Carrinhos lado a lado (o freio só vale para o da frente): empurra para se separarem */
			for(int j=0;j<quantos;++j)
				{
				if(j==i||tipo[j]<2.0f||vel[j]==0.0f)
					continue;
				float dx=ax[i]-ax[j],dy=ay[i]-ay[j],d2=dx*dx+dy*dy;
				if(d2<20.0f&&d2>1e-4f) // ~4,5 cm (um carrinho tem ~4,3 x 2,1 cm)
					{
					float d=std::sqrt(d2),empurra=std::min(0.5f,(4.5f-d)*0.25f);
					float px=ax[i]+dx/d*empurra,py=ay[i]+dy/d*empurra;
					if(dentro(px,py,1.5f)&&!agua(px,py))
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

/* ----------------------------------------------------------------- shader */

const char* Extras::nomesUniforms[NUM_UNIFORMS]={"extrasCaixa","extrasAg","extrasTesouro","extrasInfo","extrasEfeito",
	"extrasLab","extrasLab2"};

const char* Extras::glslVertexUniforms=R"GLSL(
uniform mat4 extrasCaixa; // espaço da câmera -> caixa (cm)
varying vec3 extrasPos;
)GLSL";

const char* Extras::glslVertexMain=R"GLSL(
extrasPos=(extrasCaixa*vertexCc).xyz;
)GLSL";

const char* Extras::glslFragmentUniforms=R"GLSL(
uniform vec4 extrasAg[24];   // x, y (cm), ângulo, tipo (1.xx peixe, 2.xx carrinho; xx = cor)
uniform vec4 extrasTesouro;  // x, y, raio (cm), altura a alcançar cavando (cm)
uniform vec4 extrasInfo;     // tempo (s), achou, tesouro ligado, nível do lago (cm; -1000 = sem lago)
uniform vec4 extrasEfeito;   // efeito do tema (0 nenhum, 1 lava, 2 neve, 3 poeira, 4 areia, 5 ondas, 6 brilho, 7 chuva), altura média da areia (cm)
uniform vec4 extrasLab;      // labirinto: nascente (x, y) e alvo (x, y), cm
uniform vec4 extrasLab2;     // labirinto: raio do alvo, estado (0 desligado, 1 correndo, 2 chegou), segundos no estado
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
	float ef=extrasEfeito.x;
	float ref=extrasEfeito.y;
	if(ef>0.5&&ef<1.5)
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
	if(ep.z<extrasInfo.w)
		{
		float prof=clamp((extrasInfo.w-ep.z)/4.0,0.0,1.0);
		float onda=0.05*sin(et*2.0+ep.x*0.9+ep.y*0.6);
		baseColor=vec4(mix(vec3(0.30,0.72,0.90),vec3(0.03,0.20,0.50),prof)+vec3(onda),1.0);
		}
	if(extrasInfo.z>0.5)
		{
		float d=distance(ep.xy,extrasTesouro.xy);
		if(d<extrasTesouro.z)
			{
			float falta=ep.z-extrasTesouro.w;
			if(extrasInfo.z<1.5)
				{
				if(falta<=0.0||extrasInfo.y>0.5)
					{
					vec2 q=(ep.xy-extrasTesouro.xy)/extrasTesouro.z;
					float brilho=0.75+0.25*sin(et*6.0-length(q)*8.0);
					vec3 cor=vec3(0.95,0.75,0.15)*brilho;
				if(abs(q.x)<0.7&&abs(q.y)<0.45)
					{
					cor=vec3(0.45,0.25,0.08);
					if(abs(q.x)>0.6||abs(q.y)>0.37||abs(q.y)<0.05)
						cor=vec3(1.0,0.8,0.2)*brilho;
					if(abs(q.x)<0.08&&abs(q.y)<0.12)
						cor=vec3(0.15);
					}
					float estrela=pow(max(0.0,sin(atan(q.y,q.x)*6.0+et*2.0)),30.0)*(1.0-length(q));
					baseColor=vec4(cor+vec3(estrela),1.0);
					}
				else if(falta<2.5)
					baseColor=mix(baseColor,vec4(1.0,0.82,0.25,1.0),(1.0-falta/2.5)*(0.35+0.25*sin(et*5.0)));
				}
			else
				{
				vec2 q=(ep.xy-extrasTesouro.xy)/extrasTesouro.z;
				float r=length(q);
				if(falta<=0.0||extrasInfo.y>0.5)
					{
					float ru=exRuido(q*7.0);
					vec3 rocha=mix(vec3(0.30,0.26,0.21),vec3(0.40,0.34,0.27),ru);
					float fissura=smoothstep(0.08,0.0,abs(sin(q.y*16.0+ru*2.0)));
					rocha=mix(rocha,vec3(0.18,0.14,0.11),fissura*0.5);

					float osso=0.0;
					int tf=int(extrasInfo.z+0.5);
					if(tf<=2)
						{
						vec2 qc=q-vec2(0.15,-0.05);
						float cabeca=smoothstep(0.40,0.32,length(qc*vec2(1.2,1.7)));
						float orbita=smoothstep(0.06,0.09,length((qc-vec2(0.06,0.12))*vec2(1.3,1.0)));
						float narina=smoothstep(0.04,0.07,length((qc-vec2(0.25,0.06))*vec2(1.2,1.0)));
						float boca=step(qc.y,-0.02)*step(-0.18,qc.y)*step(-0.25,qc.x)*step(qc.x,0.32);
						float dentes=abs(sin(qc.x*45.0))*boca;
						float cranio=max(0.0,cabeca*orbita*narina-boca*0.8)+dentes*0.9;
						float mand=smoothstep(0.06,0.02,abs(qc.y+0.16+qc.x*0.12))*step(-0.25,qc.x)*step(qc.x,0.30);
						float vert=smoothstep(0.06,0.02,abs(q.y+0.1+q.x*0.15))*step(-0.85,q.x)*step(q.x,-0.1);
						float cost=abs(sin(q.x*22.0))*smoothstep(0.28,0.0,abs(q.y+0.25))*step(-0.75,q.x)*step(q.x,-0.15);
						osso=max(max(cranio,mand),max(vert,cost));
						}
					else if(tf==3)
						{
						vec2 qc=q;
						float escudo=smoothstep(0.48,0.40,length((qc+vec2(0.22,0.0))*vec2(1.5,1.0)))*step(qc.x,-0.05);
						float cabeca=smoothstep(0.35,0.28,length(qc*vec2(1.3,1.5)));
						float chifre1=smoothstep(0.06,0.01,abs(qc.y-qc.x*0.7-0.18))*step(0.0,qc.x)*step(qc.x,0.45);
						float chifre2=smoothstep(0.06,0.01,abs(qc.y+qc.x*0.7+0.18))*step(0.0,qc.x)*step(qc.x,0.45);
						float chifrenas=smoothstep(0.05,0.01,abs(qc.y))*step(0.22,qc.x)*step(qc.x,0.48);
						osso=max(max(escudo,cabeca),max(max(chifre1,chifre2),chifrenas));
						}
					else if(tf==4)
						{
						vec2 qc=q;
						float crista=smoothstep(0.07,0.02,abs(qc.y-qc.x*0.5+0.15))*step(-0.5,qc.x)*step(qc.x,0.0);
						float bico=smoothstep(0.06,0.02,abs(qc.y+qc.x*0.15))*step(0.0,qc.x)*step(qc.x,0.65)*(1.0-qc.x*1.1);
						float asa1=smoothstep(0.06,0.02,abs(qc.y-qc.x*0.8-0.2))*step(-0.6,qc.x)*step(qc.x,0.3);
						float asa2=smoothstep(0.06,0.02,abs(qc.y+qc.x*0.8+0.2))*step(-0.6,qc.x)*step(qc.x,0.3);
						osso=max(max(crista,bico),max(asa1,asa2));
						}
					else
						{
						vec2 qc=q*1.2;
						float d1=smoothstep(0.12,0.04,length((qc-vec2(0.0,0.35))*vec2(1.8,0.9)-vec2(0.0,qc.y*0.3)));
						float d2=smoothstep(0.12,0.04,length((qc-vec2(0.28,-0.15))*vec2(1.2,1.4)));
						float d3=smoothstep(0.12,0.04,length((qc-vec2(-0.28,-0.15))*vec2(1.2,1.4)));
						float calcanhar=smoothstep(0.22,0.08,length((qc+vec2(0.0,0.25))*vec2(1.3,1.1)));
						osso=max(max(d1,d2),max(d3,calcanhar));
						}

					vec3 corOsso=mix(vec3(0.92,0.89,0.78),vec3(0.78,0.72,0.60),ru);
					vec3 fossil=mix(rocha,corOsso,clamp(osso,0.0,1.0));

					float aura=extrasInfo.y>0.5?(0.75+0.25*sin(et*5.0))*(1.0-r):0.0;
					float brilho=pow(max(0.0,sin(atan(q.y,q.x)*6.0+et*1.8)),25.0)*(1.0-r)*0.8;
					baseColor=vec4(fossil+vec3(aura*0.35,aura*0.28,aura*0.1)+vec3(brilho),1.0);
					}
				else if(falta<3.0)
					{
					float prox=1.0-falta/3.0;
					float gridX=smoothstep(0.05,0.0,abs(fract(q.x*3.5)-0.5));
					float gridY=smoothstep(0.05,0.0,abs(fract(q.y*3.5)-0.5));
					float grid=(gridX+gridY)*(1.0-r)*prox;
					vec3 ambar=vec3(0.85,0.68,0.42);
					baseColor=mix(baseColor,vec4(ambar+vec3(grid*0.4),1.0),prox*(0.35+0.25*sin(et*4.0)));
					}
				}
			}
		}
	/* Peixes e carrinhos (vistos de cima, com sombra): */
	for(int i=0;i<24;++i)
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
				vec3 pc=cor;
				if(corpo<1.0)
					{
					pc=mix(cor*1.25,cor*0.7,smoothstep(0.0,0.9,abs(f.y)));      // lombo claro, bordas escuras
					if(hue>0.5)
						pc*=0.85+0.15*step(0.0,sin(f.x*4.5));                    // listras em metade dos peixes
					pc+=vec3(0.35)*smoothstep(0.35,0.0,distance(f,vec2(0.9,0.25))); // brilho
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
			else if(sombra<1.0)
				baseColor.rgb*=0.72;
			}
		else
			{
			/* Carrinho: carroceria arredondada, cabine, vidros, rodas, faróis com facho e lanternas */
			float carro=exCaixa(l,vec2(2.15,1.05),0.5);
			float sombra=exCaixa(ls,vec2(2.15,1.05),0.5);
			vec2 roda=vec2(abs(l.x)-1.3,abs(l.y)-1.05);
			bool rodas=abs(roda.x)<0.42&&abs(roda.y)<0.28;
			float facho=l.x>2.0&&l.x<6.5?smoothstep(1.2+0.35*(l.x-2.0),0.0,abs(l.y))*smoothstep(6.5,2.0,l.x):0.0;
			if(carro<0.0)
				{
				vec3 cc=mix(cor*1.15,cor*0.75,smoothstep(0.0,1.0,abs(l.y)));
				float cabine=exCaixa(l-vec2(-0.25,0.0),vec2(0.95,0.78),0.35);
				if(cabine<0.0)
					{
					cc=cor*0.85;
					if(l.x>0.3)
						cc=mix(vec3(0.55,0.75,0.95),vec3(0.9,0.97,1.0),smoothstep(0.3,0.75,l.x));   // para-brisa
					else if(l.x<-0.9)
						cc=vec3(0.45,0.6,0.75);                                                    // vidro de trás
					}
				if(l.x>1.75&&abs(l.y)>0.45&&abs(l.y)<0.95)
					cc=vec3(1.0,0.98,0.75);                                                      // faróis
				if(l.x<-1.85&&abs(l.y)>0.45&&abs(l.y)<0.95)
					cc=vec3(0.95,0.1,0.1);                                                       // lanternas
				cc*=mix(1.0,0.6,smoothstep(-0.12,0.0,carro));                                  // contorno
				baseColor=vec4(cc,1.0);
				}
			else if(rodas)
				baseColor=vec4(0.08,0.08,0.08,1.0);
			else
				{
				if(sombra<0.0)
					baseColor.rgb*=0.7;
				baseColor.rgb+=vec3(1.0,0.95,0.6)*facho*0.35;
				}
			}
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
	}
)GLSL";

void Extras::uploadUniforms(const GLint locations[NUM_UNIFORMS]) const
	{
	glUniformMatrix4fvARB(locations[0],1,GL_FALSE,caixa);
	GLfloat ag[MAX_AGENTES*4];
	for(int i=0;i<MAX_AGENTES;++i)
		{
		bool visivel=i<quantos&&tipo[i]>0.5f&&vel[i]!=0.0f;
		ag[i*4+0]=ax[i];
		ag[i*4+1]=ay[i];
		ag[i*4+2]=ang[i];
		ag[i*4+3]=visivel?tipo[i]:0.0f;
		}
	glUniform4fvARB(locations[1],MAX_AGENTES,ag);
	glUniform4fARB(locations[2],tx,ty,traio,tz);
	glUniform4fARB(locations[3],GLfloat(tempo),achado?1.0f:0.0f,tesouro?GLfloat(tipoTesouro):0.0f,nivelAgua);
	glUniform4fARB(locations[4],GLfloat(efeito),referencia,GLfloat(tipoTesouro),0.0f);
	glUniform4fARB(locations[5],lsx,lsy,ltx,lty);
	glUniform4fARB(locations[6],lraio,labirinto?GLfloat(labEstado):0.0f,
	               labEstado==2?GLfloat(tempo-labChegou):GLfloat(tempo-labInicio),0.0f);
	}
