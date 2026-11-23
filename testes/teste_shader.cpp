/* Teste do shader dos extras (sarndbox/Extras.cpp) fora do SARndbox, na GPU da máquina, sem janela (EGL) e sem
   Kinect: pode rodar com a areia aberta no projetor (23/11/2026). Precisa de placa com OpenGL (o laob).
   - compila as 8 variantes (EX_JOGOS, EX_BICHOS, EX_MUNDO; Extras.h) e mostra quanto cada uma leva para compilar
     (com __GL_SHADER_DISK_CACHE=0, sem o cache do driver: o tempo de depois de instalar);
   - desenha ~360 cenas (os 7 temas, tesouro e fósseis, copie, chuva, os jogos, o labirinto, os 12 bichos de longe
     e de perto em vários estados, o laboratório) e compara pixel a pixel (a cor e extrasMarca/Objeto/Limpa/
     AguaSombra) a variante certa de cada cena com o shader com tudo ligado: tirar um bloco por #ifdef não pode
     mudar a imagem;
   - com --antes <Extras.cpp de antes>: compara também com o shader de antes (o corte por espécie, as variantes e
     a divisão do tempo tinham de deixar a imagem igual);
   - a virada do tempo dobrado (Extras::tempoSeno/tempoFracao): o quadro logo depois da virada contra o que
     sairia sem dobrar;
   - com --gpu: o tempo de GPU de algumas cenas numa tela de 1024x768 (com --antes, o de antes também).
   uso (no laob): g++ -O1 -I sarndbox testes/teste_shader.cpp -o /tmp/teste_shader -lEGL -lGL
                  /tmp/teste_shader sarndbox/Extras.cpp [--antes /tmp/Extras-antes.cpp] [--gpu] */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "MundoPlanta.h"

/* o texto de uma das strings GLSL do Extras.cpp: as raw strings R"GLSL(...)GLSL" em seguida, com o MP_GLSL
   (as constantes da planta, MundoPlanta.h) no meio */
static std::string glslDe(const std::string& s,const char* nome)
	{
	std::string chave=std::string("const char* Extras::")+nome+"=";
	size_t i=s.find(chave);
	if(i==std::string::npos)
		{
		std::fprintf(stderr,"não achei %s\n",nome);
		std::exit(2);
		}
	i+=chave.size();
	std::string r;
	while(i<s.size())
		{
		if(std::isspace((unsigned char)s[i]))
			++i;
		else if(s.compare(i,7,"R\"GLSL(")==0)
			{
			size_t j=s.find(")GLSL\"",i+7);
			r+=s.substr(i+7,j-(i+7));
			i=j+6;
			}
		else if(s.compare(i,7,"MP_GLSL")==0)
			{
			r+=MP_GLSL;
			i+=7;
			}
		else if(s[i]==';')
			return r;
		else
			break;
		}
	std::fprintf(stderr,"não entendi a string %s\n",nome);
	std::exit(2);
	}
static std::string le(const char* arq)
	{
	std::ifstream f(arq);
	if(!f)
		{
		std::fprintf(stderr,"não li %s\n",arq);
		std::exit(2);
		}
	std::stringstream b;
	b<<f.rdbuf();
	return b.str();
	}

