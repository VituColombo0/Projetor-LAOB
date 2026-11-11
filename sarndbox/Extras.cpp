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
	 modo(NADA),quantos(0),nivelAgua(SEM),semente(12345u),tempo(0.0),efeito(SEM_EFEITO)
	{
	bmin[0]=bmin[1]=-10.0f;
	bmax[0]=bmax[1]=10.0f;
	for(int i=0;i<16;++i)
		caixa[i]=(i%5==0)?1.0f:0.0f;
	for(int i=0;i<MAX_AGENTES;++i)
		{
		ax[i]=ay[i]=ang[i]=vel[i]=0.0f;
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
		if(queAgua?agua(x,y):(nivelAgua<SEM+1.0f||h>=nivelAgua+0.5f))
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
	vel[i]=peixe?(4.0f+aleatorio()*3.0f):(6.0f+aleatorio()*4.0f);
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
		if(peixe)
			{
			if(!agua(ax[i],ay[i])) // o lago secou embaixo dele: reaparece em outro lugar
				{
				nasce(i);
				continue;
				}
			ang[i]+=(aleatorio()-0.5f)*3.0f*dt; // passeia
			const float desvios[]={0.0f,0.5f,-0.5f,1.0f,-1.0f,1.6f,-1.6f,2.3f,-2.3f,PI};
			for(int k=0;k<10;++k)
				{
				float a=ang[i]+desvios[k];
				if(agua(ax[i]+std::cos(a)*3.0f,ay[i]+std::sin(a)*3.0f))
					{
					ang[i]+=std::max(-3.0f*dt,std::min(3.0f*dt,desvios[k]))+(k>0?desvios[k]*0.3f:0.0f);
					break;
					}
				}
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			if(agua(nx,ny))
				{
				ax[i]=nx;
				ay[i]=ny;
				}
			else
				ang[i]+=PI*0.5f*(aleatorio()<0.5f?1.0f:-1.0f);
			}
		else
			{
			/* Carrinho: entre as direções possíveis, a que segue mais baixo (o vale), sem subir
			   rampa íngreme, sem entrar no lago e sem sair da área iluminada: */
			float h0=altura(ax[i],ay[i]);
			const float desvios[]={0.0f,0.35f,-0.35f,0.7f,-0.7f,1.1f,-1.1f};
			float melhor=1e9f,escolha=PI;
			for(int k=0;k<7;++k)
				{
				float a=ang[i]+desvios[k];
				float px=ax[i]+std::cos(a)*4.0f,py=ay[i]+std::sin(a)*4.0f;
				float h=altura(px,py);
				if(h<SEM+1.0f||!dentro(px,py,2.0f)||agua(px,py)||(h0>SEM+1.0f&&h-h0>2.0f))
					continue;
				float nota=h+0.8f*std::abs(desvios[k]);
				if(nota<melhor)
					{
					melhor=nota;
					escolha=desvios[k];
					}
				}
			if(escolha==PI) // sem saída: dá meia-volta
				ang[i]+=PI*(0.8f+0.4f*aleatorio());
			else
				ang[i]+=std::max(-2.0f*dt,std::min(2.0f*dt,escolha))+(aleatorio()-0.5f)*0.4f*dt;
			float nx=ax[i]+std::cos(ang[i])*vel[i]*dt,ny=ay[i]+std::sin(ang[i])*vel[i]*dt;
			if(dentro(nx,ny,1.5f)&&!agua(nx,ny))
				{
				ax[i]=nx;
				ay[i]=ny;
				}
			}
		}
	}

/* ----------------------------------------------------------------- shader */

const char* Extras::nomesUniforms[NUM_UNIFORMS]={"extrasCaixa","extrasAg","extrasTesouro","extrasInfo","extrasEfeito"};

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
	for(int i=0;i<24;++i)
		{
		vec4 a=extrasAg[i];
		if(a.w<0.5)
			continue;
		vec2 r=ep.xy-a.xy;
		if(dot(r,r)>25.0)
			continue;
		float c=cos(a.z);
		float s=sin(a.z);
		vec2 l=vec2(c*r.x+s*r.y,-s*r.x+c*r.y);
		vec3 cor=vec3(0.55)+0.45*cos(6.2832*(vec3(fract(a.w))+vec3(0.0,0.33,0.67)));
		if(a.w<2.0)
			{
			l.y+=0.25*sin(et*9.0+l.x*1.5)*clamp(-l.x,0.0,3.0)/3.0;
			float corpo=(l.x*l.x)/4.84+(l.y*l.y)/0.81;
			bool cauda=l.x<-1.7&&l.x>-3.3&&abs(l.y)<(-l.x-1.7)*0.8;
			if(corpo<1.0||cauda)
				{
				vec3 pc=cor;
				if(corpo<1.0&&l.y>0.0)
					pc*=1.15;
				if(distance(l,vec2(1.3,0.25))<0.28)
					pc=vec3(0.05);
				baseColor=vec4(pc,1.0);
				}
			}
		else
			{
			if(abs(l.x)<2.1&&abs(l.y)<1.15)
				{
				vec3 cc=cor;
				if(l.x>-0.2&&l.x<1.0&&abs(l.y)<0.85)
					cc=vec3(0.75,0.9,1.0);
				if(l.x>1.8&&abs(l.y)>0.55)
					cc=vec3(1.0,1.0,0.6);
				baseColor=vec4(cc,1.0);
				}
			else if(abs(abs(l.x)-1.25)<0.45&&abs(l.y)<1.45)
				baseColor=vec4(0.05,0.05,0.05,1.0);
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
	}