#define F(tipo,nome) static tipo nome=0;
F(PFNGLCREATESHADERPROC,pCreateShader) F(PFNGLSHADERSOURCEPROC,pShaderSource) F(PFNGLCOMPILESHADERPROC,pCompileShader)
F(PFNGLGETSHADERIVPROC,pGetShaderiv) F(PFNGLGETSHADERINFOLOGPROC,pGetShaderInfoLog) F(PFNGLCREATEPROGRAMPROC,pCreateProgram)
F(PFNGLATTACHSHADERPROC,pAttachShader) F(PFNGLLINKPROGRAMPROC,pLinkProgram) F(PFNGLGETPROGRAMIVPROC,pGetProgramiv)
F(PFNGLGETPROGRAMINFOLOGPROC,pGetProgramInfoLog) F(PFNGLUSEPROGRAMPROC,pUseProgram) F(PFNGLGETUNIFORMLOCATIONPROC,pGetUniformLocation)
F(PFNGLUNIFORM4FVPROC,pUniform4fv) F(PFNGLUNIFORM1FVPROC,pUniform1fv) F(PFNGLUNIFORM1IPROC,pUniform1i) F(PFNGLUNIFORM1FPROC,pUniform1f)
F(PFNGLGENFRAMEBUFFERSPROC,pGenFramebuffers) F(PFNGLBINDFRAMEBUFFERPROC,pBindFramebuffer) F(PFNGLFRAMEBUFFERTEXTURE2DPROC,pFramebufferTexture2D)
F(PFNGLCHECKFRAMEBUFFERSTATUSPROC,pCheckFramebufferStatus) F(PFNGLACTIVETEXTUREPROC,pActiveTexture) F(PFNGLDELETEPROGRAMPROC,pDeleteProgram)
F(PFNGLGENQUERIESPROC,pGenQueries) F(PFNGLBEGINQUERYPROC,pBeginQuery) F(PFNGLENDQUERYPROC,pEndQuery) F(PFNGLGETQUERYOBJECTUI64VPROC,pGetQueryObjectui64v)
#undef F
template<class T> void pega(T& f,const char* n) {f=(T)eglGetProcAddress(n); if(!f) {std::fprintf(stderr,"falta %s\n",n); std::exit(2);}}

static const char* VS=R"(
uniform vec4 area;
varying vec3 extrasPos;
float relevo(vec2 p)
	{
	vec2 d=p-vec2(-8.0,4.0);
	return 4.8+3.0*sin(p.x*0.21)*cos(p.y*0.17)+1.5*sin(p.x*0.05+p.y*0.08)-2.5*exp(-dot(d,d)/40.0);
	}
void main()
	{
	vec2 p=mix(area.xy,area.zw,gl_Vertex.xy);
	extrasPos=vec3(p,relevo(p));
	gl_Position=vec4(gl_Vertex.xy*2.0-1.0,0.0,1.0);
	}
)";

static std::string fonteFS(const char* fu,const char* fm,const std::string& defs)
	{
	std::string s="#extension GL_ARB_texture_rectangle : enable\n";
	s+=defs;
	s+=fu;
	s+="uniform float saida;\nvoid main()\n\t{\n\tfloat zc=extrasPos.z;\n";
	s+="\tvec4 baseColor=vec4(clamp(vec3(0.15+0.11*zc,0.45+0.3*sin(zc*1.3),0.95-0.13*zc),0.0,1.0),1.0);\n";
	s+=fm;
	s+="\tif(saida<0.5)\n\t\tgl_FragColor=vec4(baseColor.rgb,1.0);\n\telse\n\t\tgl_FragColor=vec4(extrasMarca,extrasObjeto,extrasLimpa,extrasAguaSombra);\n\t}\n";
	return s;
	}

static GLuint compila(GLenum tipo,const std::string& src,const char* nome)
	{
	GLuint s=pCreateShader(tipo);
	const char* p=src.c_str();
	pShaderSource(s,1,&p,0);
	pCompileShader(s);
	GLint ok=0;
	pGetShaderiv(s,GL_COMPILE_STATUS,&ok);
	if(!ok)
		{
		char log[8192];
		pGetShaderInfoLog(s,sizeof(log),0,log);
		std::fprintf(stderr,"ERRO compilando %s:\n%s\n",nome,log);
		return 0;
		}
	return s;
	}

struct Programa {GLuint p=0; double ms=0.0; std::string nome;};
static Programa programa(const char* fu,const char* fm,const std::string& defs,const std::string& nome)
	{
	Programa r;
	r.nome=nome;
	auto t0=std::chrono::steady_clock::now();
	GLuint v=compila(GL_VERTEX_SHADER,VS,"VS");
	GLuint f=compila(GL_FRAGMENT_SHADER,fonteFS(fu,fm,defs),nome.c_str());
	if(!v||!f)
		return r;
	GLuint p=pCreateProgram();
	pAttachShader(p,v);
	pAttachShader(p,f);
	pLinkProgram(p);
	GLint ok=0;
	pGetProgramiv(p,GL_LINK_STATUS,&ok);
	if(!ok)
		{
		char log[8192];
		pGetProgramInfoLog(p,sizeof(log),0,log);
		std::fprintf(stderr,"ERRO ligando %s:\n%s\n",nome.c_str(),log);
		return r;
		}
	glFinish();
	r.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
	r.p=p;
	return r;
	}

/* ---- cena: os valores dos uniforms dos extras */
struct Cena
	{
	std::string nome;
	unsigned variante=0;
	float area[4]={-30.0f,-21.0f,30.0f,21.0f};
	float ag[32*4]={0},ag2[32*4]={0},tesouro[4]={0},info[4]={1.0f,0,0,-1000.0f},efeito[4]={0,4.8f,1.0f,0};
	float lab[4]={0},lab2[4]={0},jogo[32*4]={0},jogoInfo[4]={0},nomes[72]={0},nomesCfg[4]={0};
	float mundo[4]={0,60.0f,42.0f,0},mundo2[4]={-30.0f,-21.0f,12.0f,1.0f},chuva[16]={0},copia[4]={0},comida[4]={0};
	float racao[25*4]={0},vista[4]={0.5f,0.5f,1.0f,0};
	void tempo(float t) {info[0]=t; efeito[2]=t;} // pequeno: o de sin/cos e o de fract são o mesmo
	Cena(void) {racao[0]=1e9f;racao[1]=1e9f;racao[2]=-1e9f;racao[3]=-1e9f;for(int k=1;k<25;++k) racao[k*4+2]=-1.0f;}
	};

static void envia(GLuint p,const Cena& c)
	{
	pUseProgram(p);
	auto L=[&](const char* n){return pGetUniformLocation(p,n);};
	pUniform4fv(L("area"),1,c.area);
	pUniform4fv(L("extrasAg"),32,c.ag);
	pUniform4fv(L("extrasAg2"),32,c.ag2);
	pUniform4fv(L("extrasTesouro"),1,c.tesouro);
	pUniform4fv(L("extrasInfo"),1,c.info);
	pUniform4fv(L("extrasEfeito"),1,c.efeito);
	pUniform4fv(L("extrasLab"),1,c.lab);
	pUniform4fv(L("extrasLab2"),1,c.lab2);
	pUniform4fv(L("extrasJogo"),32,c.jogo);
	pUniform4fv(L("extrasJogoInfo"),1,c.jogoInfo);
	pUniform1fv(L("extrasNomes"),72,c.nomes);
	pUniform4fv(L("extrasNomesCfg"),1,c.nomesCfg);
	pUniform1i(L("extrasRostos"),6);
	pUniform4fv(L("extrasMundo"),1,c.mundo);
	pUniform4fv(L("extrasMundo2"),1,c.mundo2);
	pUniform4fv(L("extrasChuva"),4,c.chuva);
	pUniform4fv(L("extrasCopiaInfo"),1,c.copia);
	pUniform4fv(L("extrasComida"),1,c.comida);
	pUniform4fv(L("extrasRacao"),25,c.racao);
	pUniform4fv(L("extrasVista"),1,c.vista);
	}

static int W=480,H=336;
static GLuint fbo,tex;
static void desenha(GLuint p,const Cena& c,float saida,std::vector<float>& px)
	{
	envia(p,c);
	pUniform1f(pGetUniformLocation(p,"saida"),saida);
	glViewport(0,0,W,H);
	const int N=96;
	glBegin(GL_QUADS);
	for(int j=0;j<N;++j)
		for(int i=0;i<N;++i)
			{
			float x0=float(i)/N,x1=float(i+1)/N,y0=float(j)/N,y1=float(j+1)/N;
			glVertex2f(x0,y0);glVertex2f(x1,y0);glVertex2f(x1,y1);glVertex2f(x0,y1);
			}
	glEnd();
	px.resize(size_t(W)*H*4);
	glReadPixels(0,0,W,H,GL_RGBA,GL_FLOAT,px.data());
	}

static unsigned semente=1;
static float al(void) {semente=semente*1664525u+1013904223u; return float((semente>>8)&0xffffff)/float(0x1000000);}

static int falhas=0,iguais=0;
static void compara(const Programa& a,const Programa& b,const Cena& c)
	{
	std::vector<float> pa,pb;
	int dif[2]={0,0};
	float mx[2]={0,0};
	for(int s=0;s<2;++s)
		{
		desenha(a.p,c,float(s),pa);
		desenha(b.p,c,float(s),pb);
		for(size_t k=0;k<pa.size();++k)
			{
			float d=std::fabs(pa[k]-pb[k]);
			if(d>0.0f||(std::isnan(pa[k])!=std::isnan(pb[k])))
				{
				++dif[s];
				mx[s]=std::max(mx[s],d);
				}
			}
		}
	bool ok=dif[0]==0&&dif[1]==0;
	if(!ok)
		++falhas;
	if(ok)
		{
		++iguais;
		return;
		}
	std::printf("%s %-42s %s x %s: cor %d px diferentes (max %.2g), marcas %d (max %.2g)\n",ok?"  ok":"FALHA",c.nome.c_str(),a.nome.c_str(),b.nome.c_str(),dif[0],mx[0],dif[1],mx[1]);
	}

static void bicho(Cena& c,int i,float esp,float x,float y)
	{
	c.ag[i*4]=x; c.ag[i*4+1]=y; c.ag[i*4+2]=al()*6.2832f; c.ag[i*4+3]=esp+0.05f+0.9f*al();
	float acao=float(int(al()*5.0f)%5);
	c.ag2[i*4]=acao; c.ag2[i*4+1]=al(); c.ag2[i*4+2]=al()<0.3f?0.72f:1.0f; c.ag2[i*4+3]=al();
	}

int main(int argc,char** argv)
	{
	if(argc<2||argv[1][0]=='-')
		{
		std::fprintf(stderr,"uso: %s sarndbox/Extras.cpp [--antes Extras-antes.cpp] [--gpu]\n",argv[0]);
		return 2;
		}
	bool tempoGpu=false;
	const char* arqAntes=0;
	for(int i=2;i<argc;++i)
		{
		if(std::strcmp(argv[i],"--gpu")==0)
			tempoGpu=true;
		else if(std::strcmp(argv[i],"--antes")==0&&i+1<argc)
			arqAntes=argv[++i];
		}
	std::string fonte=le(argv[1]);
	std::string fuAgora=glslDe(fonte,"glslFragmentUniforms"),fmAgora=glslDe(fonte,"glslFragmentMain");
	std::string fuAntes,fmAntes;
	if(arqAntes)
		{
		std::string velho=le(arqAntes);
		fuAntes=glslDe(velho,"glslFragmentUniforms");
		fmAntes=glslDe(velho,"glslFragmentMain");
		/* a única mudança de desenho de propósito: as marcas da plataforma do scanner com 3/pi exato em vez de
		   0,955 (para o giro dobrado não pular na virada; muda 3 px na borda das marcas). O resto tem de ficar igual */
		size_t k=fuAntes.find("-th)*0.955)");
		if(k!=std::string::npos)
			fuAntes.replace(k,11,"-th)*0.9549297)");
		}
	/* EGL sem janela, na placa (sem X) */
	PFNEGLQUERYDEVICESEXTPROC qd=(PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
	PFNEGLGETPLATFORMDISPLAYEXTPROC gpd=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	EGLDeviceEXT devs[8]; EGLint nd=0;
	if(!qd||!gpd||!qd(8,devs,&nd)||nd<1) {std::fprintf(stderr,"sem EGL device\n");return 2;}
	EGLDisplay dpy=gpd(EGL_PLATFORM_DEVICE_EXT,devs[0],0);
	EGLint ma,mi;
	if(!eglInitialize(dpy,&ma,&mi)) {std::fprintf(stderr,"eglInitialize\n");return 2;}
	EGLint at[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE};
	EGLConfig cfg; EGLint nc=0;
	eglChooseConfig(dpy,at,&cfg,1,&nc);
	EGLint pb[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
	EGLSurface sf=eglCreatePbufferSurface(dpy,cfg,pb);
	eglBindAPI(EGL_OPENGL_API);
	EGLContext ctx=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,0);
	if(!ctx||!eglMakeCurrent(dpy,sf,sf,ctx)) {std::fprintf(stderr,"sem contexto\n");return 2;}
	std::printf("GL: %s / %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
	pega(pCreateShader,"glCreateShader");pega(pShaderSource,"glShaderSource");pega(pCompileShader,"glCompileShader");
	pega(pGetShaderiv,"glGetShaderiv");pega(pGetShaderInfoLog,"glGetShaderInfoLog");pega(pCreateProgram,"glCreateProgram");
	pega(pAttachShader,"glAttachShader");pega(pLinkProgram,"glLinkProgram");pega(pGetProgramiv,"glGetProgramiv");
	pega(pGetProgramInfoLog,"glGetProgramInfoLog");pega(pUseProgram,"glUseProgram");pega(pGetUniformLocation,"glGetUniformLocation");
	pega(pUniform4fv,"glUniform4fv");pega(pUniform1fv,"glUniform1fv");pega(pUniform1i,"glUniform1i");pega(pUniform1f,"glUniform1f");
	pega(pGenFramebuffers,"glGenFramebuffers");pega(pBindFramebuffer,"glBindFramebuffer");pega(pFramebufferTexture2D,"glFramebufferTexture2D");
	pega(pCheckFramebufferStatus,"glCheckFramebufferStatus");pega(pActiveTexture,"glActiveTexture");pega(pDeleteProgram,"glDeleteProgram");
	pega(pGenQueries,"glGenQueries");pega(pBeginQuery,"glBeginQuery");pega(pEndQuery,"glEndQuery");pega(pGetQueryObjectui64v,"glGetQueryObjectui64v");

	/* painel de rostos de mentira na unidade 6 */
	GLuint rost;
	glGenTextures(1,&rost);
	pActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D,rost);
	std::vector<unsigned char> img(1024*128*4);
	for(size_t k=0;k<img.size();++k) img[k]=(unsigned char)((k*37)&255);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1024,128,0,GL_RGBA,GL_UNSIGNED_BYTE,img.data());
	pActiveTexture(GL_TEXTURE0);

	/* tela de teste em ponto flutuante */
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1024,768,0,GL_RGBA,GL_FLOAT,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	pGenFramebuffers(1,&fbo);
	pBindFramebuffer(GL_FRAMEBUFFER,fbo);
	pFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
	if(pCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {std::fprintf(stderr,"fbo\n");return 2;}

	/* programas */
	std::printf("== compilação (ms; sem o cache do driver se __GL_SHADER_DISK_CACHE=0)\n");
	Programa antes;
	if(arqAntes)
		antes=programa(fuAntes.c_str(),fmAntes.c_str(),"","antes");
	const char* nomesV[8]={"v0","J","B","JB","M","JM","BM","JBM"};
	Programa v[8];
	for(unsigned k=0;k<8;++k)
		{
		std::string d;
		if(k&1) d+="#define EX_JOGOS\n";
		if(k&2) d+="#define EX_BICHOS\n";
		if(k&4) d+="#define EX_MUNDO\n";
		v[k]=programa(fuAgora.c_str(),fmAgora.c_str(),d,nomesV[k]);
		}
	if(arqAntes)
		std::printf("  antes (tudo junto) %.0f;",antes.ms);
	for(unsigned k=0;k<8;++k)
		std::printf(" %s %.0f;",nomesV[k],v[k].ms);
	std::printf("\n");
	if(arqAntes&&!antes.p)
		return 1;
	for(unsigned k=0;k<8;++k)
		if(!v[k].p)
			{
			std::printf("FALHA: a variante %s não compilou\n",nomesV[k]);
			return 1;
			}

	/* cenas */
	std::vector<Cena> cenas;
	const float tempos[3]={0.37f,2.9f,7.3f};
	for(int e=0;e<=6;++e)
		for(int ti=0;ti<3;++ti)
			{
			Cena c; c.nome="tema "+std::to_string(e)+" t"+std::to_string(ti); c.efeito[0]=float(e); c.tempo(tempos[ti]);
			cenas.push_back(c);
			}
	for(int tipo=1;tipo<=5;++tipo)
		for(int achou=0;achou<2;++achou)
			{
			Cena c; c.nome="tesouro "+std::to_string(tipo)+(achou?" achado":""); c.tempo(3.1f);
			c.tesouro[0]=2.0f;c.tesouro[1]=-1.0f;c.tesouro[2]=tipo>=2?8.5f:5.0f;c.tesouro[3]=4.0f;
			c.info[1]=achou?1.0f+2.2f:0.0f; c.info[2]=float(tipo); c.efeito[0]=float(tipo%7);
			cenas.push_back(c);
			}
	for(int k=0;k<2;++k)
		{
		Cena c; c.nome=k?"copie conseguiu":"copie 57%"; c.tempo(1.3f); c.copia[0]=1.0f; c.copia[3]=k?1091.0f:57.0f;
		cenas.push_back(c);
		}
	{
	Cena c; c.nome="chuva"; c.tempo(2.2f);
	float ch[16]={5,3,0.8f,2.0f, -12,-6,0.4f,0.3f, 15,8,1.0f,5.5f, 0,0,0,0};
	std::memcpy(c.chuva,ch,sizeof(ch));
	cenas.push_back(c);
	}
	/* jogos: as peças de cada tipo, em cada estado */
	const int jogos[4]={1,2,4,6};
	for(int jj=0;jj<4;++jj)
		for(int fase=1;fase<=2;++fase)
			{
			Cena c; c.nome="jogo "+std::to_string(jogos[jj])+" fase "+std::to_string(fase); c.variante=1; c.tempo(4.4f);
			c.jogoInfo[0]=float(jogos[jj]); c.jogoInfo[1]=float(fase); c.jogoInfo[2]=3.3f; c.jogoInfo[3]=1.0f;
			if(jogos[jj]==6) c.efeito[3]=1.0f;
			const int tipos[8]={1,2,3,3,5,10,11,12};
			for(int i=0;i<8;++i)
				{
				float* a=c.jogo+i*8;
				a[0]=-24.0f+i*7.0f; a[1]=(i%2?6.0f:-6.0f); a[2]=3.0f+0.3f*i; a[3]=float(tipos[i]);
				a[4]=float(i%3); a[5]=0.0f; a[6]=float(i%4); a[7]=float(i%2);
				}
			c.efeito[0]=float(jj+fase);
			cenas.push_back(c);
			}
	for(int est=1;est<=2;++est)
		{
		Cena c; c.nome="labirinto estado "+std::to_string(est); c.variante=1; c.tempo(5.0f);
		c.lab[0]=-20;c.lab[1]=0;c.lab[2]=20;c.lab[3]=5; c.lab2[0]=4.0f;c.lab2[1]=float(est);c.lab2[2]=1.7f;
		c.jogoInfo[0]=99.0f; c.jogoInfo[1]=0.0f;
		for(int i=0;i<3;++i) {float* a=c.jogo+i*8; a[0]=-8.0f+8.0f*i; a[1]=2.0f; a[2]=3.2f; a[3]=12.0f; a[4]=float(i%2);}
		cenas.push_back(c);
		}
	/* bichos de longe: 32, todas as espécies, com lago, comida e ração, em cada tema */
	for(int e=0;e<=6;++e)
		for(int lago=0;lago<2;++lago)
			{
			Cena c; c.nome="bichos tema "+std::to_string(e)+(lago?" lago":""); c.variante=2; c.tempo(1.1f+0.9f*e);
			c.efeito[0]=float(e); c.info[3]=lago?4.6f:-1000.0f;
			for(int i=0;i<32;++i)
				bicho(c,i,float(1+i%12),-27.0f+54.0f*al(),-18.0f+36.0f*al());
			c.comida[0]=-5;c.comida[1]=4;c.comida[2]=4.5f;c.comida[3]=0.6f;
			c.racao[0]=4;c.racao[1]=-6;c.racao[2]=16;c.racao[3]=4;
			for(int k=1;k<=6;++k) {float* r=c.racao+k*4; r[0]=6+1.5f*k; r[1]=-4+0.8f*k; r[2]=k<3?0.2f:3.0f; r[3]=std::floor((0.4f+0.1f*k)*20.0f)+0.3f;}
			cenas.push_back(c);
			}
	/* bichos de perto: uma espécie no meio, em vários estados e ângulos (o corte por espécie e o Destaque) */
	for(int esp=1;esp<=12;++esp)
		for(int k=0;k<24;++k)
			{
			Cena c; c.nome="perto especie "+std::to_string(esp)+" #"+std::to_string(k); c.variante=2; c.tempo(0.5f+0.29f*k);
			c.area[0]=-9;c.area[1]=-9;c.area[2]=9;c.area[3]=9;
			c.efeito[0]=float(k%7);
			c.info[3]=(k%2)?6.5f:-1000.0f; // lago cobrindo (molhado) ou sem lago
			bicho(c,0,float(esp),0.0f,0.0f);
			c.ag2[0]=float(k%5); c.ag2[1]=float(k%6)/5.0f; c.ag2[2]=(k%4==3)?0.72f:1.0f; c.ag2[3]=float(k%7)/6.0f;
			bicho(c,1,float(1+(esp+3)%12),6.5f,-5.0f); // um vizinho perto (o Destaque de um sobre o outro)
			cenas.push_back(c);
			}
	/* Mundo LAOB */
	for(int k=0;k<12;++k)
		{
		Cena c; c.nome="mundo #"+std::to_string(k); c.variante=6; c.tempo(1.0f+0.6f*k);
		float bits=float(int(al()*4096.0f)),bits2=float(int(al()*16.0f));
		c.mundo[0]=bits; c.mundo[3]=1.0f+2.0f*bits2; c.mundo2[2]=k%3==0?3.0f:(k%3==1?12.0f:20.0f);
		if(k%4==3) {c.vista[0]=0.35f;c.vista[1]=0.6f;c.vista[2]=2.0f;}
		for(int i=0;i<6;++i)
			{
			bicho(c,i,3.0f,-25.0f+50.0f*al(),-17.0f+34.0f*al());
			c.ag2[i*4]=float(i%7); c.ag2[i*4+3]=float(i<4?i+1:0);
			}
		c.nomesCfg[0]=4; c.nomesCfg[1]=float(k%4); c.nomesCfg[2]=2;
		for(int i=0;i<4;++i)
			{
			c.nomes[i*9]=float(5+(i%2?16:0)+(i==0?32:0));
			for(int l=0;l<5;++l) c.nomes[i*9+1+l]=float(0x5BEA+l*37);
			}
		c.ag2[4*4]=5.0f; // conversa
		cenas.push_back(c);
		}

	std::printf("== %s: %zu cenas (só as diferenças aparecem)\n",arqAntes?"antes x a variante de cada cena, e ela x tudo ligado":"a variante de cada cena x tudo ligado",cenas.size());
	for(size_t i=0;i<cenas.size();++i)
		{
		if(arqAntes)
			compara(antes,v[cenas[i].variante],cenas[i]);
		if(cenas[i].variante!=7)
			compara(v[7],v[cenas[i].variante],cenas[i]);
		}

	{
	/* virada do tempo dobrado (23/11/2026): o quadro logo depois da virada (tempo pequeno) contra o que sairia sem
	   dobrar (tempo + período). Só informa: sobra a imprecisão do float perto de 25600 s (medido no laob: menos
	   pixels mudam do que com 3 ms de animação) e, no laboratório, o slide do miniauditório e os LEDs sorteados,
	   que trocam de conteúdo, mas no instante em que já trocariam (o período é múltiplo do deles) */
	std::printf("== virada do tempo: sem dobrar (tempo + período) x dobrado; px com diferença > 0,05\n");
	const double PF=25600.0,PS=200.0*3.14159265358979323846;
	std::vector<float> pa,pb;
	for(size_t i=0;i<cenas.size();++i)
		{
		const Cena& c0=cenas[i];
		if(c0.nome.find("perto")!=std::string::npos&&c0.nome.find("#0")==std::string::npos)
			continue;
		for(int qual=0;qual<2;++qual)
			{
			Cena a=c0,b=c0;
			if(qual==0) a.info[0]=float(PF+c0.info[0]);
			else a.efeito[2]=float(PS+c0.efeito[2]);
			int n=0; float mx=0.0f;
			for(int s=0;s<2;++s)
				{
				desenha(v[7].p,a,float(s),pa);
				desenha(v[7].p,b,float(s),pb);
				for(size_t k=0;k<pa.size();++k)
					{
					float d=std::fabs(pa[k]-pb[k]);
					mx=std::max(mx,d);
					n+=d>0.05f;
					}
				}
			if(n>0||mx>0.05f)
				std::printf("  %-40s %s: %d px (max %.2f)\n",c0.nome.c_str(),qual?"sin/cos (es)":"fract (et)",n,mx);
			}
		}
	std::printf("  (fim da virada)\n");
	}
	if(tempoGpu)
		{
		/* tempo de GPU numa tela de 1024x768 (média de 20 quadros) */
		W=1024;H=768;
		GLuint q;
		pGenQueries(1,&q);
		auto mede=[&](GLuint p,const Cena& c)->double
			{
			envia(p,c);
			pUniform1f(pGetUniformLocation(p,"saida"),0.0f);
			glViewport(0,0,W,H);
			double soma=0.0;
			for(int r=0;r<25;++r)
				{
				pBeginQuery(GL_TIME_ELAPSED,q);
				glBegin(GL_QUADS);
				const int N=96;
				for(int j=0;j<N;++j)
					for(int i=0;i<N;++i)
						{
						float x0=float(i)/N,x1=float(i+1)/N,y0=float(j)/N,y1=float(j+1)/N;
						glVertex2f(x0,y0);glVertex2f(x1,y0);glVertex2f(x1,y1);glVertex2f(x0,y1);
						}
				glEnd();
				pEndQuery(GL_TIME_ELAPSED);
				GLuint64 ns=0;
				pGetQueryObjectui64v(q,GL_QUERY_RESULT,&ns);
				if(r>=5) soma+=double(ns)*1e-6;
				}
			return soma/20.0;
			};
		std::printf("== GPU (ms por quadro de 1024x768, só o desenho dos extras + o relevo de mentira)\n");
		std::vector<int> idx;
		for(size_t i=0;i<cenas.size();++i)
			if(cenas[i].nome=="tema 0 t0"||cenas[i].nome=="tema 3 t0"||cenas[i].nome=="tesouro 1"||cenas[i].nome=="copie 57%"||cenas[i].nome=="jogo 2 fase 1"
			   ||cenas[i].nome=="labirinto estado 1"||cenas[i].nome=="bichos tema 0"||cenas[i].nome=="bichos tema 5 lago"||cenas[i].nome=="mundo #1")
				idx.push_back(int(i));
		for(int i:idx)
			{
			const Cena& c=cenas[i];
			double b=mede(v[c.variante].p,c);
			if(arqAntes)
				std::printf("  %-28s antes %.2f  agora %.2f (%s)\n",c.nome.c_str(),mede(antes.p,c),b,nomesV[c.variante]);
			else
				std::printf("  %-28s %.2f (%s)\n",c.nome.c_str(),b,nomesV[c.variante]);
			}
		}
	std::printf("%d comparações iguais\n",iguais);
	std::printf(falhas?"HÁ FALHAS (%d)\n":"TUDO OK\n",falhas);
	return falhas?1:0;
	}
